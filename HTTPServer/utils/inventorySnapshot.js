const { WAREHOUSE_SLOT_MIN, LOADOUT_SLOT_MAX } = require('./slotLayout');

// 통과하면 { slots: Map<slot_index, entry> }, 아니면 { error: { message, code } } (전부 400)
function validateSnapshot(inventory) {
    const slots = new Map();

    for (const entry of inventory) {
        if (
            !Number.isInteger(entry.item_id) || entry.item_id <= 0 ||
            !Number.isInteger(entry.slot_index) ||
            entry.slot_index < WAREHOUSE_SLOT_MIN || entry.slot_index > LOADOUT_SLOT_MAX ||
            !Number.isInteger(entry.quantity) || entry.quantity < 1
        ) {
            return { error: { message: "인벤토리 스냅샷 형식이 올바르지 않습니다.", code: "ERR_BAD_REQUEST" } };
        }
        if (slots.has(entry.slot_index)) {
            return { error: { message: "스냅샷에 중복된 슬롯이 있습니다.", code: "ERR_DUPLICATE_SLOT" } };
        }
        slots.set(entry.slot_index, entry);
    }

    return { slots };
}

function totalsMatch(dbRows, inventory) {
    const dbTotals = new Map();
    for (const row of dbRows) {
        dbTotals.set(row.item_id, (dbTotals.get(row.item_id) ?? 0) + row.quantity);
    }

    const snapTotals = new Map();
    for (const entry of inventory) {
        snapTotals.set(entry.item_id, (snapTotals.get(entry.item_id) ?? 0) + entry.quantity);
    }

    if (dbTotals.size !== snapTotals.size) return false;
    for (const [itemId, total] of dbTotals) {
        if (snapTotals.get(itemId) !== total) return false;
    }

    return true;
}

module.exports = { validateSnapshot, totalsMatch };
