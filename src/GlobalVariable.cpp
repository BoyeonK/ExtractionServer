#include "GlobalVariable.h"

#include "IoUringWrapper.h"
#include "SocketWrapper.h"
#include "DediManager.h"
#include "MysqlHandle.h"

IoUringWrapper* IORing = nullptr;
HttpIPCSession* HttpSession = nullptr;
sw::redis::Redis* pRedis = nullptr;
DediManager* pDediManager = nullptr;
DBProxyService* pDBProxyService = nullptr;
MysqlHandle* pMysql = nullptr;