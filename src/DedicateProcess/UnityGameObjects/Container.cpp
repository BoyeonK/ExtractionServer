#include "Container.h"
#include "../ItemDataManager.h"

static void FillSlotProto(const Slot& slot, External_Game_Protocol::InventorySlot* outSlot) {
    outSlot->set_slot_index(slot.slotIndex);
    auto* item = outSlot->mutable_item();
    item->set_blueprint_id(slot.item.blueprintId);
    item->set_instance_uid(slot.item.instanceUid);
    item->set_item_type(static_cast<uint32_t>(ItemDataManager::GetType(slot.item.blueprintId)));
    item->set_quantity(slot.quantity);
}

void Container::SerializeOpenContainer(External_Game_Protocol::D2CResponseOpenContainer* outMsg) const {
    outMsg->set_container_object_id(objectId);
    outMsg->set_container_version(_containerVersion);
    outMsg->set_container_volume(_containerVolume);

    for (const Slot& slot : _inventorySlots) {
        if (!slot.IsEmpty())
            FillSlotProto(slot, outMsg->add_container_slots());
    }
}

void Container::SerializeRecentContainerInfo(External_Game_Protocol::D2CResponseRecentContainerInfo* outMsg) const {
    outMsg->set_container_object_id(objectId);
    outMsg->set_container_version(_containerVersion);
    outMsg->set_container_volume(_containerVolume);

    for (const Slot& slot : _inventorySlots) {
        if (!slot.IsEmpty())
            FillSlotProto(slot, outMsg->add_container_slots());
    }
}
