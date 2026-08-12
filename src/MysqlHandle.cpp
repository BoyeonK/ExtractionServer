#include "MysqlHandle.h"

#include <mysql_driver.h>
#include <iostream>

bool MysqlHandle::Init(const std::string& url, const std::string& user,
                       const std::string& password, const std::string& schema) {
    _url      = url;
    _user     = user;
    _password = password;
    _schema   = schema;

    return Get() != nullptr;
}

sql::Connection* MysqlHandle::Get() {
    try {
        if (_conn != nullptr) {
            if (_conn->isValid())
                return _conn.get();

            if (_conn->reconnect()) {
                _conn->setSchema(_schema);
                std::cout << "[MysqlHandle] 재연결 성공" << std::endl;
                return _conn.get();
            }
            _conn.reset();
        }

        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        _conn.reset(driver->connect(_url, _user, _password));
        _conn->setSchema(_schema);
        return _conn.get();

    } catch (const sql::SQLException& e) {
        std::cerr << "[MysqlHandle] 연결 실패: " << e.what() << std::endl;
        _conn.reset();
        return nullptr;
    }
}
