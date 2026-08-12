#pragma once

#include <sw/redis++/redis++.h>
#include <mysql_connection.h>

namespace RedisHandler {
    /**
     * @brief MySQL의 아이템 정보를 Redis 마스터 데이터 캐시로 구축
     */
    void InitializeItemCache(sql::Connection* db_conn, sw::redis::Redis& redis);

    // bool CheckUserSession(sw::redis::Redis& redis, const std::string& uuid);
}
