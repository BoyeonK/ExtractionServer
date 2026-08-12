#pragma once

#include <mysql_connection.h>
#include <memory>
#include <string>

// 상시 MySQL 연결. Redis 핸들(pRedis)과 달리 커넥션 풀도 자동 재연결도 없으므로
// 이 래퍼가 사용 직전 생존 확인과 재연결을 맡는다. wait_timeout(기본 8시간) 이나
// HeatWave 페일오버로 끊긴 sql::Connection 은 이후 모든 사용에서 예외를 던지는
// 죽은 객체가 되며, 이탈이 드문 구간이 길어지면 실제로 그 상태에 도달한다.
class MysqlHandle {
public:
    bool Init(const std::string& url, const std::string& user,
              const std::string& password, const std::string& schema);

    // 살아 있는 연결을 반환한다. 끊겼으면 재연결을 시도하고, 실패하면 nullptr.
    sql::Connection* Get();

private:
    std::unique_ptr<sql::Connection> _conn;

    std::string _url;
    std::string _user;
    std::string _password;
    std::string _schema;
};
