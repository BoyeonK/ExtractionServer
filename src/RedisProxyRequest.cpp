#include "RedisProxyRequest.h"
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
                    
                    // 루프백(127.0.0.1) 무시
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
void UpdateEntryTokenRequest::Execute(sw::redis::Redis* pRedis) {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            if (envIp != nullptr) {
                // Oracle Compute 환경
                std::cout << "[System] 환경변수 IP 감지: " << envIp << std::endl;
                return std::string(envIp);
            } else {
                // 로컬
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

void UpdateEntryTokenRequest::Execute(sw::redis::Redis* pRedis) {
    try {
        auto pipe = pRedis->pipeline();
        std::string portStr = std::to_string(_port);
        std::string fdStr = std::to_string(_gameProcessFd);
        
        static std::string serverIp = []() {
            const char* envIp = std::getenv("MY_INSTANCE_IP");
            if (envIp != nullptr) {
                // Oracle Compute 환경
                std::cout << "[System] 환경변수 IP 감지: " << envIp << std::endl;
                return std::string(envIp);
            } else {
                // 로컬
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

            // 락 값에 게임 시작 표식을 남긴다. 티켓 TTL(300초)이 게임보다 짧아 진행 중에
            // 티켓이 사라지는데, 그때 /cancel 이 "만료된 대기 티켓"으로 오해해 락을 풀어버리는 것을
            // 막는 유일한 단서다 (match.js 의 matchCancel 스크립트가 이 접두어를 검사한다).
            // TTL 을 다시 걸어주는 이유: SET 은 기존 TTL 을 지우므로, 이탈 통보가 유실됐을 때의
            // 백스톱이 함께 사라진다.
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

bool NotifyPlayerLeftRequest::ApplyInventoryToDb() {
    // 게스트(uid < 0)는 users 테이블에 행이 없다 (auth.js 가 guest_uid_counter 로 음수 발급).
    // user_inventory.uid 는 users(uid) 에 FK 가 걸려 있어 INSERT 가 FK 위반으로 실패하므로,
    // 반출물을 파기한다는 정책과 스키마가 같은 결론이다. 여기서 명시적으로 빠진다.
    if (_uid < 0) {
        std::cout << "[NotifyPlayerLeft] 게스트 이탈 — DB 반영 생략 (uid=" << _uid << ")" << std::endl;
        return true;
    }

    if (pMysql == nullptr) return false;

    sql::Connection* pConn = pMysql->Get();
    if (pConn == nullptr) return false;

    try {
        pConn->setAutoCommit(false);

        // 레이드 대상 범위만 지운다 — 창고(0~79)는 레이드에 들어가지 않으므로 건드리면 안 된다.
        {
            std::unique_ptr<sql::PreparedStatement> pstmt(
                pConn->prepareStatement(
                    "DELETE FROM user_inventory WHERE uid = ? AND slot_index >= ?"));
            pstmt->setInt(1, _uid);
            pstmt->setInt(2, INVENTORY_SLOT_INDEX_BEGIN);
            pstmt->executeUpdate();
        }

        // 사망·연결 끊김은 _slots 가 비어 있어 위 DELETE 만으로 "빈손"이 완성된다.
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
            // 연결이 이미 죽은 경우다. 다음 Get() 이 재연결하므로 여기서는 넘긴다.
        }
        return false;
    }
}

void NotifyPlayerLeftRequest::Execute(sw::redis::Redis* pRedis) {
    // MySQL 먼저, 락 해제 나중이다. 두 저장소를 한 트랜잭션으로 묶을 수 없으므로 순서가 정책이 된다 —
    // 락을 먼저 풀면 반영 실패 시 유저가 반영 안 된 인벤토리로 새 매치를 시작할 수 있다.
    bool applied = false;
    for (int attempt = 0; attempt < DB_RETRY_COUNT && applied == false; ++attempt)
        applied = ApplyInventoryToDb();   // 첫 시도가 죽은 연결로 실패하면 Get() 이 재연결한다

    if (applied == false) {
        // 여기서 락을 계속 잡아두면 그 유저는 영구히 매칭 불가가 된다. 레이드 하나 유실이
        // 영구 잠금보다 낫다고 보아 락은 풀고, 복구할 수 있도록 페이로드를 남긴다.
        std::cerr << "[NotifyPlayerLeft] DB 반영 최종 실패 — 락은 해제한다. uid=" << _uid
                  << ", reason=" << _leaveReason << ", slots=" << _slots.size() << std::endl;
        for (const auto& row : _slots) {
            std::cerr << "    itemId=" << row.itemId
                      << " slotIndex=" << row.slotIndex
                      << " quantity=" << row.quantity << std::endl;
        }
    }

    try {
        // 유저 단위 락이므로 개인 이탈 확정 시점에 푸는 것이 맞다.
        pRedis->del("active_match:" + std::to_string(_uid));
        std::cout << "[NotifyPlayerLeft] 처리 완료 (uid=" << _uid
                  << ", reason=" << _leaveReason
                  << ", dbApplied=" << (applied ? "true" : "false") << ")" << std::endl;
    } catch (const sw::redis::Error& e) {
        // 락이 남는다. TTL 이 없으므로 수동 해제가 필요하다.
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