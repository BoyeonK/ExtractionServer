#include "DBProxyRequest.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <cppconn/prepared_statement.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ObjectPool.h"
#include "GlobalVariable.h"
#include "MysqlHandle.h"

namespace NetworkUtil {
    std::string GetLocalIp() {
        struct ifaddrs *interfaces = nullptr;
        struct ifaddrs *temp_addr = nullptr;
        std::string resultIP = "127.0.0.1";

        if (getifaddrs(&interfaces) == 0) {
            temp_addr = interfaces;
            while (temp_addr != nullptr) {
                if (temp_addr->ifa_addr != nullptr && temp_addr->ifa_addr->sa_family == AF_INET) {
                    std::string ifName = temp_addr->ifa_name;
                    
                    if (ifName != "lo") {
                        struct sockaddr_in* addr_in = reinterpret_cast<struct sockaddr_in*>(temp_addr->ifa_addr);
                        char ipBuffer[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(addr_in->sin_addr), ipBuffer, INET_ADDRSTRLEN);

                        resultIP = ipBuffer;

                        // WSL의 가상 스위치나 Oracle Compute의 기본 어댑터인 eth0를 찾으면 즉시 종료
                        if (ifName == "eth0") {
                            break; 
                        }
                    }
                }
                temp_addr = temp_addr->ifa_next;
            }
        }
        if (interfaces != nullptr) freeifaddrs(interfaces);
        
        return resultIP;
    }
}

// TODO : 
    // 1. ticketIds를 key로 하는 row에
        // 1-1. status를 SUCCESS로 변경하기
        // 1-2. ticketIds를 key로 하는 row에 roomToken colunm추가하고 token값 넣기
    
    // 2. token을 key로하는 row를 만들고,
        // 2-1. udpServerIp 추가
        // 2-2. udpServerPort 추가
        // 2-3. sessionId 추가
        // 2-4. securityKey 추가

/*
void UpdateEntryTokenRequest::Execute() {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            if (envIp != nullptr) {
                std::cout << "[System] 환경변수 IP 감지: " << envIp << std::endl;
                return std::string(envIp);
            } else {
                std::string localIp = NetworkUtil::GetLocalIp();
                std::cout << "[System] 로컬 환경 감지. 자동 할당 IP: " << localIp << std::endl;
                return localIp;
            }
        }();

        for (size_t i = 0; i < _tickets.size(); ++i) {
            std::string redisKey = _tickets[i]; 

            std::vector<std::pair<std::string, std::string>> updateFields = {
                {"status", "SUCCESS"},
                {"udpServerIp", serverIp},
                {"udpServerPort", portStr},
                {"roomToken", _tokens[i]}
            };

            pipe.hmset(redisKey, updateFields.begin(), updateFields.end());
        }
        pipe.exec();

        std::cout << "매치 테스트 9 - O : UpdateEntryTokenRequest 처리 완료." << std::endl;

        // 4. HTTP서버에 해당 유저가 성공적으로 도착했음을 알림. (MySQL에서 해당 인벤토리 수치만큼 차감하기 위해)
    } catch (const sw::redis::Error& e) {
        std::cerr << "UpdateEntryTokenRequest 에러 발생: " << e.what() << '\n';
        // 1. false를 담아 데디 서버에 전송
    }
}
*/

void UpdateEntryTokenRequest::Execute() {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        std::string fdStr = std::to_string(_gameProcessFd);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            if (envIp != nullptr) {
                std::cout << "[System] 환경변수 IP 감지: " << envIp << std::endl;
                return std::string(envIp);
            } else {
                std::string localIp = NetworkUtil::GetLocalIp();
                std::cout << "[System] 로컬 환경 감지. 자동 할당 IP: " << localIp << std::endl;
                return localIp;
            }
        }();

        for (size_t i = 0; i < _ticketIds.size(); ++i) {
            const std::string& ticketId = _ticketIds[i];
            const std::string& token = _tokens[i];

            std::string sessionIdStr = std::to_string(_sessionIds[i]);
            std::string securityKeyStr = std::to_string(_securityKeys[i]);

            auto loadoutType = pRedis->hget(ticketId, "loadout_type");
            std::string loadoutTypeStr = loadoutType ? *loadoutType : "FREE";

            if (auto uid = pRedis->hget(ticketId, "uid")) {
                pipe.set("active_match:" + *uid, "INGAME:" + ticketId,
                         std::chrono::seconds(ACTIVE_MATCH_TTL_SEC));
            } else {
                std::cerr << "[UpdateEntryToken] 티켓에 uid 가 없어 INGAME 표식을 남기지 못했다: "
                          << ticketId << std::endl;
            }

            std::vector<std::pair<std::string, std::string>> ticketFields = {
                {"status", "SUCCESS"},
                {"token", token}
            };
            pipe.hmset(ticketId, ticketFields.begin(), ticketFields.end());

            std::vector<std::pair<std::string, std::string>> tokenFields = {
                {"udp_server_ip", serverIp},
                {"port", portStr},
                {"session_id", sessionIdStr},
                {"security_key", securityKeyStr},
                {"fd", fdStr},
                {"ticket", ticketId},
                {"loadout_type", loadoutTypeStr}
            };
            pipe.hmset(token, tokenFields.begin(), tokenFields.end());

            pipe.expire(token, 300); 
        }

        pipe.exec();

        std::cout << "매치 테스트 9 - O : UpdateEntryTokenRequest 처리 완료." << std::endl;

        // TODO: HTTP 서버에 해당 유저가 성공적으로 도착했음을 알림.
        
    } catch (const sw::redis::Error& e) {
        std::cerr << "UpdateEntryTokenRequest 에러 발생: " << e.what() << '\n';
        // TODO: 1. false를 담아 데디 서버에 전송
    }
}

