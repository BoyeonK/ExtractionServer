#pragma once

#include <string>
#include <netinet/in.h>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <cstdint>

class GameRoom;

class SendBuffer;

class PendingPacket {
public:
    PendingPacket() { };
    PendingPacket(int32_t size) : allocSize(size) {};
    virtual ~PendingPacket() { };

    virtual void ReleaseThis() = 0;
    virtual unsigned char* GetData() = 0;

    uint32_t             seqNum;
    uint32_t             allocSize;
    sockaddr_in          destAddr;
    uint32_t             sentAtMs;   // 송신 시각 (ms)
    bool                 isPool = false;
    int                  retryCount = 0;
};

class PendingPacket256 : public PendingPacket {
public:
    PendingPacket256(uint32_t size) : PendingPacket(size) { 
        isPool = true;
    }

    void ReleaseThis() override;
    unsigned char* GetData() override { return data; }

    unsigned char data[256];
};

class PendingPacket512 : public PendingPacket {
public:
    PendingPacket512(uint32_t size) : PendingPacket(size) { 
        isPool = true;
    }

    void ReleaseThis() override;
    unsigned char* GetData() override { return data; }

    unsigned char data[512];
};

class PendingPacket1024 : public PendingPacket {
public:
    PendingPacket1024(uint32_t size) : PendingPacket(size) { 
        isPool = true;
    }

    void ReleaseThis() override;
    unsigned char* GetData() override { return data; }

    unsigned char data[1024];
};

class PendingPacketUnlimited : public PendingPacket {
public:
    PendingPacketUnlimited(uint32_t size) : PendingPacket(size) { 
        data = new unsigned char[size];
    }

    ~PendingPacketUnlimited() override {
        delete[] data; // 메모리 누수 방지
    }

    void ReleaseThis() override {
        delete this;  // 소멸자에서 delete[] data 처리됨
    };

    unsigned char* GetData() override { return data; }
    unsigned char* data; 
};

class PlayerSession {
public:
    PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom);
    ~PlayerSession() {
        for (auto& [seq, pending] : _pendingReliable)
            pending->ReleaseThis();
    }
    
    enum class SessionState {
        INIT,
        CONNECTED,
    };

    const std::string& GetEntryToken() const;

    int32_t GetSessionId()          const { return _sessionId; }
    SessionState GetSessionState()  const { return _sessionState; }
    GameRoom* GetGameRoom()         const { return _pRoom; }
    uint32_t GetSecurityKey()       const { return _securityKey; }

    // ── 송신 시퀀스 ──────────────────────────────────────────────
    void Send(SendBuffer* buffer);
    uint32_t NextSendRSeq() { return ++_sendRSeq; }   // reliable 채널
    uint16_t NextSendUSeq() { return ++_sendUSeq; }   // unreliable 채널

    // ── 수신 상태 업데이트 + 중복 감지 ───────────────────────────
    // return true  : 새 패킷 → 처리 가능
    // return false : 중복 또는 윈도우 밖 오래된 패킷 → 버림
    bool UpdateRRecvState(uint32_t rSeqNum);   // reliable 채널 (비트필드 추적)
    bool UpdateURecvState(uint16_t uSeqNum);   // unreliable 채널 (signed 차 비교)

    // ── 현재 ACK 상태 (헤더에 피기백용) ──────────────────────────
    std::pair<uint32_t, uint32_t> GetAckState() const { return {_rRecvHighestSeq, _rRecvBitfield}; }

    // ── 상대가 보내온 ACK로 재전송 큐 정리 ───────────────────────
    void ProcessIncomingAck(uint32_t ackSeqNum, uint32_t ackBitfield);

    // ── reliable 패킷 재전송 큐 등록 ─────────────────────────────
    void RegisterReliable(uint32_t seqNum, const unsigned char* buf, uint32_t size, const sockaddr_in& dest, uint32_t nowMs);

    // timeout된 패킷 포인터 목록 반환 (sentAtMs 갱신 및 retryCount 증가는 호출자 몫)
    std::vector<PendingPacket*> GetRetransmitCandidates(uint32_t nowMs);

    // ── RTT ──────────────────────────────────────────────────────
    uint32_t GetRttMs() const { return _rttMs; }
    void     UpdateRtt(uint32_t echoTs, uint32_t nowMs);

    // ── timestamp echo (RTT 계산을 위해 수신한 timestamp 보관) ───
    uint32_t GetLastRecvTimestamp() const { return _lastRecvTimestamp; }
    void     SetLastRecvTimestamp(uint32_t ts) { _lastRecvTimestamp = ts; }

    // ── 주소 ─────────────────────────────────────────────────────
    sockaddr_in GetAddress() const { return _clientAddr; }
    void SetIp(const std::string& ip);
    void SetPort(uint16_t port);

private:
    int32_t     _uid = 0;
    std::string _ticket;
    std::string _entryToken;
    int32_t     _sessionId;
    uint32_t    _securityKey;
    GameRoom*   _pRoom;
    SessionState _sessionState = SessionState::INIT;

    sockaddr_in _clientAddr = {};
    std::chrono::time_point<std::chrono::steady_clock> _lastRecvTime;

    // 송신 시퀀스 (채널 분리)
    uint32_t _sendRSeq = 0;   // reliable 채널
    uint16_t _sendUSeq = 0;   // unreliable 채널

    // 수신 ACK 상태 (reliable 채널 전용)
    uint32_t _rRecvHighestSeq = 0;
    uint32_t _rRecvBitfield   = 0;   // bit[0]=rRecvHighest-1, bit[31]=rRecvHighest-32
    bool     _hasRRecv        = false;

    // 수신 상태 (unreliable 채널 - dedup용)
    uint16_t _uRecvHighestSeq = 0;
    bool     _hasURecv        = false;

    // RTT
    uint32_t _rttMs             = 100; // 초기값 100ms
    uint32_t _lastEchoTs        = 0;
    uint32_t _lastRecvTimestamp = 0;

    // reliable 재전송 대기열
    std::unordered_map<uint32_t, PendingPacket*> _pendingReliable;
};
