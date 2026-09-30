#include <list>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <unordered_map>
#include <functional>
#include <unistd.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QApplication>
#include "DataDec.h"
#include "DataEnc.h"
#include "UDPClient.h"
#include "TCPServer.h"
#include "LANShare.h"
#include "IOUtils.h"
#include "NetWorldUtils.h"
#include "TokenDBUtil.h"
#include "TimeTools.h"
#include "CodeUtils.h"
#include "Utils.h"
#include "LException.h"
#include "ByteUtils.h"

#include "LHttpServer.h"
#include "ByteArrayIOUtils.h"
#include "StringLockManager.h"
#include <utility>
#include <vector>

#include "BatteryUtils.h"
#include "MediaIdPathDBUtil.h"

LANShare *instance = nullptr;

#define BUFF_SIZE (1024 * 1024 * 2)

// ===== 分段并行传输（FS_SHARE_SEG）全局协调表 =====
// 对应安卓端 SegCoord.MAP：按 segId 聚合同一文件的多条连接。
static std::mutex g_segMapMutex;
static std::unordered_map<std::string, SegCoord *> g_segMap;

// 加入某个 segId 的分段传输，返回该 segId 唯一的协调器。
// 落盘路径必须「算完再发布」：resolver 只在锁内、由竞争胜出的首段调用一次，
// 所有段都用 coord.filePath 那一个路径，
// 否则 avoidDuplication 每调一次得到不同文件名，会把一个文件拆成多个。
typedef std::function<QString()> SegPathResolver;
static SegCoord *segCoordJoin(const std::string &segId, mlong fileSize, int segCount,
                               LFile *fileContent, const QString &peerName,
                               const SegPathResolver &resolver) {
    {
        std::lock_guard<std::mutex> lk(g_segMapMutex);
        auto it = g_segMap.find(segId);
        if (it != g_segMap.end()) return it->second;
    }
    std::lock_guard<std::mutex> lk(g_segMapMutex);
    auto it = g_segMap.find(segId);
    if (it != g_segMap.end()) return it->second;
    QString path = resolver();
    auto *c = new SegCoord();
    c->segId = segId;
    c->fileSize = fileSize;
    c->segCount = segCount;
    c->fileContent = fileContent;
    c->filePath = path;
    c->peerName = peerName;
    c->creatorId = std::this_thread::get_id();
    g_segMap[segId] = c;
    return c;
}

static void segCoordRemove(const std::string &segId) {
    std::lock_guard<std::mutex> lk(g_segMapMutex);
    auto it = g_segMap.find(segId);
    if (it != g_segMap.end()) {
        delete it->second;
        g_segMap.erase(it);
    }
}

void LANShare::addDevice(const Device &device) {
    std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
    lock.lock();
    onLineDevices[device.getDevIp().toStdString() + ":" +
                  std::to_string(device.getDevPort())] = device;
    lock.unlock();
    LHttpServer::sendDeviceList();
}

void LANShare::removeDevice(const Device &device) {
    std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
    lock.lock();
    onLineDevices.erase(
        device.getDevIp().toStdString() + ":" + std::to_string(device.getDevPort()));
    lock.unlock();
    LHttpServer::sendDeviceList();
}

LANShare::LANShare(LANShareWindow *mainWindow) : mainWindow(mainWindow) {
    // qDebug() << "Battery: " <<BatteryUtils::getBatteryPercentage();
    instance = this;
    updateMDevices();
    mainWindow->updateWebServiceIp();
    udpServer = std::make_unique<UDPServer>(config.udpPort);
    tcpServer = std::make_unique<TCPServer>(config.tcpPort);
    tcpServer->bind();
    lhttpServer = std::make_unique<LHttpServer>(this);
    // 通知设备我已上线
    for (const auto &device: getMDevices()) {
        noticeDeviceOnLineByIp(device.getDevBrotIp());
    }
}

LANShare::~LANShare() {
    instance = nullptr;
}

void LANShare::updateMDevices() {
    std::mutex &lock = StringLockManager::getStringLock("mDevicesMutex");
    lock.lock();
    mDevices = NetWorldUtils::getDevices();
    lock.unlock();
}

void encData(mbyte *buffer, int len, int off, mlong index) {
    int j = 0;
    for (int i = off; i < len + off; i++) {
        int v = (buffer[i] - 1) ^ (int) ((index + j) & 0xFF);
        buffer[i] = (mbyte) v;
        j++;
    }
}

void decData(mbyte *buffer, int len, int off, mlong index) {
    int j = 0;
    for (int i = off; i < len + off; i++) {
        int v = (buffer[i] ^ (int) ((index + j) & 0xFF)) + 1;
        buffer[i] = (mbyte) v;
        j++;
    }
}

std::unique_ptr<TCPClient> makeSocket(QString ip, int port) {
    auto client = std::make_unique<TCPClient>(std::move(ip), port);
    if (!client->connect()) {
        return nullptr;
    }
    auto magicBytes = std::make_unique<mbyte[]>(4);
    ByteUtils::intToBytes(MAGIC_NUM, magicBytes.get());
    try {
        client->send(magicBytes.get(), 4);
    } catch (LException &e) {
        return nullptr;
    }
    TimeTools::sleep_ms(10);
    return client;
}

void makeDataEnc(const Device &device, DataEnc *dataEnc) {
    dataEnc->putInt(device.getDevPort());
    dataEnc->putString(device.getDevIp());
    dataEnc->putString(device.getDevName());
    dataEnc->putInt(device.getDevMode());
    dataEnc->putString(config.uniqueUUid);
    dataEnc->putInt(DATA_VERSION);
    dataEnc->putInt(device.getBatteryLevel());
    dataEnc->putByte(device.getChargeStatus());
    // webDeviceCount：PC 没有 web 设备，固定写 0 占位。
    // 必须写：接收端无条件读这个字段，缺了它会把后面的消息长度前缀误读成
    // webDeviceCount，进而错位读取，把消息内容吃掉，表现为收到空白文本。
    dataEnc->putInt(0);
}

void udpSend(UDPServer *udpServer, DataEnc *dataEnc, const QString &ip, int port) {
    mbyte *data = dataEnc->encData();
    auto *newBytes = new mbyte[2048 + 4];
    ByteUtils::intToBytes(MAGIC_NUM, newBytes);
    int dataLen = dataEnc->getDataLen();
    memcpy(newBytes + 4, data, dataLen);
    udpServer->sendto(ip, port, newBytes, dataLen + 4);
    delete[] newBytes;
}

void udpSend(UDPClient *udpClient, DataEnc *dataEnc, const QString &ip, int port) {
    mbyte *data = dataEnc->encData();
    auto *newBytes = new mbyte[2048 + 4];
    ByteUtils::intToBytes(MAGIC_NUM, newBytes);
    int dataLen = dataEnc->getDataLen();
    memcpy(newBytes + 4, data, dataLen);
    udpClient->sendto(ip, port, newBytes, dataLen + 4);
    delete[] newBytes;
}

mlong baseSend(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc) {
    qDebug("lFile：%llX", file);
    mlong totalSend = size;
    mlong thatSend = 0;
    IOInter *ioUtils;
    if (f->getType() == LFile::BYTEARRAY) {
        ioUtils = new ByteArrayIOUtils(f->getByteArray());
    } else if (f->getType() == LFile::FILE) {
        ioUtils = new IOUtils(f->path, QFile::ReadOnly);
    } else if (f->getType() == LFile::STREAM) {
        ioUtils = f->getIoInter();
    } else {
        qDebug("发送的文件类型不支持");
        return -1;
    }
    qDebug("发送文件: %s", f->fileName.toStdString().c_str());
    try {
        qDebug("文件大小: %lld", f->fileSize);
        dataEnc.reset();
        dataEnc.setByteCmd(FS_DATA);
        int ten;
        int p = 0;
        mbyte read = 0;
        while ((ten = ioUtils->read(dataEnc.getBuffer() + DataEnc::headerSize(), BUFF_SIZE - DataEnc::headerSize())) >
               0) {
            dataEnc.setDataIndex(ten);
            MTCPClient *mtcpClient = file->mioUtil;
            if (mtcpClient->send(dataEnc.getData(), dataEnc.getDataLen()) != dataEnc.getDataLen()) {
                thatSend = -3;
                qDebug("send error");
                break;
            }
            mtcpClient->recvo(&read, 1);
            if (read == FS_BREAK) {
                thatSend = 0;
                break;
            }
            if (!file->isNextStep()) {
                thatSend = -3;
                break;
            }
            totalSend += ten;
            thatSend += ten;
            int progress = (int) ((double) totalSend * 100.0 / (double) totalFileSize);
            if (progress != p) {
                qDebug("progress: %d", progress);
                p = progress;
                emit
                LANShareWindow::getInstance()->sigRecviceFileProgress(f->getUuid(), progress);
            }
        }
    } catch (LException &e) {
        qDebug() << e.what().c_str();
        thatSend = 0;
    }
    ioUtils->close();
    if (f->getType() != LFile::STREAM) {
        delete ioUtils;
    }
    qDebug("文件大小:%lld 已发送文件大小:%lld", f->getFileSize(), thatSend);
    dataEnc.reset();
    if (thatSend != f->fileSize) {
        dataEnc.setByteCmd(FS_CLOSE);
        qDebug("文件发送失败");
    } else {
        dataEnc.setByteCmd(FS_END);
        qDebug("文件发送成功");
    }
    try {
        file->mioUtil->TCPClient::send(dataEnc.getData(), dataEnc.getDataLen());
    } catch (LException &e) {
    }
    return thatSend;
}

