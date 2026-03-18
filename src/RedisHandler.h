#pragma once

#include <sw/redis++/redis++.h>
#include <mysql_connection.h>
#include <queue>
#include <memory>
#include "RedisProxyRequest.h"

namespace RedisHandler {
    /**
     * @brief MySQL의 아이템 정보를 Redis 마스터 데이터 캐시로 구축
     */
    void InitializeItemCache(sql::Connection* db_conn, sw::redis::Redis& redis);
    
    // bool CheckUserSession(sw::redis::Redis& redis, const std::string& uuid);
}

class RedisProxyService {
public:
    RedisProxyService(sw::redis::Redis* pRedis) : _pRedis(pRedis) {};

    void RegisterRedisRequest(PendingRedisRequest* pRequest) {
        _requestQueue.push(pRequest);
    }

    void ExecuteAll() {
        while (!_requestQueue.empty()) {
            auto request = _requestQueue.front();
            _requestQueue.pop();
            
            request->Execute(_pRedis); 
            request->ReturnToPool();
        }
    }

private:
    sw::redis::Redis* _pRedis;
    std::queue<PendingRedisRequest*> _requestQueue;
};
