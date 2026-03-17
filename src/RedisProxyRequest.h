#pragma once

#include "RedisHandler.h"

class RoomCreationRequest : public PendingRedisRequest {
public:
    RoomCreationRequest(int fd, std::vector<std::string> tickets)
        : PendingRedisRequest(fd), _tickets(std::move(tickets)) {}

    void Execute(RedisClient* pRedis) override {
        try {
            // 1. 만들어진 방의 인원들에 대한 stauts변경
            // 2. ip와 port와 roomToken을 Redis에 명시
            // 3. HTTP서버에 해당 유저가 성공적으로 도착했음을 알림. (MySQL에서 해당 인벤토리 수치만큼 차감하기 위해)
            // 4. 위의 과정에서 문제 없었으면 true를 담아 데디서버에 IPC전송
        } catch (const sw::redis::Error& e) {
            std::cerr << "RoomCreationRequest 에러 발생: " << e.what() << '\n';
            // 1. false를 담아 데디 서버에 전송
        }
    }

    void ReturnToPool() override {
        _tickets.clear(); 
        _gameProcessFd = -1;

        ObjectPool<RoomCreationRequest>::Release(this);
    }

private:

private:
    std::vector<std::string> _tickets;
};