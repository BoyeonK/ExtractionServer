#include <iostream>
#include "SocketWrapper.h"
#include "PacketHandler.h"
#include "IoUringWrapper.h"
#include "ObjectPool.h"
#include "DediManager.h"
#include "IPCProtocol/IPC_HTTP.pb.h"
#include "IPCProtocol/IPC_Dedicate.pb.h"

void IPCListenSocketWrapper::Init() {
    _listenFd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (_listenFd == -1) throw std::runtime_error("Socket creation failed");

    unlink(_sockPath.c_str());

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, _sockPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(_listenFd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(_listenFd);
        throw std::runtime_error("Bind failed for path: " + _sockPath);
    }

    if (listen(_listenFd, _queueSize) == -1) {
        close(_listenFd);
        throw std::runtime_error("Listen failed");
    }
}

Session::Session(int fd, IoUringWrapper* uring) : _fd(fd), _uring(uring) {

}

Session::~Session() {
    if (_fd != -1)
        ::close(_fd);
}

HttpIPCSession::HttpIPCSession(int fd, IoUringWrapper* uring) : Session(fd, uring) {

}

HttpIPCSession::~HttpIPCSession() {

}

void HttpIPCSession::Recv() {
    H2SReadTask* readTask = ObjectPool<H2SReadTask>::Acquire(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), this);
    _uring->RegisterRecv(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), readTask);
}

void HttpIPCSession::Send(SendBuffer* sendBuffer) {

}

void HttpIPCSession::OnReadComplete(int readBytes) {
    if (readBytes > 0) {
        _recvBuffer.OnRead(readBytes);
        while (true) {
            size_t currentDataSize = _recvBuffer.DataSize();

            if (currentDataSize < sizeof(PacketHeader)) {
                break;
            }
            
            PacketHeader header = *(reinterpret_cast<PacketHeader*>(_recvBuffer.ProcessedPos()));

            if (currentDataSize < header._size) {
                break;
            }

            if (PacketHandler::HandlePacket(this, _recvBuffer.ProcessedPos(), header._size)){
                _recvBuffer.OnProcess(header._size);
            } else {
                return;
            }
        }
        Recv();
    }
    else if (readBytes == 0) {
        //TODO : 0byte Recv 처리
    } 
    else {
        //TODO : 에러 처리
        /*
        switch(readBytes):
        case -EAGAIN:
            break;
        case -EWOULDBLOCK:
            break;
        case -ECONNRESET:
            break;
        */
    }
}

void HttpIPCSession::OnWriteComplete(int result) {

}

