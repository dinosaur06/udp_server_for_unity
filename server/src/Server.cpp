// Server.cpp
#include "Server.h"
#include "Protocol.h"
#include "GameLogicHandler.h"
#include "RateLimiter.h"
#include "Monitor.h"
#include "SessionManager.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

bool Server::Init(int port) {
    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ < 0) return false;

    // 논블로킹 설정
    int flags = fcntl(sock_, F_GETFL, 0);
    fcntl(sock_, F_SETFL, flags | O_NONBLOCK);

    // SO_REUSEADDR
    int opt = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Server] Bind failed on port " << port << std::endl;
        return false;
    }

    std::cout << "========================================\n"
        << "   Fortress Game Server\n"
        << "   Port: " << port << " | Mode: UDP + epoll\n"
        << "========================================\n";
    return true;
}

void Server::ProcessPacket(const char* buf, int len, const sockaddr_in& from) {
    if ((size_t)len < sizeof(PacketHeader)) return;

    auto* hdr = reinterpret_cast<const PacketHeader*>(buf);
    if (hdr->magic != MAGIC) return;

    // RateLimiter 체크
    uint32_t ip = from.sin_addr.s_addr;
    if (!RateLimiter::Get().Allow(ip)) {
        g_blockedCount++;
        return;
    }

    // 통계 갱신
    g_packetCount++;
    g_totalBytes += len;

    const char* payload = buf + sizeof(PacketHeader);
    auto& logic = GameLogicHandler::Get();

    switch (hdr->type) {
    case PacketType::CONNECT_REQ:   logic.HandleConnectReq(*hdr, payload, sock_, from); break;
    case PacketType::DISCONNECT:    logic.HandleDisconnect(*hdr, payload, sock_, from); break;
    case PacketType::HEARTBEAT:     logic.HandleHeartbeat(*hdr, payload, sock_, from); break;
    case PacketType::PLAYER_MOVE:   logic.HandlePlayerMove(*hdr, payload, sock_, from); break;
    case PacketType::PLAYER_SHOOT:  logic.HandlePlayerShoot(*hdr, payload, sock_, from); break;
    default: break;
    }
}

void Server::Run() {
    running_ = true;

    int epfd = epoll_create1(0);
    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = sock_;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sock_, &ev);

    epoll_event events[64];
    char buf[2048];

    StartMonitoring(sock_);

    while (running_) {
        int n = epoll_wait(epfd, events, 64, 100); // 100ms timeout
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd != sock_) continue;
            sockaddr_in from{};
            socklen_t fromLen = sizeof(from);
            int len = recvfrom(sock_, buf, sizeof(buf), 0,
                (sockaddr*)&from, &fromLen);
            if (len > 0) ProcessPacket(buf, len, from);
        }
    }

    close(epfd);
    StopMonitoring();
    close(sock_);
}

void Server::Stop() { running_ = false; }
