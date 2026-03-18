#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <vector>
#include <utility>

class PendingRedisRequest {
public:
    PendingRedisRequest(int fd) : _gameProcessFd(fd) {}
    virtual ~PendingRedisRequest() = default;

    virtual void Execute(sw::redis::Redis* pRedis) = 0;
    virtual void ReturnToPool() = 0;

protected:
    int _gameProcessFd;
};

class UpdateEntryTokenRequest : public PendingRedisRequest {
public:
    UpdateEntryTokenRequest(int fd, std::vector<std::string> tickets, std::vector<std::string> tokens, int32_t port)
    : PendingRedisRequest(fd), _tickets(std::move(tickets)), _tokens(std::move(tokens)), _port(port)
    {}
    
    void Execute(sw::redis::Redis* pRedis) override;

    void ReturnToPool() override;

private:

private:
    std::vector<std::string> _tickets;
    std::vector<std::string> _tokens;
    int32_t _port;
};