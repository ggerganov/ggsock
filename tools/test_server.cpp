/*! \file test_server.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "ggsock/Communicator.h"

#include <thread>
#include <vector>

int main(int argc, char ** argv) {
    printf("Usage: %s port\n", argv[0]);
    if (argc < 2) {
        return -1;
    }

    int port = atoi(argv[1]);

    auto worker = std::thread([&]() {
        GGSock::Communicator server;

        std::vector<char> data(128*1024);

        while(true) {
            printf("init server, port = %d\n", port);
            if (server.listen(port, 1000) == false) continue;
            while (server.isConnected()) {
                printf("sending client 2\n");
                server.send(95);
                server.send(94, data.data(), data.size());
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });

    GGSock::Communicator server;

    std::vector<char> data(128*1024);

    while(true) {
        printf("init server, port = %d\n", port);
        if (server.listen(port, 1000) == false) continue;
        while (server.isConnected()) {
            printf("sending client 1\n");
            server.send(95);
            server.send(94, data.data(), data.size());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

    worker.join();

    return 0;
}
