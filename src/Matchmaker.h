#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

struct MatchTicket {
    MatchTicket(std::string tid, int32_t uid, int32_t aggr, int32_t mid, int32_t cType)
    : ticketId(tid), uId(uid), aggression(aggr), mapId(mid), characterType(cType), startTime(std::chrono::steady_clock::now()) ,isMatched(false)
    { }

    std::string ticketId;
    int32_t uId;
    int32_t aggression;
    int32_t mapId;
    int32_t characterType;
    std::chrono::time_point<std::chrono::steady_clock> startTime;
    bool isMatched = false;
    bool isValid = true;
    int32_t GetWaitTimeSeconds() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
    }
};

using TicketVector = std::vector<MatchTicket*>;

//map별로 MatchMaker를 만들 예정
class MatchMaker {
public:
    MatchMaker(int32_t mapId, int32_t maxAgression) 
        :_mapId(mapId), 
        _maxAgression(maxAgression),
        _bucket(maxAgression + 1),  
        _pivotMemorizationFlags(maxAgression + 1, false)
    { }

    void AddSingleMatchTicket(MatchTicket* pTicket);
    void AddNewMatchTickets();
    void AddRematchTickets();
    void ReleaseAndDeleteTickets();

    void FindMatchGroup();
    bool VerifyAndSetMatchStatus(const TicketVector& matchedGroup);
    void StartMatchMakeInternal();
 
    void StartMatchMake();

private:
    int32_t _mapId;
    int32_t _maxAgression;
    TicketVector _ticketsToAdd;
    TicketVector _ticketsToRelease;
    TicketVector _ticketsToDelete;
    
    TicketVector _timeSortedTicketVector;

    // index = agression 별 분류
    std::vector<TicketVector> _bucket;
    std::vector<TicketVector> _matchedGroups;
    std::vector<bool> _pivotMemorizationFlags;
};