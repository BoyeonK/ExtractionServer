#pragma once

#include <mysql_connection.h>
#include <memory>
#include <string>

// sql::Connection 은 자동 재연결이 없다. MySQL 은 반드시 이 래퍼의 Get() 으로 받을 것.
class MysqlHandle {
public:
    bool Init(const std::string& url, const std::string& user,
              const std::string& password, const std::string& schema);

    sql::Connection* Get();

private:
    std::unique_ptr<sql::Connection> _conn;

    std::string _url;
    std::string _user;
    std::string _password;
    std::string _schema;
};
