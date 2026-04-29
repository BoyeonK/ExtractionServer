#pragma once

//packetId
enum : uint16_t {
    PKT_ID_C2D_CHANNEL_OPEN            = 0,
    PKT_ID_D2C_RESPONSE_CHANNEL_OPEN   = 1,
    PKT_ID_C2D_HEART_BEAT         = 2,
    PKT_ID_D2C_HEART_BEAT         = 3,
    PKT_ID_C2D_REQUEST_BLUEPRINT  = 4,
    PKT_ID_D2C_RESPONSE_BLUEPRINT = 5,
    PKT_ID_C2D_REQUEST_SPAWN_ME   = 6,
    PKT_ID_D2C_RESPONSE_SPAWN_ME  = 7,
    PKT_ID_MAX                    = 8,
};