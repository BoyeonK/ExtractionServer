#pragma once

#include <vector>
#include <unordered_map>
#include <iterator>
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
        pkt.mutable_player_infos()->Reserve(group.size());

        for (auto& ticket : group) {
            if (ticket == nullptr) continue;

            std::unordered_map<std::string, std::string> val;
            pRedis->hgetall(ticket->ticketId, std::inserter(val, val.begin()));

            if (val.empty()) continue;

            auto* info = pkt.add_player_infos();
            info->set_ticket_id(ticket->ticketId);
            info->set_uid(std::stoi(val.at("uid")));
            info->set_user_id(val.at("user_id"));
            info->set_rating(std::stoi(val.at("rating")));

            // TODO : json형식의 string이 아니라 다른 형태로 패키징해서 전송하기
            info->set_inventory_items(val.at("inventory_items"));
            info->set_equipment_items(val.at("equipment_items"));

            info->set_character_type(ticket->characterType);
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

    void PumpRecvTasks(int count = 10);
    void Send(SendBuffer* buffer, const sockaddr_in& destAddr);
    void OnRecvComplete(int bytesTransferred, unsigned char* buffer, const sockaddr_in& clientAddr);
    void OnWriteComplete(int result);

private:
    int _fd;
    IoUringWrapper* _uring;
};
