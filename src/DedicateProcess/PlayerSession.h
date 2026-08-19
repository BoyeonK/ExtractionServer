#pragma once

#include <string>
#include <netinet/in.h>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include "Player.h"
#include "../IPCProtocol/IPC_Dedicate.pb.h"

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
    uint32_t             sentAtMs;
    bool                 isPool = false;
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
        delete[] data;
    }

    void ReleaseThis() override {
        delete this;
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
        LEFT,
    };

    enum class LeaveReason {
        NONE,
        RECALLED,
        DEAD,
        DISCONNECTED,
    };

    enum class LeaveState {
        NONE,
        PENDING,
        DETACHED,
        FINALIZED,
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

    void SerializeInventoryForIPC(IPC_Protocol::D2MNotifyPlayerLeft* outMsg) const;
    int32_t            GetObjectId()       const { return _player.GetObjectId(); }
    void               SetObjectId(int32_t id)   { _player.SetObjectId(id); }
    int32_t            GetInteractingContainerId() const { return _player.GetInteractingContainerId(); }
    void               SetInteractingContainerId(int32_t id) { _player.SetInteractingContainerId(id); }
    PlayerInventory&   GetInventoryMutable() { return _player.GetInventory(); }
    uint32_t           GetFireSequence()    const { return _player.GetFireSequence(); }
    void               IncrementFireSequence()    { _player.IncrementFireSequence(); }

    // 5회 × 1000ms = 약 5초
    static constexpr uint32_t RECALL_REQUIRED_PASS_COUNT = 5;
    static constexpr uint32_t RECALL_TICK_INTERVAL_MS    = 1000;

    bool     IsRecalling()         const { return _isRecalling; }
    uint32_t GetRecallGeneration() const { return _recallGeneration; }
    uint32_t GetRecallPassCount()  const { return _recallPassCount; }
    uint32_t GetRecallSpotIndex()  const { return _recallSpotIndex; }

    bool BeginRecall(uint32_t spotIndex) {
        if (_isRecalling) return false;
        ++_recallGeneration;
        _isRecalling     = true;
        _recallPassCount = 0;
        _recallSpotIndex = spotIndex;
        return true;
    }

    uint32_t AddRecallPass() { return ++_recallPassCount; }

    void EndRecall() {
        _isRecalling = false;
        ++_recallGeneration;
    }

    // 클라이언트는 0.1초마다 상태를 보내므로 6초는 연속 60회 유실에 해당한다
    static constexpr uint32_t DISCONNECT_TIMEOUT_MS     = 6000;
    static constexpr uint32_t LEAVE_FINALIZE_TIMEOUT_MS = 3000;

    using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

    LeaveState  GetLeaveState()      const { return _leaveState; }
    LeaveReason GetLeaveReason()     const { return _leaveReason; }
    uint32_t    GetLeaveNotifyRSeq() const { return _leaveNotifyRSeq; }
    TimePoint   GetLeaveMarkedAt()   const { return _leaveMarkedAt; }
    bool        IsLeaving()          const { return _leaveState != LeaveState::NONE; }
    void        SetLeaveState(LeaveState state) { _leaveState = state; }
    void        SetLeaveNotifyRSeq(uint32_t rSeq) { _leaveNotifyRSeq = rSeq; }

    void MarkLeaving(LeaveReason reason, uint32_t notifyRSeq = 0);
    void FinalizeLeave();

    void Send(SendBuffer* buffer);
    uint32_t NextSendRSeq() { return ++_sendRSeq; }
    uint16_t NextSendUSeq() { return ++_sendUSeq; }
    uint32_t GetLastSentRSeq() const { return _sendRSeq; }

    bool UpdateRRecvState(uint32_t rSeqNum);
    bool UpdateURecvState(uint16_t uSeqNum);

    std::pair<uint32_t, uint32_t> GetAckState() const { return {_rRecvHighestSeq, _rRecvBitfield}; }

    void ProcessIncomingAck(uint32_t ackSeqNum, uint32_t ackBitfield);

    void RegisterReliable(uint32_t seqNum, const unsigned char* buf, uint32_t size, const sockaddr_in& dest, uint32_t nowMs);

    std::vector<PendingPacket*> GetRetransmitCandidates(uint32_t nowMs);

    // keepSeq 한 장만 남기고 전부 폐기한다 (keepSeq == 0 이면 전부).
    void ClearPendingReliableExcept(uint32_t keepSeq);
    bool IsReliablePending(uint32_t seqNum) const { return _pendingReliable.find(seqNum) != _pendingReliable.end(); }

    uint32_t GetRttMs() const { return _rttMs; }
    void     UpdateRtt(uint32_t echoTs, uint32_t nowMs);

    // 클라이언트가 마지막으로 되돌려준 서버 timestamp. 서버 시계 도메인이라 NowMs() 와 그대로 비교된다.
    // 0 = 아직 한 번도 받지 못함
    uint32_t GetLastEchoTs() const { return _lastEchoTs; }

    uint32_t GetLastRecvTimestamp() const { return _lastRecvTimestamp; }
    // 클라 재전송분은 최초 송신 시각을 그대로 달고 오므로 역행을 버린다.
    // 클라 시계가 한 세션 안에서 단조라는 전제 (재입장은 세션 객체가 새로 생성돼 0 부터 시작)
    void     SetLastRecvTimestamp(uint32_t ts) {
        if (_lastRecvTimestamp != 0 && static_cast<int32_t>(ts - _lastRecvTimestamp) <= 0) return;
        _lastRecvTimestamp = ts;
    }

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

    uint32_t _sendRSeq = 0;
    uint16_t _sendUSeq = 0;

    uint32_t _rRecvHighestSeq = 0;
    uint32_t _rRecvBitfield   = 0;   // bit[0]=rRecvHighest-1, bit[31]=rRecvHighest-32
    bool     _hasRRecv        = false;

    uint16_t _uRecvHighestSeq = 0;
    bool     _hasURecv        = false;

    uint32_t _rttMs             = 100;
    uint32_t _lastEchoTs        = 0;
    uint32_t _lastRecvTimestamp = 0;

    LeaveState  _leaveState      = LeaveState::NONE;
    LeaveReason _leaveReason     = LeaveReason::NONE;
    TimePoint   _leaveMarkedAt   = {};
    uint32_t    _leaveNotifyRSeq = 0;    // 0 = 없음

    bool     _isRecalling      = false;
    uint32_t _recallGeneration = 0;
    uint32_t _recallPassCount  = 0;
    uint32_t _recallSpotIndex  = 0;

    std::unordered_map<uint32_t, PendingPacket*> _pendingReliable;
};
