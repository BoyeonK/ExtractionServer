#pragma once
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

#include "RecvBuffer.h"
#include "SendBuffer.h"

class IoUringWrapper;

class IPCListenSocketWrapper {
public:
    IPCListenSocketWrapper(std::string sockPath, int queueSize) 
        : _sockPath(std::move(sockPath)), _queueSize(queueSize), _listenFd(-1) {}

    ~IPCListenSocketWrapper() {
        if (_listenFd != -1) {
            close(_listenFd);
            unlink(_sockPath.c_str());
        }
    }

    IPCListenSocketWrapper(const IPCListenSocketWrapper&) = delete;
    IPCListenSocketWrapper& operator=(const IPCListenSocketWrapper&) = delete;

    void Init();
    int GetFd() const { return _listenFd; }

private:
    int _listenFd;
    std::string _sockPath;
    int _queueSize;
};

class Session {
public:
    Session(int fd, IoUringWrapper* uring);
    virtual ~Session();

    virtual void Recv() {};
    virtual void Send(SendBuffer* sendBuffer) {};
    int GetFd() const { return _fd; }

    virtual void OnReadComplete(int result) {};
    virtual void OnWriteComplete(int result) {};

    int _fd;
    IoUringWrapper* _uring;
    RecvBuffer _recvBuffer;
};

class HttpIPCSession : public Session {
public:
    HttpIPCSession(int fd, IoUringWrapper* uring);
    ~HttpIPCSession();

    void Recv() override;
    void Send(SendBuffer* sendBuffer) override;

    void OnReadComplete(int readBytes) override;
    void OnWriteComplete(int result) override;
};
