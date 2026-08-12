#include "RedisHandler.h"
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <iostream>
#include "absl/container/flat_hash_map.h"

namespace RedisHandler {

    void InitializeItemCache(sql::Connection* db_conn, sw::redis::Redis& redis) {
        if (!db_conn) {
            std::cerr << "MySQL 핸들 문제로 Redis Init실패" << std::endl;
            return;
        }

        try {
            redis.command("EVAL", "local keys = redis.call('keys', ARGV[1]) for i, k in ipairs(keys) do redis.call('del', k) end return #keys", 0, "item_meta:*");
            std::cout << "C2-1 - OK : Redis의 이전 item_meta 데이터 삭제" << std::endl;

            std::unique_ptr<sql::PreparedStatement> pstmt(
                db_conn->prepareStatement("SELECT item_id, item_name, item_type, description FROM items")
            );
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            auto pipe = redis.pipeline();
            bool has_data = false;

            while (res->next()) {
                has_data = true;
                std::string item_id = res->getString("item_id");
                std::string key = "item_meta:" + item_id;

                absl::flat_hash_map<std::string, std::string> item_data = {
                    {"name", res->getString("item_name")},
                    {"type", res->getString("item_type")},
                    {"desc", res->getString("description")}
                };

                pipe.hmset(key, item_data.begin(), item_data.end());
            }

            if (has_data) {
                pipe.exec();
                std::cout << "C2-2 - OK : 아이템 메타 데이터 로드 완" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "C2 - X : Redis 초기화 오류: " << e.what() << std::endl;
        }
    }
}