mlong baseSendEnc(LFile *file, LFile *f, mlong size, mlong totalFileSize, DataEnc &dataEnc) {
    mlong totalSend = size;
    mlong thatSend = 0;
    IOInter *ioUtils;
    if (f->getType() == LFile::BYTEARRAY) {
        ioUtils = new ByteArrayIOUtils(f->getByteArray());
    } else if (f->getType() == LFile::FILE) {
        ioUtils = new IOUtils(f->path, QFile::ReadOnly);
    } else if (f->getType() == LFile::STREAM) {
        ioUtils = f->getIoInter();
    } else {
        qDebug("发送的文件类型不支持");
        return -1;
    }
    qDebug("发送文件: %s", f->fileName.toStdString().c_str());
    try {
        qDebug("文件大小: %lld", f->fileSize);
        dataEnc.reset();
        dataEnc.setByteCmd(FS_DATA);
        int ten;
        int p = 0;
        mbyte read = 0;
        while ((ten = ioUtils->read(dataEnc.getBuffer() + DataEnc::headerSize(), BUFF_SIZE - DataEnc::headerSize())) >
               0) {
            dataEnc.setDataIndex(ten);
            encData(dataEnc.getBuffer(), ten, DataEnc::headerSize(), thatSend);
            MTCPClient *mtcpClient = file->mioUtil;
            if (mtcpClient->send(dataEnc.getData(), dataEnc.getDataLen()) != dataEnc.getDataLen()) {
                thatSend = -3;
                qDebug("send error");
                break;
            }
            mtcpClient->recvo(&read, 1);
            if (read == FS_BREAK) {
                thatSend = 0;
                break;
            }
            if (!file->isNextStep()) {
                thatSend = -3;
                break;
            }
            totalSend += ten;
            thatSend += ten;
            int progress = (int) ((double) totalSend * 100.0 / (double) totalFileSize);
            if (progress != p) {
                qDebug("progress: %d", progress);
                p = progress;
                emit
                LANShareWindow::getInstance()->sigRecviceFileProgress(f->getUuid(), progress);
            }
        }
        ioUtils->close();
    } catch (LException &e) {
        qDebug() << e.what().c_str();
        thatSend = 0;
    }
    if (file->getType() != LFile::STREAM) {
        delete ioUtils;
    }
    qDebug("文件大小:%lld 已发送文件大小:%lld", file->getFileSize(), thatSend);
    dataEnc.reset();
    if (thatSend != file->fileSize) {
        dataEnc.setByteCmd(FS_CLOSE);
        qDebug("文件发送失败");
    } else {
        dataEnc.setByteCmd(FS_END);
        qDebug("文件发送成功");
    }
    try {
        file->mioUtil->TCPClient::send(dataEnc.getData(), dataEnc.getDataLen());
    } catch (LException &e) {
    }
    return thatSend;
}

mlong baseRecv(const LFile *mfile,
               IOUtils &fileIO,
               DataDec &dataDec, mlong mTotalRecv) {
    mlong totalRecv = mTotalRecv;
    mlong thatTotal = 0;
    int p = 0;
    MTCPClient *mioUtil = mfile->mioUtil;
    mbyte *buffer = dataDec.getBuffer();
    while (true) {
        try {
            if (mioUtil->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) break;
            dataDec.setData(buffer, BUFF_SIZE);
            mbyte rCmd = dataDec.getByteCmd();
            if (rCmd == FS_DATA) {
                // 数据包长度
                int length = dataDec.getLength();
                if (mioUtil->recvo(buffer, DataEnc::headerSize(), length, 0) != length) {
                    qDebug("recvo error");
                    break;
                }
                int rel = fileIO.write((char *) buffer, DataEnc::headerSize(), length);
                totalRecv += rel;
                thatTotal += rel;
                int progress = (int) ((double) totalRecv * 100.0 / (double) mfile->fileSize);
                if (p != progress) {
                    qDebug("progress:%d", progress);
                    p = progress;
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileProgress(mfile->getUuid(), progress);
                }
                mbyte send;
                if (mfile->isNextStep()) {
                    send = FS_NEXT;
                } else {
                    send = FS_BREAK;
                }
                mioUtil->send(&send, 1);
            } else if (rCmd == FS_END) {
                // 传输完毕
                break;
            } else /*if (rCmd == FS_CLOSE)*/ {
                // 关闭传输
                /*totalRecv = 0;
                break;*/
                return -3;
            }
        } catch (LException &e) {
            thatTotal = 0;
            //           qDebug();
        }
    }
    fileIO.close();
    return thatTotal;
}

mlong baseRecvDec(const LFile *mfile,
                  IOUtils &fileIO,
                  DataDec &dataDec, mlong mTotalRecv) {
    mlong totalRecv = mTotalRecv;
    mlong thatTotal = 0;
    int p = 0;
    MTCPClient *mioUtil = mfile->mioUtil;
    mbyte *buffer = dataDec.getBuffer();
    while (true) {
        try {
            if (mioUtil->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) break;
            dataDec.setData(buffer, BUFF_SIZE);
            mbyte rCmd = dataDec.getByteCmd();
            if (rCmd == FS_DATA) {
                // 数据包长度
                int length = dataDec.getLength();
                if (mioUtil->recvo(buffer, DataEnc::headerSize(), length, 0) != length) {
                    qDebug("recvo error");
                    break;
                }
                decData(buffer, length, DataEnc::headerSize(), thatTotal);
                int rel = fileIO.write((char *) buffer, DataEnc::headerSize(), length);
                totalRecv += rel;
                thatTotal += rel;
                int progress = (int) ((double) totalRecv * 100.0 / (double) mfile->fileSize);
                if (p != progress) {
                    qDebug("progress:%d", progress);
                    p = progress;
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileProgress(mfile->getUuid(), progress);
                }
                mbyte send;
                if (mfile->isNextStep()) {
                    send = FS_NEXT;
                } else {
                    send = FS_BREAK;
                }
                mioUtil->send(&send, 1);
            } else if (rCmd == FS_END) {
                // 传输完毕
                break;
            } else/* if (rCmd == FS_CLOSE)*/ {
                // 关闭传输
                /*totalRecv = 0;
                break;*/
                return -3;
            }
        } catch (LException &e) {
            thatTotal = 0;
            //           qDebug();
        }
    }
    fileIO.close();
    return thatTotal;
}

QString avoidDuplication(const QFileInfo &outFile) {
    QString name = outFile.fileName();
    if (outFile.exists()) {
        for (int s = 1; s < 65535; s++) {
            QString str;
            if (name.contains(".")) {
                QString prefix = name.mid(0, name.lastIndexOf(".")) + "(" + QString::number(s) + ")";
                QString suffix = name.mid(name.lastIndexOf("."));
                str = prefix + suffix;
            } else {
                str = name + "(" + QString::number(s) + ")";
            }
            QFileInfo fileInfo = QFileInfo(outFile.path(), str);
            if (!fileInfo.exists()) {
                return fileInfo.filePath();
            }
        }
    }
    return outFile.filePath();
}

mlong findFile(std::list<LFile> &listFile, mlong size, const QString &path) {
    QDir dir(path);
    qDebug() << "扫描路径:" << path;
    if (!dir.exists())
        return false;
    dir.setFilter(QDir::Dirs | QDir::Files);
    //    dir.setSorting(QDir::DirsFirst);
    QFileInfoList list = dir.entryInfoList();
    int i = 0;
    mlong fileSize = size;
    do {
        const QFileInfo &fileInfo = list.at(i);
        if (fileInfo.fileName() == "." | fileInfo.fileName() == "..") {
            i++;
            continue;
        }
        if (fileInfo.isDir()) {
            fileSize += findFile(listFile, 0, fileInfo.filePath());
        } else {
            LFile file;
            file.setFileName(fileInfo.fileName());
            file.setIsDirectory(false);
            file.setFileSize(fileInfo.size());
            file.setPath(fileInfo.filePath());
            listFile.push_back(file);
            fileSize += fileInfo.size();
            qDebug() << "扫描到文件: " + fileInfo.filePath() << "大小:" << fileInfo.size();
        }
        i++;
    } while (i < list.size());
    return fileSize;
}

