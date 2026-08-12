#include "HTTPserver.h"
#include <limits.h>
#include <unistd.h>
#include <string>
#include <sys/prctl.h>
#include <iostream>    
#include <csignal>
#include <cstdlib>

using namespace std;

static string getExecutablePath() {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    
    if (len == -1) {
        perror("readlink failed");
        return "";
    }
    
    buf[len] = '\0';
    string path(buf);
    
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash == string::npos) {
        return ".";
    }
    
    return path.substr(0, lastSlash);
}

bool launchNode(pid_t& pid) {
    pid = fork();

    if (pid < 0) {
        perror("C3-1 : X : Fork failed");
        return false;
    }

    if (pid == 0) { 
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        string lobbyPath = getExecutablePath() + "/../HTTPServer/index.js";
        execlp("node", "node", lobbyPath.c_str(), NULL);
        perror("execlp failed");
        _exit(1);
    } 
    else {
        std::cout << "C3-1 - OK : PID: " << pid << std::endl;
        return true;
    }
}