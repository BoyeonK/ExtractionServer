#pragma once

#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../GlobalVariable.h"
#include "../SocketWrapper.h"
#include "../IoUringWrapper.h"

class SendBuffer;

class D2MSession : public Session {
public:
    D2MSession(int fd, IoUringWrapper* uring) : Session(fd, uring) {};
    ~D2MSession() {};

    void Recv() override;
    void Send(SendBuffer* sendBuffer) override;

    void OnReadComplete(int readBytes) override;
    void OnWriteComplete(int result) override;
};

class D2CSession {
public:
    D2CSession(int fd, IoUringWrapper* ring);

    void PumpRecvTasks(int count = 10);
    void Send(SendBuffer* buffer, const sockaddr_in& destAddr);
    void OnRecvComplete(int bytesTransferred, unsigned char* buffer, const sockaddr_in& clientAddr);
    void OnWriteComplete(int result);

private:
    int _fd;
    IoUringWrapper* _uring;
};
