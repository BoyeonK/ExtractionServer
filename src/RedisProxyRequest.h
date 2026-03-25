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
    UpdateEntryTokenRequest(int fd, std::vector<std::string> ticketIds, std::vector<int32_t> sessionIds, std::vector<std::string> tokens, std::vector<int32_t> securityKeys, int32_t port)
    : PendingRedisRequest(fd), _ticketIds(std::move(ticketIds)), _sessionIds(std::move(sessionIds)), _tokens(std::move(tokens)), _securityKeys(std::move(securityKeys)), _port(port)
    {}
    
    void Execute(sw::redis::Redis* pRedis) override;

    void ReturnToPool() override;

private:

private:
    std::vector<std::string> _ticketIds;
    std::vector<int32_t> _sessionIds;
    std::vector<std::string> _tokens;
    std::vector<int32_t> _securityKeys;
    int32_t _port;
};