#pragma once

#include <cstdint>
#include <functional>

#include "IPCProtocol/IPC_HTTP.pb.h"
#include "IPCProtocol/IPC_Dedicate.pb.h"
#include "GlobalVariable.h"
#include "SocketWrapper.h"
#include "IoUringWrapper.h"

extern std::function<bool(Session*, unsigned char*, int32_t)> GProtoPacketHandler[UINT16_MAX];

enum : uint16_t {
	PKT_ID_M2H_WELCOME = 0,
    PKT_ID_H2M_WELCOME = 1,
	PKT_ID_H2M_MATCH_MAKE = 2,
	PKT_ID_H2M_MATCH_MAKE_CANCEL = 3,
	PKT_ID_H2M2D_BIND_CLIENT_IP_TO_SESSION = 4,

	PKT_ID_D2M_INIT_COMPLETE = 100,
	PKT_ID_M2D_MAKE_ROOM_FOR_THIS_GROUP = 101,
	PKT_ID_M2D_UPDATE_ENTRY_TOKEN = 102,
};

// D로끝나는 경우와 M으로 끝나는 경우 (수신자가 메인 or 데디인경우)
bool Handle_Invalid(Session* pSession, unsigned char* buffer, int32_t len);
bool Handle_H2M_Welcome(Session* pSession, IPC_Protocol::H2MWelcome& pkt);
bool Handle_H2M_MatchMake(Session* pSession, IPC_Protocol::H2MMatchMake& pkt);
bool Handle_H2M_MatchMakeCancel(Session* pSession, IPC_Protocol::H2MMatchMakeCancel& pkt);
bool Handle_H2M2D_BindClientIpToSession(Session* pSession, IPC_Protocol::H2M2DBindClientIpToSession& pkt);

bool Handle_D2M_InitComplete(Session* pSession, IPC_Protocol::D2MInitComplete& pkt);
bool Handle_M2D_MakeRoomForThisGroup(Session* pSession, IPC_Protocol::M2DMakeRoomForThisGroup& pkt);
bool Handle_D2M_UpdateEntryToken(Session* pSession, IPC_Protocol::D2MUpdateEntryToken& pkt);

#pragma pack(push, 1)
struct PacketHeader {
	uint16_t _id;
	uint16_t _size;
};
#pragma pack(pop)

class PacketHandler {
public:
	static void Init() {
		for (int i=0; i < UINT16_MAX; i++)
			GProtoPacketHandler[i] = Handle_Invalid;

		GProtoPacketHandler[PKT_ID_H2M_WELCOME] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::H2MWelcome>(Handle_H2M_Welcome, pSession, buffer, len); };
		GProtoPacketHandler[PKT_ID_H2M_MATCH_MAKE] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::H2MMatchMake>(Handle_H2M_MatchMake, pSession, buffer, len); };
		GProtoPacketHandler[PKT_ID_H2M_MATCH_MAKE_CANCEL] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::H2MMatchMakeCancel>(Handle_H2M_MatchMakeCancel, pSession, buffer, len); };
		GProtoPacketHandler[PKT_ID_H2M2D_BIND_CLIENT_IP_TO_SESSION] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::H2M2DBindClientIpToSession>(Handle_H2M2D_BindClientIpToSession, pSession, buffer, len); };
		
		GProtoPacketHandler[PKT_ID_D2M_INIT_COMPLETE] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::D2MInitComplete>(Handle_D2M_InitComplete, pSession, buffer, len); };
		GProtoPacketHandler[PKT_ID_M2D_MAKE_ROOM_FOR_THIS_GROUP] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::M2DMakeRoomForThisGroup>(Handle_M2D_MakeRoomForThisGroup, pSession, buffer, len); };
		GProtoPacketHandler[PKT_ID_M2D_UPDATE_ENTRY_TOKEN] = [](Session* pSession, unsigned char* buffer, int32_t len) { return HandlePacket<IPC_Protocol::D2MUpdateEntryToken>(Handle_D2M_UpdateEntryToken, pSession, buffer, len); };
	}

	static bool HandlePacket(Session* pSession, unsigned char* buffer, int32_t len) {
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		
		return GProtoPacketHandler[header->_id](pSession, buffer, len);
	}

	static SendBuffer* MakeSendBuffer(const IPC_Protocol::M2HWelcome& pkt) { return MakeSendBuffer(pkt, PKT_ID_M2H_WELCOME); }
	static SendBuffer* MakeSendBuffer(const IPC_Protocol::H2M2DBindClientIpToSession& pkt) { return MakeSendBuffer(pkt, PKT_ID_H2M2D_BIND_CLIENT_IP_TO_SESSION); }
	static SendBuffer* MakeSendBuffer(const IPC_Protocol::D2MInitComplete& pkt) { return MakeSendBuffer(pkt, PKT_ID_D2M_INIT_COMPLETE); }
	static SendBuffer* MakeSendBuffer(const IPC_Protocol::M2DMakeRoomForThisGroup& pkt) { return MakeSendBuffer(pkt, PKT_ID_M2D_MAKE_ROOM_FOR_THIS_GROUP); }
	static SendBuffer* MakeSendBuffer(const IPC_Protocol::D2MUpdateEntryToken& pkt) { return MakeSendBuffer(pkt, PKT_ID_M2D_UPDATE_ENTRY_TOKEN); }

public:
	static IPC_Protocol::M2HWelcome MakeM2HWelcomePkt(int32_t value) {
        IPC_Protocol::M2HWelcome pkt;
        pkt.set_echo_message(value);
        return pkt;
    }

private:
	template<typename PBType, typename HandlerFunc>
	static bool HandlePacket(HandlerFunc func, Session* pSession, unsigned char* buffer, int32_t len) {
		PBType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
			return false;

		return func(pSession, pkt);
	}

	template<typename PBType>
	static SendBuffer* MakeSendBuffer(const PBType& pkt, uint16_t pktId) {
		uint16_t dataSize = static_cast<uint16_t>(pkt.ByteSizeLong());
		uint16_t packetSize = dataSize + sizeof(PacketHeader);

		SendBuffer* sendBuffer = IORing->OpenSendBuffer(packetSize);
		if (sendBuffer == nullptr) {
			//TODO : 에러처리
			return nullptr;
		}
		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->_size = packetSize;
		header->_id = pktId;
		pkt.SerializeToArray(&header[1]/*(++header)*/, dataSize);
		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};

