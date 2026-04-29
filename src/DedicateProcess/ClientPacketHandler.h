#pragma once

#include <cstdint>
#include <functional>
#include <chrono>
#include "xxhash.h"
#include "../SendBuffer.h"
#include "../GlobalVariable.h"
#include "../IoUringWrapper.h"
#include "ExternalProtocol/External_Protocol.pb.h"
#include "DediServerService.h"
#include "PlayerSession.h"
#include "enum.h"

// ── UDP 패킷 헤더 (35B) ──────────────────────────────────────────────────────
#pragma pack(push, 1)
struct UDPHeader {
    // ── 보안 서명 ────────────────────── 8B
    uint64_t signature;   // 8B - xxHash64 서명 (해싱 전에는 반드시 0이어야 함)

    // ── 기본 필드 ────────────────────── 11B
    uint16_t packetId;    // 2B - 패킷 종류 식별
    uint16_t sessionId;   // 2B - 세션 식별
    uint32_t rSeqNum;     // 4B - reliable 채널 시퀀스
    uint16_t uSeqNum;     // 2B - unreliable 채널 시퀀스 (기존의 안전한 16비트 유지!)
    uint8_t  flags;       // 1B - 플래그

    // ── ACK 필드 (reliable 채널 전용) ── +8B
    uint32_t ackRSeqNum;  // 4B - 수신 확인한 가장 최신 reliable 시퀀스 번호
    uint32_t ackBitfield; // 4B - 이전 32개 reliable 패킷 수신 여부

    // ── 타임스탬프 ───────────────────── +8B
    uint32_t timestamp;      // 4B - 송신 시각 (ms, steady_clock 기준)
    uint32_t timestampEcho;  // 4B - 상대방 timestamp 반사 (RTT 계산용)

    // ── Total: 35B ───────────────────────────────────────────────

    UDPHeader(uint16_t packetId, uint16_t sessionId,
              uint32_t rSeqNum, uint16_t uSeqNum,
              uint8_t flags,
              uint32_t ackRSeqNum = 0, uint32_t ackBitfield = 0,
              uint32_t timestamp = 0,  uint32_t timestampEcho = 0)
        : signature(0), // ✨ 해싱을 위해 0으로 초기화
          packetId(packetId), sessionId(sessionId),
          rSeqNum(rSeqNum), uSeqNum(uSeqNum),
          flags(flags),
          ackRSeqNum(ackRSeqNum), ackBitfield(ackBitfield),
          timestamp(timestamp), timestampEcho(timestampEcho)
    {}
};
#pragma pack(pop)

static_assert(sizeof(UDPHeader) == 35, "UDPHeader size mismatch");

// ── 플래그 비트 ──────────────────────────────────────────────────────────────
enum : uint8_t {
    FLAG_HAS_ACK    = 0x01,  // ackRSeqNum / ackBitfield 유효
    FLAG_RELIABLE   = 0x02,  // 이 패킷은 ACK를 요구함 (재전송 대상)
    FLAG_FRAGMENTED = 0x04,  // 예약 - 미사용
};

extern std::function<bool(PlayerSession*, unsigned char*, int32_t, const sockaddr_in&)> GClientPacketHandler[PKT_ID_MAX];

bool Handle_Client_Invalid(PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr);
bool Handle_C2D_ChannelOpen(PlayerSession* pSession, External_Game_Protocol::C2DChannelOpen& pkt, const sockaddr_in& clientAddr);
bool Handle_C2D_HeartBeat(PlayerSession* pSession, External_Game_Protocol::C2DHeartBeat& pkt, const sockaddr_in& clientAddr);
bool Handle_C2D_RequestBlueprint(PlayerSession* pSession, External_Game_Protocol::C2DRequestBlueprint& pkt, const sockaddr_in& clientAddr);
bool Handle_C2D_RequestSpawnMe(PlayerSession* pSession, External_Game_Protocol::C2DRequestSpawnMe& pkt, const sockaddr_in& clientAddr);

class ClientPacketHandler {
public:
    // 현재 ms 타임스탬프 (steady_clock 기반, wrap-around 안전)
    static uint32_t NowMs() {
        using namespace std::chrono;
        return static_cast<uint32_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
    }

