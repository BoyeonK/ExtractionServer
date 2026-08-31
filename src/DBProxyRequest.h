#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#include <queue>

// match.js 의 ACTIVE_MATCH_TTL_SEC 와 같아야 한다
inline constexpr int ACTIVE_MATCH_TTL_SEC = 900;

class PendingDBRequest {
public:
    PendingDBRequest(int fd) : _gameProcessFd(fd) {}
    virtual ~PendingDBRequest() = default;

    virtual void Execute() = 0;
    virtual void ReturnToPool() = 0;

protected:
    int _gameProcessFd;
};

class DBProxyService {
public:
    void RegisterDBRequest(PendingDBRequest* pRequest);
    bool ExecuteAll();

private:
    std::queue<PendingDBRequest*> _requestQueue;
};

class UpdateEntryTokenRequest : public PendingDBRequest {
public:
    UpdateEntryTokenRequest(int fd, std::vector<std::string> ticketIds, std::vector<int32_t> sessionIds, std::vector<std::string> tokens, std::vector<int32_t> securityKeys, int32_t port)
    : PendingDBRequest(fd), _ticketIds(std::move(ticketIds)), _sessionIds(std::move(sessionIds)), _tokens(std::move(tokens)), _securityKeys(std::move(securityKeys)), _port(port)
    {}
    
    void Execute() override;

    void ReturnToPool() override;

private:

private:
    std::vector<std::string> _ticketIds;
    std::vector<int32_t> _sessionIds;
    std::vector<std::string> _tokens;
    std::vector<int32_t> _securityKeys;
    int32_t _port;
};

class NotifyPlayerLeftRequest : public PendingDBRequest {
public:
    // user_inventory.slot_index 레이아웃 — 창고 0~79 / 인벤토리 80~104 / 장착 105~107
    static constexpr int32_t INVENTORY_SLOT_INDEX_BEGIN = 80;
    static constexpr int32_t LOADOUT_SLOT_INDEX_BEGIN   = 105;
    static constexpr int     DB_RETRY_COUNT             = 2;

    // slotIndex 는 user_inventory.slot_index 기준의 최종 값 (변환은 생성 측에서 끝낸다)
    struct SlotRow {
        uint32_t itemId;
        int32_t  slotIndex;
        int32_t  quantity;
    };

    NotifyPlayerLeftRequest(int fd, int32_t uid, int32_t leaveReason, std::vector<SlotRow> slots)
    : PendingDBRequest(fd), _uid(uid), _leaveReason(leaveReason), _slots(std::move(slots))
    {}

    void Execute() override;
    void ReturnToPool() override;

private:
    bool ApplyInventoryToDb();

    int32_t _uid;
    int32_t _leaveReason;
    std::vector<SlotRow> _slots;
};