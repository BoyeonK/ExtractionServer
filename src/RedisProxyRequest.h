#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <vector>
#include <utility>
#include "RedisHandler.h"
#include "ObjectPool.h"

class PendingRedisRequest {
public:
    PendingRedisRequest(int fd) : _gameProcessFd(fd) {}
    virtual ~PendingRedisRequest() = default;

    virtual void Execute(sw::redis::Redis* pRedis) = 0;
    virtual void ReturnToPool() = 0;

protected:
    int _gameProcessFd;
};

class RoomCreationRequest : public PendingRedisRequest {
public:
    RoomCreationRequest(int fd, std::vector<std::string> tickets, std::vector<std::string> tokens)
        : PendingRedisRequest(fd), _tickets(std::move(tickets)), _tokens(std::move(tokens))
        {}

    void Execute(sw::redis::Redis* pRedis) override {
        try {
            // 1. _ticket에 해당하는 row의 token을 _token으로 기입
            // 2. 만들어진 방의 인원들에 대한 status "SUCCESS"로 변경
            // 3. ip와 port를 해당 row에 기입.
            // 4. HTTP서버에 해당 유저가 성공적으로 도착했음을 알림. (MySQL에서 해당 인벤토리 수치만큼 차감하기 위해)
            // 5. 위의 과정에서 문제 없었으면 true를 담아 데디서버에 IPC전송
        } catch (const sw::redis::Error& e) {
            std::cerr << "RoomCreationRequest 에러 발생: " << e.what() << '\n';
            // 1. false를 담아 데디 서버에 전송
        }
    }

    void ReturnToPool() override {
        _tickets.clear(); 
        _tokens.clear();
        _gameProcessFd = -1;

        ObjectPool<RoomCreationRequest>::Release(this);
    }

private:

private:
    std::vector<std::string> _tickets;
    std::vector<std::string> _tokens;
};