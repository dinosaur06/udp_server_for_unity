#include <vector>
#include <chrono>
#include "GameLogicHandler.h"
#include "SessionManager.h"
#include <cstring>
#include <iostream>
#include <sys/socket.h>

GameLogicHandler& GameLogicHandler::Get() {
    static GameLogicHandler instance;
    return instance;
}

// ── 전송 헬퍼 ────────────────────────────────────────────────────────────────

void GameLogicHandler::SendTo(int sock, const sockaddr_in& addr,
    PacketType type, uint32_t sessionId,
    const void* payload, size_t payloadLen) {
    std::vector<char> buf(sizeof(PacketHeader) + payloadLen);
    auto* hdr = reinterpret_cast<PacketHeader*>(buf.data());
    hdr->magic = MAGIC;
    hdr->type = type;
    hdr->length = (uint16_t)payloadLen;
    hdr->sessionId = sessionId;
    hdr->sequence = ++seqCounter_;
    if (payloadLen > 0) std::memcpy(buf.data() + sizeof(PacketHeader), payload, payloadLen);
    sendto(sock, buf.data(), (int)buf.size(), 0,
        (sockaddr*)&addr, sizeof(addr));
}

void GameLogicHandler::Broadcast(int sock, PacketType type,
    const void* payload, size_t payloadLen,
    uint32_t excludeId) {
    SessionManager::Get().ForEach([&](Session& s) {
        if (s.state == SessionState::ACTIVE && s.id != excludeId)
            SendTo(sock, s.addr, type, s.id, payload, payloadLen);
        });
}

// ── 패킷 핸들러 ──────────────────────────────────────────────────────────────

void GameLogicHandler::HandleConnectReq(const PacketHeader& hdr,
    const char* payload, int sock,
    const sockaddr_in& from) {
    auto* req = reinterpret_cast<const ConnectReqPayload*>(payload);

    // 버전 체크
    ConnectAckPayload ack{};
    if (req->version < 1) {
        ack.accepted = 0;
        std::strncpy(ack.reason, "Version mismatch", sizeof(ack.reason));
        SendTo(sock, from, PacketType::CONNECT_ACK, 0, &ack, sizeof(ack));
        return;
    }

    Session* session = SessionManager::Get().CreateSession(from);
    std::strncpy(session->playerName, req->playerName, sizeof(session->playerName) - 1);
    session->playerName[sizeof(session->playerName) - 1] = '\0';
    session->state = SessionState::ACTIVE;

    // 스폰 위치 (고정 or 랜덤으로 확장 가능)
    session->player.x = 0; session->player.y = 1; session->player.z = 0;
    session->player.hp = 100.0f;
    session->player.alive = true;

    ack.accepted = 1;
    ack.sessionId = session->id;
    SendTo(sock, from, PacketType::CONNECT_ACK, session->id, &ack, sizeof(ack));

    // 다른 플레이어들에게 스폰 알림
    PlayerSpawnPayload spawn{};
    spawn.x = session->player.x;
    spawn.y = session->player.y;
    spawn.z = session->player.z;
    Broadcast(sock, PacketType::PLAYER_SPAWN, &spawn, sizeof(spawn), session->id);

    std::cout << "[Game] Player joined: " << session->playerName
        << " (session #" << session->id << ")" << std::endl;

    BroadcastWorldSnapshot(sock);
}

void GameLogicHandler::HandleDisconnect(const PacketHeader& hdr,
    const char* payload, int sock,
    const sockaddr_in& from) {
    Session* s = SessionManager::Get().FindByAddr(from);
    if (!s) return;
    std::cout << "[Game] Player disconnected: " << s->playerName << std::endl;
    SessionManager::Get().RemoveSession(s->id);
    BroadcastWorldSnapshot(sock);
}

void GameLogicHandler::HandleHeartbeat(const PacketHeader& hdr,
    const char* payload, int sock,
    const sockaddr_in& from) {
    Session* s = SessionManager::Get().FindByAddr(from);
    if (!s) return;
    s->lastHeartbeat = std::chrono::steady_clock::now();
    SendTo(sock, from, PacketType::HEARTBEAT_ACK, s->id, nullptr, 0);
}

