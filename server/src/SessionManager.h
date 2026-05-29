#pragma once
#pragma once
#include "Session.h"
#include <unordered_map>
#include <mutex>
#include <vector>
#include <functional>

class SessionManager {
public:
    static SessionManager& Get();

    // 세션 생성/삭제
    Session* CreateSession(const sockaddr_in& addr);
    void     RemoveSession(uint32_t sessionId);
    void     RemoveSession(const sockaddr_in& addr);

    // 조회
    Session* FindById(uint32_t sessionId);
    Session* FindByAddr(const sockaddr_in& addr);

    // 전체 순회 (브로드캐스트 등)
    void ForEach(std::function<void(Session&)> fn);

    // 타임아웃된 세션 정리 (heartbeat 기준)
    void PurgeTimedOut(int timeoutSeconds = 10);

    int Count() const;

private:
    SessionManager() = default;
    uint32_t GenerateId();

    mutable std::mutex mutex_;
    std::unordered_map<uint32_t, Session>    byId_;
    std::unordered_map<uint32_t, uint32_t>   addrToId_; // addr hash → sessionId

    uint32_t nextId_ = 1;

    static uint32_t HashAddr(const sockaddr_in& addr);
};
