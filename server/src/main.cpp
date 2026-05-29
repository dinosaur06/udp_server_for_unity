#include <csignal>
#include <iostream>
#include "Server.h"

static Server g_server;

void OnSignal(int sig) {
    std::cout << "\n[!] Shutdown signal received...\n";
    g_server.Stop();
}

int main() {
    signal(SIGINT, OnSignal);
    signal(SIGTERM, OnSignal);

    if (!g_server.Init(9000)) return 1;
    g_server.Run();
    return 0;
}
