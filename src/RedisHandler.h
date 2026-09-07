#pragma once

#include <sw/redis++/redis++.h>
#include <mysql_connection.h>

namespace RedisHandler {
    /**
     * @brief 시동 시 Redis db 0을 통째로 파기한다 (guest_uid_counter 만 보존)
     */
    void ResetKeyspace(sw::redis::Redis& redis);

    /**
     * @brief MySQL의 아이템 정보를 Redis 마스터 데이터 캐시로 구축
     */
    void InitializeItemCache(sql::Connection* db_conn, sw::redis::Redis& redis);

    // bool CheckUserSession(sw::redis::Redis& redis, const std::string& uuid);
}
