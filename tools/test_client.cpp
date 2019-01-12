/*! \file test_server.cpp
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

int main() {
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
            if (client.connect("127.0.0.1", 12003, 0) == false) continue;
            printf("after connect\n");
            while (client.isConnected() == false) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
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
    }
#endif

    return 0;
}
