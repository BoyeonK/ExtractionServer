#pragma once

#include <string>
#include <unordered_map>
#include <netinet/in.h>
#include <chrono>

class Player {

private:
    int32_t _uid;
    std::string _ticket;
    std::string _entryToken;
    std::string _nickname;
    // TODO : Items부분이 확정되면 고정 크기의 vector로 교체하기
    std::unordered_map<int32_t, int32_t> _inventory; // 아이템 개수 확인용, key = item_id, value = quantity
    
    sockaddr_in _clientAddr = {}; 
    std::chrono::time_point<std::chrono::steady_clock> _lastRecvTime;
    // TODO : 플레이어의 좌표, 체력 등 생각나는대로 추가. 여기서부터는 실제 클라이언트와 연결하고 아다리 맞춰가야 할듯
};