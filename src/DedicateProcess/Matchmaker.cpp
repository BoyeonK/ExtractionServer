#include "Matchmaker.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <iostream>
#include <sw/redis++/redis++.h>
#include "../GlobalVariable.h"
#include "../ObjectPool.h"
#include "DediManager.h"

// ipc를 통해 받은 ticket에 해당하는 구조체를 대기열에 추가할 함수
// 현재 매치메이킹 프로세스(메인 프로세스)는 싱글스레드로 동작할 예정이므로 mutex를 포함한 동기화 사용 X
void MatchMaker::AddSingleMatchTicket(MatchTicket* pTicket) {
    std::cout << "매치 테스트 5 - O : 티켓 대기열에 추가됨"  << std::endl;
    _ticketsToAdd.emplace_back(pTicket);
}

void MatchMaker::AddNewMatchTickets() {
    // 1. _ticketsToAdd를 먼저 들어온 친구가 앞에 오도록 정렬
    // 2. 정렬된 _ticketsToAdd를 이 시점에 _timeSortedTicketVector와 병합
    // 3. 앞에서부터 순회하면서 agression을 읽고, _bucket[agression]에 뒤에서부터 꽂음
        // (_bucket의 맨 뒤가 _ticketsToAdd의 맨 앞보다 먼저 온 친구라는것을 보장 해 줄 예정)

    if (_ticketsToAdd.empty()) return;

    std::stable_sort(_ticketsToAdd.begin(), _ticketsToAdd.end(), 
        [](const MatchTicket* a, const MatchTicket* b) {
            return a->startTime < b->startTime;
        }
    );

    for (MatchTicket* ticket : _ticketsToAdd) {
        _bucket[ticket->aggression].push_back(ticket);
    }

    // 1-2. 정렬된 _ticketsToAdd를 _timeSortedTicketVector와 병합
    TicketVector newMainQueue;
    newMainQueue.reserve(_timeSortedTicketVector.size() + _ticketsToAdd.size()); // 메모리 재할당 방지

    std::merge(
        _timeSortedTicketVector.begin(), _timeSortedTicketVector.end(),
        _ticketsToAdd.begin(), _ticketsToAdd.end(),
        std::back_inserter(newMainQueue),
        [](const MatchTicket* a, const MatchTicket* b) {
            return a->startTime < b->startTime;
        }
    );

    _timeSortedTicketVector = std::move(newMainQueue);
    _ticketsToAdd.clear();
}

void MatchMaker::ReleaseAndDeleteTickets() {
    auto shouldRemove = [](const MatchTicket* t) { return (t->isMatched || t->isValid == false); };

    for (MatchTicket* ticket : _timeSortedTicketVector) {
        if (ticket->isMatched) {
            _ticketsToRelease.push_back(ticket);
        } else if (ticket->isValid == false) {
            _ticketsToDelete.push_back(ticket);
        }
    }

    _timeSortedTicketVector.erase(
        std::remove_if(_timeSortedTicketVector.begin(), _timeSortedTicketVector.end(), shouldRemove),
        _timeSortedTicketVector.end()
    );

    for (TicketVector& bucketVec : _bucket) {
        if (bucketVec.empty()) continue;

        bucketVec.erase(
            std::remove_if(bucketVec.begin(), bucketVec.end(), shouldRemove),
            bucketVec.end()
        );
    }

    for (MatchTicket* pTicket : _ticketsToRelease) {
        if (pTicket != nullptr) {
            ObjectPool<MatchTicket>::Release(pTicket);
        }
    }

    _ticketsToRelease.clear();

    if (_ticketsToDelete.empty()) return;
    std::vector<std::string> keysToDelete;
    keysToDelete.reserve(_ticketsToDelete.size());

    for (MatchTicket* pTicket : _ticketsToDelete) {
        if (pTicket != nullptr) {
            keysToDelete.push_back(std::move(pTicket->ticketId));

            ObjectPool<MatchTicket>::Release(pTicket);
        }
    }

    if (!keysToDelete.empty() && pRedis != nullptr) {
        try {
            pRedis->del(keysToDelete.begin(), keysToDelete.end());
        } catch (const sw::redis::Error& e) {
            std::cerr << "대기열 처리 중 에러 발생 - Ticket소거 과정: " << e.what() << std::endl;
        }
    }

    _ticketsToDelete.clear();
}

