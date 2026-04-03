#pragma once

#include <cstdint>
#include <functional>
#include "../SendBuffer.h"
#include "../GlobalVariable.h"
#include "../IoUringWrapper.h"
#include "ExternalProtocol/External_Protocol.pb.h"
#include "DediServerService.h"
#include "PlayerSession.h"
#include "enum.h"

#pragma pack(push, 1)
struct UDPHeader {
    UDPHeader(uint16_t packetId, uint16_t sessionId, uint32_t sequenceNum, uint32_t securityKey, uint8_t flags)
    : packetId(packetId), sessionId(sessionId), sequenceNum(sequenceNum), securityKey(securityKey), flags(flags)
    {};

    uint16_t packetId;
    uint16_t sessionId;
    uint32_t sequenceNum;
    uint32_t securityKey;
    uint8_t  flags;
};
#pragma pack(pop)

extern std::function<bool(PlayerSession*, unsigned char*, int32_t, const sockaddr_in&)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr);
bool Handle_C2D_TestPkt(PlayerSession* pSession, External_Game_Protocol::C2DTestPkt& pkt, const sockaddr_in& clientAddr);

class ClientPacketHandler {
public:
    static void Init() {
        for (int i=0; i < PKT_ID_MAX; i++)
			GClientPacketHandler[i] = Handle_Client_Invalid;
        
        GClientPacketHandler[PKT_ID_C2D_TEST_PKT] = [](PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) { return HandleClientPacket<External_Game_Protocol::C2DTestPkt>(Handle_C2D_TestPkt, pSession, payloadAddr, payloadSize, clientAddr); };
    }

    static bool HandleClientPacket(int bytesTransferred, unsigned char* buffer, const sockaddr_in& clientAddr) {
        UDPHeader* pHeader = reinterpret_cast<UDPHeader*>(buffer);

        uint16_t packetId   = pHeader->packetId;
        uint16_t sessionId  = pHeader->sessionId;
        uint32_t seqNum     = pHeader->sequenceNum;
        uint32_t secKey     = pHeader->securityKey;
        uint8_t flags       = pHeader->flags;

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

        if (pSession->IsNewSequenceNum(packetId, seqNum) == false && flags == 0) {
            return false;
        }

        return GClientPacketHandler[packetId](pSession, payloadAddr, payloadSize, clientAddr);
	}

    static SendBuffer* MakeD2CPacket(const External_Game_Protocol::D2CTestPkt& pkt, PlayerSession* pSession) { return MakeD2CPacket(pkt, pSession, PKT_ID_D2C_TEST_PKT); }

private:
    template<typename PBType, typename HandlerFunc>
	static bool HandleClientPacket(HandlerFunc func, PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
		PBType pkt;
        if (pkt.ParseFromArray(payloadAddr, payloadSize) == false)
			return false;

		return func(pSession, pkt, clientAddr);
	}

    template<typename PBType>
    static SendBuffer* MakeD2CPacket(const PBType& protobufPkt, PlayerSession* pSession, uint16_t pktId) {
        if (pSession == nullptr) {
            return nullptr;
        }

        uint32_t payloadSize = static_cast<uint32_t>(protobufPkt.ByteSizeLong());
        uint32_t totalSize = sizeof(UDPHeader) + payloadSize;

        SendBuffer* sendBuffer = IORing->OpenSendBuffer(totalSize);
        if (sendBuffer == nullptr)
            return nullptr;

        UDPHeader* pHeader = reinterpret_cast<UDPHeader*>(sendBuffer->Buffer());
        *pHeader = UDPHeader(
            pktId,
            static_cast<uint16_t>(pSession->GetSessionId()),
            pSession->GenerateSequenceNum(pktId),
            pSession->GetSecurityKey(),
            0
        );

        protobufPkt.SerializeToArray(sendBuffer->Buffer() + sizeof(UDPHeader), static_cast<int>(payloadSize));
        sendBuffer->Close(totalSize);

        return sendBuffer;
    }
};