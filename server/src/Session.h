#pragma once
#pragma once
#include <cstdint>
#include <string>
#include <chrono>
#include <netinet/in.h>

enum class SessionState {
    CONNECTING,
    ACTIVE,
    DISCONNECTING,
};

struct PlayerState {
    float x = 0, y = 1, z = 0;
    float rotX = 0, rotY = 0;
    float hp = 100.0f;
    uint8_t flags = 0;
    bool alive = false;
};

struct Session {
    uint32_t    id;
    sockaddr_in addr;
    SessionState state = SessionState::CONNECTING;

    char        playerName[32] = {};
    PlayerState player;

    uint32_t    lastSequence = 0;
    std::chrono::steady_clock::time_point lastHeartbeat;

    // 통계 (RateLimiter와 별개로 세션 단위 집계)
    uint64_t totalPackets = 0;
    uint64_t totalBytes = 0;
};
