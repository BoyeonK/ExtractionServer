#include "RedisProxyRequest.h"
#include <iostream>
#include "ObjectPool.h"

void UpdateEntryTokenRequest::Execute(sw::redis::Redis* pRedis) {
    try {
        // 1. _ticket에 해당하는 row의 token을 _token으로 기입
        // 2. 만들어진 방의 인원들에 대한 status "SUCCESS"로 변경
        // 3. ip와 port를 해당 row에 기입.
        // 4. HTTP서버에 해당 유저가 성공적으로 도착했음을 알림. (MySQL에서 해당 인벤토리 수치만큼 차감하기 위해)
        // 5. 위의 과정에서 문제 없었으면 true를 담아 데디서버에 IPC전송

        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        
        // TODO: 환경변수에서 가져오기
        std::string serverIp = "127.0.0.1"; 

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