#include "GlobalVariable.h"
#include "IOTask.h"
#include "IoUringWrapper.h"
#include "DediManager.h"
#include "ObjectPool.h"

H2SAcceptTask::H2SAcceptTask(int listenFd, IoUringWrapper* uring) : _uring(uring) {
    fd = listenFd;
    type = IOTaskType::ACCEPT_IPC_HTTP;
}

void H2SAcceptTask::callback(int result) {
    if (result < 0) {
        std::cerr << "C3-3 - X HTTP IPC 연결 실패: " << result << std::endl;
        _uring->RegisterAcceptTask(fd, this);
        return;
    }

    int httpIPCsockFd = result;
    std::cout << "C3-3 - OK HTTP IPC 연결 성공 FD: " << httpIPCsockFd << std::endl;

    if (HttpSession == nullptr) {
        HttpSession = new HttpIPCSession(httpIPCsockFd, _uring);
        HttpSession->Recv();
    }

    //_uring->RegisterAcceptTask(fd, this); H2S 소켓은 유일해야 함. 나중에 문제가 생겼을 때, 재실행 로직은 구현 예정 없음 ㅋ
    ObjectPool<H2SAcceptTask>::Release(this);
}


H2SReadTask::H2SReadTask(int fd, void* buf, size_t len, HttpIPCSession* pSession) : _pSession(pSession) {
    this->fd = fd;
    this->type = IOTaskType::READ_IPC_HTTP;
}

void H2SReadTask::callback(int readBytes) {
    _pSession->OnReadComplete(readBytes);
    ObjectPool<H2SReadTask>::Release(this);
}

DediAcceptTask::DediAcceptTask(int listenFd, IoUringWrapper* uring) : _uring(uring) {
    fd = listenFd;
    type = ACCEPT_IPC_DEDICATE;
}

void DediAcceptTask::callback(int result) {
    if (result < 0) {
        _uring->RegisterAcceptTask(fd, this);
        return;
    }

    int DediIPCsockFd = result;

    M2DTempSession* pTempSession = new M2DTempSession(DediIPCsockFd, _uring);
    pDediManager->OnAcceptDedi(DediIPCsockFd, pTempSession);

    _uring->RegisterAcceptTask(fd, this);
}

DediRecvTask::DediRecvTask(int fd, void* buf, size_t len, Session* pSession) : _pSession(pSession) {
    this->fd = fd;
    this->type = IOTaskType::READ_IPC_DEDICATE;
}

void DediRecvTask::callback(int readBytes) {
    _pSession->OnReadComplete(readBytes);
    ObjectPool<DediRecvTask>::Release(this);
}

IPCSendTask::IPCSendTask(SendBuffer* buffer, Session* session) {
    this->fd = session->_fd;
    this->pBuffer = buffer;
    this->pSession = session;
    this->type = IOTaskType::SEND_IPC;
}

void IPCSendTask::callback(int result) {
    pSession->OnWriteComplete(result);

    // TODO : 지금은 한번에 보내진다고 가정하지만,
    // FM대로 하자면, 모종의 이유로 한번에 모든 버퍼가 보내지지 못하는 경우(result < pBuffer->WriteSize()인 경우)
    // 그 차분만큼의 버퍼를 재전송 요청하는 로직을 짜야함. (SendBuffer의 _index를 result만큼 시프트, WriteSize를 result만큼 감소시켜서 재시도)
    // 아마 위의 OnWriteComplete에서 인자로 이 객체의 포인터 this를 넣고 해당 경우를 대비해야겠지만 귀찮으므로 패스 
    ObjectPool<SendBuffer>::Release(pBuffer);
    ObjectPool<IPCSendTask>::Release(this);
}