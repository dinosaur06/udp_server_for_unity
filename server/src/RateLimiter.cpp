#include "RateLimiter.h"
#include <iostream>
#include <arpa/inet.h>

RateLimiter& RateLimiter::Get() {
    static RateLimiter instance;
    return instance;
}

RateLimiter::RateLimiter() {}

void RateLimiter::SetRate(int pps, int burst) {
    rate_ = pps;
    burst_ = burst;
}

void RateLimiter::Refill(IpBucket& b) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - b.lastRefill).count();
    int add = (int)(elapsed * rate_);
    if (add > 0) {
        b.tokens = std::min(b.tokens + add, burst_);
        b.lastRefill = now;
    }
}

bool RateLimiter::Allow(uint32_t ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& b = buckets_[ip];

    if (b.blocked) return false;

    // 첫 접근이면 초기화
    if (b.tokens == 0 && b.violations == 0) {
        b.tokens = burst_;
        b.lastRefill = std::chrono::steady_clock::now();
    }

    Refill(b);

    if (b.tokens > 0) {
        b.tokens--;
        return true;
    }

    // 버킷 고갈 → 위반
    b.violations++;
    if (b.violations >= blockThreshold_) {
        b.blocked = true;
        // IP를 human-readable로 변환해서 출력
        in_addr ipAddr;
        ipAddr.s_addr = ip;
        std::cout << "[RateLimit] BLOCKED: " << inet_ntoa(ipAddr)
            << " (violations: " << b.violations << ")" << std::endl;
    }
    return false;
}

bool RateLimiter::IsBlocked(uint32_t ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(ip);
    return it != buckets_.end() && it->second.blocked;
}

void RateLimiter::Unblock(uint32_t ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = buckets_.find(ip);
    if (it != buckets_.end()) {
        it->second.blocked = false;
        it->second.violations = 0;
        it->second.tokens = burst_;
    }
}