void GameLogicHandler::HandlePlayerMove(const PacketHeader& hdr,
    const char* payload, int sock,
    const sockaddr_in& from) {
    Session* s = SessionManager::Get().FindByAddr(from);
    if (!s || s->state != SessionState::ACTIVE) return;

    auto* move = reinterpret_cast<const PlayerMovePayload*>(payload);

    // 서버 권위(Server Authority) 검증: 이동 속도 제한
    constexpr float MAX_MOVE_DELTA = 10.0f;
    float dx = move->x - s->player.x;
    float dz = move->z - s->player.z;
    if (dx * dx + dz * dz > MAX_MOVE_DELTA * MAX_MOVE_DELTA) {
        std::cout << "[Anti-cheat] Teleport detected: session #" << s->id << std::endl;
        return; // 텔레포트 의심 → 무시
    }

    s->player.x = move->x;
    s->player.y = move->y;
    s->player.z = move->z;
    s->player.rotX = move->rotX;
    s->player.rotY = move->rotY;
    s->player.flags = move->flags;

    // 스냅샷은 Monitor 루프에서 주기적으로 브로드캐스트 (매 패킷마다 X)
}

void GameLogicHandler::HandlePlayerShoot(const PacketHeader& hdr,
    const char* payload, int sock,
    const sockaddr_in& from) {
    Session* shooter = SessionManager::Get().FindByAddr(from);
    if (!shooter || !shooter->player.alive) return;

    auto* shoot = reinterpret_cast<const PlayerShootPayload*>(payload);

    // 서버 사이드 히트 판정 (단순 구체 충돌, 추후 정밀화 가능)
    constexpr float HIT_RADIUS = 0.6f;
    SessionManager::Get().ForEach([&](Session& target) {
        if (target.id == shooter->id || !target.player.alive) return;

        // 레이-구체 교차 테스트
        float ox = shoot->originX - target.player.x;
        float oy = shoot->originY - target.player.y;
        float oz = shoot->originZ - target.player.z;
        float b = ox * shoot->dirX + oy * shoot->dirY + oz * shoot->dirZ;
        float c = ox * ox + oy * oy + oz * oz - HIT_RADIUS * HIT_RADIUS;
        if (b * b - c < 0) return; // 미스

        // 히트 처리
        float damage = (oy > 1.5f) ? 100.0f : 25.0f; // 헤드샷 여부
        target.player.hp -= damage;

        PlayerHitPayload hit{};
        hit.targetSessionId = target.id;
        hit.damage = damage;
        hit.hitZone = (oy > 1.5f) ? 1 : 0;

        Broadcast(sock, PacketType::PLAYER_HIT, &hit, sizeof(hit));

        if (target.player.hp <= 0) {
            target.player.alive = false;
            PlayerDeathPayload death{};
            death.victimSessionId = target.id;
            death.killerSessionId = shooter->id;
            death.weaponId = shoot->weaponId;
            Broadcast(sock, PacketType::PLAYER_DEATH, &death, sizeof(death));
            std::cout << "[Game] Kill: " << shooter->playerName
                << " → " << target.playerName << std::endl;
        }
        });
}

void GameLogicHandler::BroadcastWorldSnapshot(int sock) {
    WorldSnapshotPayload snap{};
    snap.playerCount = 0;

    SessionManager::Get().ForEach([&](Session& s) {
        if (s.state != SessionState::ACTIVE || snap.playerCount >= 16) return;
        auto& ps = snap.players[snap.playerCount++];
        ps.sessionId = s.id;
        ps.x = s.player.x;  ps.y = s.player.y;  ps.z = s.player.z;
        ps.rotX = s.player.rotX; ps.rotY = s.player.rotY;
        ps.hp = s.player.hp;
        ps.flags = s.player.flags;
        });

    Broadcast(sock, PacketType::WORLD_SNAPSHOT, &snap, sizeof(snap));
}
