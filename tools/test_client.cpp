/*! \file test_client.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#ifdef __EMSCRIPTEN__
#include "emscripten/emscripten.h"
#endif

#include "ggsock/Communicator.h"

#include <thread>

void update() {
}

int main(int argc, char ** argv) {
    printf("Usage: %s ip port\n", argv[0]);
    if (argc < 3) {
        return -1;
    }

    std::string ip = "127.0.0.1";
    int port = 12003;

    if (argc > 1) ip = argv[1];
    if (argc > 2) port = atoi(argv[2]);

    printf("Connecting to %s : %d\n", ip.c_str(), port);

    GGSock::Communicator client;
    client.setMessageCallback(95, [](const char * dataBuffer, size_t dataSize) {
        printf("Received message 95\n");
        return 0;
    });
    client.setMessageCallback(94, [](const char * dataBuffer, size_t dataSize) {
        printf("Received message 94, dataSize = %d\n", (int) dataSize);
        return 0;
    });

    std::thread worker = std::thread([&]() {
        while(true) {
            printf("init client\n");
            if (client.connect(ip, port, 1000) == false) continue;
            printf("after connect\n");

            while (client.isConnected()) {
                client.send(195);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update, 0, 1);
#else
    while(true) {
        update();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
#endif

    return 0;
}
