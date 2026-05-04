#include "UnityGameObject.h"

void UnityGameObject::Serialize(External_Game_Protocol::UnityGameObject* pPkt) const {
    if (pPkt == nullptr) return;

    pPkt->set_object_id(objectId);
    pPkt->set_object_type(static_cast<int32_t>(objectType));

    External_Game_Protocol::TransformInfo* pTrans = pPkt->mutable_transform();

    position.Serialize(pTrans->mutable_position());

    if (IsYFixed) {
        pTrans->set_yaw_angle(yawAngle);
    } else {
        quaternion.Serialize(pTrans);
    }
}

External_Game_Protocol::UnityGameObject UnityGameObject::Serialize() const {
    External_Game_Protocol::UnityGameObject pkt;
    Serialize(&pkt);
    return pkt;
}

