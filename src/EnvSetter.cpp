#include "EnvSetter.h"

#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

void SetEnv() {
    std::ifstream envFile(".env");
    if (!envFile.is_open()) {
        std::cerr << ".env파일 읽기 에러. (Path: " << std::filesystem::current_path() << ")" << std::endl;
        return;
    }

    std::string line;
    while (std::getline(envFile, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos) {
            std::string key = line.substr(0, eqPos);
            std::string value = line.substr(eqPos + 1);

            key.erase(key.find_last_not_of(" \t") + 1);
            key.erase(0, key.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));

            if (value.length() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.substr(1, value.length() - 2);
            }

            if (setenv(key.c_str(), value.c_str(), 1) == 0) {
                //std::cout << "환경 변수 설정 성공: " << key << std::endl;
            } else {
                std::cerr << "환경 변수 설정 실패: " << key << std::endl;
            }
        }
    }
    envFile.close();
}