#include "LWebSocketServer.h"
#include "ByteUtils.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>


int WS_FIN = 128;
int OPCODE_CONTINUATION = 0;
int OPCODE_TEXT = 1;   // 文本
int OPCODE_BINARY = 2; // 字节
int OPCODE_CLOSE = 8;  // 关闭
int OPCODE_PING = 9;   // ping
int OPCODE_PONG = 10;  // pong


LWebSocketServer::LWebSocketServer(Request *request, Response *response)
        : tcpClient(request->getTcpClient()), request(request), response(response) {
    try {
        QString webSocketKey = request->getHeaderValue("Sec-WebSocket-Key");
        response->writeWebSocket(webSocketKey);
    }
    catch (const std::exception &e) {
        qDebug() << "error:" << e.what();
    }
}

bool LWebSocketServer::isClosed() const {
    return closed;
}

bool LWebSocketServer::isControlFrame(int frame) const {
    return frame == OPCODE_CLOSE || frame == OPCODE_PING || frame == OPCODE_PONG;
}

QByteArray LWebSocketServer::readFrame() {
    while (true) {
        char curByte = (char) tcpClient->read();
        char mask = (char) tcpClient->read();
        bool isFin = (curByte & 0x80) != 0;
        int opcode = curByte & 0xF;
        // 检查保留位错误
        if ((curByte & 0x70) > 0) {
            return {};
        }
        int frameLength = static_cast<int>(getFrameLength(mask));
        // 控制帧必须 FIN=1 且 payload <= 125
        if (isControlFrame(opcode) && (!isFin || frameLength > 125)) {
            return {};
        }
        if (opcode == OPCODE_TEXT || opcode == OPCODE_BINARY) {
            QByteArray masks(4, '\0');
            tcpClient->recv(masks.data(), 4);
            QByteArray data(frameLength, '\0');
            tcpClient->recv(data.data(), frameLength);
            for (int i = 0; i < data.size(); ++i) {
                data[i] = data[i] ^ masks[i & 0x3];
            }
            return data;
        } else if (opcode == OPCODE_PING) {
            // 心跳保活：收到 PING，回复 PONG（携带相同 payload）
            QByteArray masks(4, '\0');
            tcpClient->recv(masks.data(), 4);
            QByteArray data(frameLength, '\0');
            tcpClient->recv(data.data(), frameLength);
            for (int i = 0; i < data.size(); ++i) {
                data[i] = data[i] ^ masks[i & 0x3];
            }
            sendFrame(data, OPCODE_PONG);
            // 继续读取下一帧，不返回空以免调用方误判断连
            continue;
        } else if (opcode == OPCODE_PONG) {
            // 收到 PONG，忽略，继续读取下一帧
            continue;
        } else if (opcode == OPCODE_CLOSE) {
            // 收到关闭帧，回送 CLOSE 帧后关闭连接
            sendFrame(QByteArray(), OPCODE_CLOSE);
            close();
            return {};
        }
        // 未知 opcode，忽略
        return {};
    }
}

qint64 LWebSocketServer::getFrameLength(char mask) {
    qint64 frameLength = mask & 0x7F;
    if (frameLength == 126) {
        mbyte buf[2];
        tcpClient->recv(buf, 2);
        frameLength = ByteUtils::bytesToShort(buf);
    } else if (frameLength == 127) {
        mbyte buf[8];
        tcpClient->recv(buf, 8);
        frameLength = ByteUtils::bytesToLong(buf);
    }
    return frameLength;
}

void LWebSocketServer::sendString(const QByteArray &msg) {
    sendFrame(msg, OPCODE_TEXT);
}

void LWebSocketServer::sendFrame(const QByteArray &data, int opCode) {
    tcpClient->send(WS_FIN | opCode);
    int len = data.size();
    if (len < 126) {
        tcpClient->send(len);
    } else if (len <= 65535) {
        mbyte buff[2];
        ByteUtils::shortToBytes((short) len, buff);
        tcpClient->send(126);
        tcpClient->send(buff, 2);
    } else {
        mbyte buff[8];
        ByteUtils::longToBytes(len, buff);
        tcpClient->send(127);
        tcpClient->send(buff, 8);
    }
    int i = tcpClient->send(data.data(), data.size());
    if (i < 0) {
        close();
    }
}

void LWebSocketServer::close() {
    tcpClient->close();
    closed = true;
}
