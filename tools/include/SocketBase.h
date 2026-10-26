//
// Created by fgsqme on 2022/3/9 0009.
//

#ifndef LANSHARE_WIN_SOCKETBASE_H
#define LANSHARE_WIN_SOCKETBASE_H

#include "Type.h"

#include <QString>
#include <QByteArray>
#include <vector>
#include <cstring>
#include <cstdio>

#if defined(PLATFORM_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET mFd;
#else

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <string>

typedef int mFd;
#endif

/** 通用地址类型：同一份代码同时容纳 IPv4 / IPv6 地址 */
#define CLIENT_ADDR sockaddr_storage

/** 平台句柄（Windows 为 SOCKET，其它平台为 int）与 int 互转 */
inline mFd fdFromInt(int fd) {
    return (mFd) (mlong) fd;
}

inline int intFromFd(mFd fd) {
    return (int) (mlong) fd;
}

/** select 轮询间隔（毫秒），保证关闭 Socket 后阻塞接收可以及时退出 */
#define SELECT_POLL_MS 200

/** 无效 socket（Windows 为 INVALID_SOCKET，其它平台为 -1） */
inline mFd netInvalidFd() {
#if defined(PLATFORM_WINDOWS)
    return INVALID_SOCKET;
#else
    return -1;
#endif
}

/** 判断 socket 是否有效 */
inline bool fdValid(mFd fd) {
#if defined(PLATFORM_WINDOWS)
    return fd != INVALID_SOCKET;
#else
    return fd >= 0;
#endif
}

/**
 * 网络栈初始化
 * Windows 需要 WSAStartup，其它平台为空操作；进程内只会真正执行一次
 * @return 初始化成功返回 true
 */
inline bool netInit() {
#if defined(PLATFORM_WINDOWS)
    static const bool inited = []() {
        WSADATA wsaData{};
        return ::WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
    }();
    return inited;
#else
    return true;
#endif
}

/**
 * 关闭 socket（屏蔽 closesocket / close 差异）
 * @param fd 待关闭的 socket
 * @return 关闭结果，fd 无效时返回 -1
 */
inline int netClose(mFd fd) {
    if (!fdValid(fd)) {
        return -1;
    }
#if defined(PLATFORM_WINDOWS)
    return ::closesocket(fd);
#else
    return ::close((int) fd);
#endif
}

/** 获取最近一次网络错误码 */
inline int netLastError() {
#if defined(PLATFORM_WINDOWS)
    return (int) ::WSAGetLastError();
#else
    return errno;
#endif
}

/** 错误码是否属于可忽略的中断/超时（接收循环可继续等待） */
inline bool netErrRetryable(int err) {
#if defined(PLATFORM_WINDOWS)
    return err == WSAEINTR || err == WSAETIMEDOUT || err == WSAEWOULDBLOCK;
#else
    return err == EINTR || err == EAGAIN || err == EWOULDBLOCK;
#endif
}

/** 设置 socket 选项（屏蔽 const char* / const void* 与长度类型差异） */
inline bool setSockOpt(mFd fd, int level, int optname, const void *val, size_t len) {
    return ::setsockopt(fd, level, optname, reinterpret_cast<const char *>(val), (socklen_t) len) == 0;
}

