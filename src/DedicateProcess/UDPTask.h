#pragma once

#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../ObjectPool.h"
#include "../IOTask.h"

class D2CSession;

class D2CRecvTask : public IOTask {
public:
    D2CRecvTask(int fd, D2CSession* pSession);

    void callback(int readBytes) override;
    struct msghdr* GetMsgHdr() { return &_msgHdr; }

private:
    D2CSession* _pSession;

    struct sockaddr_in _clientAddr = {};
    struct iovec _iovec = {};
    struct msghdr _msgHdr = {};
    alignas(64) unsigned char _recvBuffer[1024];
};

class D2CSendTask : public IOTask {
public:
    D2CSendTask(int fd, D2CSession* pSession, SendBuffer* buffer, const sockaddr_in& destAddr);
    void callback(int result);

    struct msghdr* GetMsgHdr() { return &_msgHdr; }

private:
    SendBuffer* _pBuffer;
    D2CSession* _pSession;

    struct sockaddr_in _destAddr; 
    struct iovec _iovec;
    struct msghdr _msgHdr;
};