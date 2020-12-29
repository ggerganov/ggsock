/*! \file test_server.cpp
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#include "ggsock/communicator.h"

#include <thread>
#include <vector>

int main(int argc, char ** argv) {
    printf("Usage: %s port\n", argv[0]);
    if (argc < 2) {
        return -1;
    }

    int port = atoi(argv[1]);

    GGSock::Communicator server0(false);
    GGSock::Communicator server1(false);

    std::vector<char> data(128*1024);

    while (true) {
        server0.listen(port, 0);
        server1.listen(port, 0);

        if (server0.isConnected()) {
            printf("sending client 1\n");
            server0.send(95);
            server0.send(94, data.data(), data.size());
        }

        if (server1.isConnected()) {
            printf("sending client 2\n");
            server1.send(95);
            server1.send(94, data.data(), data.size());
        }

        server0.update();
        server1.update();

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

    return 0;
}
