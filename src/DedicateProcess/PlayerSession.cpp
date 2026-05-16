#include "PlayerSession.h"

#include <utility>
#include <random>
#include <iostream>
#include <arpa/inet.h>
#include <algorithm>
#include <cstring>
#include "../ObjectPool.h"
#include "DediServerService.h"
#include "DedicateGlobalVariable.h"
#include "DediSessions.h"
#include "../SendBuffer.h"

void PendingPacket256::ReleaseThis() {
    ObjectPool<PendingPacket256>::Release(this);
}

void PendingPacket512::ReleaseThis() {
    ObjectPool<PendingPacket512>::Release(this);
}

void PendingPacket1024::ReleaseThis() {
    ObjectPool<PendingPacket1024>::Release(this);
}

PlayerSession::PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom,
                             int32_t uid, const std::string& userId, int32_t rating,
                             const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots,
                             int32_t characterType)
    : _player(uid, userId, rating, inventorySlots, equipmentSlots, characterType),
      _ticket(ticket), _entryToken(token), _sessionId(sessionId), _pRoom(pRoom)
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

void PlayerSession::Send(SendBuffer* buffer) {
    pDediServer->Send(buffer, GetAddress());
}

bool PlayerSession::UpdateRRecvState(uint32_t rSeqNum) {
    if (!_hasRRecv) {
        _rRecvHighestSeq = rSeqNum;
        _rRecvBitfield   = 0;
        _hasRRecv        = true;
        _lastRecvTime    = std::chrono::steady_clock::now();
        return true;
    }

    if (rSeqNum > _rRecvHighestSeq) {
        uint32_t diff = rSeqNum - _rRecvHighestSeq;
        if (diff >= 32) {
            // 윈도우를 완전히 벗어남 → 기존 비트필드 소멸
            _rRecvBitfield = 0;
        } else {
            // 기존 비트필드를 diff만큼 밀고, 이전 highest 위치를 수신 완료로 표시
            _rRecvBitfield = (_rRecvBitfield << diff) | (1u << (diff - 1));
        }
        _rRecvHighestSeq = rSeqNum;
        _lastRecvTime    = std::chrono::steady_clock::now();
        return true;
    }

    if (rSeqNum == _rRecvHighestSeq) {
        return false; // 중복
    }

    // rSeqNum < _rRecvHighestSeq
    uint32_t diff = _rRecvHighestSeq - rSeqNum;
    if (diff > 32) {
        return false; // 윈도우 밖 → 버림
    }

    uint32_t bit = 1u << (diff - 1);
    if (_rRecvBitfield & bit) {
        return false; // 이미 수신
    }

    _rRecvBitfield |= bit;
    return true;
}

bool PlayerSession::UpdateURecvState(uint16_t uSeqNum) {
    if (!_hasURecv) {
        _uRecvHighestSeq = uSeqNum;
        _hasURecv        = true;
        _lastRecvTime    = std::chrono::steady_clock::now();
        return true;
    }

    // wrap-around 안전 비교: signed 차가 양수면 새 패킷
    if (static_cast<int16_t>(uSeqNum - _uRecvHighestSeq) <= 0)
        return false; // 오래됐거나 중복 → 버림

    _uRecvHighestSeq = uSeqNum;
    _lastRecvTime    = std::chrono::steady_clock::now();
    return true;
}

void PlayerSession::ProcessIncomingAck(uint32_t ackSeqNum, uint32_t ackBitfield) {
    auto releaseBySeq = [&](uint32_t seq) {
        auto it = _pendingReliable.find(seq);
        if (it != _pendingReliable.end()) {
            it->second->ReleaseThis();
            _pendingReliable.erase(it);
        }
    };

    releaseBySeq(ackSeqNum);

    if (_pendingReliable.empty() || ackBitfield == 0) return;

    for (int i = 0; i < 32 && ackBitfield != 0; i++) {
        if (ackBitfield & 1u) {
            releaseBySeq(ackSeqNum - static_cast<uint32_t>(i + 1));
            if (_pendingReliable.empty()) break;
        }
        ackBitfield >>= 1;
    }
}

void PlayerSession::RegisterReliable(uint32_t seqNum, const unsigned char* buf, uint32_t size, const sockaddr_in& dest, uint32_t nowMs) {
    PendingPacket* pPending = nullptr;
    if (size <= 256) {
        pPending = ObjectPool<PendingPacket256>::Acquire(size);
    } else if (size <= 512) {
        pPending = ObjectPool<PendingPacket512>::Acquire(size);
    } else if (size <= 1024) {
        pPending = ObjectPool<PendingPacket1024>::Acquire(size);
    } else {
        pPending = new PendingPacketUnlimited(size);
    }

    pPending->seqNum     = seqNum;
    memcpy(pPending->GetData(), buf, size);
    pPending->destAddr   = dest;
    pPending->sentAtMs   = nowMs;
    pPending->retryCount = 0;
    _pendingReliable.emplace(seqNum, pPending);
}

std::vector<PendingPacket*> PlayerSession::GetRetransmitCandidates(uint32_t nowMs) {
    uint32_t timeout = std::min(std::max(_rttMs * 3u / 2u, 50u), 1000u);
    std::vector<PendingPacket*> result;
    for (auto& [seq, pending] : _pendingReliable) {
        // wrap-around 안전 비교
        uint32_t elapsed = nowMs - pending->sentAtMs;
        if (elapsed >= timeout) {
            result.push_back(pending);
        }
    }
    return result;
}

void PlayerSession::UpdateRtt(uint32_t echoTs, uint32_t nowMs) {
    if (echoTs == 0 || echoTs == _lastEchoTs) return;
    _lastEchoTs = echoTs;

    uint32_t rtt = nowMs - echoTs;
    // EWMA (alpha ≈ 0.125)
    _rttMs = (_rttMs * 7u + rtt) / 8u;
    if (_rttMs < 20u) _rttMs = 20u;
}

void PlayerSession::SetIp(const std::string& ip) {
    _clientAddr.sin_family = AF_INET;

    int result = inet_pton(AF_INET, ip.c_str(), &_clientAddr.sin_addr);

    if (result == 0) {
        std::cerr << "PlayerSession::SetIp : 유효하지 않은 IP 형식입니다. 바인딩 거부: " << ip << '\n';
    } else if (result < 0) {
        std::cerr << "PlayerSession::SetIp : IP 변환 중 예외 발생.\n";
    }
}

void PlayerSession::SetPort(uint16_t port) {
    _clientAddr.sin_port = htons(port);
}

void PlayerSession::SetSessionState(PlayerSession::SessionState state) {
    _sessionState = state;
}