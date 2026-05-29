#include "SessionManager.h"
#include <iostream>
#include <chrono>

SessionManager& SessionManager::Get() {
    static SessionManager instance;
    return instance;
}

uint32_t SessionManager::HashAddr(const sockaddr_in& addr) {
    return addr.sin_addr.s_addr ^ ((uint32_t)addr.sin_port << 16);
}

Session* SessionManager::CreateSession(const sockaddr_in& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t hash = HashAddr(addr);

    // 이미 있으면 기존 반환
    auto it = addrToId_.find(hash);
    if (it != addrToId_.end()) {
        return &byId_[it->second];
    }

    uint32_t id = GenerateId();
    Session& s = byId_[id];
    s.id = id;
    s.addr = addr;
    s.state = SessionState::CONNECTING;
    s.lastHeartbeat = std::chrono::steady_clock::now();
    addrToId_[hash] = id;

    std::cout << "[Session] Created #" << id << std::endl;
    return &s;
}

void SessionManager::RemoveSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = byId_.find(sessionId);
    if (it == byId_.end()) return;

    addrToId_.erase(HashAddr(it->second.addr));
    byId_.erase(it);
    std::cout << "[Session] Removed #" << sessionId << std::endl;
}

void SessionManager::RemoveSession(const sockaddr_in& addr) {
    uint32_t hash = HashAddr(addr);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = addrToId_.find(hash);
    if (it == addrToId_.end()) return;
    uint32_t id = it->second;
    addrToId_.erase(it);
    byId_.erase(id);
}

Session* SessionManager::FindById(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = byId_.find(sessionId);
    return it != byId_.end() ? &it->second : nullptr;
}

Session* SessionManager::FindByAddr(const sockaddr_in& addr) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = addrToId_.find(HashAddr(addr));
    if (it == addrToId_.end()) return nullptr;
    return &byId_[it->second];
}

void SessionManager::ForEach(std::function<void(Session&)> fn) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, session] : byId_) fn(session);
}

void SessionManager::PurgeTimedOut(int timeoutSeconds) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = byId_.begin(); it != byId_.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.lastHeartbeat).count();
        if (elapsed > timeoutSeconds) {
            std::cout << "[Session] Timeout: #" << it->second.id << std::endl;
            addrToId_.erase(HashAddr(it->second.addr));
            it = byId_.erase(it);
        }
        else {
            ++it;
        }
    }
}

int SessionManager::Count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return (int)byId_.size();
}

uint32_t SessionManager::GenerateId() {
    return nextId_++;
}
