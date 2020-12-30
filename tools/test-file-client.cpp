#include "ggsock/communicator.h"
#include "ggsock/file-server.h"
#include "ggsock/serialization.h"

#include <thread>
#include <cstring>
#include <cassert>

std::function<void()> g_update;

void update() {
    g_update();
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

    bool hasFileInfos = false;
    bool hasRequestedFiles = false;
    GGSock::FileServer::TFileInfos fileInfos;
    std::map<GGSock::FileServer::TURI, GGSock::FileServer::FileData> files;

    GGSock::Communicator client(false);

    client.setErrorCallback([](GGSock::Communicator::TErrorCode code) {
        printf("Disconnected with code = %d\n", code);
    });

    client.setMessageCallback(GGSock::FileServer::MsgFileInfosResponse, [&](const char * dataBuffer, size_t dataSize) {
        printf("Received message %d, size = %d\n", GGSock::FileServer::MsgFileInfosResponse, (int) dataSize);

        size_t offset = 0;
        GGSock::Unserialize()(fileInfos, dataBuffer, dataSize, offset);

        for (const auto & info : fileInfos) {
            printf("    - %s : %s (size = %d, chunks = %d)\n", info.second.uri.c_str(), info.second.filename.c_str(), (int) info.second.filesize, (int) info.second.nChunks);
            files[info.second.uri].info = info.second;
            files[info.second.uri].data.resize(info.second.filesize);
        }

        hasFileInfos = true;

        return 0;
    });

    client.setMessageCallback(GGSock::FileServer::MsgFileChunkResponse, [&](const char * dataBuffer, size_t dataSize) {
        GGSock::FileServer::FileChunkResponseData data;

        size_t offset = 0;
        GGSock::Unserialize()(data, dataBuffer, dataSize, offset);

        printf("Received chunk %d for file '%s', size = %d\n", data.chunkId, data.uri.c_str(), (int) data.data.size());
        std::memcpy(files[data.uri].data.data() + data.pStart, data.data.data(), data.pLen);

        if (data.chunkId == files[data.uri].info.nChunks - 1) {
            if (data.uri == "test-uri-0") {
                for (int i = 0; i < (int) files[data.uri].data.size(); ++i) {
                    assert(files[data.uri].data[i] == i%101);
                }
            }
            if (data.uri == "test-uri-1") {
                for (int i = 0; i < (int) files[data.uri].data.size(); ++i) {
                    assert(files[data.uri].data[i] == (3*i + 1)%103);
                }
            }
        }

        return 0;
    });

    g_update = [&]() {
        if (client.connect(ip, port, 0)) {
            printf("Started connecting ...\n");
        }

        if (client.isConnected()) {
            if (!hasFileInfos) {
                client.send(GGSock::FileServer::MsgFileInfosRequest);
            } else if (hasRequestedFiles == false) {
                printf("Requesting files ...\n");
                for (const auto & fileInfo : fileInfos) {
                    for (int i = 0; i < fileInfo.second.nChunks; ++i) {
                        GGSock::FileServer::FileChunkRequestData data;
                        data.uri = fileInfo.second.uri;
                        data.chunkId = i;
                        data.nChunksHave = 0;
                        data.nChunksExpected = fileInfo.second.nChunks;

                        GGSock::SerializationBuffer buffer;
                        GGSock::Serialize()(data, buffer);
                        client.send(GGSock::FileServer::MsgFileChunkRequest, buffer.data(), buffer.size());
                        client.update();
                    }
                }
                hasRequestedFiles = true;
            }
        }

        client.update();
    };

    while(true) {
        update();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