// =====================================================================
// 分段并行传输（FS_SHARE_SEG）—— 接收侧
// 对应安卓端 fsSegShare / runSeg / finishSeg / recvSegFile。
// 一条连接只负责文件的某一段，接收端按偏移落盘；多条连接靠 segId 聚合。
// =====================================================================

void LANShare::fsSegShare(DataDec &dataDec, const Device &device,
                          std::unique_ptr<TCPClient> &tcpClientRef) {
    TCPClient *tcpClient = tcpClientRef.get();
    // 握手帧里设备信息之后的第一个字节就是 encData 标志
    bool encData = dataDec.getBool();
    auto *buffer = new mbyte[BUFF_SIZE];
    try {
        // 读分段描述帧：12 字节头 + 载荷
        if (tcpClient->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) {
            throw std::runtime_error("seg read header error");
        }
        DataDec fd(buffer, DataEnc::headerSize());
        int plen = fd.getLength();
        if (plen < 0 || plen > BUFF_SIZE - DataEnc::headerSize()) {
            throw std::runtime_error("seg payload length invalid");
        }
        if (tcpClient->recvo(buffer, DataEnc::headerSize(), plen, 0) != plen) {
            throw std::runtime_error("seg read payload error");
        }
        fd.setData(buffer, DataEnc::headerSize() + plen);
        mlong fileSize = fd.getLong();
        std::string fileName = fd.getString();
        std::string segId = fd.getString();
        int segIndex = fd.getInt();
        int segCount = fd.getInt();
        mlong segStart = fd.getLong();
        mlong segLen = fd.getLong();
        qDebug("SEG-IN segId=%s idx=%d/%d start=%lld len=%lld fileSize=%lld name=%s",
               segId.c_str(), segIndex, segCount, fileSize, segLen, fileSize, fileName.c_str());

        // 候选落盘路径（不在这里调 avoidDuplication）：真正的路径由首段在
        // segCoordJoin 锁内算一次，其余段复用 coord->filePath，绝不能各算各的。
        QString saveDir = QDir::cleanPath(config.saveFilePath);
        QString candidate = saveDir + QDir::separator() + QString::fromStdString(fileName);

        auto *fileContent = new LFile();
        fileContent->setFileName(QString::fromStdString(fileName));
        fileContent->setFileSize(fileSize);
        fileContent->setUuid(Utils::getUUID());
        fileContent->setNextStep(true);

        SegCoord *coord = segCoordJoin(segId, fileSize, segCount, fileContent,
            device.getDevName(),
            [candidate]() -> QString {
                // 只在竞争胜出的首段、锁内执行一次
                return avoidDuplication(QFileInfo(candidate));
            });
        // 非首段：自己 new 的 fileContent 没被采用，释放掉（安卓靠 GC，C++ 要手动）
        if (coord->fileContent != fileContent) {
            delete fileContent;
        }
        bool first = coord->isFirst();
        QString outPath = coord->filePath;
        qDebug("SEG-JOIN segId=%s idx=%d/%d first=%d path=%s",
               segId.c_str(), segIndex, segCount, first ? 1 : 0, outPath.toStdString().c_str());

        // 确认弹窗：只在首段且未开启自动接收时弹；其余段直接收
        if (first && !config.acceptRecvFiles) {
            auto *af = new AcceptFiles();
            af->device = device;
            af->needEncData = encData;
            af->isSeg = true;
            af->fileSize = fileSize;
            af->fileName = QString::fromStdString(fileName);
            af->segId = segId;
            af->segIndex = segIndex;
            af->segCount = segCount;
            af->segStart = segStart;
            af->segLen = segLen;
            af->encData = encData;
            af->files.push_back(coord->fileContent);
            // 连接所有权转移给 AcceptFiles，handleTcp 不再持有，避免双重释放
            af->tcpClient = std::move(tcpClientRef);
            emit LANShareWindow::getInstance()->sigRequstRecvFiles(af);
            delete[] buffer;
            return;
        }

        runSeg(coord, segIndex, segCount, segStart, segLen, tcpClient, encData, outPath);
    } catch (const std::exception &e) {
        qDebug("SEG-ERROR fsSegShare: %s", e.what());
    }
    delete[] buffer;
}

void LANShare::runSeg(SegCoord *coord, int segIndex, int segCount,
                       mlong segStart, mlong segLen, TCPClient *tcpClient,
                       bool encData, const QString &outFile) {
    qDebug("SEG-RUN idx=%d/%d segId=%s start=%lld len=%lld",
           segIndex, segCount, coord->segId.c_str(), segStart, segLen);
    try {
        if (coord->isBroken()) {
            qDebug("SEG-SKIP idx=%d segId=%s reason=ALREADY_BROKEN", segIndex, coord->segId.c_str());
            if (coord->arrive()) finishSeg(coord, outFile, false);
            return;
        }
        // 预分配全长：并发句柄各自扩展长度会互相截断，必须在写之前定死。
        // 弹窗那条连接可能不是 idx=0，所以首连接（isFirst）也必须预分配。
        if (segIndex == 0 || coord->isFirst()) {
            QFile pre(outFile);
            if (pre.open(QIODevice::ReadWrite)) {
                pre.resize(coord->fileSize);
                pre.close();
            }
        }
        // 只让一条连接把「接收中」进度行加进列表，所有段共用同一个 coord
        if (coord->presentOnce()) {
            emit LANShareWindow::getInstance()->sigRecviceFile(
                coord->fileContent, coord->fileContent->getUuid(),
                coord->fileContent->getFileName(),
                coord->peerName, coord->fileSize, true, true, false);
        }
        long long thatTotal = recvSegFile(tcpClient, segIndex, segCount, segStart, segLen,
                                           outFile, coord, encData);
        bool last = coord->arrive();
        long long total = coord->receivedTotal();
        // 终态字节只看本段自己收满没有，不能用整体 total（非最后一段此时还没凑齐）
        bool segOk = !coord->isBroken() && thatTotal == segLen;
        bool allOk = segOk && total == coord->fileSize;
        qDebug("SEG-DONE idx=%d/%d segId=%s thatSeg=%lld/%lld total=%lld/%lld last=%d ok=%d",
               segIndex, segCount, coord->segId.c_str(), thatTotal, segLen, total, coord->fileSize,
               last ? 1 : 0, segOk ? 1 : 0);
        // 每段都回一个终态字节：2=OK, 3=FAIL
        mbyte resp = segOk ? 2 : 3;
        try { tcpClient->send(&resp, 1); } catch (...) {}
        if (last) finishSeg(coord, outFile, allOk);
    } catch (const std::exception &e) {
        qDebug("SEG-ERROR runSeg idx=%d: %s", segIndex, e.what());
        coord->markBroken();
        // 即使本段中途炸了也必须算「已结束」，否则凑不满 segCount 没人收尾
        if (coord->arrive()) finishSeg(coord, outFile, false);
    }
    try { tcpClient->close(); } catch (...) {}
}

void LANShare::finishSeg(SegCoord *coord, const QString &outFile, bool ok) {
    if (!coord->finishOnce()) {
        qDebug("SEG-FINISH-DUP segId=%s", coord->segId.c_str());
        return;
    }
    LFile *fc = coord->fileContent;
    if (ok) {
        fc->setPath(outFile);
        qDebug("SEG-FINISH segId=%s ok=1 received=%lld/%lld path=%s",
               coord->segId.c_str(), coord->receivedTotal(), coord->fileSize, outFile.toStdString().c_str());
        emit LANShareWindow::getInstance()->sigRecviceFileSuccess(fc->getUuid(), true, outFile);
    } else {
        qDebug("SEG-FINISH segId=%s ok=0 received=%lld/%lld",
               coord->segId.c_str(), coord->receivedTotal(), coord->fileSize);
        emit LANShareWindow::getInstance()->sigRecviceFileSuccess(fc->getUuid(), false, outFile);
        // 任何一段缺失/出错都按整体失败处理，删掉半截文件
        if (QFile::exists(outFile)) {
            QFile::remove(outFile);
        }
    }
    std::string sid = coord->segId;
    segCoordRemove(sid);
    delete fc;
}