void MatchMaker::FindMatchGroup() {
    _matchedGroups.clear();
    std::fill(_pivotMemorizationFlags.begin(), _pivotMemorizationFlags.end(), false);

    for (MatchTicket* pivot : _timeSortedTicketVector) {
        if (pivot->isMatched) continue;

        int pivotAggr = pivot->aggression;

        if (_pivotMemorizationFlags[pivotAggr]) continue;

        int waitTime = pivot->GetWaitTimeSeconds();
        int allowedDiff = 0; 
        int targetMinPlayers = 4;

        // TODO : 테스트를 위해서 극단적인 값을 넣어둠. 나중에 변경 필요        
        if (waitTime >= 8) {
            allowedDiff = 1;
            targetMinPlayers = 1;
        } else if (waitTime >= 40) {
            allowedDiff = 1;
            targetMinPlayers = 2;
        } else if (waitTime >= 20) {
            allowedDiff = 0;
            targetMinPlayers = 3;
        }

        TicketVector matchedGroup;
        matchedGroup.reserve(4);
        matchedGroup.push_back(pivot);

        int searchMin = std::max(0, pivotAggr - allowedDiff);
        int searchMax = std::min(_maxAgression, pivotAggr + allowedDiff);

        int searchRange = 0;
        while (searchRange <= allowedDiff) {
            if (searchRange == 0) {
                for (MatchTicket* candidate : _bucket[pivotAggr]) {
                    if (matchedGroup.size() == 4) break;
                    if (candidate == pivot || candidate->isMatched) continue;
                    matchedGroup.push_back(candidate);
                }
            } else {
                int uaggr = pivotAggr + searchRange;
                int daggr = pivotAggr - searchRange;

                if (matchedGroup.size() < 4 && uaggr <= searchMax) {
                    for (MatchTicket* candidate : _bucket[uaggr]) {
                        if (matchedGroup.size() == 4) break;
                        if (candidate->isMatched) continue;
                        matchedGroup.push_back(candidate);
                    }
                }              

                if (matchedGroup.size() < 4 && daggr >= searchMin) {
                    for (MatchTicket* candidate : _bucket[daggr]) { 
                        if (matchedGroup.size() == 4) break;
                        if (candidate->isMatched) continue;
                        matchedGroup.push_back(candidate);
                    }
                }
            }   
            
            if (matchedGroup.size() == 4) break;
            searchRange++;
        }

        if (matchedGroup.size() >= targetMinPlayers) {
            for (MatchTicket* member : matchedGroup)
                member->isMatched = true;

            _matchedGroups.push_back(std::move(matchedGroup));          
        } else {
            _pivotMemorizationFlags[pivotAggr] = true;
        }
    }
}

bool MatchMaker::VerifyAndSetMatchStatus(const TicketVector& matchedGroup) {
    if (matchedGroup.empty() || pRedis == nullptr) return false;

    std::string luaScript = R"(
        for i = 1, #KEYS do
            local status = redis.call('HGET', KEYS[i], 'status')
            if status ~= 'WAITING' then
                return 0
            end
        end
        
        for i = 1, #KEYS do
            redis.call('HSET', KEYS[i], 'status', ARGV[1])
        end
        return 1
    )";

    std::vector<std::string> keys;
    keys.reserve(matchedGroup.size());
    for (MatchTicket* ticket : matchedGroup) {
        keys.push_back(ticket->ticketId); 
    }

    std::vector<std::string> args = {"INPROGRESS"}; 

    try {
        long long result = pRedis->eval<long long>(
            luaScript, 
            keys.begin(), keys.end(), 
            args.begin(), args.end()
        );
        
        return result == 1; // 1이면 전원 성공, 0이면 누군가 취소함
        
    } catch (const sw::redis::Error& e) {
        std::cerr << "[Redis Error] Lua 스크립트 실행 실패: " << e.what() << std::endl;
        return false;
    }
}

void MatchMaker::StartMatchMakeInternal() {
    for (auto& ticketVec : _matchedGroups) {
        bool isMatchValid = VerifyAndSetMatchStatus(ticketVec);
        if (isMatchValid) {
            if (pDediManager->DistributePlayerGroup(ticketVec)) {
                std::cout<< "매치 테스트 6 - DediProcess에 매칭 완료된 그룹에 대한 정보 IPC 전송" << std::endl;
            } else {
                auto pipe = pRedis->pipeline();
                for (const auto& ticket : ticketVec) {
                    pipe.hset(ticket->ticketId, "status", "WAITING");
                    ticket->isMatched = false;
                }
                pipe.exec();
            }
        } else {
            for (auto& ticket : ticketVec) {
                auto statusOpt = pRedis->hget(ticket->ticketId, "status");

                if (statusOpt && *statusOpt == "WAITING") {
                    ticket->isMatched = false;
                } else {
                    // 아마 HTTP서버 선에서 취소처리 되었거나, 버그가 발생한 경우.
                    ticket->isValid = false;
                }
            }
        }       
    }
}

void MatchMaker::StartMatchMake() {
    AddNewMatchTickets();
    FindMatchGroup();
    StartMatchMakeInternal();
    ReleaseAndDeleteTickets();
}
