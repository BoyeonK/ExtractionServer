#pragma once

#include <vector>
#include <netinet/in.h>
#include <sys/socket.h>
#include "../GlobalVariable.h"
#include "../SocketWrapper.h"
#include "../IPCProtocol/IPC_Dedicate.pb.h"
#include "../IoUringWrapper.h"
#include "Matchmaker.h"

class SendBuffer;

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

    bool AllocatePlayers(TicketVector& ticketVec);
    void FlushPendingTickets();

private:
    static IPC_Protocol::M2DMakeRoomForThisGroup MakeM2DMakeRoomForThisGroup(int mapId, TicketVector& group) {
		IPC_Protocol::M2DMakeRoomForThisGroup pkt;

        pkt.set_map_id(mapId);
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
    D2CSession(int fd, IoUringWrapper* ring);

    void RegisterRecv();
    void OnRecvComplete(int bytesTransferred);
    void OnWriteComplete(int result);

private:
    int _fd;
    IoUringWrapper* _uring;

    // io_uring 비동기 작업 동안 메모리가 유지되어야 하는 변수들 (Session이 소유!)
    struct sockaddr_in _clientAddr = {};
    struct iovec _iovec = {};
    struct msghdr _msgHdr = {};
    unsigned char _recvBuffer[2048] = {0};
};