long long LANShare::recvSegFile(TCPClient *tcpClient, int segIndex, int segCount,
                                 mlong segStart, mlong segLen, const QString &outFile,
                                 SegCoord *coord, bool dec) {
    const int headerLen = DataEnc::headerSize();
    auto *buffer = new mbyte[BUFF_SIZE];
    long long segOff = 0;
    long long thatTotal = 0;
    int lastProgress = 0;
    IOUtils fileIO(outFile, QFile::ReadWrite);
    fileIO.setSeek(segStart);
    qDebug("SEG-RECV-START idx=%d/%d segId=%s start=%lld len=%lld dec=%d",
           segIndex, segCount, coord->segId.c_str(), segStart, segLen, dec ? 1 : 0);
    try {
        while (true) {
            if (tcpClient->recvo(buffer, 0, headerLen, 0) != headerLen) {
                qDebug("SEG-SHORT idx=%d segId=%s atSeg=%lld reason=HEADER_EOF",
                       segIndex, coord->segId.c_str(), segOff);
                coord->markBroken();
                break;
            }
            DataDec headDec(buffer, headerLen);
            mbyte cmd = headDec.getByteCmd();
            if (cmd == FS_DATA) {
                int length = headDec.getLength();
                if (length < 0 || length > BUFF_SIZE - headerLen) {
                    qDebug("SEG-BAD-LEN idx=%d len=%d", segIndex, length);
                    coord->markBroken();
                    break;
                }
                if (tcpClient->recvo(buffer, headerLen, length, 0) != length) {
                    qDebug("SEG-SHORT idx=%d segId=%s atSeg=%lld got<want",
                           segIndex, coord->segId.c_str(), segOff);
                    coord->markBroken();
                    break;
                }
                // 加密偏移是相对本段的（发送端从段内 0 开始计数）
                if (dec) decData(buffer, length, headerLen, segOff);
                fileIO.setSeek(segStart + thatTotal);
                fileIO.write((char *) buffer, headerLen, length);
                segOff += length;
                thatTotal += length;
                long long sum = coord->addReceived(length);
                int progress = (int) std::min<long long>(99, sum * 100 / coord->fileSize);
                if (progress != lastProgress) {
                    lastProgress = progress;
                    emit LANShareWindow::getInstance()->sigRecviceFileProgress(
                        coord->fileContent->getUuid(), progress);
                }
                mbyte ack;
                if (coord->fileContent->isNextStep()) {
                    ack = FS_NEXT;
                } else {
                    ack = FS_BREAK;
                    coord->markBroken();
                }
                tcpClient->send(&ack, 1);
                if (coord->isBroken()) break;
            } else if (cmd == FS_END) {
                break;
            } else if (cmd == FS_CLOSE && segOff == segLen) {
                // 发送端计数误判发来 FS_CLOSE，但本段字节已收满：按正常收尾
                qDebug("SEG-FULL-CLOSE idx=%d segId=%s segOff=%lld/%lld",
                       segIndex, coord->segId.c_str(), segOff, segLen);
                break;
            } else {
                qDebug("SEG-UNKNOWN-CMD idx=%d cmd=%d", segIndex, cmd);
                coord->markBroken();
                break;
            }
        }
    } catch (const std::exception &e) {
        qDebug("SEG-RECV-EX idx=%d: %s", segIndex, e.what());
        coord->markBroken();
    }
    fileIO.close();
    delete[] buffer;
    qDebug("SEG-RECV-END idx=%d segId=%s thatSeg=%lld/%lld",
           segIndex, coord->segId.c_str(), thatTotal, segLen);
    return thatTotal;
}

void LANShare::startHandleRecvSeg(bool accept, AcceptFiles *af) {
    SegCoord *coord = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_segMapMutex);
        auto it = g_segMap.find(af->segId);
        if (it != g_segMap.end()) coord = it->second;
    }
    TCPClient *tcpClient = af->tcpClient.get();
    if (coord == nullptr || tcpClient == nullptr) {
        if (tcpClient) try { tcpClient->close(); } catch (...) {}
        delete af;
        return;
    }
    if (accept) {
        runSeg(coord, af->segIndex, af->segCount, af->segStart, af->segLen,
               tcpClient, af->encData, coord->filePath);
    } else {
        coord->markBroken();
        mbyte closeByte = FS_CLOSE;
        try { tcpClient->send(&closeByte, 1); } catch (...) {}
        if (coord->arrive()) finishSeg(coord, coord->filePath, false);
        try { tcpClient->close(); } catch (...) {}
    }
    af->tcpClient.release(); // 已由 runSeg / 上面关闭
    delete af;
}

/**
 * 发送文件
 */
void LANShare::sendFile(const Device &device, std::vector<LFile *> selectFiles, int count) {
    // 分段并行传输：未加密 + 对端 v5+ + 全是普通文件 + 每个文件够大，才走多连接
    if (isParallelEligible(device, selectFiles, config.encData)) {
        qDebug("SEND-PARALLEL: device=%s files=%zu", device.getDevName().toStdString().c_str(), selectFiles.size());
        sendFileParallel(device, selectFiles);
        return;
    }
    std::unique_ptr<TCPClient> client = makeSocket(device.getDevIp(), device.getDevPort());
    if (client == nullptr) {
        return;
    }
    auto *buffer = new mbyte[BUFF_SIZE];
    std::vector<Device>::iterator p1;
    std::vector<Device> mDevices = LANShare::getInstance()->getMDevices();
    for (p1 = mDevices.begin(); p1 != mDevices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                  device.getDevIp())) {
            std::string userName = config.clientName.toStdString();
            DataEnc dataEnc(buffer, BUFF_SIZE);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(FS_SHARE_FILE);
            dataEnc.setCount(count);
            dataEnc.putBool(config.encData);
            qDebug() << "IP:" << device.getDevIp();
            try {
                client->send(dataEnc.getData(), dataEnc.getDataLen());
            } catch (LException &e) {
                return;
            }
            for (int i = 0; i < count; ++i) {
                LFile *file = selectFiles[i];
                file->setIndex(i);
                file->setMioUtil(new MTCPClient(client->getFd()));
                file->setUuid(Utils::getUUID());
                if (file->isDirectory()) {
                    std::list<LFile> filelist;
                    mlong fileSize = findFile(filelist, 0, file->getPath());
                    file->setFileSize(fileSize);
                    file->setFileList(filelist);
                }
                qDebug("文件字节大小: %lld", file->fileSize);
                qDebug() << "文件名:" << file->fileName;
                qDebug("文件大小: %s", Utils::computeSize(file->fileSize).c_str());
                dataEnc.reset();
                dataEnc.putLong(file->fileSize);
                dataEnc.putString(file->fileName);
                dataEnc.putInt(file->getFileType());
                dataEnc.putString(std::string(""));
                if (file->isDirectory()) {
                    dataEnc.putInt((int) file->getFileList().size());
                }
                emit
                LANShare::getInstance()->mainWindow->sigRecviceFile(
                    file,
                    file->getUuid(),
                    file->fileName,
                    device.getDevName() + " <- " + config.clientName,
                    file->fileSize,
                    false,
                    !file->isDirectory(),
                    false
                );
                try {
                    client->send(dataEnc.getData(), dataEnc.getDataLen());
                } catch (LException &e) {
                    return;
                }
            }
            try {
                if (!client->recvo(buffer, DataEnc::headerSize())) {
                    qDebug("error");
                    return;
                }
            } catch (LException &e) {
                return;
            }
            DataDec dataDec(buffer, DataEnc::headerSize());
            if (dataDec.getCmd() == FS_NOT_AGREE) {
                qDebug() << device.getDevName() << " 取消接收文件";
                return;
            }
            // 已选择文件列表
            for (int i = 0; i < count; ++i) {
                LFile *file = selectFiles[i];
                mlong total = 0;
                mlong thatTotal = 0;
                // 文件夹发送
                if (file->isDirectory()) {
                    std::list<LFile> fileLis = file->getFileList();
                    QFileInfo fileInfo(file->getPath());
                    QString p = fileInfo.path();
                    p = p.remove(p.size(), 1);
                    for (auto f: fileLis) {
                        QString path = f.getPath();
                        path = path.remove(p);
                        qDebug() << "发送文件:" << path;
                        dataEnc.reset();
                        dataEnc.putLong(f.getFileSize());
                        dataEnc.putString(path);
                        file->mioUtil->send(dataEnc.getData(), dataEnc.getDataLen());
                        f.setUuid(file->getUuid());
                        if (config.encData) {
                            thatTotal = baseSendEnc(file, &f, total, file->getFileSize(), dataEnc);
                        } else {
                            thatTotal = baseSend(file, &f, total, file->getFileSize(), dataEnc);
                        }
                        if (thatTotal == -3) {
                            total = 0;
                            break;
                        } else if (thatTotal <= 0) {
                            continue;
                        }
                        total += thatTotal;
                    }
                } else {
                    // 文件发送
                    if (config.encData) {
                        total += baseSendEnc(file, file, total, file->getFileSize(), dataEnc);
                    } else {
                        total += baseSend(file, file, total, file->getFileSize(), dataEnc);
                    }
                }
                qDebug() << "文件大小:" << file->fileSize << "发送文件大小:" << total;
                if (total != file->fileSize) {
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(file->getUuid(), false, file->getPath());
                } else {
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(file->getUuid(), true, file->getPath());
                    LANShare::getInstance()->lhttpServer->sendWebSocketMessage(
                        file->getFileName(),
                        device.getDevName(),
                        file->getPath(),
                        (!file->isDirectory() && Utils::isPhoto(file->getFileName())) ? 1 : 2,
                        device.getDevMode(),
                        Utils::computeSize(file->getFileSize()).c_str(),
                        false,
                        false,
                        !file->isDirectory()
                    );
                }
                mbyte read = 0;
                // 等待接收方响应再继续发送文件
                client->recvo(&read, 1);
                delete file->mioUtil;
                delete file;
            }
            qDebug("结束");
            client->close();
            break;
        }
    }
    delete[] buffer;
}