    static void Init() {
        for (int i = 0; i < PKT_ID_MAX; i++)
            GClientPacketHandler[i] = Handle_Client_Invalid;

        GClientPacketHandler[PKT_ID_C2D_CHANNEL_OPEN] = [](PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
            return HandleClientPacketPayload<External_Game_Protocol::C2DChannelOpen>(Handle_C2D_ChannelOpen, pSession, payloadAddr, payloadSize, clientAddr);
        };
        GClientPacketHandler[PKT_ID_C2D_HEART_BEAT] = [](PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
            return HandleClientPacketPayload<External_Game_Protocol::C2DHeartBeat>(Handle_C2D_HeartBeat, pSession, payloadAddr, payloadSize, clientAddr);
        };
        GClientPacketHandler[PKT_ID_C2D_REQUEST_BLUEPRINT] = [](PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
            return HandleClientPacketPayload<External_Game_Protocol::C2DRequestBlueprint>(Handle_C2D_RequestBlueprint, pSession, payloadAddr, payloadSize, clientAddr);
        };
        GClientPacketHandler[PKT_ID_C2D_REQUEST_SPAWN_ME] = [](PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
            return HandleClientPacketPayload<External_Game_Protocol::C2DRequestSpawnMe>(Handle_C2D_RequestSpawnMe, pSession, payloadAddr, payloadSize, clientAddr);
        };
    }

    // ── 수신 진입점 ──────────────────────────────────────────────────────────
    static bool HandleClientPacket(int bytesTransferred, unsigned char* buffer, const sockaddr_in& clientAddr) {
        if (bytesTransferred < static_cast<int>(sizeof(UDPHeader)))
            return false;

        UDPHeader* pHeader = reinterpret_cast<UDPHeader*>(buffer);

        uint16_t packetId  = pHeader->packetId;
        uint16_t sessionId = pHeader->sessionId;
        uint32_t rSeqNum   = pHeader->rSeqNum;
        uint16_t uSeqNum   = pHeader->uSeqNum;
        uint8_t  flags     = pHeader->flags;

        if (packetId >= PKT_ID_MAX)
            return false;

        unsigned char* payloadAddr = buffer + sizeof(UDPHeader);
        int32_t payloadSize = bytesTransferred - static_cast<int>(sizeof(UDPHeader));

        PlayerSession* pSession = pDediServer->GetPlayerSession(sessionId);
        if (pSession == nullptr)
            return false;

        uint64_t receivedSignature = pHeader->signature;
        pHeader->signature = 0;
        XXH64_state_t hashState;
        XXH64_reset(&hashState, 0);
        XXH64_update(&hashState, buffer, bytesTransferred); 
        uint32_t secKey = pSession->GetSecurityKey();
        XXH64_update(&hashState, &secKey, sizeof(secKey));

        uint64_t calculatedSignature = XXH64_digest(&hashState);
        
        if (calculatedSignature != receivedSignature) {
            return false; 
        }

        pHeader->signature = receivedSignature;

        // 채널에 맞는 수신 상태 업데이트 (중복 감지)
        if (flags & FLAG_RELIABLE) {
            if (pSession->UpdateRRecvState(rSeqNum) == false)
                return false;
        } else {
            if (pSession->UpdateURecvState(uSeqNum) == false)
                return false;
        }

        // 상대방 ACK 처리 → 재전송 큐에서 확인된 패킷 제거
        if (flags & FLAG_HAS_ACK)
            pSession->ProcessIncomingAck(pHeader->ackRSeqNum, pHeader->ackBitfield);

        // 수신한 timestamp 보관 (다음 송신 시 echo)
        if (pHeader->timestamp != 0)
            pSession->SetLastRecvTimestamp(pHeader->timestamp);

        // RTT 갱신
        if (pHeader->timestampEcho != 0)
            pSession->UpdateRtt(pHeader->timestampEcho, NowMs());

        return GClientPacketHandler[packetId](pSession, payloadAddr, payloadSize, clientAddr);
    }

    // ── 공개 송신 헬퍼 (unreliable) ──────────────────────────────────────────
    static SendBuffer* MakeD2CResponseChannelOpen(const External_Game_Protocol::D2CResponseChannelOpen& pkt, PlayerSession* pSession) {
        return MakeD2CPacketImpl(pkt, pSession, PKT_ID_D2C_RESPONSE_CHANNEL_OPEN, /*reliable=*/false);
    }

    // ── 공개 송신 헬퍼 (reliable) ────────────────────────────────────────────
    // 전송 후 RegisterReliable까지 처리하므로 destAddr를 반드시 전달해야 함
    static SendBuffer* MakeD2CResponseChannelOpenReliable(const External_Game_Protocol::D2CResponseChannelOpen& pkt,
                                                           PlayerSession* pSession, const sockaddr_in& destAddr) {
        return MakeD2CPacketImpl(pkt, pSession, PKT_ID_D2C_RESPONSE_CHANNEL_OPEN, /*reliable=*/true, &destAddr);
    }

