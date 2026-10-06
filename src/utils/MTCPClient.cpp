//
// Created by fgsqme on 2023/1/31.
//

#include "MTCPClient.h"
#include "LException.h"


/** 构造函数 */
MTCPClient::MTCPClient(int fd) : TCPClient(fd) {

}

/** 析构函数 */
MTCPClient::~MTCPClient() {
    this->close();
}

/** 判断是否已关闭 */
bool MTCPClient::IsClose() const {
    return isClose;
}

/** 发送数据（已关闭时抛异常） */
int MTCPClient::send(const void *buff, int len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::send(buff, len, flag);
}

/** 发送数据（不检查关闭状态） */
int MTCPClient::sendp(const void *buff, int len, int flag) const {
    return TCPClient::send(buff, len, flag);
}

/** 接收数据（已关闭时抛异常） */
int MTCPClient::recv(void *buff, int len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::recv(buff, len, flag);
}

/** 完整接收指定长度数据（已关闭时抛异常） */
int MTCPClient::recvo(void *buff, size_t len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::recvo(buff, len, flag);
}

/** 完整接收指定长度数据（带偏移，已关闭时抛异常） */
int MTCPClient::recvo(void *buff, int index, size_t len, int flag) const {
    if (isClose) throw LException("recv is close");
    return TCPClient::recvo(buff, index, len, flag);
}

/** 关闭连接（仅设置关闭标志） */
int MTCPClient::close()  {
    isClose = true;
    return 1;
}


