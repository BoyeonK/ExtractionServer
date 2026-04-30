#include "UnityGameObject.h"

void UnityGameObject::Serialize(External_Game_Protocol::UnityGameObject* pPkt) const {
    if (pPkt == nullptr) return;

    pPkt->set_object_id(objectId);
    pPkt->set_object_type(objectType);

    External_Game_Protocol::Vector3* pPos = pPkt->mutable_position();
    pPos->set_x(position.x);
    pPos->set_y(position.y);
    pPos->set_z(position.z);
}

External_Game_Protocol::UnityGameObject UnityGameObject::Serialize() const {
    External_Game_Protocol::UnityGameObject pkt;
    Serialize(&pkt);
    return pkt;
}