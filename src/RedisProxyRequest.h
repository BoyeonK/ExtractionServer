#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <vector>
#include <utility>
#include <cstdint>

// active_match 락의 백스톱 TTL. 정상 해제는 이탈 확정(NotifyPlayerLeftRequest)이고,
// 이 값은 그 통보가 유실됐을 때 락이 영구 잔류하는 것을 막는 용도다.
// match.js 의 /start 쪽 EX 값과 같아야 하며, 최대 게임 길이보다 길어야 한다.
inline constexpr int ACTIVE_MATCH_TTL_SEC = 3600;

class PendingRedisRequest {
public:
    PendingRedisRequest(int fd) : _gameProcessFd(fd) {}
    virtual ~PendingRedisRequest() = default;

    virtual void Execute(sw::redis::Redis* pRedis) = 0;
    virtual void ReturnToPool() = 0;

protected:
    int _gameProcessFd;
};

class UpdateEntryTokenRequest : public PendingRedisRequest {
public:
    UpdateEntryTokenRequest(int fd, std::vector<std::string> ticketIds, std::vector<int32_t> sessionIds, std::vector<std::string> tokens, std::vector<int32_t> securityKeys, int32_t port)
    : PendingRedisRequest(fd), _ticketIds(std::move(ticketIds)), _sessionIds(std::move(sessionIds)), _tokens(std::move(tokens)), _securityKeys(std::move(securityKeys)), _port(port)
    {}
    
    void Execute(sw::redis::Redis* pRedis) override;

    void ReturnToPool() override;

private:

private:
    std::vector<std::string> _ticketIds;
    std::vector<int32_t> _sessionIds;
    std::vector<std::string> _tokens;
    std::vector<int32_t> _securityKeys;
    int32_t _port;
};

// 이탈 확정 반영 — 인벤토리 MySQL 반영 + active_match 락 해제.
//
// 이 큐를 타는 이유는 두 가지다. ① 이탈은 몰려서 발생한다 (룸 하나가 끝나면 4명이 같은 창에서
// 확정되고, 여러 룸이 겹치면 더) ② MySQL → Redis 순서 보장과 재시도를 한 곳에 갇혀 두려면
// 요청 객체가 필요하다. 큐 이름은 Redis 전용처럼 보이지만 실제 역할은
// "블로킹 외부 저장소 작업 큐"이므로 MySQL 작업도 여기 얹는다.
class NotifyPlayerLeftRequest : public PendingRedisRequest {
public:
    // user_inventory.slot_index 레이아웃 — 창고 0~79 / 인벤토리 80~104 / 장착 105~107.
    // 레이드에 들어가는 범위는 인벤토리부터이므로, 반영 시 지우는 하한도 이 값이다.
    static constexpr int32_t INVENTORY_SLOT_INDEX_BEGIN = 80;
    static constexpr int32_t LOADOUT_SLOT_INDEX_BEGIN   = 105;
    // 첫 시도가 죽은 연결로 실패하면 MysqlHandle::Get() 이 재연결하므로 2회로 충분하다
    static constexpr int     DB_RETRY_COUNT             = 2;

    // slotIndex 는 MySQL user_inventory.slot_index 기준의 최종 값이다 (변환은 생성 측에서 끝낸다)
    struct SlotRow {
        uint32_t itemId;
        int32_t  slotIndex;
        int32_t  quantity;
    };

    NotifyPlayerLeftRequest(int fd, int32_t uid, int32_t leaveReason, std::vector<SlotRow> slots)
    : PendingRedisRequest(fd), _uid(uid), _leaveReason(leaveReason), _slots(std::move(slots))
    {}

    void Execute(sw::redis::Redis* pRedis) override;
    void ReturnToPool() override;

private:
    // return true: 반영 성공(또는 반영 대상 아님). false: 재시도해도 실패.
    bool ApplyInventoryToDb();

    int32_t _uid;
    int32_t _leaveReason;
    std::vector<SlotRow> _slots;
};