// =====================================================================
// 分段并行传输（FS_SHARE_SEG）—— 发送侧
// 对应安卓端 isParallelEligible / fileSendParallel / sendFileParallel / sendOneSeg。
// =====================================================================

bool LANShare::isParallelEligible(const Device &device, const std::vector<LFile *> &fileList, bool encData) {
    if (encData) return false;
    if (device.getDataVersion() < DATA_VERSION_5) return false;
    if (PARALLEL_SEGS <= 1) return false;
    if (fileList.empty()) return false;
    for (LFile *f: fileList) {
        if (!isFileParallelEligible(f)) return false;
    }
    return true;
}

bool LANShare::isFileParallelEligible(LFile *f) {
    if (f == nullptr) return false;
    // 分段只处理普通文件：流/uri/目录没有可按偏移复用的文件句柄
    if (f->isDirectory()) return false;
    if (f->getType() != LFile::FILE) return false;
    // 每段 base = 总长/段数，base 必须 >= PARALLEL_MIN_SIZE 才值得切
    if (f->getFileSize() < (mlong) PARALLEL_MIN_SIZE * PARALLEL_SEGS) return false;
    return true;
}

void LANShare::sendFileParallel(const Device &device, std::vector<LFile *> selectFiles) {
    for (LFile *f: selectFiles) {
        if (!isFileParallelEligible(f)) {
            // 兜底：理论上入口判定已挡住，真走到这就跳过这个文件
            qDebug("SEND-PARA skip non-eligible file");
            continue;
        }
        sendFileParallelOne(device, f);
    }
}

long long LANShare::sendFileParallelOne(const Device &device, LFile *file) {
    const int segs = PARALLEL_SEGS;
    const long long total = file->getFileSize();
    const long long base = total / segs;
    if (segs <= 1 || device.getDataVersion() < DATA_VERSION_5 || base < PARALLEL_MIN_SIZE) {
        qDebug("SEND-PARA skip file peerVer=%d base=%lld", device.getDataVersion(), base);
        return -1;
    }
    // 找本机与对端同网段的设备（握手帧要带本机设备信息）
    Device mDevice;
    bool found = false;
    std::vector<Device> mDevices = getInstance()->getMDevices();
    for (const Device &d: mDevices) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(d.getDevNetMask()),
                                   d.getDevIp(), device.getDevIp())) {
            mDevice = d;
            found = true;
            break;
        }
    }
    if (!found) {
        qDebug("SEND-PARA no local device for peer=%s", device.getDevIp().toStdString().c_str());
        return -1;
    }

    // 进度行（与单流发送一样在列表里显示一条）
    auto *progressFile = new LFile();
    progressFile->setFileName(file->getFileName());
    progressFile->setFileSize(total);
    QString progressUuid = Utils::getUUID();
    progressFile->setUuid(progressUuid);
    progressFile->setPath(file->getPath());
    emit LANShareWindow::getInstance()->sigRecviceFile(
        progressFile, progressUuid, file->getFileName(),
        device.getDevName() + " <- " + config.clientName, total, false, true, false);

    std::string segId = Utils::getUUID().toStdString();
    auto *sentTotal = new std::atomic<long long>(0);
    auto *lastProgress = new std::atomic<int>(-1);
    std::vector<long long> got(segs, 0);
    std::vector<long long> exp(segs, 0);
    std::atomic<int> broken(0);

    qDebug("SEND-PARA-START segId=%s segs=%d total=%lld each=%lld peer=%s",
           segId.c_str(), segs, total, base, device.getDevIp().toStdString().c_str());

    std::vector<std::thread> threads;
    for (int i = 0; i < segs; i++) {
        long long start = i * base;
        long long len = (i == segs - 1) ? (total - start) : base;
        exp[i] = len;
        if (len <= 0) { got[i] = 0; continue; }
        threads.emplace_back([&, i, start, len]() {
            try {
                got[i] = sendOneSeg(mDevice, device, file, segId, i, segs, start, len,
                                    sentTotal, progressUuid, lastProgress);
            } catch (const std::exception &e) {
                qDebug("SEND-SEG-EX idx=%d: %s", i, e.what());
                broken.fetch_add(1);
            }
        });
    }
    for (auto &t: threads) if (t.joinable()) t.join();

    long long sum = 0;
    bool ok = broken.load() == 0;
    for (int i = 0; i < segs; i++) {
        sum += got[i];
        if (got[i] != exp[i]) ok = false;
    }
    if (sum != total) ok = false;
    qDebug("SEND-PARA segId=%s sum=%lld/%lld broken=%d ok=%d",
           segId.c_str(), sum, total, broken.load(), ok ? 1 : 0);

    emit LANShareWindow::getInstance()->sigRecviceFileSuccess(progressUuid, ok, file->getPath());
    delete sentTotal;
    delete lastProgress;
    // progressFile 已交给 UI（sigRecviceFile），由界面侧持有；与现有单流发送一致
    return ok ? sum : -1;
}

long long LANShare::sendOneSeg(const Device &mDevice, const Device &device, LFile *file,
                               const std::string &segId, int idx, int segs,
                               long long start, long long len,
                               std::atomic<long long> *sentTotal, const QString &progressUuid,
                               std::atomic<int> *lastProgress) {
    const int headerLen = DataEnc::headerSize();
    if (len <= 0) return 0;
    std::unique_ptr<TCPClient> client = makeSocket(device.getDevIp(), device.getDevPort());
    if (client == nullptr) return -1;
    auto *hbuf = new mbyte[BUFF_SIZE];
    long long sent = 0;
    bool peerBroke = false;
    try {
        // 握手帧：本机设备信息 + cmd FS_SHARE_SEG + count 1 + bool false
        DataEnc hs(hbuf, BUFF_SIZE);
        makeDataEnc(mDevice, &hs);
        hs.setCmd(FS_SHARE_SEG);
        hs.setCount(1);
        hs.putBool(false);
        client->send(hs.getData(), hs.getDataLen());

        // 分段描述帧（无设备信息）：整文件大小/名 + segId + 段号/段数 + 起点/长度
        DataEnc fd(hbuf, BUFF_SIZE);
        fd.reset();
        fd.putLong(file->getFileSize());
        fd.putString(file->getFileName());
        fd.putString(segId);
        fd.putInt(idx);
        fd.putInt(segs);
        fd.putLong(start);
        fd.putLong(len);
        client->send(fd.getData(), fd.getDataLen());

        // 数据阶段：每段独立句柄 seek 到起点，读到 len 为止，不加密
        IOUtils segIO(file->getPath(), QFile::ReadOnly);
        segIO.setSeek(start);
        long long remaining = len;
        while (remaining > 0) {
            int want = (int) std::min<long long>(BUFF_SIZE - headerLen, remaining);
            int n = segIO.read(hbuf + headerLen, want);
            if (n <= 0) break;
            DataEnc de(hbuf, BUFF_SIZE);
            de.setByteCmd(FS_DATA);
            de.setDataIndex(n);
            client->send(de.getData(), de.getDataLen());
            mbyte ack = 0;
            client->recvo(&ack, 1);
            if (ack == FS_BREAK) {
                peerBroke = true;
                break;
            }
            sent += n;
            remaining -= n;
            long long done = sentTotal->fetch_add(n) + n;
            int p = (int) std::min<long long>(99, done * 100 / file->getFileSize());
            int lp = lastProgress->load();
            if (p != lp && lastProgress->compare_exchange_strong(lp, p)) {
                emit LANShareWindow::getInstance()->sigRecviceFileProgress(progressUuid, p);
            }
        }
        segIO.close();

        // 尾帧：FS_END 正常，FS_CLOSE 异常
        DataEnc tail(hbuf, BUFF_SIZE);
        tail.reset();
        tail.setByteCmd((sent == len && !peerBroke) ? FS_END : FS_CLOSE);
        client->send(tail.getData(), tail.getDataLen());

        // 终态字节：数据阶段每块已各读一个 FS_NEXT，这里再读一个 2(OK)/3(FAIL)
        mbyte term = 0;
        client->recvo(&term, 1);
        bool ok = (sent == len) && !peerBroke && term == 2;
        qDebug("SEND-SEG idx=%d/%d segId=%s sent=%lld/%lld term=%d ok=%d",
               idx, segs, segId.c_str(), sent, len, term, ok ? 1 : 0);
    } catch (const std::exception &e) {
        qDebug("SEND-SEG-EX idx=%d: %s", idx, e.what());
        sent = -1;
    }
    delete[] hbuf;
    try { client->close(); } catch (...) {}
    return sent;
}

