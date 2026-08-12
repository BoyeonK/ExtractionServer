#pragma once

#include <sw/redis++/redis++.h>

class IoUringWrapper;
class HttpIPCSession;
class DediManager;
class DBProxyService;
class MysqlHandle;

extern IoUringWrapper* IORing;
extern HttpIPCSession* HttpSession;
extern sw::redis::Redis* pRedis;
extern DediManager* pDediManager;
extern DBProxyService* pDBProxyService;
extern MysqlHandle* pMysql;
