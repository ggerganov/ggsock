#include "ggsock/communicator.h"

#include <thread>

int main() {
    {
        GGSock::Communicator server(true);
        server.setErrorCallback([](GGSock::Communicator::TErrorCode code) {
            printf("Network error = %d\n", code);
        });

        for (int i = 0; i < 3; ++i) {
            printf("iter %d\n", i);
            if (server.isConnected()) return 1;
            if (server.listen(12345, 10) == true) return 2;
            if (server.isConnected()) return 3;
            if (server.disconnect() == false) return 4;
            if (server.isConnected()) return 5;
        }

        for (int i = 0; i < 3; ++i) {
            GGSock::Communicator client(true);
            if (server.listen(12345, 0) == false) return 6;
            if (client.connect("127.0.0.1", 12345, 100) == false) return 7;
            while (client.isConnected() == false) {}
            while (server.isConnected() == false) {}
            if (client.connect("127.0.0.1", 12345, 100) == true) return 8;
            if (client.disconnect() == false) return 9;
            while (server.isConnected()) {}
            while (client.isConnected()) {}
            if (client.connect("127.0.0.1", 12345, 100) == true) return 10;
            if (server.listen(12345, 0) == false) return 11;
            if (client.connect("127.0.0.1", 12345, 100) == false) return 12;
            while (client.isConnected() == false) {}
            while (server.isConnected() == false) {}
            if (client.disconnect() == false) return 13;
            while (client.isConnected()) {}
            while (server.isConnected()) {}
        }
    }

    {
        GGSock::Communicator server(true);
        server.setErrorCallback([](GGSock::Communicator::TErrorCode code) {
            printf("Network error = %d\n", code);
        });
        server.setMessageCallback(42, [&](const char * , size_t dataSize) {
            printf("Received buffer. Size = %d\n", (int) dataSize);
            server.send(43);
            return true;
        });

        server.listen(12345, 0);

        GGSock::Communicator client(true);
        client.setMessageCallback(43, [](const char * , size_t ) {
            printf("Received acknowledgment\n");
            return true;
        });

        if (client.connect("127.0.0.1", 12345, 100) == false) return 14;

        while (client.isConnected() == false) {}
        while (server.isConnected() == false) {}

        char buf[16];

        if (client.send(42, buf, 16) == false) return 15;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (client.send(42, buf, 16) == false) return 16;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (client.send(42, buf, 16) == false) return 17;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (client.disconnect() == false) return 18;
    }

    printf("Done!\n");

    return 0;
}
