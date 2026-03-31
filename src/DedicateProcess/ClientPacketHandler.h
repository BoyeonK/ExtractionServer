#pragma once

#include <cstdint>
#include <functional>
#include "ExternalProtocol/External_Protocol.pb.h"
#include "DediServerService.h"
#include "PlayerSession.h"

#pragma pack(push, 1)
struct UDPHeader {
    uint16_t packetId;
    uint16_t sessionId;
    uint32_t sequenceNum;
    uint32_t securityKey;
    uint8_t  flags;
};
#pragma pack(pop)


//packetId
enum : uint16_t {
    PKT_ID_C2D_TEST_PKT = 0,
    PKT_ID_D2C_TEST_PKT = 1,
    PKT_ID_MAX = 2,
};

extern std::function<bool(PlayerSession*, unsigned char*, int32_t)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize);
bool Handle_C2D_TestPkt(PlayerSession* pSession, External_Game_Protocol::C2DTestPkt& pkt);

class ClientPacketHandler {
public:
    static void Init() {
        for (int i=0; i < PKT_ID_MAX; i++)
			GClientPacketHandler[i] = Handle_Client_Invalid;
        
        GClientPacketHandler[PKT_ID_C2D_TEST_PKT] = [](PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize) { return HandleClientPacket<External_Game_Protocol::C2DTestPkt>(Handle_C2D_TestPkt, pSession, payloadAddr, payloadSize); };
    }

    static bool HandleClientPacket(int bytesTransferred, unsigned char* buffer, const sockaddr_in& clientAddr) {
        UDPHeader* pHeader = reinterpret_cast<UDPHeader*>(buffer);

        uint16_t packetId   = pHeader->packetId;
        uint16_t sessionId  = pHeader->sessionId;
        uint32_t seqNum     = pHeader->sequenceNum;
        uint32_t secKey     = pHeader->securityKey;
        uint8_t flag        = pHeader->flags;

        if (packetId >= PKT_ID_MAX) {
            return false;
        }

        unsigned char* payloadAddr = reinterpret_cast<unsigned char*>(buffer) + sizeof(UDPHeader);
        int32_t payloadSize = bytesTransferred - sizeof(UDPHeader);

        PlayerSession* pSession = pDediServer->GetPlayerSession(sessionId);
        if (pSession == nullptr) {
            return false;
        }
            
        if (pSession->GetSecurityKey() != secKey) {
            return false;
        }

        if (pSession->IsNewSequenceNum(seqNum) == false && flag == 0) {
            return false;
        }

        return GClientPacketHandler[packetId](pSession, payloadAddr, payloadSize);
	}

private:
    template<typename PBType, typename HandlerFunc>
	static bool HandleClientPacket(HandlerFunc func, PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize) {
		PBType pkt;
        if (pkt.ParseFromArray(payloadAddr, payloadSize) == false)
			return false;

		return func(pSession, pkt);
	}
};