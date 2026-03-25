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

void UpdateEntryTokenRequest::ReturnToPool() {
    _tickets.clear(); 
    _tokens.clear();
    _gameProcessFd = -1;

    ObjectPool<UpdateEntryTokenRequest>::Release(this);
}