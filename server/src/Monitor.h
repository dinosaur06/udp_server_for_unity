#pragma once
#pragma once
#include <atomic>
#include <string>

extern std::atomic<int>       g_packetCount;
extern std::atomic<long long> g_totalBytes;
extern std::atomic<int>       g_blockedCount; // RateLimiter가 차단한 패킷 수

void StartMonitoring(int serverSock); // serverSock: 스냅샷 브로드캐스트에 사용
void StopMonitoring();
