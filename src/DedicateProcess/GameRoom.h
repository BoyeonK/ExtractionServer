#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "Player.h" //추후 .cpp파일로 이전, 아마 GameRoom.h와 순환참조될거임

class GameRoom {
public:
    GameRoom(int mapId, std::vector<std::string> ticketIds) : _mapId(mapId), _ticketIds(std::move(ticketIds)) { }

    void Clear() {

    }

private:
    void Initialize() {
        
    }


private:
    int32_t _mapId;
    std::vector<std::string> _ticketIds;
    std::unordered_map<std::string, Player*> _players;
};