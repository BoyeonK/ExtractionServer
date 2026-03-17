// 1. 커널이 데이터를 채워줄 "바구니" (Session이 들고 있는 버퍼)
char session_buffer[1024]; 

// 2. 읽기 요청 등록 (SQE 작성)
void request_read(int fd, struct io_uring *ring) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);

    // [중요] 커널에게 "이 fd에서 데이터를 읽어서, 이 주소(session_buffer)에 담아줘"라고 예약합니다.
    io_uring_prep_read(sqe, fd, session_buffer, sizeof(session_buffer), 0);

    // [중요] 나중에 이 작업이 끝났을 때 누군지 알아보기 위해 '이름표(Task)'를 붙입니다.
    io_uring_sqe_set_data(sqe, my_read_task_ptr);

    io_uring_submit(ring);
}

// 3. 완료 확인 (CQE 확인)
void check_completion(struct io_uring *ring) {
    struct io_uring_cqe *cqe;
    
    // 완료된 작업이 있는지 봅니다.
    if (io_uring_peek_cqe(ring, &cqe) == 0) {
        // [결과물] cqe->res에 "실제로 몇 바이트를 읽었는지" 숫자가 들어있습니다.
        int bytes_read = cqe->res;

        if (bytes_read > 0) {
            // [데이터 위치] 이미 session_buffer에 데이터가 들어와 있습니다!
            // 우리는 그냥 session_buffer를 읽기만 하면 됩니다.
            printf("받은 데이터: %s\n", session_buffer);
        }

        io_uring_cqe_seen(ring, cqe);
    }
}



// 이 부분을 옮겨야됨 지금
bool MakeRoomForThisGroup(std::vector<std::string>& ticketIds) {
    if (ticketIds.empty() || pRedis == nullptr) return false;
    auto pipe = pRedis->pipeline();
    for (const auto& ticketId : ticketIds) {
        pipe.hmget(ticketId, {"mapId", "status"});
    }
    auto replies = pipe.exec();

    std::string expectedMapId = "";
    bool isGroupValid = true;

    for (int i = 0; i < replies.size(); ++i) {
        // hmget의 결과 std::vector<OptionalString>
        auto fields = replies.get<std::vector<sw::redis::OptionalString>>(i);
        
        if (fields.size() < 2 || !fields[0] || !fields[1]) {
            std::cerr << "매치 테스트 7 - X : MapID나 Status상태 확인 필요 (없는 것 같음)" << ticketIds[i] << std::endl;
            isGroupValid = false;
            break;
        }

        std::string mapId = *(fields[0]);
        std::string status = *(fields[1]);

        if (i == 0) {
            expectedMapId = mapId;
        } 
        else if (mapId != expectedMapId) {
            std::cerr << "매치 테스트 7 - X : 그룹 내 Map ID 불일치 (" << expectedMapId << " vs " << mapId << ")" << std::endl;
            isGroupValid = false;
            break;
        }

        if (status != "INPROGRESS") {
            std::cerr << "매치 테스트 7 - X : INPROGRESS가 아닌 상태 발견 (" << status << ")" << std::endl;
            isGroupValid = false;
            break;
        }
    }

    if (isGroupValid == false) {
        auto failPipe = pRedis->pipeline();
        for (const auto& ticketId : ticketIds) {
            failPipe.hset(ticketId, "status", "FAILED");
        }
        failPipe.exec();
        return false; 
    }

    // TODO : IP주소 환경변수에 적어놓기
    // std::string publicIp = GetPublicIP(); 
    std::string publicIp = "127.0.0.1";
    auto successPipe = pRedis->pipeline();
    
    static int32_t roomId = 0;
    roomId++;

    for (const auto& ticketId : ticketIds) {
        successPipe.hset(ticketId, "udpServerIp", publicIp);
        successPipe.hset(ticketId, "udpServerPort", std::to_string(_udpPort));
        _ticketToRoomId[ticketId] = roomId; //혹시 중복이면 덮어쓰기
    }
    successPipe.exec();

    GameRoom* newRoom = ObjectPool<GameRoom>::Acquire(std::stoi(expectedMapId), ticketIds);
    _gameRooms.insert({roomId, newRoom});

    std::cout << "매치 테스트 7 - O : 매치 그룹에 대한 Room 할당 완료 및 Redis Ticket 업데이트" << std::endl;

    return true;
}