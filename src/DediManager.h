#pragma once

#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstdlib> 
#include <chrono>
#include <unordered_map>

#include "M2DSessions.h"
#include "Matchmaker.h"
#include "IPCProtocol/IPC_HTTP.pb.h"

class DediManager {
enum MapType : int32_t {
    MAP_NONE = -1,
    MAP_TUTORIAL = 0,
    MAP_TENERIFE = 1,
    //MAP_DESERT,
    //MAP_FOREST,
    MAP_MAX
};
static constexpr int MAX_AGRESSION = 20;

public:
    DediManager();
    ~DediManager();

    bool AddSingleMatchTicket(MatchTicket* pTicket);
    int SpawnSingleServer();
    void OnAcceptDedi(int DediIPCsockFd, M2DTempSession* pTempSession);
    bool FinalizeConnection(int pid, int fd);
    bool DistributePlayerGroup(TicketVector& ticketVec);
    void MatchMake();
    void BindClientIpToSession(IPC_Protocol::H2M2DBindClientIpToSession& pkt);

private:
    bool FindAvailableSessionAndDistributePlayerGroup(TicketVector& ticketVec);
    bool DistributePlayerGroup(TicketVector& ticketVec, int pid);

private:
    //key = pid, 
    std::unordered_map<int, M2DSession*> _dediSessions;
    std::unordered_map<int, int> _sessionFd2pid;

    //key = fd, 여기서 pid를 받은 임시 세션을 아래의 pid key의 세션과 합체
    std::unordered_map<int, M2DTempSession*> _tempSessions;

    //index = mapid
    std::vector<MatchMaker> _matchmakers;

    std::chrono::time_point<std::chrono::steady_clock> _lastTimeMatchStarted;
};