void LANShare::startHandleRecvFile(
    bool accept,
    const Device &device,
    bool needEncData,
    const std::vector<LFile *> &files,
    const std::unique_ptr<TCPClient> &tcpClient
) {
    auto *buffer = new mbyte[BUFF_SIZE];
    // 同意接收文件
    DataEnc dataEnc(buffer, BUFF_SIZE);
    if (accept) {
        dataEnc.setCmd(FS_AGREE);
        tcpClient->send(dataEnc.getData(), dataEnc.getDataLen());
        TimeTools::sleep_ms(100);
        for (LFile *item: files) {
            mlong total = 0;
            DataDec dataDec(buffer, BUFF_SIZE);
            if (item->isDirectory()) {
                qDebug() << "接收文件夹" << device.getDevName() << "文件数量:" << item->getSubFileCount() << " 大小:"
                        << item->fileSize;
                for (int i = 0; i < item->getSubFileCount(); ++i) {
                    dataDec.reset();
                    if (tcpClient->recvo(buffer, DataEnc::headerSize()) != DataEnc::headerSize()) return;
                    const int length = dataDec.getLength();
                    if (tcpClient->recvo(buffer, DataEnc::headerSize(), length, 0) != length) return;
                    mlong fileLength = dataDec.getLong();
                    char *c = dataDec.getStr();
                    QString fileName = c;
                    delete[] c;
                    QString saveFilePath = QDir::cleanPath(config.saveFilePath);
                    QString newPath = saveFilePath + SEPARATORS + fileName;
                    const int index = static_cast<int>(newPath.lastIndexOf("/"));
                    QString folder = newPath.mid(0, index);
                    mUtils::createMultipleFolders(folder);
                    qDebug() << "  接收" << device.getDevName() << "文件:" << fileName << " 大小:" << fileLength;
                    QFileInfo outFile(newPath);
                    newPath = avoidDuplication(outFile);
                    IOUtils fileIO(newPath, QFile::WriteOnly);
                    mlong rel;
                    if (needEncData) {
                        rel = baseRecvDec(item, fileIO, dataDec, total);
                    } else {
                        rel = baseRecv(item, fileIO, dataDec, total);
                    }
                    if (rel == -3) {
                        break;
                    } else if (rel <= 0) {
                        continue;
                    }
                    total += rel;
                }
                if (total != item->fileSize) {
                    // 需要删除文件操作
                    qDebug("recv fail");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(
                        item->getUuid(), false,
                        config.saveFilePath + item->getFileName()
                    );
                } else {
                    qDebug("recv sucess");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(
                        item->getUuid(), true,
                        config.saveFilePath + item->getFileName());
                    LHttpServer::sendWebSocketMessage(
                        item->getFileName(),
                        device.getDevName(),
                        "",
                        2,
                        device.getDevMode(),
                        Utils::computeSize(item->getFileSize()).c_str(),
                        true,
                        false,
                        false
                    );
                }
            } else {
                QString saveFilePath = QDir::cleanPath(config.saveFilePath);
                mUtils::createMultipleFolders(saveFilePath);
                qDebug() << "saveFilePath:" << saveFilePath;
                QString newPath = saveFilePath + SEPARATORS + item->fileName;
                QFileInfo outFile(newPath);
                newPath = avoidDuplication(outFile);
                IOUtils fileIO(newPath, QFile::WriteOnly);
                if (needEncData) {
                    total += baseRecvDec(item, fileIO, dataDec, 0);
                } else {
                    total += baseRecv(item, fileIO, dataDec, 0);
                }
                TimeTools::sleep_ms(200);
                if (total != item->fileSize) {
                    // 需要删除文件操作
                    fileIO.deleteFile();
                    qDebug("recv fail");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(item->getUuid(), false,
                                                                         fileIO.getFilePath());
                } else {
                    qDebug("recv sucess");
                    emit
                    LANShareWindow::getInstance()->sigRecviceFileSuccess(item->getUuid(), true,
                                                                         fileIO.getFilePath());
                    const mlong mediaId = item->getMediaId();
                    if (mediaId > -1) {
                        MediaIdPathDBUtil mediaIdPathDbUtil;
                        mediaIdPathDbUtil.addMediaIdPath(mediaId, item->getFileName(), item->getPath(), QDateTime{},
                                                         true);
                    }
                    LHttpServer::sendWebSocketMessage(
                        item->getFileName(),
                        device.getDevName(),
                        newPath,
                        (!item->isDirectory() && Utils::isPhoto(item->getFileName())) ? 1 : 2,
                        device.getDevMode(),
                        Utils::computeSize(item->getFileSize()).c_str(),
                        true,
                        false
                    );
                }
            }
            mbyte send = 2;
            // 响应给发送方继续发送文件
            tcpClient->send(&send, 1);
            delete item->mioUtil;
            delete item;
        }
    } else {
        dataEnc.setCmd(FS_NOT_AGREE);
        tcpClient->send(dataEnc.getData(), dataEnc.getDataLen());
        for (const LFile *item: files) {
            qDebug("Not agree");
            emit
            LANShareWindow::getInstance()->sigRecviceFileSuccess(item->getUuid(), false, "");
        }
    }
    delete[] buffer;
    qDebug("client close");
    tcpClient->close();
}

/**
 * 接收文件操作
 */
