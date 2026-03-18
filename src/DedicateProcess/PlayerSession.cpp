#include "PlayerSession.h"
#include <utility>

PlayerSession::PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom)
    : _ticket(ticket), _entryToken(token), _sessionId(sessionId), _pRoom(pRoom)
{
    _lastRecvTime = std::chrono::steady_clock::now();
}

const std::string& PlayerSession::GetEntryToken() const {
    return _entryToken;
}