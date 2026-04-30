#include "UnityGameObject.h"

void UnityGameObject::Serialize(External_Game_Protocol::UnityGameObject* pPkt) const {
    if (pPkt == nullptr) return;

    pPkt->set_object_id(objectId);
    pPkt->set_object_type(objectType);
    position.Serialize(pPkt->mutable_position());
    front.Serialize(pPkt->mutable_front());
}

External_Game_Protocol::UnityGameObject UnityGameObject::Serialize() const {
    External_Game_Protocol::UnityGameObject pkt;
    Serialize(&pkt);
    return pkt;
}

