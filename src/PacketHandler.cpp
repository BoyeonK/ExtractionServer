#include "PacketHandler.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <iterator>
#include "ObjectPool.h"
#include "RedisProxyRequest.h"
#include "RedisHandler.h"
#include "DediManager.h"
#include "DedicateProcess/Matchmaker.h"
#include "DedicateProcess/DediServerService.h"

using namespace std;

std::function<bool(Session*, unsigned char*, int32_t)> GProtoPacketHandler[UINT16_MAX];

bool Handle_Invalid(Session* pSession, unsigned char* buffer, int32_t len) {
    if (pSession == nullptr) return false;
    cout << "Something goes wrong, Client sent invalid packet" << endl;
    return false;
}

bool Handle_H2M_Welcome(Session* pSession, IPC_Protocol::H2MWelcome& pkt) {
    if (pSession == nullptr) return false;

    cout << "H3-2 - OK 받은 echo_message : " << pkt.echo_message() << endl;

    return true;
}

bool Handle_H2M_MatchMake(Session* pSession, IPC_Protocol::H2MMatchMake& pkt) {
    if (pSession == nullptr) return false;

    // TODO : 
    // 1. pkt에서 ticket_redis_key를 읽는다.
    // 2. 해당 key로 Redis에 접근하여 table을 읽고, MatchTicket struct를 pool에서 할당받고 읽은 값을 토대로 초기화.
    // 3. Matchmaker의 AddSingleMatchTicket으로 대기열에 추가한다.
    string ticketKey = pkt.ticket_redis_key();
    std::cout << "매치 테스트 3 - O : ticket: "<< ticketKey << "를 받아 Handle_H2M_MatchMake 핸들러 함수 실행" << endl;
    try {
        std::unordered_map<std::string, std::string> val;
        pRedis->hgetall(ticketKey, std::inserter(val, val.begin()));

        if (val.empty()) return false;

        MatchTicket* pTicket = ObjectPool<MatchTicket>::Acquire(
            ticketKey,
            std::stoi(val.at("uid")),
            std::stoi(val.at("aggression")),
            std::stoi(val.at("map_id"))
        );

        if (pDediManager->AddSingleMatchTicket(pTicket) == false) {
            std::cerr << "매치 테스트 3 - X : pDediManager의 AddSingleMatchTicket이 false" << std::endl;
            ObjectPool<MatchTicket>::Release(pTicket);
            return false;
        }

    } catch (const sw::redis::Error& e) {
        std::cerr << "매치 테스트 3 - X : Redis Error: " << e.what() << std::endl;
        return false;
    }

    return true;
}

bool Handle_H2M_MatchMakeCancel(Session* pSession, IPC_Protocol::H2MMatchMakeCancel& pkt) {
    if (pSession == nullptr) return false;

    string ticketKey = pkt.ticket_redis_key();
    std::cout << "매치 취소 테스트 3 - O : ticket: "<< ticketKey << "를 받아 Handle_H2M_MatchMakeCancel 핸들러 함수 실행" << endl;

    return true;
}

bool Handle_H2M2D_BindClientIpToSession(Session* pSession, IPC_Protocol::H2M2DBindClientIpToSession& pkt) {
    if (pSession == nullptr) return false;

    // ㅋㅋㅋㅋ 나중에 보면 문제가 될 레거시코드의 탄생
    if (HttpIPCSession* pD2HSession = dynamic_cast<HttpIPCSession*>(pSession)) {
        pDediManager->BindClientIpToSession(pkt);
    } else if (D2MSession* pD2MSession = dynamic_cast<D2MSession*>(pSession)) {
        
    } else {
        std::cerr << "비정상적인 세션에서 IP 바인딩을 시도했습니다!\n";
        return false;
    }

    return true;
}

bool Handle_D2M_InitComplete(Session* pSession, IPC_Protocol::D2MInitComplete& pkt) {
    if (pSession == nullptr) return false;
    // 이 함수가 실행될 일은 없다. 정작 이 패킷을 사용해야 할 TempSession은 패킷 핸들러를 참조하지 않고
    // D2M_InitComplete를 처리할 전용 함수를 들고 있음.
    cout << "이거 보면 병슨 ㅋ" << endl;

    return true;
}

bool Handle_M2D_MakeRoomForThisGroup(Session* pSession, IPC_Protocol::M2DMakeRoomForThisGroup& pkt) {
    if (pSession == nullptr || pDediServer == nullptr) return false;

    std::cout << "매치 테스트 6 - O : DedicateProcess에서 IPC 요청 받음" << std::endl;
    int mapId = pkt.map_id();
    std::vector<std::string> ticketIds;
    ticketIds.reserve(pkt.ticket_id_size());
    for (int i = 0; i < pkt.ticket_id_size(); ++i)
        ticketIds.push_back(pkt.ticket_id(i));

    return pDediServer->MakeRoomForThisGroup(mapId, ticketIds);
}

bool Handle_D2M_UpdateEntryToken(Session* pSession, IPC_Protocol::D2MUpdateEntryToken& pkt) {
    if (pSession == nullptr) return false;

    std::cout << "매치 테스트 8 - O : DedicateProcess에서 IPC 요청 받음" << std::endl;

    int vecSize = pkt.size();
    if (vecSize == -1) {
        return false;
    }
    
    int32_t port = pkt.port();
    std::vector<std::string> ticketIds;
    std::vector<std::int32_t> sessionIds;
    std::vector<std::string> tokens;
    std::vector<std::int32_t> securityKeys;
    ticketIds.reserve(vecSize);
    sessionIds.reserve(vecSize);
    tokens.reserve(vecSize);
    securityKeys.reserve(vecSize);

    for (int i=0; i<vecSize; i++) {
        ticketIds.push_back(pkt.ticket_ids(i));
        sessionIds.push_back(pkt.session_ids(i));
        tokens.push_back(pkt.entry_tokens(i));
        securityKeys.push_back(pkt.security_keys(i));
    }
    
    UpdateEntryTokenRequest* pRequest = ObjectPool<UpdateEntryTokenRequest>::Acquire(
        pSession->GetFd(), 
        std::move(ticketIds),
        std::move(sessionIds),
        std::move(tokens),
        std::move(securityKeys),
        port
    );

    pRedisProxyService->RegisterRedisRequest(pRequest);
    std::cout << "매치 테스트 9 : ProxyRedisService객체에 UpdateEntryTokenRequest요청 등록" << std::endl;

    return true;
}