#pragma once

#include "CombatObject.h"
#include <algorithm>
#include <cstdint>

class HostileNPC : public CombatObject {
public:
    static constexpr int32_t NO_TARGET         = -1;
    static constexpr int32_t DEFAULT_MIN_AGGRO = 1;
    static constexpr int32_t DEFAULT_MAX_AGGRO = 8;

    // 와이어의 '주도권자 없음'. proto3 기본값 0 은 실재하는 object_id 라 쓸 수 없다
    static constexpr uint32_t NO_AUTHORITY_ID = 0xFFFFFFFF;

    int32_t GetTargetId()      const { return _targetId; }
    int32_t GetAggro()         const { return _aggro; }
    int32_t GetMinAggro()      const { return _minAggro; }
    int32_t GetMaxAggro()      const { return _maxAggro; }
    int32_t GetAttackDamage()  const { return _attackDamage; }

    bool IsServerDriven()      const { return _targetId == NO_TARGET; }
    bool IsAuthority(int32_t playerObjectId) const {
        return playerObjectId != NO_TARGET && playerObjectId == _targetId;
    }

    enum class RequestResult : uint8_t {
        REJECTED,           // 관문에 걸림
        AGGRO_CHANGED,      // 수락. 주도권은 그대로라 통보하지 않는다
        AUTHORITY_CHANGED,  // 주도권이 옮겨감 — 통보한다
    };

    // 주도권자의 aggro 변경. MIN 지정이 곧 반납이다
    RequestResult ApplyAggroRequest(int32_t requesterObjectId, int32_t aggro) {
        if (!IsAuthority(requesterObjectId)) return RequestResult::REJECTED;

        return ApplyAccepted(aggro, requesterObjectId);
    }

    // 비주도권자의 주도권 이전 요청. 클램프 후 현재 aggro 보다 커야 승인된다
    RequestResult ApplyAuthorityRequest(int32_t requesterObjectId, int32_t aggro) {
        if (requesterObjectId == NO_TARGET) return RequestResult::REJECTED;
        if (std::clamp(aggro, _minAggro, _maxAggro) <= _aggro) return RequestResult::REJECTED;

        return ApplyAccepted(aggro, requesterObjectId);
    }

    // 피격은 MAX 를 주장하는 주도권 이전 요청과 같은 연산이다 — 관문을 그대로 지난다
    RequestResult ApplyDamageAuthority(int32_t attackerObjectId) {
        return ApplyAuthorityRequest(attackerObjectId, _maxAggro);
    }

    bool ReleaseAuthority() {
        if (IsServerDriven()) return false;

        SetAggroInternal(_minAggro, NO_TARGET);
        return true;
    }

    void FillMovementInfo(External_Game_Protocol::GameObjectMovementInfo* pInfo) const {
        pInfo->set_object_id(objectId);

        External_Game_Protocol::TransformInfo* pTransform = pInfo->mutable_transform();
        position.Serialize(pTransform->mutable_position());

        if (IsYFixed) pTransform->set_yaw_angle(yawAngle);
        else          quaternion.Serialize(pTransform);

        pInfo->set_state(state);
    }

    void ApplyMovementInfo(const External_Game_Protocol::GameObjectMovementInfo& info) {
        const External_Game_Protocol::TransformInfo& transform = info.transform();

        if (transform.has_position()) {
            const auto& pos = transform.position();
            position = { pos.x(), pos.y(), pos.z() };
        }

        if (transform.has_compressed_quat())
            quaternion.DeserializeFrom(transform.compressed_quat());
        else if (transform.has_yaw_angle())
            yawAngle = transform.yaw_angle();

        state = static_cast<uint16_t>(info.state());
    }

protected:
    HostileNPC(uint32_t objectId, ObjectType objectType, bool isYFixed, Vector3 position,
               int32_t maxHp, int32_t attackDamage,
               int32_t minAggro = DEFAULT_MIN_AGGRO,
               int32_t maxAggro = DEFAULT_MAX_AGGRO)
        : CombatObject(objectId, objectType, isYFixed, position, maxHp)
        , _minAggro(minAggro), _maxAggro(maxAggro)
        , _attackDamage(attackDamage), _aggro(minAggro) {}

private:
    RequestResult ApplyAccepted(int32_t aggro, int32_t requesterObjectId) {
        const int32_t before = _targetId;
        SetAggroInternal(aggro, requesterObjectId);

        return (_targetId != before) ? RequestResult::AUTHORITY_CHANGED
                                     : RequestResult::AGGRO_CHANGED;
    }

    // aggro == MIN 과 targetId == NO_TARGET 의 등가를 세우는 유일한 지점.
    // 이 등가가 깨지면 aggro 가 MAX 인 채 주인 없는 오브젝트가 되어 누구도 가져갈 수 없다
    void SetAggroInternal(int32_t aggro, int32_t targetId) {
        aggro = std::clamp(aggro, _minAggro, _maxAggro);

        if (aggro == _minAggro || targetId == NO_TARGET) {
            _aggro    = _minAggro;
            _targetId = NO_TARGET;
            return;
        }

        _aggro    = aggro;
        _targetId = targetId;
    }

    const int32_t _minAggro;
    const int32_t _maxAggro;
    const int32_t _attackDamage;

    int32_t _aggro;
    int32_t _targetId = NO_TARGET;   // 플레이어 objectId. 세션 id 는 FIFO 재사용이라 쓸 수 없다
};
