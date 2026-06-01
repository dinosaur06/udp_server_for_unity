#pragma once
#pragma once
#include "Protocol.h"
#include "Session.h"
#include <netinet/in.h>

class GameLogicHandler {
public:
    static GameLogicHandler& Get();

    void HandleConnectReq(const PacketHeader& hdr, const char* payload, int sock, const sockaddr_in& from);
    void HandleDisconnect(const PacketHeader& hdr, const char* payload, int sock, const sockaddr_in& from);
    void HandleHeartbeat(const PacketHeader& hdr, const char* payload, int sock, const sockaddr_in& from);
    void HandlePlayerMove(const PacketHeader& hdr, const char* payload, int sock, const sockaddr_in& from);
    void HandlePlayerShoot(const PacketHeader& hdr, const char* payload, int sock, const sockaddr_in& from);

    // 월드 스냅샷 생성 및 브로드캐스트
    void BroadcastWorldSnapshot(int sock);

private:
    GameLogicHandler() = default;

    // 헬퍼: 특정 세션에 패킷 전송
    void SendTo(int sock, const sockaddr_in& addr, PacketType type,
        uint32_t sessionId, const void* payload, size_t payloadLen);

    // 모든 ACTIVE 세션에 브로드캐스트
    void Broadcast(int sock, PacketType type,
        const void* payload, size_t payloadLen, uint32_t excludeId = 0);

    uint32_t seqCounter_ = 0;
};
