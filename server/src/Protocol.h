#pragma once
#pragma once
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)

// ── 매직 넘버 ──────────────────────────────
constexpr uint32_t MAGIC = 0x46545253; // 'FTRS'

// ── 패킷 타입 ──────────────────────────────
enum class PacketType : uint16_t {
    // 연결 관리
    CONNECT_REQ = 1,
    CONNECT_ACK = 2,
    DISCONNECT = 3,
    HEARTBEAT = 4,
    HEARTBEAT_ACK = 5,

    // 게임 로직
    PLAYER_MOVE = 10,
    PLAYER_SHOOT = 11,
    PLAYER_HIT = 12,
    PLAYER_DEATH = 13,
    GAME_STATE = 14,
    PLAYER_SPAWN = 15,

    // 서버→클라 브로드캐스트
    WORLD_SNAPSHOT = 20,

    // 시스템
    SERVER_KICK = 99,
};

// ── 공통 헤더 (모든 패킷 앞에 붙음) ────────
struct PacketHeader {
    uint32_t magic;      // MAGIC
    PacketType type;
    uint16_t length;     // 헤더 제외 페이로드 길이
    uint32_t sessionId;  // 클라이언트 세션 ID (미연결=0)
    uint32_t sequence;   // 패킷 순서 번호
};

// ── 페이로드 정의 ──────────────────────────

struct ConnectReqPayload {
    char playerName[32];
    uint32_t version;    // 클라이언트 버전 (호환 체크용)
};

struct ConnectAckPayload {
    uint32_t sessionId;  // 서버가 발급한 세션 ID
    uint8_t  accepted;   // 1=허용, 0=거부
    char     reason[32]; // 거부 사유
};

struct PlayerMovePayload {
    float x, y, z;          // 위치
    float rotX, rotY;        // 회전 (pitch, yaw)
    uint8_t flags;           // 0x01=grounded, 0x02=jumping, 0x04=crouching
};

struct PlayerShootPayload {
    float originX, originY, originZ;  // 총구 위치
    float dirX, dirY, dirZ;           // 발사 방향 (normalized)
    uint16_t weaponId;
};

struct PlayerHitPayload {
    uint32_t targetSessionId;
    float    damage;
    uint8_t  hitZone;   // 0=body, 1=head, 2=limb
};

struct PlayerDeathPayload {
    uint32_t victimSessionId;
    uint32_t killerSessionId;
    uint16_t weaponId;
};

struct PlayerSpawnPayload {
    float x, y, z;
    float yaw;
};

// 월드 스냅샷 - 서버가 주기적으로 모든 플레이어 상태를 브로드캐스트
struct PlayerSnapshot {
    uint32_t sessionId;
    float x, y, z;
    float rotX, rotY;
    float hp;
    uint8_t flags;
};

struct WorldSnapshotPayload {
    uint8_t playerCount;
    PlayerSnapshot players[16]; // 최대 16명
};

struct ServerKickPayload {
    char reason[64];
};

#pragma pack(pop)
