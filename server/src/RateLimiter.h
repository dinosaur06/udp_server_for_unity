#pragma once
#pragma once
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cstdint>

struct IpBucket {
    int     tokens;
    std::chrono::steady_clock::time_point lastRefill;
    int     violations = 0;       // 제한 초과 횟수
    bool    blocked = false;   // 완전 차단 여부
};

class RateLimiter {
public:
    static RateLimiter& Get();

    // true = 허용, false = 차단
    bool Allow(uint32_t ip);
    bool IsBlocked(uint32_t ip);
    void Unblock(uint32_t ip);

    // 설정
    void SetRate(int packetsPerSecond, int burstSize);

private:
    RateLimiter();
    void Refill(IpBucket& bucket);

    std::mutex mutex_;
    std::unordered_map<uint32_t, IpBucket> buckets_;

    int rate_ = 200;   // 허용 PPS (초당 패킷)
    int burst_ = 400;   // 버스트 허용량
    int blockThreshold_ = 5; // 위반 N회 → 완전 차단
};
