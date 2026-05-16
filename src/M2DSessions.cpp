#include "M2DSessions.h"

#include <iostream>
#include "SocketWrapper.h"
#include "ObjectPool.h"
#include "PacketHandler.h"
#include "DediManager.h"

// ----------------------
// M2DSession
// ----------------------

M2DSession::~M2DSession() {}

void M2DSession::BindSocket(int fd) {
    _fd = fd;
    _state = M2DSession::SessionState::Ready;
    Recv();
}

void M2DSession::Recv() {
    DediRecvTask* readTask = ObjectPool<DediRecvTask>::Acquire(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), this);
    _uring->RegisterRecv(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), readTask);
}

void M2DSession::Send(SendBuffer* sendBuffer) {
    IPCSendTask* pTask = ObjectPool<IPCSendTask>::Acquire(sendBuffer, this);
    IORing->RegisterIPCSendTask(pTask);
}

void M2DSession::OnReadComplete(int readBytes) {
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

void M2DSession::OnWriteComplete(int result) {

}

bool M2DSession::AllocatePlayers(TicketVector& ticketVec) {
    if (GetAffordablePlayers() >= static_cast<int>(ticketVec.size()) && _state != SessionState::Terminated) {
        _allocatedPlayers += ticketVec.size();

        int mapId = ticketVec[0]->mapId;

        IPC_Protocol::M2DMakeRoomForThisGroup pkt = MakeM2DMakeRoomForThisGroup(mapId, ticketVec);
        _tempMatchPkts.push_back(std::move(pkt));

        if (_state == SessionState::Ready){
            FlushPendingTickets();
        }

        return true;
    }
    return false;
}

void M2DSession::FlushPendingTickets() {
    if (_tempMatchPkts.empty()) return;

    for (auto& pkt : _tempMatchPkts) {
        SendBuffer* sendBuffer = PacketHandler::MakeSendBuffer(pkt);
        Send(sendBuffer);
    }
    _tempMatchPkts.clear();
}

// ----------------------
// M2DTempSession
// ----------------------

void M2DTempSession::Recv() {
    DediRecvTask* readTask = ObjectPool<DediRecvTask>::Acquire(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), this);
    _uring->RegisterRecv(_fd, _recvBuffer.ReadPos(), _recvBuffer.FreeSize(), readTask);
}

void M2DTempSession::OnReadComplete(int readBytes) {
    _recvBuffer.OnRead(readBytes);
    if (readBytes > 0) {
        if (_recvBuffer.DataSize() < sizeof(PacketHeader)) {
            Recv();
            return;
        }

        PacketHeader header = *(reinterpret_cast<PacketHeader*>(_recvBuffer.ProcessedPos()));

        if (_recvBuffer.DataSize() < header._size) {
            Recv();
            return;
        }

        void* payloadPtr = _recvBuffer.ProcessedPos() + sizeof(PacketHeader);
        size_t payloadSize = header._size - sizeof(PacketHeader);
        IPC_Protocol::D2MInitComplete pkt;
        if (pkt.ParseFromArray(payloadPtr, static_cast<int>(payloadSize))) {
            int childPid = pkt.pid();
            if (pDediManager->FinalizeConnection(childPid, this->GetFd())) {
                this->ReleaseFd(); // 성공 시 FD 소유권 포기 (M2DSession이 가져감)
            } else {
                std::cerr << "[DediTemp] Failed to finalize connection for PID: " << childPid << std::endl;
            }

            delete this;
            return;
        } else {
            // 패킷 해석 실패 (잘못된 접근)
            std::cerr << "[DediTemp] Protobuf Parse Error!" << std::endl;
            delete this;
            return;
        }
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
