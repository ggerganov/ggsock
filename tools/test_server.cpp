/*! \file test_server.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "ggsock/Communicator.h"

#include <thread>
#include <vector>

int main() {
    GGSock::Communicator server;

    std::vector<char> data(128*1024);

    while(true) {
        printf("init server\n");
        if (server.listen(12002, 0) == false) continue;
        printf("after listen\n");
        while (server.isConnected() == false) {}
        while (server.isConnected()) {
            server.send(95);
            server.send(94, data.data(), data.size());
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

    return 0;
}
