#pragma once
#include <liburing.h>
#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>

#include "IOTask.h"
#include "SendBuffer.h"

class IoUringWrapper {
public:
    IoUringWrapper();
    ~IoUringWrapper();

    IoUringWrapper(const IoUringWrapper&) = delete;
    IoUringWrapper& operator=(const IoUringWrapper&) = delete;

    bool ExecuteCQTask();
    void RegisterRecv(int fd, void* buf, int32_t len, IOTask* task);
    void RegisterIPCSendTask(IPCSendTask* pIPCSendTask);

    void RegisterRecvMsg(int fd, struct msghdr* msg, IOTask* task);
    void RegisterSendMsg(int fd, struct msghdr* msg, IOTask* task);

    void RegisterAcceptTask(int listenFd, IOTask* task);

    SendBuffer* OpenSendBuffer(uint32_t allocSize);

private:
    struct io_uring _ring;
    std::shared_ptr<SendBufferChunk> _sendBufferChunkRef = nullptr;
};