/** 允许地址复用，便于服务重启后立即重新绑定同一端口 */
inline bool enableReuseAddr(mFd fd) {
    int opt = 1;
    return setSockOpt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

/** 允许广播（IPv4 生效，IPv6 使用组播地址，该选项无副作用） */
inline bool enableBroadcast(mFd fd) {
    int opt = 1;
    return setSockOpt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
}

/**
 * 设置 IPv6 socket 是否只处理 IPv6
 * 必须显式设置：Linux 默认允许 v4 映射（会独占端口），Windows 默认为仅 v6，
 * 显式置 1 后与独立的 IPv4 socket 双栈并存，行为在所有平台一致
 */
inline bool enableV6Only(mFd fd, bool v6only) {
    int opt = v6only ? 1 : 0;
    return setSockOpt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
}

/** 设置地址中的端口（兼容 IPv4 / IPv6） */
inline void addrSetPort(CLIENT_ADDR &addr, int port) {
    const unsigned short netPort = htons((unsigned short) port);
    if (addr.ss_family == AF_INET) {
        ((sockaddr_in *) &addr)->sin_port = netPort;
    } else if (addr.ss_family == AF_INET6) {
        ((sockaddr_in6 *) &addr)->sin6_port = netPort;
    }
}

/**
 * 解析 IP 字符串为 socket 地址
 * 支持 IPv4、IPv6（可携带 %网卡名 或 %区域索引）、广播地址、主机名，空串表示任意地址
 * @param ip 地址字符串
 * @param port 端口
 * @param out 解析结果（输出）
 * @param outLen 地址结构长度（输出）
 * @return 解析成功返回 true
 */
inline bool resolveAddr(const QString &ip, int port, CLIENT_ADDR &out, socklen_t &outLen) {
    memset(&out, 0, sizeof(out));
    const QByteArray raw = ip.trimmed().toUtf8();
    const char *str = raw.constData();
    const unsigned short netPort = (unsigned short) port;

    // 空串 / 0.0.0.0：监听本机所有 IPv4 接口
    if (raw.isEmpty() || strcmp(str, "0.0.0.0") == 0) {
        out.ss_family = AF_INET;
        auto *v4 = (sockaddr_in *) &out;
        v4->sin_addr.s_addr = htonl(INADDR_ANY);
        v4->sin_port = htons(netPort);
        outLen = sizeof(sockaddr_in);
        return true;
    }

    // IPv4 字面量
    if (inet_pton(AF_INET, str, &((sockaddr_in *) &out)->sin_addr) == 1) {
        out.ss_family = AF_INET;
        ((sockaddr_in *) &out)->sin_port = htons(netPort);
        outLen = sizeof(sockaddr_in);
        return true;
    }

    // IPv6 字面量，形如 fe80::1%12 或 fe80::1%wlan0
    QByteArray v6Str = raw;
    int zoneIdx = raw.indexOf('%');
    if (zoneIdx >= 0) {
        v6Str = raw.left(zoneIdx);
    }
    if (inet_pton(AF_INET6, v6Str.constData(), &((sockaddr_in6 *) &out)->sin6_addr) == 1) {
        out.ss_family = AF_INET6;
        auto *v6 = (sockaddr_in6 *) &out;
        v6->sin6_port = htons(netPort);
        if (zoneIdx >= 0) {
            const QByteArray zone = raw.mid(zoneIdx + 1);
            bool numeric = false;
            uint32_t scope = zone.toUInt(&numeric);
            if (numeric && scope > 0) {
                v6->sin6_scope_id = scope;
            } else {
#if defined(PLATFORM_WINDOWS)
                // Windows 下不支持按网卡名称查询索引，仅接受数字区域索引
                v6->sin6_scope_id = 0;
#else
                v6->sin6_scope_id = (uint32_t) if_nametoindex(zone.constData());
#endif
            }
        }
        outLen = sizeof(sockaddr_in6);
        return true;
    }

    // 其余交给系统解析（主机名、服务名等）
    if (!netInit()) {
        return false;
    }
    struct addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *res = nullptr;
    const QByteArray service = QByteArray::number(port);
    if (::getaddrinfo(str, service.constData(), &hints, &res) != 0 || res == nullptr) {
        return false;
    }
    bool ok = false;
    if (res->ai_addrlen <= sizeof(CLIENT_ADDR)) {
        memset(&out, 0, sizeof(out));
        memcpy(&out, res->ai_addr, res->ai_addrlen);
        // getaddrinfo 未填充端口时补上，避免服务名为空导致端口丢失
        addrSetPort(out, port);
        outLen = (socklen_t) res->ai_addrlen;
        ok = true;
    }
    ::freeaddrinfo(res);
    return ok;
}

/** 获取地址中的端口 */
inline int addrPort(const CLIENT_ADDR &addr) {
    if (addr.ss_family == AF_INET) {
        return ntohs(((const sockaddr_in *) &addr)->sin_port);
    }
    if (addr.ss_family == AF_INET6) {
        return ntohs(((const sockaddr_in6 *) &addr)->sin6_port);
    }
    return -1;
}

/** 地址结构实际长度 */
inline socklen_t addrLength(const CLIENT_ADDR &addr) {
    if (addr.ss_family == AF_INET) {
        return sizeof(sockaddr_in);
    }
    if (addr.ss_family == AF_INET6) {
        return sizeof(sockaddr_in6);
    }
    return sizeof(CLIENT_ADDR);
}

/** 判断是否为 IPv4 映射的 IPv6 地址（::ffff:a.b.c.d） */
inline bool isV4Mapped(const CLIENT_ADDR &addr) {
    if (addr.ss_family != AF_INET6) {
        return false;
    }
    const auto *v6 = (const sockaddr_in6 *) &addr;
    return IN6_IS_ADDR_V4MAPPED((const struct in6_addr *) &v6->sin6_addr) != 0;
}

/**
 * 将 IPv4 映射的 IPv6 地址还原为普通 IPv4 地址
 * 双栈 socket 收到的 IPv4 数据会以该形式上报，还原后便于按 IPv4 处理
 */
inline void unmapV4(CLIENT_ADDR &addr) {
    if (!isV4Mapped(addr)) {
        return;
    }
    const auto *v6 = (const sockaddr_in6 *) &addr;
    const auto *bytes = (const unsigned char *) &v6->sin6_addr;
    CLIENT_ADDR v4{};
    v4.ss_family = AF_INET;
    memcpy(&((sockaddr_in *) &v4)->sin_addr.s_addr, bytes + 12, 4);
    ((sockaddr_in *) &v4)->sin_port = v6->sin6_port;
    addr = v4;
}

/**
 * 地址转字符串
 * IPv4 映射地址会还原为点分十进制，便于与业务层已有的 IPv4 地址直接比较
 * @param addr socket 地址
 * @param withZone 链路本地 IPv6 是否附带 %区域索引（附带后可直接回传 resolveAddr）
 * @return 地址字符串，地址族不支持时返回空串
 */
inline QString addrHost(const CLIENT_ADDR &addr, bool withZone = false) {
    char buf[INET6_ADDRSTRLEN] = {0};
    // 部分平台的 inet_ntop 第二参为非 const 指针，这里取局部副本后传入
    if (addr.ss_family == AF_INET) {
        sockaddr_in v4 = *(const sockaddr_in *) &addr;
        if (inet_ntop(AF_INET, &v4.sin_addr, buf, sizeof(buf)) == nullptr) {
            return QString();
        }
        return QString::fromLatin1(buf);
    }
    if (addr.ss_family == AF_INET6) {
        CLIENT_ADDR tmp = addr;
        unmapV4(tmp);
        if (tmp.ss_family == AF_INET) {
            return addrHost(tmp, withZone);
        }
        sockaddr_in6 v6 = *(const sockaddr_in6 *) &addr;
        if (inet_ntop(AF_INET6, &v6.sin6_addr, buf, sizeof(buf)) == nullptr) {
            return QString();
        }
        QString host = QString::fromLatin1(buf);
        if (withZone && v6.sin6_scope_id != 0) {
            host += QChar('%') + QString::number(v6.sin6_scope_id);
        }
        return host;
    }
    return QString();
}

/**
 * 等待任意一个 socket 可读
 * @param fds 待监听的 socket 集合，无效 socket 会被忽略
 * @param timeoutMs 超时毫秒数，小于 0 表示无限等待
 * @param errored 输出参数，置 true 表示 select 本身出错（socket 已不可用）
 * @return 第一个可读的 socket，超时或出错返回无效 fd
 */
inline mFd selectReadable(const std::vector<mFd> &fds, int timeoutMs, bool *errored = nullptr) {
    if (errored != nullptr) {
        *errored = false;
    }
    fd_set readSet;
    FD_ZERO(&readSet);
    bool any = false;
    mFd maxFd = netInvalidFd();
    for (mFd fd: fds) {
        if (!fdValid(fd)) {
            continue;
        }
        FD_SET(fd, &readSet);
        if (!any || fd > maxFd) {
            maxFd = fd;
            any = true;
        }
    }
    if (!any) {
        return netInvalidFd();
    }
    timeval tv{};
    timeval *ptv = nullptr;
    if (timeoutMs >= 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
        ptv = &tv;
    }
#if defined(PLATFORM_WINDOWS)
    // Windows 下第一个参数被忽略，固定传 0
    int ret = ::select(0, &readSet, nullptr, nullptr, ptv);
#else
    int ret = ::select((int) maxFd + 1, &readSet, nullptr, nullptr, ptv);
#endif
    if (ret < 0) {
        if (errored != nullptr) {
            // 被信号中断不算错误，调用方可继续等待
            *errored = !netErrRetryable(netLastError());
        }
        return netInvalidFd();
    }
    if (ret == 0) {
        return netInvalidFd();
    }
    for (mFd fd: fds) {
        if (fdValid(fd) && FD_ISSET(fd, &readSet)) {
            return fd;
        }
    }
    return netInvalidFd();
}


#endif //LANSHARE_WIN_SOCKETBASE_H
