#pragma once

#include <sw/redis++/redis++.h>
#include <mysql_connection.h>
#include <memory>

namespace RedisHandler {
    /**
     * @brief MySQL의 아이템 정보를 Redis 마스터 데이터 캐시로 구축
     */
    void InitializeItemCache(sql::Connection* db_conn, sw::redis::Redis& redis);
    
    // bool CheckUserSession(sw::redis::Redis& redis, const std::string& uuid);
}

/*
class RedisProxyService {
public:
    RedisProxyService(RedisClient* pRedis) : _pRedis(pRedis) {};

    void RegisterRedisRequest(PendingRedisRequest* pRequest) {
        _requestQueue.push(std::move(pRequest));
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
    RedisClient* _pRedis;
    std::queue<PendingRedisRequest*> _requestQueue;
};

class PendingRedisRequest {
public:
    PendingRedisRequest(int fd) : _gameProcessFd(fd) {}
    virtual ~PendingRedisRequest() = default;

    virtual void Execute() {}
    virtual void ReturnToPool() = 0;

protected:
    int _gameProcessFd;
};
*/
