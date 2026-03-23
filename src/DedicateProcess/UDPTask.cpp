#include "UDPTask.h"
#include "DediSessions.h"

D2CRecvTask::D2CRecvTask(int fd, D2CSession* pSession) : _pSession(pSession) {
    this->fd = fd;
    this->type = IOTaskType::READ_CLIENT;

    _iovec.iov_base = _recvBuffer;
    _iovec.iov_len = sizeof(_recvBuffer);

    std::memset(&_msgHdr, 0, sizeof(_msgHdr));
    _msgHdr.msg_name = &_clientAddr;
    _msgHdr.msg_namelen = sizeof(_clientAddr);
    _msgHdr.msg_iov = &_iovec;
    _msgHdr.msg_iovlen = 1;
}

void D2CRecvTask::callback(int readBytes) {
    if (_pSession && readBytes > 0) {
        _pSession->OnRecvComplete(readBytes, _recvBuffer, _clientAddr);
    }

    ObjectPool<D2CRecvTask>::Release(this);
}

D2CSendTask::D2CSendTask(int fd, SendBuffer* buffer, const sockaddr_in& destAddr) 
: _pBuffer(buffer), _destAddr(destAddr) {
    this->fd = fd;
    this->type = IOTaskType::SEND_CLIENT;

    _iovec.iov_base = _pBuffer->Buffer();
    _iovec.iov_len = _pBuffer->WriteSize();

    std::memset(&_msgHdr, 0, sizeof(_msgHdr));
    _msgHdr.msg_name = &_destAddr;
    _msgHdr.msg_namelen = sizeof(_destAddr);
    _msgHdr.msg_iov = &_iovec;
    _msgHdr.msg_iovlen = 1;
}

void D2CSendTask::callback(int result) {
    if (result < 0) {
        std::cerr << "[UDP Send Error] result: " << result << std::endl;
    }
    _pSession->OnWriteComplete(result);

    //TODO : SendBuffer*의 처리, Broadcast를 구현할거라면 Broadcast용 별도의 SendTask를 만들어야 할듯
    ObjectPool<SendBuffer>::Release(_pBuffer);
    ObjectPool<D2CSendTask>::Release(this);
}