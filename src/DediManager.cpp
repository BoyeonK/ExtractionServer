#include "DediManager.h"

#include "PacketHandler.h"

DediManager::DediManager() {
    _matchmakers.reserve(MapType::MAP_MAX);
    for (int i = 0; i < MapType::MAP_MAX; i++) {
        _matchmakers.emplace_back(i, MAX_AGRESSION);
    }
}

bool DediManager::AddSingleMatchTicket(MatchTicket* pTicket) {
    int32_t mid = pTicket->mapId;

    if (mid >= 0 && mid < MapType::MAP_MAX) {
        std::cout << "매치 테스트 4 - O : 맵 id = " << mid << "에 대기열 추가 요청 , ticket : " << pTicket->ticketId << std::endl;
        _matchmakers[mid].AddSingleMatchTicket(pTicket);
        return true;
    }
    else 
        return false;
}

int DediManager::SpawnSingleServer() {
    // OPTION: waitpid 가 없어 죽은 자식이 좀비로 남는다. 연결 전에 죽는 경우
    //   (execl·InitUDP 실패)는 소켓 EOF 도 오지 않아 이쪽으로만 알 수 있다
    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "C4-1 - X : 프로세스 생성(fork) 실패!" << std::endl;
        return -1;
    }
    // 자식의 초기화가 아래 M2DSession 생성보다 빠를 일은 없다고 보고 넘어간다
    else if (pid == 0) {
        execl("./LinuxServerTest", "./LinuxServerTest", "--dedicated", (char*)nullptr);
        std::cerr << "C4-1 - X : exec 실패! (바이너리 경로 확인 필요)" << std::endl;
        exit(1);
    } 
    else {
        M2DSession* pSession = new M2DSession(pid, IORing);
        _dediSessions[pid] = pSession;
        std::cout << "C4-1 - OK : 데디케이티드 프로세스 띄움 - PID: " << pid << std::endl;
        return pid;
    }
    return -1;
}

void DediManager::OnAcceptDedi(int DediIPCsockFd, M2DTempSession* pTempSession) {
    _tempSessions[DediIPCsockFd] = pTempSession;
    pTempSession->Recv();
}

bool DediManager::FinalizeConnection(int pid, int fd) {
    auto itTemp = _tempSessions.find(fd);
    auto itDedi = _dediSessions.find(pid);

    if (itTemp != _tempSessions.end() && itDedi != _dediSessions.end()) {
        M2DTempSession* pTemp = itTemp->second;
        M2DSession* pReal = itDedi->second;

        pReal->BindSocket(fd);
        _sessionFd2pid[fd] = pid;
        
        _tempSessions.erase(itTemp);
        //delete pTemp;

        std::cout << "C4-2 : OK PID = " << pid << " 연결 및 인증 완료!" << std::endl;
        return true;
    }
    
    std::cerr << "C4-2 : X 인증 실패: 존재하지 않는 PID(" << pid << ") 또는 FD" << std::endl;
    return false;
}

bool DediManager::DistributePlayerGroup(TicketVector& ticketVec) {
    if (FindAvailableSessionAndDistributePlayerGroup(ticketVec) == true)
        return true;

    //TODO : 새로 만들어서 할당
    int pid = SpawnSingleServer();

    if (pid == -1) {
        return false;
    }

    return DistributePlayerGroup(ticketVec, pid);
}

void DediManager::MatchMake() {
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastTimeMatchStarted).count();

    if (elapsedMs >= 4000) {
        _lastTimeMatchStarted = now;

        for (auto& matchmaker : _matchmakers) {
            matchmaker.StartMatchMake();
        }
    }
}

void DediManager::BindClientIpToSession(IPC_Protocol::H2M2DBindClientIpToSession& pkt) {
    if (pRedis == nullptr) {
        return;
    }

    try {
        const std::string& tokenKey = pkt.token();
        auto optFdStr = pRedis->hget(tokenKey, "fd");

        if (!optFdStr) {
            std::cerr << "Redis에 Token(" << tokenKey << ")이 없거나 'fd' 필드가 존재하지 않습니다.\n";
            return;
        }

        int dediFd = std::stoi(*optFdStr); 

        auto pidIt = _sessionFd2pid.find(dediFd);
        if (pidIt == _sessionFd2pid.end()) {
            std::cerr << "FD(" << dediFd << ")에 매핑된 PID를 찾을 수 없습니다.\n";
            return;
        }
        int pid = pidIt->second;
        
        auto sessionIt = _dediSessions.find(pid);
        if (sessionIt == _dediSessions.end()) {
            std::cerr << "PID(" << pid << ")에 해당하는 Dedi 세션을 찾을 수 없습니다.\n";
            return;
        }

        M2DSession* pDediSession = sessionIt->second;
        SendBuffer* sendBuffer = PacketHandler::MakeSendBuffer(pkt);
        pDediSession->Send(sendBuffer);
        std::cout << "매치 테스트 11-3 : [메인프로세스] HTTPS서버의 IPC요청에 의해 토큰과 IP를 DediServer에 전송 및 사용한 Redis Field파기" << std::endl;

        auto optTicketStr = pRedis->hget(tokenKey, "ticket");
        if (optTicketStr) {
            std::string ticketKey = *optTicketStr;
            pRedis->del({tokenKey, ticketKey});
        } else {
            pRedis->del(tokenKey);
        }

    } catch (const sw::redis::Error& e) {
        std::cerr << "BindClientIpToSession 실패: " << e.what() << '\n';
    } catch (const std::exception& e) {
        std::cerr << "데이터 변환 실패: " << e.what() << '\n';
    }
}

bool DediManager::FindAvailableSessionAndDistributePlayerGroup(TicketVector& ticketVec) {
    for (const auto& [pid, pSession] : _dediSessions) {
        if (pSession->AllocatePlayers(ticketVec)) {
            return true;
        }
    }
    return false;
}

bool DediManager::DistributePlayerGroup(TicketVector& ticketVec, int pid) {
    auto it = _dediSessions.find(pid);
    
    if (it != _dediSessions.end()) {
        return it->second->AllocatePlayers(ticketVec);
    } else {
        std::cerr << "DediManager - DistributePlayersGroup 존재하지 않는 세션 PID: " << pid << std::endl;
    }
    return false;
}