    static SendBuffer* MakeD2CHeartBeat(const External_Game_Protocol::D2CHeartBeat& pkt,
                                         PlayerSession* pSession, const sockaddr_in& destAddr) {
        return MakeD2CPacketImpl(pkt, pSession, PKT_ID_D2C_HEART_BEAT, /*reliable=*/true, &destAddr);
    }

    static SendBuffer* MakeD2CResponseBlueprint(const External_Game_Protocol::D2CResponseBlueprint& pkt,
                                                  PlayerSession* pSession, const sockaddr_in& destAddr) {
        return MakeD2CPacketImpl(pkt, pSession, PKT_ID_D2C_RESPONSE_BLUEPRINT, /*reliable=*/true, &destAddr);
    }

    static SendBuffer* MakeD2CResponseSpawnMe(const External_Game_Protocol::D2CResponseSpawnMe& pkt,
                                               PlayerSession* pSession, const sockaddr_in& destAddr) {
        return MakeD2CPacketImpl(pkt, pSession, PKT_ID_D2C_RESPONSE_SPAWN_ME, /*reliable=*/true, &destAddr);
    }

private:
    // ── 페이로드 파싱 후 핸들러 호출 ─────────────────────────────────────────
    template<typename PBType, typename HandlerFunc>
    static bool HandleClientPacketPayload(HandlerFunc func, PlayerSession* pSession, unsigned char* payloadAddr, int32_t payloadSize, const sockaddr_in& clientAddr) {
        PBType pkt;
        if (pkt.ParseFromArray(payloadAddr, payloadSize) == false)
            return false;
        return func(pSession, pkt, clientAddr);
    }

    // ── 패킷 직렬화 + 헤더 구성 (unreliable / reliable 공통) ────────────────
    template<typename PBType>
    static SendBuffer* MakeD2CPacketImpl(const PBType& protobufPkt, PlayerSession* pSession, uint16_t pktId, bool reliable, const sockaddr_in* destAddr = nullptr) {
        if (pSession == nullptr)
            return nullptr;

        uint32_t payloadSize = static_cast<uint32_t>(protobufPkt.ByteSizeLong());
        uint32_t totalSize   = sizeof(UDPHeader) + payloadSize;

        SendBuffer* sendBuffer = IORing->OpenSendBuffer(totalSize);
        if (sendBuffer == nullptr)
            return nullptr;

        uint32_t nowMs   = NowMs();
        uint32_t rSeqNum = 0;
        uint16_t uSeqNum = 0;

        if (reliable)
            rSeqNum = pSession->NextSendRSeq();
        else
            uSeqNum = pSession->NextSendUSeq();

        auto [ackRSeq, ackBf] = pSession->GetAckState();
        uint8_t flags = reliable ? (FLAG_RELIABLE | FLAG_HAS_ACK) : 0;
        if (!reliable && pSession->HasRRecv()) flags |= FLAG_HAS_ACK;

        // 헤더 세팅 (서명 필드는 우선 0으로 초기화)
        UDPHeader* pHeader = reinterpret_cast<UDPHeader*>(sendBuffer->Buffer());
        *pHeader = UDPHeader(
            pktId,
            static_cast<uint16_t>(pSession->GetSessionId()),
            rSeqNum,
            uSeqNum,
            flags,
            ackRSeq,
            ackBf,
            nowMs,
            pSession->GetLastRecvTimestamp()
        );

        // 2. 페이로드 직렬화 (헤더 바로 뒤에 Protobuf 데이터 복사)
        protobufPkt.SerializeToArray(sendBuffer->Buffer() + sizeof(UDPHeader),
                                    static_cast<int>(payloadSize));

        XXH64_state_t hashState;
        XXH64_reset(&hashState, 0); // 0은 시드(Seed) 값
        XXH64_update(&hashState, sendBuffer->Buffer(), totalSize);
        uint32_t secKey = pSession->GetSecurityKey();
        XXH64_update(&hashState, &secKey, sizeof(secKey));

        pHeader->signature = XXH64_digest(&hashState);

        // reliable 패킷은 재전송 큐에 등록
        if (reliable && destAddr != nullptr) {
            pSession->RegisterReliable(rSeqNum, sendBuffer->Buffer(), totalSize, *destAddr, nowMs);
        }

        sendBuffer->Close(totalSize);

        return sendBuffer;
    }
};
