#pragma once
#include "../SocketWrapper.h"
#include "Matchmaker.h"
#include "../IPCProtocol/IPC_Dedicate.pb.h"
#include "PacketHandler.h"
#include "UDPTask.h"

class M2DSession : public Session {
static constexpr int MAX_PLAYER_PER_PROCESS = 50;
public:
    enum class SessionState {
        Initializing,
        Ready,
        Terminated,
    };

    M2DSession(int pid, IoUringWrapper* uring) : Session(-1, uring), _pid(pid), _state(SessionState::Initializing) {}
    ~M2DSession();

    void BindSocket(int fd);

    void Recv() override;
    void Send(SendBuffer* sendBuffer) override;

    void OnReadComplete(int readBytes) override;
    void OnWriteComplete(int result) override;

    int GetAffordablePlayers() const { 
        return MAX_PLAYER_PER_PROCESS - _allocatedPlayers; 
    }

    bool AllocatePlayers(TicketVector& ticketVec) {
        if (GetAffordablePlayers() >= static_cast<int>(ticketVec.size()) && _state != SessionState::Terminated) {
            _allocatedPlayers += ticketVec.size();

            IPC_Protocol::M2DMakeRoomForThisGroup pkt = MakeM2DMakeRoomForThisGroup(ticketVec);
            _tempMatchPkts.push_back(std::move(pkt));

            if (_state == SessionState::Ready){
                FlushPendingTickets(); 
            } 

            return true;
        }
        return false;
    }

    void FlushPendingTickets() {
        if (_tempMatchPkts.empty()) return;

        for (auto& pkt : _tempMatchPkts) {
            SendBuffer* sendBuffer = PacketHandler::MakeSendBuffer(pkt);
            Send(sendBuffer);
        }
        _tempMatchPkts.clear();
    }

private:
    static IPC_Protocol::M2DMakeRoomForThisGroup MakeM2DMakeRoomForThisGroup(TicketVector& group) {
		IPC_Protocol::M2DMakeRoomForThisGroup pkt;

        pkt.mutable_ticket_id()->Reserve(group.size());

        for (auto& ticket : group) {
            if (ticket != nullptr) {
                pkt.add_ticket_id(ticket->ticketId);
            }
        }
        return pkt;
	}

    int _pid;
    int _ingamePlayers = 0;
    int _allocatedPlayers = 0;
    SessionState _state;
    std::vector<IPC_Protocol::M2DMakeRoomForThisGroup> _tempMatchPkts;
};

class M2DTempSession : public Session {
public:
    M2DTempSession(int fd, IoUringWrapper* uring) : Session(fd, uring) {};
    ~M2DTempSession() {}

    void ReleaseFd() { _fd = -1; }
    void Recv() override;
    void OnReadComplete(int readBytes) override;

private:
    void Send(SendBuffer* sendBuffer) override {}
    void OnWriteComplete(int result) override {}
};

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
    D2CSession(int fd, IoUringWrapper* ring) : _fd(fd), _uring(ring) {
        // msghdr 및 iovec 메모리 세팅 (세션이 생성될 때 한 번만 해두면 됩니다)
        _iovec.iov_base = _recvBuffer;
        _iovec.iov_len = sizeof(_recvBuffer);

        _msgHdr.msg_name = &_clientAddr;
        _msgHdr.msg_namelen = sizeof(_clientAddr);
        _msgHdr.msg_iov = &_iovec;
        _msgHdr.msg_iovlen = 1;
        _msgHdr.msg_control = nullptr;
        _msgHdr.msg_controllen = 0;
    }

    void RegisterRecv() {
        D2CRecvTask* recvTask = ObjectPool<D2CRecvTask>::Acquire(_fd, this);
        
        _uring->RegisterRecvMsg(_fd, &_msgHdr, recvTask);
    }

    void OnRecvComplete(int bytesTransferred) {
        if (bytesTransferred > 0) {
            // 송신자 IP/Port 확인 가능
            uint16_t port = ntohs(_clientAddr.sin_port);
            std::cout << "[UDP Recv] " << bytesTransferred << " bytes from port " << port << std::endl;

            // TODO: 버퍼(_recvBuffer) 파싱 -> TicketID 추출 -> 라우팅
        }

        RegisterRecv();
    }

    void OnWriteComplete(int result) {

    }

private:
    int _fd;
    IoUringWrapper* _uring;

    // io_uring 비동기 작업 동안 메모리가 유지되어야 하는 변수들 (Session이 소유!)
    struct sockaddr_in _clientAddr = {};
    struct iovec _iovec = {};
    struct msghdr _msgHdr = {};
    unsigned char _recvBuffer[2048] = {0};
};