void LANShare::handleTcp(std::unique_ptr<TCPClient> tcpClient) {
    auto *buffer = new mbyte[BUFF_SIZE];
    if (tcpClient->recvo(buffer, 4) != 4) return;
    const int magicNum = ByteUtils::bytesToInt(buffer, 0);
    //    qDebug() << "magicNum: " << magicNum;
    if (magicNum != MAGIC_NUM) {
        char buff[5];
        memcpy(buff, buffer, 4);
        buff[4] = '\0';
        QString str(buff);
        str = str.toUpper();
        //        qDebug() << "str:" << buff;
        if (config.webService && (str.startsWith("GET") || str.startsWith("POST"))) {
            getInstance()->lhttpServer->httpServer->newClient(tcpClient.get(), str);
        } else {
            tcpClient->close();
        }
        delete[] buffer;
        return;
    }
    if (tcpClient->recvo(buffer, DataEnc::headerSize()) != DataEnc::headerSize()) return;
    DataDec dataDec(buffer, BUFF_SIZE);
    int length = dataDec.getLength();
    if (tcpClient->recvo(buffer, DataEnc::headerSize(), length, 0) != length) return;
    dataDec.setData(buffer, DataEnc::headerSize() + length);
    // 设备端口
    const int devPort = dataDec.getInt();
    // 设备ip
    char *devIp = dataDec.getStr();
    // 设备名
    char *devName = dataDec.getStr();
    // 设备类型
    const int devMode = dataDec.getInt();
    // 设备唯一码
    const char *uniqueUUid = dataDec.getStr();
    // 协议版本
    const int dataVersion = dataDec.getInt();
    // 电量
    const int batteryLevel = dataDec.getInt();
    // 充电状态
    const mbyte chargeStatus = dataDec.getByte();
    int webDeviceCount = dataDec.getInt();
    Device device;
    device.setDevMode(devMode);
    device.setDevName(devName);
    device.setDevIp(devIp);
    device.setDevNetMask("");
    device.setDevPort(devPort);
    device.setSetTime(TimeTools::getCurrentTime());
    device.setDataVersion(dataVersion);
    device.setUniqueUUid(uniqueUUid);
    device.setBatteryLevel(batteryLevel);
    device.setChargeStatus(chargeStatus);
    getInstance()->addDevice(device);
    if (webDeviceCount > 0) {
        for (int i = 0; i < webDeviceCount; i++) {
            int webDevicePort = dataDec.getInt();
            QString webDeviceIp = dataDec.getString().c_str();
            QString webDeviceName = dataDec.getString().c_str();
            QString address = webDeviceIp + ":" + QString::number(webDevicePort);
            Device debDevice = getInstance()->onLineDevices[address.toStdString()];
            debDevice.setDevPort(webDevicePort);
            debDevice.setDevIp(webDeviceIp);
            debDevice.setDevName(webDeviceName);
            debDevice.setUniqueUUid(webDeviceIp + QString::number(webDevicePort) + webDeviceName);
            debDevice.setDevMode(Device::L_WEB);
            debDevice.setSetTime(TimeTools::getCurrentTime());
            getInstance()->addDevice(debDevice);
        }
    }
    const int cmd = dataDec.getCmd();
    if (cmd == FS_SHARE_FILE) {
        const int count = dataDec.getCount();
        const boolean needEncData = dataDec.getBool();
        qDebug("file count:%d", count);
        qDebug("ip:%s:%d name:%s", devIp, devPort, devName);
        std::vector<LFile *> files;
        for (int i = 0; i < count; i++) {
            if (tcpClient->recvo(buffer, 0, DataEnc::headerSize(), 0) != DataEnc::headerSize()) return;
            length = dataDec.getLength();
            if (tcpClient->recvo(buffer, DataEnc::headerSize(), length, 0) != length) return;
            dataDec.setData(buffer, DataEnc::headerSize() + length);
            const mlong fileSize = dataDec.getLong();
            // 文件名称
            char *strFilename = dataDec.getStr();
            const int fileType = dataDec.getInt();
            const char *videoTime = dataDec.getStr();
            qDebug("fileType:%d fileName:%s  fileSize:%lld", fileType, strFilename, fileSize);
            auto *lfile = new LFile();
            lfile->setFileName(strFilename);
            lfile->setFileSize(fileSize);
            if (fileType == FILE_FOLDER) {
                const int fileCount = dataDec.getInt();
                lfile->setSubFileCount(fileCount);
                lfile->setIsDirectory(true);
            } else if (fileType == FILE_IMAGE || fileType == FILE_VIEDO) {
                const mlong mediaId = dataDec.getLongDefualt(-1);
                lfile->setMediaId(mediaId);
                lfile->setIsDirectory(false);
            } else {
                lfile->setIsDirectory(false);
            }
            lfile->setMioUtil(new MTCPClient(tcpClient->getFd()));
            lfile->setUuid(Utils::getUUID());
            files.push_back(lfile);
            emit
            LANShareWindow::getInstance()->sigRecviceFile(
                lfile,
                lfile->getUuid(),
                strFilename,
                devName,
                fileSize,
                true,
                !lfile->isDirectory(),
                false
            );
            delete videoTime;
            delete strFilename;
        }
        if (config.acceptRecvFiles) {
            startHandleRecvFile(true, device, needEncData, files, tcpClient);
        } else {
            auto *acceptFiles = new AcceptFiles();
            acceptFiles->device = device;
            acceptFiles->files = files;
            acceptFiles->tcpClient = std::move(tcpClient);
            emit
            LANShareWindow::getInstance()->sigRequstRecvFiles(acceptFiles);
        }
    } else if (cmd == FS_SHARE_SEG) {
        fsSegShare(dataDec, device, tcpClient);
    } else if (cmd == FS_MESSAGE) {
        const char *message = dataDec.getStr();
        const QString decode = mUtils::decMessage(message, config.messageKey);
        qDebug() << "devName:" << devName << " message:" << decode;
        emit
        LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
        delete[] message;
        qDebug("client close");
        tcpClient->close();
    } else if (cmd == FS_GET_NO_SYNC_MEDIA) {
        const int count = dataDec.getCount();
        DataEnc dataEnc(buffer, BUFF_SIZE);
        int syncCount = 0;
        for (int i = 0; i < count; i++) {
            const mlong mediaId = dataDec.getLong();
            MediaIdPathDBUtil mediaIdPathDbUtil;
            if (!mediaIdPathDbUtil.isIdExists(mediaId)) {
                syncCount++;
                dataEnc.putLong(mediaId);
            }
        }
        dataEnc.setCount(syncCount);
        tcpClient->send(dataEnc.getData(), dataEnc.getDataLen());
        qDebug("client close");
        tcpClient->close();
    }
    delete[] buffer;
    delete[] devIp;
    delete[] devName;
    delete[] uniqueUUid;
}

/**
 * 扫描设备
 */
void LANShare::scannDevice() {
    mbyte buffer[2048];
    LANShare *lanShare = LANShare::getInstance();
    while (lanShare->isRun) {
        lanShare->updateMDevices();
        std::vector<Device> devices = lanShare->getMDevices();
        std::vector<Device>::iterator p1;
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            DataEnc dataEnc(buffer, 2048);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_GET_DEVICES);
            UDPClient udpClient(p1->getDevIp());
            //            qDebug() << "广播ip: " << p1->getDevBrotIp();
            //            udpClient.sendto(p1->getDevBrotIp(), config.udpPort, dataEnc.getData(), dataEnc.getDataLen());
            udpSend(&udpClient, &dataEnc, p1->getDevBrotIp(), config.udpPort);
            udpClient.close();
            std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
            lock.lock();
            std::map<std::string, Device>::iterator iter;
            for (iter = lanShare->onLineDevices.begin(); iter != lanShare->onLineDevices.end();) {
                mlong devTime = iter->second.getSetTime();
                mlong currentTime = TimeTools::getCurrentTime();
                mlong timeOut = currentTime - devTime;
                if (iter->second.getCanRemove() && timeOut > (1000 * 20)) {
                    //LLog::Debug("timeOut:%d ip:%s", timeOut, iter->second.getDevIp().c_str());
                    lanShare->onLineDevices.erase(iter++);
                } else {
                    ++iter;
                }
            }
            lock.unlock();
        }
        sleep(5);
        // 打印当前设备数
        //        qDebug("device count:%llu", lanShare->onLineDevices.size());
    }
}

void LANShare::noticeDeviceOnLineByIp(const QString &ip) const {
    std::vector<Device> devices = getMDevices();
    std::vector<Device>::iterator p1;
    for (p1 = devices.begin(); p1 != devices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(), ip)) {
            auto *bytes = new mbyte[2048];
            DataEnc dataEnc(bytes, 2048);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_SET_DEVICES);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, ip, config.udpPort);
            delete[] bytes;
            break;
        }
    }
}

void LANShare::noticeDeviceOffLineByIp(const QString &ip) const {
    std::vector<Device> devices = getMDevices();
    std::vector<Device>::iterator p1;
    for (p1 = devices.begin(); p1 != devices.end(); p1++) {
        if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(), ip)) {
            auto *bytes = new mbyte[2048];
            DataEnc dataEnc(bytes, 2048);
            makeDataEnc(*p1, &dataEnc);
            dataEnc.setCmd(UDP_DEVICE_OFF_LINE);
            UDPClient udpClient(p1->getDevIp());
            udpSend(&udpClient, &dataEnc, ip, config.udpPort);
            delete[] bytes;
            break;
        }
    }
}

LANShare *LANShare::getInstance() {
    return instance;
}

/**
 * 监听指令
 */
