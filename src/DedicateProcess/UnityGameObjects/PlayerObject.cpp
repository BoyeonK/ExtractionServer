#include "PlayerObject.h"

void PlayerObject::ApplyState(const External_Game_Protocol::PlayerState& state) {
    if (!state.has_movement_info()) return;

    const auto& movementInfo = state.movement_info();
    const auto& transform    = movementInfo.transform();

    if (transform.has_position()) {
        const auto& pos = transform.position();
        position = { pos.x(), pos.y(), pos.z() };
    }

    if (transform.has_compressed_quat())
        quaternion.DeserializeFrom(transform.compressed_quat());
    else if (transform.has_yaw_angle())
        yawAngle = transform.yaw_angle();

    this->state = static_cast<uint16_t>(movementInfo.state());
    pitch       = state.pitch();

    if (state.has_velocity()) {
        const auto& vel = state.velocity();
        velocity = { vel.x(), vel.y(), vel.z() };
    } else {
        velocity = { 0.0f, 0.0f, 0.0f }; 
    }
}

void PlayerObject::FillState(External_Game_Protocol::PlayerState* pState) const {
    auto* pMovementInfo = pState->mutable_movement_info();
    pMovementInfo->set_object_id(objectId);

    auto* pTransform = pMovementInfo->mutable_transform();
    position.Serialize(pTransform->mutable_position());

    if (IsYFixed)
        pTransform->set_yaw_angle(yawAngle);
    else
        quaternion.Serialize(pTransform);

    pMovementInfo->set_state(state);

    pState->set_pitch(pitch);
    velocity.Serialize(pState->mutable_velocity());
}

void PlayerObject::SetWeapons(uint32_t primaryId, uint32_t secondaryId) {
    _primaryWeaponId   = primaryId;
    _secondaryWeaponId = secondaryId;
    _isUsingPrimary    = true;
}

uint32_t PlayerObject::GetCurrentWeaponId() const {
    return _isUsingPrimary ? _primaryWeaponId : _secondaryWeaponId;
}

void PlayerObject::SwitchWeapon() {
    _isUsingPrimary = !_isUsingPrimary;
}

void PlayerObject::SetArmor(uint32_t armorId) {
    _armorId = armorId;
}