void DBProxyService::RegisterDBRequest(PendingDBRequest* pRequest) {
    _requestQueue.push(pRequest);
}

bool DBProxyService::ExecuteAll() {
    if (_requestQueue.empty())
        return false;

    while (!_requestQueue.empty()) {
        auto request = _requestQueue.front();
        _requestQueue.pop();

        request->Execute();
        request->ReturnToPool();
    }

    return true;
}

bool NotifyPlayerLeftRequest::ApplyInventoryToDb() {
    // 게스트(uid < 0)는 users 테이블에 행이 없어 user_inventory.uid 의 FK 가 INSERT 를 거부한다.
    if (_uid < 0) {
        std::cout << "[NotifyPlayerLeft] 게스트 이탈 — DB 반영 생략 (uid=" << _uid << ")" << std::endl;
        return true;
    }

    if (pMysql == nullptr) return false;

    sql::Connection* pConn = pMysql->Get();
    if (pConn == nullptr) return false;

    try {
        pConn->setAutoCommit(false);

        // 창고(0~79)는 레이드 대상이 아니므로 건드리지 않는다.
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                pConn->prepareStatement(
                    "DELETE FROM user_inventory WHERE uid = ? AND slot_index >= ?"));
            pstmt->setInt(1, _uid);
            pstmt->setInt(2, INVENTORY_SLOT_INDEX_BEGIN);
            pstmt->executeUpdate();
        }

        if (_slots.empty() == false) {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                pConn->prepareStatement(
                    "INSERT INTO user_inventory (uid, item_id, slot_index, quantity) "
                    "VALUES (?, ?, ?, ?)"));

            for (const auto& row : _slots) {
                pstmt->setInt(1, _uid);
                pstmt->setInt(2, static_cast<int32_t>(row.itemId));
                pstmt->setInt(3, row.slotIndex);
                pstmt->setInt(4, row.quantity);
                pstmt->executeUpdate();
            }
        }

        pConn->commit();
        pConn->setAutoCommit(true);
        return true;

    } catch (const sql::SQLException& e) {
        std::cerr << "[NotifyPlayerLeft] DB 반영 실패 (uid=" << _uid << "): " << e.what() << std::endl;
        try {
            pConn->rollback();
            pConn->setAutoCommit(true);
        } catch (const sql::SQLException&) {
        }
        return false;
    }
}

void NotifyPlayerLeftRequest::Execute() {
    // MySQL 먼저, 락 해제 나중 (fail-closed). 순서를 바꾸면 반영 실패한 인벤토리로 새 매치가 시작된다.
    bool applied = false;
    for (int attempt = 0; attempt < DB_RETRY_COUNT && applied == false; ++attempt)
        applied = ApplyInventoryToDb();

    if (applied == false) {
        std::cerr << "[NotifyPlayerLeft] DB 반영 최종 실패 — 락은 해제한다. uid=" << _uid
                  << ", reason=" << _leaveReason << ", slots=" << _slots.size() << std::endl;
        for (const auto& row : _slots) {
            std::cerr << "    itemId=" << row.itemId
                      << " slotIndex=" << row.slotIndex
                      << " quantity=" << row.quantity << std::endl;
        }
    }

    try {
        pRedis->del("active_match:" + std::to_string(_uid));
        std::cout << "[NotifyPlayerLeft] 처리 완료 (uid=" << _uid
                  << ", reason=" << _leaveReason
                  << ", dbApplied=" << (applied ? "true" : "false") << ")" << std::endl;
    } catch (const sw::redis::Error& e) {
        std::cerr << "[NotifyPlayerLeft] active_match 해제 실패 (uid=" << _uid << "): "
                  << e.what() << std::endl;
    }
}

void NotifyPlayerLeftRequest::ReturnToPool() {
    _slots.clear();
    _uid = 0;
    _leaveReason = 0;
    _gameProcessFd = -1;

    ObjectPool<NotifyPlayerLeftRequest>::Release(this);
}

void UpdateEntryTokenRequest::ReturnToPool() {
    _ticketIds.clear(); 
    _sessionIds.clear();
    _tokens.clear();
    _securityKeys.clear();
    _gameProcessFd = -1;

    ObjectPool<UpdateEntryTokenRequest>::Release(this);
}