void LANShare::handleUdp() {
    auto *buffer = new mbyte[4096];
    sockaddr_in clientAddr{};
    LANShare *lanShare = LANShare::getInstance();
flag:
    while (lanShare->isRun) {
        int len = lanShare->udpServer->recv(&clientAddr, buffer, 4096);
        if (len <= 0) {
            qDebug("runRecive recv len is <= 0");
            break;
        }
        int magicNum = ByteUtils::bytesToInt(buffer, 0);
        if (magicNum != MAGIC_NUM) {
            continue;
        }
        DataDec dataDec(buffer + 4, len - 4);
        dataDec.decAllData();
        // 设备端口
        int devPort = dataDec.getInt();
        // 设备ip
        char *devIp = dataDec.getStr();
        // 设备名
        char *devName = dataDec.getStr();
        // 设备类型
        int devMode = dataDec.getInt();
        // 设备唯一码
        char *uniqueUUid = dataDec.getStr();
        int dataVersion = dataDec.getInt();
        int batteryLevel = dataDec.getInt();
        mbyte chargeStatus = dataDec.getByte();
        int webDeviceCount = dataDec.getInt();
        //        qDebug() << "devIp：" << devIp;
        // 判断是否为自己发送的指令
        std::vector<Device> devices = lanShare->getMDevices();
        std::vector<Device>::iterator p1;
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            if (strcmp(devIp, p1->getDevIp().toUtf8().data()) == 0) goto flag;
        }
        Device device;
        device.setDevMode(devMode);
        device.setDevName(devName);
        device.setDevIp(devIp);
        device.setDevNetMask("");
        device.setDevPort(devPort);
        device.setSetTime(TimeTools::getCurrentTime());
        device.setDataVersion(dataVersion);
        device.setUniqueUUid(uniqueUUid);
        device.setBatteryLevel(batteryLevel);
        device.setChargeStatus(chargeStatus);
        getInstance()->addDevice(device);
        if (webDeviceCount > 0) {
            for (int i = 0; i < webDeviceCount; i++) {
                int webDevicePort = dataDec.getInt();
                QString webDeviceIp = dataDec.getString().c_str();
                QString webDeviceName = dataDec.getString().c_str();
                QString address = webDeviceIp + ":" + QString::number(webDevicePort);
                Device debDevice = getInstance()->onLineDevices[address.toStdString()];
                debDevice.setDevPort(webDevicePort);
                debDevice.setDevIp(webDeviceIp);
                debDevice.setDevName(webDeviceName);
                debDevice.setUniqueUUid(webDeviceIp + QString::number(webDevicePort) + webDeviceName);
                debDevice.setDevMode(Device::L_WEB);
                debDevice.setSetTime(TimeTools::getCurrentTime());
                getInstance()->addDevice(debDevice);
            }
        }
        int cmd = dataDec.getCmd();
        //       qDebug("cmd: %d", cmd);
        if (cmd == UDP_SET_DEVICES) {
            // 添加或覆盖设备
            // qDebug("device on line ip:%s:%d devName:%s devModel:%d", ip, port, devName, devModel);
        } else if (cmd == UDP_GET_DEVICES) {
            //获取设备命令
            lanShare->noticeDeviceOnLineByIp(devIp);
        } else if (cmd == UDP_DEVICE_OFF_LINE) {
            //设备下线
            getInstance()->removeDevice(device);
        } else if (cmd == UDP_MESSAGE) {
            // 消息
            char *message = dataDec.getStr();
            QString decode = mUtils::decMessage(message, config.messageKey);
            qDebug() << "devName:" << devName << " message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            lanShare->lhttpServer->sendWebSocketMessage(decode, devName, "", 0, devMode, "", true, false);
            delete[] message;
        } else if (cmd == UDP_MESSAGE_TO_CLIPBOARD) {
            // 消息写入剪切板
            char *message = dataDec.getStr();
            QString decode = mUtils::decMessage(message, config.messageKey);
            qDebug() << "devName:" << devName << " clip message:" << decode;
            emit
            LANShareWindow::getInstance()->sigNewMessage(device, decode, true);
            emit
            LANShareWindow::getInstance()->sigCopyText(decode);
            delete[] message;
        } else if (cmd == UDP_SEND_MEDIA_MUTE) {
            // 暂停
            if (!config.receivceMute) {
                continue;
            }
            qDebug("静音");
#if defined(PLATFORM_WINDOWS)
            try {
                if (!lanShare->muted) {
                    lanShare->systemVolume = Utils::volume();
                    qDebug() << "volume:" << lanShare->systemVolume;
                    Utils::setVolum(0);
                    TimeTools::sleep_ms(500);
                    //                keybd_event(VK_VOLUME_MUTE, 0, 0, 0);
                    //                keybd_event(VK_VOLUME_MUTE, 0, 2, 0);
                    lanShare->muted = true;
                }
            } catch (const std::exception &e) {
            }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
            lanShare->systemVolume = Utils::volume();
            qDebug() << "volume:" << lanShare->systemVolume;
            Utils::setVolum(0);
            lanShare->muted = true;
#endif
        } else if (cmd == UDP_SEND_MEDIA_RESTORE) {
            if (!config.receivceMute) {
                continue;
            }
            qDebug("恢复音量");
#if defined(PLATFORM_WINDOWS)
            try {
                Utils::setVolum(lanShare->systemVolume);
                lanShare->muted = false;
            } catch (const std::exception &e) {
            }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
        }
        delete[] devIp;
        delete[] devName;
        delete[] uniqueUUid;
    }
}

/**
 * 创建TCP服务
 */
void LANShare::createTcpServer() {
    LANShare *lanShare = LANShare::getInstance();
    while (lanShare->isRun) {
        std::unique_ptr<TCPClient> tcpClient = lanShare->tcpServer->accept();
        if (tcpClient == nullptr) {
            return;
        }
        std::thread(handleTcp, std::move(tcpClient)).detach();
    }
}

std::vector<Device> LANShare::getMDevices() const {
    std::mutex &lock = StringLockManager::getStringLock("mDevicesMutex");
    lock.lock();
    std::vector<Device> devices = mDevices;
    lock.unlock();
    return devices;
}

/**
 * 发送消息
 */
void LANShare::broadcastMessage(Device *toDevice, const QString &message, bool isClip, bool shareWS) {
    int strlen = CodeUtils::getUtf8StrLen(message.toUtf8().data());
    if (strlen <= 0) {
        return;
    }
    if (shareWS) {
        lhttpServer->sendWebSocketMessage(message, config.clientName, "", 0, 1, "", false, false);
    }
    QByteArray encMessage = mUtils::encMessage(message, config.messageKey);
    int buffLen = 1024 + encMessage.size();
    auto *buffer = new mbyte[buffLen];
    if (strlen > 700) {
        std::vector<Device> devices = getMDevices();
        std::vector<Device>::iterator p1;
        if (toDevice == nullptr) {
            std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
            lock.lock();
            std::map<std::string, Device>::iterator iter;
            for (iter = onLineDevices.begin(); iter != onLineDevices.end(); iter++) {
                for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                    if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                              iter->second.getDevIp())) {
                        DataEnc dataEnc(buffer, buffLen);
                        makeDataEnc(*p1, &dataEnc);
                        dataEnc.setCmd(FS_MESSAGE);
                        dataEnc.putString(encMessage);
                        std::unique_ptr<TCPClient> socket = makeSocket(iter->second.getDevIp(),
                                                                       iter->second.getDevPort());
                        if (socket == nullptr) {
                            break;
                        }
                        socket->send(dataEnc.getData(), dataEnc.getDataLen());
                        break;
                    }
                }
            }
            lock.unlock();
        } else {
            for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                          toDevice->getDevIp())) {
                    DataEnc dataEnc(buffer, buffLen);
                    dataEnc.setCmd(FS_MESSAGE);
                    makeDataEnc(*p1, &dataEnc);
                    dataEnc.putString(encMessage);
                    std::unique_ptr<TCPClient> socket = makeSocket(toDevice->getDevIp(), toDevice->getDevPort());
                    if (socket == nullptr) {
                        break;
                    }
                    socket->send(dataEnc.getData(), dataEnc.getDataLen());
                    break;
                }
            }
        }
        return;
    }
    std::vector<Device> devices = getMDevices();
    std::vector<Device>::iterator p1;
    if (toDevice == nullptr) {
        std::mutex &lock = StringLockManager::getStringLock("mMapMutex");
        lock.lock();
        std::map<std::string, Device>::iterator iter;
        for (iter = onLineDevices.begin(); iter != onLineDevices.end(); iter++) {
            for (p1 = devices.begin(); p1 != devices.end(); p1++) {
                if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                          iter->second.getDevIp())) {
                    DataEnc dataEnc(buffer, buffLen);
                    makeDataEnc(*p1, &dataEnc);
                    dataEnc.setCmd(isClip ? UDP_MESSAGE_TO_CLIPBOARD : UDP_MESSAGE);
                    dataEnc.putString(encMessage);
                    udpSend(udpServer.get(), &dataEnc, iter->second.getDevIp(), config.udpPort);
                    break;
                }
            }
        }
        lock.unlock();
    } else {
        for (p1 = devices.begin(); p1 != devices.end(); p1++) {
            if (NetWorldUtils::subNet(NetWorldUtils::getMaskMapLength(p1->getDevNetMask()), p1->getDevIp(),
                                      toDevice->getDevIp())) {
                DataEnc dataEnc(buffer, buffLen);
                dataEnc.setCmd(isClip ? UDP_MESSAGE_TO_CLIPBOARD : UDP_MESSAGE);
                makeDataEnc(*p1, &dataEnc);
                dataEnc.putString(encMessage);
                udpSend(udpServer.get(), &dataEnc, toDevice->getDevIp(), config.udpPort);
                break;
            }
        }
    }
    delete[] buffer;
}

const std::map<std::string, Device> &LANShare::getOnLineDevices() const {
    return onLineDevices;
}

void LANShare::close() {
    // 通知设备我已下线
    for (const auto &device: getMDevices()) {
        this->noticeDeviceOffLineByIp(device.getDevBrotIp());
    }
    udpServer->close();
    tcpServer->close();
    isRun = false;
    qDebug() << "LANShare Server is closed";
}
