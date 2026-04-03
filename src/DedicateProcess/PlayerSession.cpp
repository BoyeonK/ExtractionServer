#include "PlayerSession.h"

#include <utility>
#include <random>
#include <iostream>
#include <arpa/inet.h>

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

bool PlayerSession::IsNewSequenceNum(uint16_t packetId, uint32_t seqNum) {
    if (seqNum > _sequenceNums[packetId]) {
        _sequenceNums[packetId] = seqNum;
        _lastRecvTime = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

void PlayerSession::SetPort(uint16_t port) {
    _clientAddr.sin_port = htons(port);
}

void PlayerSession::SetIp(const std::string& ip) {
    _clientAddr.sin_family = AF_INET;
    
    int result = inet_pton(AF_INET, ip.c_str(), &_clientAddr.sin_addr);
    
    if (result == 1) {

    } else if (result == 0) {
        std::cerr << "PlayerSession::SetIp : 유효하지 않은 IP 형식입니다. 바인딩 거부: " << ip << '\n';
    } else {
        std::cerr << "PlayerSession::SetIp : IP 변환 중 예외 발생.\n";
    }
}
