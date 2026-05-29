#pragma once
// Server.h
#pragma once

class Server {
public:
    bool Init(int port);
    void Run();   // epoll 메인 루프
    void Stop();
private:
    int sock_ = -1;
    bool running_ = false;

    void ProcessPacket(const char* buf, int len, const sockaddr_in& from);
};
