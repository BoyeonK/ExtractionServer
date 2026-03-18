#include "DediManager.h"

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
    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "C4-1 - X : 프로세스 생성(fork) 실패!" << std::endl;
        return -1;
    }
    // 이론상 else구문의 pSession의 생성이 execl으로 실행된 DedicateMain의 초기화과정보다 늦으면 에러 발생함.
    // 하지만,
    // M2DSession 객체 하나 띄우는게, 새로운 프로세스를 실행하고 환경변수 불러오고 io_uring객체 하나 만들고 redis연결하고 unix domain 소켓 만들고 connect요청한 다음 4byte짜리 헤더 뒤에 DediInitComplete패킷을 protobuf로 직렬화한 payload달고 send 하는거보다 늦을게 분명하기에 그냥 넘어감 ㅅㄱㄹ
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
            
        _tempSessions.erase(itTemp);

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
        std::cout << "매치 딸깍" << std::endl;
        _lastTimeMatchStarted = now;

        for (auto& matchmaker : _matchmakers) {
            matchmaker.StartMatchMake();
        }
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