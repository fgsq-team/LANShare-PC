//
// Created by fgsqme on 2023/1/31.
//

#ifndef LANSHARE_WIN_MIOUTIL_H
#define LANSHARE_WIN_MIOUTIL_H

#include "IOUtils.h"
#include "TCPClient.h"

class MTCPClient : public TCPClient {
private:
    bool isClose = false;
public:
    MTCPClient(int fd);

    ~MTCPClient();

    bool IsClose() const;

    int send(const void *buff, int len, int flag = 0) const;

    int sendp(const void *buff, int len, int flag = 0) const;

    int recv(void *buff, int len, int flag = 0) const;

    int recvo(void *buff, size_t len, int flag = 0) const;

    int recvo(void *buff, int index, size_t len, int flag = 0) const;

    int close() ;

};


#endif //LANSHARE_WIN_MIOUTIL_H
