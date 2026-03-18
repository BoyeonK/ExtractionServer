#include "RedisProxyRequest.h"
#include <iostream>
#include <cstdlib>
#include "ObjectPool.h"

void UpdateEntryTokenRequest::Execute(sw::redis::Redis* pRedis) {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            return envIp ? std::string(envIp) : std::string("127.0.0.1");
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