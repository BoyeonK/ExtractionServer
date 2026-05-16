#include "DediSessions.h"

#include <iostream>
#include "../SocketWrapper.h"
#include "../ObjectPool.h"
#include "../PacketHandler.h"
#include "UDPTask.h"
#include "ClientPacketHandler.h"

// ----------------------
// D2MSession
// ----------------------

void D2MSession::Recv() {
    DediRecvTask* readTask = ObjectPool<DediRecvTask>::Acquire(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), this);
    _uring->RegisterRecv(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), readTask);
}

void D2MSession::Send(SendBuffer* sendBuffer) {
    IPCSendTask* pTask = ObjectPool<IPCSendTask>::Acquire(sendBuffer, this);
    IORing->RegisterIPCSendTask(pTask);
}

void D2MSession::OnReadComplete(int readBytes) {
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
                // TODO :
                // 이 프로세스는 망했다. 아래의 작동을 DediServerService에 추가하고 사망시마다 활용해야 할 듯함.
                // 1. 가능하다면, 연결된 모든 플레이어와 메인 프로세스에 부고 소식을 알린다.
                    // 1-1. 이 프로세스에서 연결된 모든 플레이어의 정보를 메인프로세스에 뱉고 죽는다. (ticket들)
                    // 1-2. 메인프로세스에서 이 플레이어의 정보를 HTTP서버로 전송한다.
                    // 1-3. HTTP서버에서 받은 플레이어의 ticket을 모두 조회하여, 아이템을 모두 롤백한다.
                // 2. 이 프로세스에 할당된 모든 자원을 반환하고 자결한다.
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

void D2MSession::OnWriteComplete(int result) {

}

// ------------------------------------------------------------------------
// D2CSession (기존의 Session을 상속받디 않는 독립적인친구, 클라이언트와 UDP연결쪽)
// ------------------------------------------------------------------------

D2CSession::D2CSession(int fd, IoUringWrapper* ring) : _fd(fd), _uring(ring) {}

void D2CSession::PumpRecvTasks(int count) {
    for (int i = 0; i < count; i++) {
        D2CRecvTask* recvTask = ObjectPool<D2CRecvTask>::Acquire(_fd, this);
        _uring->RegisterRecvMsg(_fd, recvTask->GetMsgHdr(), recvTask);
    }
}

void D2CSession::Send(SendBuffer* buffer, const sockaddr_in& destAddr) {
    D2CSendTask* sendTask = ObjectPool<D2CSendTask>::Acquire(_fd, this, buffer, destAddr);
    _uring->RegisterSendMsg(_fd, sendTask->GetMsgHdr(), sendTask);
}

void D2CSession::OnRecvComplete(int bytesTransferred, unsigned char* buffer, const sockaddr_in& clientAddr) {
    // 이전 분기에서 bytesTransferred가 양수인 경우만 선택적으로 여기까지 들어옴
    if (bytesTransferred >= sizeof(UDPHeader)) {
        ClientPacketHandler::HandleClientPacket(bytesTransferred, buffer, clientAddr);
    }

    PumpRecvTasks(1);
}

void D2CSession::OnWriteComplete(int result) {

}
