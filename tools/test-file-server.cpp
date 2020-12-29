#include "ggsock/file-server.h"

#include <vector>
#include <thread>

int main(int argc, char ** argv) {
    printf("Usage: %s port\n", argv[0]);
    if (argc < 2) {
        return -1;
    }

    int port = atoi(argv[1]);

    GGSock::FileServer server;

    if (server.init({ 4, 8, 128, 128, port}) == false) {
        printf("Failed to initialize GGSock::FileServer\n");
        return -1;
    }

    GGSock::FileServer::FileData file0 { { "test-uri-0", 0, "test-filename-0" }, std::vector<char>(6343) };
    GGSock::FileServer::FileData file1 { { "test-uri-1", 0, "test-filename-1" }, std::vector<char>(3535342) };
    GGSock::FileServer::FileData file2 { { "test-uri-2", 0, "test-filename-2" }, std::vector<char>(37) };

    for (int i = 0; i < (int) file0.data.size(); ++i) file0.data[i] = i%101;
    for (int i = 0; i < (int) file1.data.size(); ++i) file1.data[i] = (3*i + 1)%103;

    server.addFile(std::move(file1));
    server.addFile(std::move(file0));
    server.addFile(std::move(file2));

    server.startListening();

    while (true) {
        printf("Listening ...\n");
        auto clientInfos = server.getClientInfos();

        if (clientInfos.size() > 0) {
            printf("Connected clients:\n");
            for (const auto & client : clientInfos) {
                printf(" - %d : %s\n", client.first, client.second.address.c_str());
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
