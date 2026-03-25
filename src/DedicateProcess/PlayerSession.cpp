#include "PlayerSession.h"
#include <utility>
#include <random>

PlayerSession::PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom)
    : _ticket(ticket), _entryToken(token), _sessionId(sessionId), _pRoom(pRoom)
{
    _lastRecvTime = std::chrono::steady_clock::now();
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int32_t> dist(1, 2147483647); 

    _securityKey = dist(gen);
}

const std::string& PlayerSession::GetEntryToken() const {
    return _entryToken;
}

const int32_t& PlayerSession::GetSecurityKey() const {
    return _securityKey;
}