#include "Monitor.h"
#include "SessionManager.h"
#include "GameLogicHandler.h"
#include "RateLimiter.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
using namespace std;

atomic<int>       g_packetCount(0);
atomic<long long> g_totalBytes(0);
atomic<int>       g_blockedCount(0);

static bool    g_running = false;
static int     g_sock = -1;

struct Stats {
    int    second;
    int    pps;
    int    blocked;
    double kbps;
    int    sessions;
};
static vector<Stats> g_history;

static void MonitorLoop() {
    int elapsed = 0;
    while (g_running) {
        this_thread::sleep_for(chrono::seconds(1));

        int   pps = g_packetCount.exchange(0);
        int   blocked = g_blockedCount.exchange(0);
        long long bytes = g_totalBytes.exchange(0);
        int   sessions = SessionManager::Get().Count();

        g_history.push_back({ elapsed, pps, blocked, bytes / 1024.0, sessions });

        // 실시간 경고
        if (pps > 1000)
            cout << "\a[WARN] High PPS at " << elapsed << "s: " << pps << " pps" << endl;
        if (blocked > 100)
            cout << "\a[WARN] High block rate at " << elapsed << "s: " << blocked << " blocked" << endl;

        // 10초마다 세션 타임아웃 정리
        if (elapsed % 10 == 0)
            SessionManager::Get().PurgeTimedOut(10);

        // 2초마다 월드 스냅샷 브로드캐스트
        if (elapsed % 2 == 0 && g_sock != -1)
            GameLogicHandler::Get().BroadcastWorldSnapshot(g_sock); // friend or expose

        elapsed++;
    }
}

static void PrintFinalReport() {
    long long totalPkts = 0;
    int maxPPS = 0, maxPPSAt = 0;
    int totalBlocked = 0;

    for (auto& s : g_history) {
        totalPkts += s.pps;
        totalBlocked += s.blocked;
        if (s.pps > maxPPS) { maxPPS = s.pps; maxPPSAt = s.second; }
    }

    cout << "\n" << string(45, '=') << "\n"
        << "       [ Fortress Server Final Report ]\n"
        << string(45, '=') << "\n"
        << " Uptime        : " << g_history.size() << " sec\n"
        << " Total Packets : " << totalPkts << "\n"
        << " Total Blocked : " << totalBlocked << "\n"
        << " Peak PPS      : " << maxPPS << " (at " << maxPPSAt << "s)\n"
        << string(45, '=') << "\n";

    ofstream f("security_report.txt");
    if (f.is_open()) {
        f << "Fortress Server Security Report\n"
            << string(45, '=') << "\n"
            << "Uptime   : " << g_history.size() << "s\n"
            << "Total Rx : " << totalPkts << " packets\n"
            << "Blocked  : " << totalBlocked << " packets\n"
            << "Peak PPS : " << maxPPS << " at " << maxPPSAt << "s\n\n"
            << "[Timeline]\n"
            << setw(6) << "Time" << " | " << setw(6) << "PPS"
            << " | " << setw(7) << "Blocked" << " | "
            << setw(8) << "KB/s" << " | Sessions\n";
        for (auto& s : g_history)
            f << setw(6) << s.second << " | " << setw(6) << s.pps
            << " | " << setw(7) << s.blocked << " | "
            << setw(8) << fixed << setprecision(2) << s.kbps
            << " | " << s.sessions << "\n";
        cout << "[+] Report saved to security_report.txt\n";
    }
}

void StartMonitoring(int serverSock) {
    g_sock = serverSock;
    g_running = true;
    thread(MonitorLoop).detach();
}

void StopMonitoring() {
    g_running = false;
    this_thread::sleep_for(chrono::milliseconds(600));
    PrintFinalReport();
}
