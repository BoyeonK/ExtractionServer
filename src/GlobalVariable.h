#pragma once

#include <sw/redis++/redis++.h>

class IoUringWrapper;
class HttpIPCSession;
class DediManager;
class RedisProxyService;

extern IoUringWrapper* IORing;
extern HttpIPCSession* HttpSession;
extern sw::redis::Redis* pRedis;
extern DediManager* pDediManager;
extern RedisProxyService* pRedisProxyService;
