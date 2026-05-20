#include "RedisProxyRequest.h"
#include <iostream>
#include <cstdlib>
#include <ifaddrs.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ObjectPool.h"

namespace NetworkUtil {
    std::string GetLocalIp() {
        struct ifaddrs *interfaces = nullptr;
        struct ifaddrs *temp_addr = nullptr;
        std::string resultIP = "127.0.0.1";

        if (getifaddrs(&interfaces) == 0) {
            temp_addr = interfaces;
            while (temp_addr != nullptr) {
                if (temp_addr->ifa_addr != nullptr && temp_addr->ifa_addr->sa_family == AF_INET) {
                    std::string ifName = temp_addr->ifa_name;
                    
                    // 루프백(127.0.0.1) 무시
                    if (ifName != "lo") {
                        struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(temp_addr->ifa_addr);
                        char ipBuffer[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(addr_in->sin_addr), ipBuffer, INET_ADDRSTRLEN);

                        resultIP = ipBuffer;

                        // WSL의 가상 스위치나 AWS EC2의 기본 어댑터인 eth0를 찾으면 즉시 종료
                        if (ifName == "eth0") {
                            break; 
                        }
                    }
                }
                temp_addr = temp_addr->ifa_next;
            }
        }
        if (interfaces != nullptr) freeifaddrs(interfaces);
        
        return resultIP;
    }
}

// TODO : 
    // 1. ticketIds를 key로 하는 row에
        // 1-1. status를 SUCCESS로 변경하기
        // 1-2. ticketIds를 key로 하는 row에 roomToken colunm추가하고 token값 넣기
    
    // 2. token을 key로하는 row를 만들고,
        // 2-1. udpServerIp 추가
        // 2-2. udpServerPort 추가
        // 2-3. sessionId 추가
        // 2-4. securityKey 추가

/*
void UpdateEntryTokenRequest::Execute(sw::redis::Redis* pRedis) {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            if (envIp != nullptr) {
                // AWS EC2 환경
                std::cout << "[System] 환경변수 IP 감지: " << envIp << std::endl;
                return std::string(envIp);
            } else {
                // 로컬
                std::string localIp = NetworkUtil::GetLocalIp();
                std::cout << "[System] 로컬 환경 감지. 자동 할당 IP: " << localIp << std::endl;
                return localIp;
            }
        }();

        for (size_t i = 0; i < _tickets.size(); ++i) {
            std::string redisKey = _tickets[i]; 

            std::vector<std::pair<std::string, std::string>> updateFields = {
                {"status", "SUCCESS"},
                {"udpServerIp", serverIp},
                {"udpServerPort", portStr},
                {"roomToken", _tokens[i]}
            };

            pipe.hmset(redisKey, updateFields.begin(), updateFields.end());
        }
        pipe.exec();

        std::cout << "매치 테스트 9 - O : UpdateEntryTokenRequest 처리 완료." << std::endl;

        // 4. HTTP서버에 해당 유저가 성공적으로 도착했음을 알림. (MySQL에서 해당 인벤토리 수치만큼 차감하기 위해)
    } catch (const sw::redis::Error& e) {
        std::cerr << "UpdateEntryTokenRequest 에러 발생: " << e.what() << '\n';
        // 1. false를 담아 데디 서버에 전송
    }
}
*/

void UpdateEntryTokenRequest::Execute(sw::redis::Redis* pRedis) {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        std::string fdStr = std::to_string(_gameProcessFd);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            if (envIp != nullptr) {
                // AWS EC2 환경
                std::cout << "[System] 환경변수 IP 감지: " << envIp << std::endl;
                return std::string(envIp);
            } else {
                // 로컬
                std::string localIp = NetworkUtil::GetLocalIp();
                std::cout << "[System] 로컬 환경 감지. 자동 할당 IP: " << localIp << std::endl;
                return localIp;
            }
        }();

        for (size_t i = 0; i < _ticketIds.size(); ++i) {
            const std::string& ticketId = _ticketIds[i];
            const std::string& token = _tokens[i];

            std::string sessionIdStr = std::to_string(_sessionIds[i]);
            std::string securityKeyStr = std::to_string(_securityKeys[i]);

            auto loadoutType = pRedis->hget(ticketId, "loadout_type");
            std::string loadoutTypeStr = loadoutType ? *loadoutType : "FREE";

            std::vector<std::pair<std::string, std::string>> ticketFields = {
                {"status", "SUCCESS"},
                {"token", token}
            };
            pipe.hmset(ticketId, ticketFields.begin(), ticketFields.end());

            std::vector<std::pair<std::string, std::string>> tokenFields = {
                {"udp_server_ip", serverIp},
                {"port", portStr},
                {"session_id", sessionIdStr},
                {"security_key", securityKeyStr},
                {"fd", fdStr},
                {"ticket", ticketId},
                {"loadout_type", loadoutTypeStr}
            };
            pipe.hmset(token, tokenFields.begin(), tokenFields.end());

            pipe.expire(token, 300); 
        }

        pipe.exec();

        std::cout << "매치 테스트 9 - O : UpdateEntryTokenRequest 처리 완료." << std::endl;

        // TODO: HTTP 서버에 해당 유저가 성공적으로 도착했음을 알림.
        
    } catch (const sw::redis::Error& e) {
        std::cerr << "UpdateEntryTokenRequest 에러 발생: " << e.what() << '\n';
        // TODO: 1. false를 담아 데디 서버에 전송
    }
}

void UpdateEntryTokenRequest::ReturnToPool() {
    _ticketIds.clear(); 
    _sessionIds.clear();
    _tokens.clear();
    _securityKeys.clear();
    _gameProcessFd = -1;

    ObjectPool<UpdateEntryTokenRequest>::Release(this);
}