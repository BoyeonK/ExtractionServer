#pragma once

#include <string>
#include <netinet/in.h>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "Player.h"

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
    PlayerSession(const std::string& ticket, const std::string& token, int32_t sessionId, GameRoom* pRoom,
                  int32_t uid, const std::string& userId, int32_t rating,
                  const std::vector<Slot>& inventorySlots, const std::vector<Slot>& equipmentSlots,
                  int32_t characterType);
    ~PlayerSession() {
        for (auto& [seq, pending] : _pendingReliable)
            pending->ReleaseThis();
    }
    
    enum class SessionState {
        INIT,
        CONNECTED,
        INPLAY,
        DISCONNECTED,
    };

    const std::string& GetEntryToken() const;

    int32_t GetSessionId()          const { return _sessionId; }
    SessionState GetSessionState()  const { return _sessionState; }
    bool IsActiveState()            const {
        return _sessionState == SessionState::CONNECTED ||
               _sessionState == SessionState::INPLAY;
    }
    bool IsInplay()                 const { return _sessionState == SessionState::INPLAY; }
    GameRoom* GetGameRoom()         const { return _pRoom; }
    uint32_t GetSecurityKey()       const { return _securityKey; }

    int32_t            GetUid()            const { return _player.GetUid(); }
    const std::string& GetUserId()         const { return _player.GetUserId(); }
    int32_t            GetRating()         const { return _player.GetRating(); }
    const std::vector<Slot>& GetInventorySlots()    const { return _player.GetInventory().GetInventorySlots(); }
    const Slot&              GetPrimaryWeapon()     const { return _player.GetInventory().GetPrimaryWeapon(); }
    const Slot&              GetSecondaryWeapon()   const { return _player.GetInventory().GetSecondaryWeapon(); }
    const Slot&              GetArmorSlot()         const { return _player.GetInventory().GetArmorSlot(); }
    int32_t            GetCharacterType()  const { return _player.GetCharacterType(); }
    void SerializeFullInventory(External_Game_Protocol::D2CFullInventorySync* outMsg) const { _player.GetInventory().SerializeFullInventory(outMsg); }
    int32_t            GetObjectId()       const { return _player.GetObjectId(); }
    void               SetObjectId(int32_t id)   { _player.SetObjectId(id); }
    int32_t            GetInteractingContainerId() const { return _player.GetInteractingContainerId(); }
    void               SetInteractingContainerId(int32_t id) { _player.SetInteractingContainerId(id); }
    PlayerInventory&   GetInventoryMutable() { return _player.GetInventory(); }
    uint32_t           GetFireSequence()    const { return _player.GetFireSequence(); }
    void               IncrementFireSequence()    { _player.IncrementFireSequence(); }

    // ── 귀환(탈출) 진행 상태 ─────────────────────────────────────
    // 승인된 귀환은 RECALL_TICK_INTERVAL_MS 간격으로 위치를 재검사하고,
    // RECALL_REQUIRED_PASS_COUNT 회를 모두 통과하면 확정된다 (= 약 5초).
    //
    // TimerExecuter 는 등록된 타이머의 취소를 지원하지 않으므로, '취소'는
    // 세대(generation)를 올려 이미 예약된 콜백이 스스로 포기하게 만드는 방식이다.
    static constexpr uint32_t RECALL_REQUIRED_PASS_COUNT = 5;
    static constexpr uint32_t RECALL_TICK_INTERVAL_MS    = 1000;

    bool     IsRecalling()         const { return _isRecalling; }
    uint32_t GetRecallGeneration() const { return _recallGeneration; }
    uint32_t GetRecallPassCount()  const { return _recallPassCount; }
    uint32_t GetRecallSpotIndex()  const { return _recallSpotIndex; }

    // 새 귀환 시작. 이미 진행 중이면 아무것도 하지 않고 false.
    bool BeginRecall(uint32_t spotIndex) {
        if (_isRecalling) return false;
        ++_recallGeneration;
        _isRecalling     = true;
        _recallPassCount = 0;
        _recallSpotIndex = spotIndex;
        return true;
    }

    // 위치 검사 1회 통과 → 누적 통과 횟수 반환
    uint32_t AddRecallPass() { return ++_recallPassCount; }

    // 진행 중인 귀환 종료 (성공/취소 공통).
    // 세대를 올리므로 아직 타이머 큐에 남은 콜백은 실행되더라도 스스로 포기한다.
    void EndRecall() {
        _isRecalling = false;
        ++_recallGeneration;
    }

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
    void SetSessionState(PlayerSession::SessionState state);

private:
    Player      _player;
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

    // 귀환(탈출) 진행 상태
    bool     _isRecalling      = false;
    uint32_t _recallGeneration = 0;   // 요청 세대 — 예약된 타이머 콜백의 유효성 판별용
    uint32_t _recallPassCount  = 0;   // 통과한 위치 검사 횟수 (0 ~ RECALL_REQUIRED_PASS_COUNT)
    uint32_t _recallSpotIndex  = 0;   // 진행 중인 귀환의 스팟 인덱스

    // reliable 재전송 대기열
    std::unordered_map<uint32_t, PendingPacket*> _pendingReliable;
};
