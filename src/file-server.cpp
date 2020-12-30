#include "ggsock/file-server.h"

#include "ggsock/communicator.h"

#include "ggsock/serialization.h"

#include <atomic>
#include <thread>
#include <queue>
#include <mutex>

namespace GGSock {

//
// Serialization
//

// FileServer::FileInfo

template <>
bool Serialize::operator()<FileServer::FileInfo>(const FileServer::FileInfo & obj, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    res = res && operator()(obj.uri, buffer, offset);
    res = res && operator()(obj.filesize, buffer, offset);
    res = res && operator()(obj.filename, buffer, offset);
    res = res && operator()(obj.nChunks, buffer, offset);

    return res;
}

template <>
bool Unserialize::operator()<FileServer::FileInfo>(FileServer::FileInfo & obj, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    res = res && operator()(obj.uri, bufferData, bufferSize, offset);
    res = res && operator()(obj.filesize, bufferData, bufferSize, offset);
    res = res && operator()(obj.filename, bufferData, bufferSize, offset);
    res = res && operator()(obj.nChunks, bufferData, bufferSize, offset);

    return res;
}

// FileServer::FileChunkRequestData

template <>
bool Serialize::operator()<FileServer::FileChunkRequestData>(const FileServer::FileChunkRequestData & obj, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    res = res && operator()(obj.uri, buffer, offset);
    res = res && operator()(obj.chunkId, buffer, offset);
    res = res && operator()(obj.nChunksHave, buffer, offset);
    res = res && operator()(obj.nChunksExpected, buffer, offset);

    return res;
}

template <>
bool Unserialize::operator()<FileServer::FileChunkRequestData>(FileServer::FileChunkRequestData & obj, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    res = res && operator()(obj.uri, bufferData, bufferSize, offset);
    res = res && operator()(obj.chunkId, bufferData, bufferSize, offset);
    res = res && operator()(obj.nChunksHave, bufferData, bufferSize, offset);
    res = res && operator()(obj.nChunksExpected, bufferData, bufferSize, offset);

    return res;
}

// FileServer::FileChunkResponseData

template <>
bool Serialize::operator()<FileServer::FileChunkResponseData>(const FileServer::FileChunkResponseData & obj, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    res = res && operator()(obj.uri, buffer, offset);
    res = res && operator()(obj.chunkId, buffer, offset);
    res = res && operator()(obj.data, buffer, offset);
    res = res && operator()(obj.pStart, buffer, offset);
    res = res && operator()(obj.pLen, buffer, offset);

    return res;
}

template <>
bool Unserialize::operator()<FileServer::FileChunkResponseData>(FileServer::FileChunkResponseData & obj, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    res = res && operator()(obj.uri, bufferData, bufferSize, offset);
    res = res && operator()(obj.chunkId, bufferData, bufferSize, offset);
    res = res && operator()(obj.data, bufferData, bufferSize, offset);
    res = res && operator()(obj.pStart, bufferData, bufferSize, offset);
    res = res && operator()(obj.pLen, bufferData, bufferSize, offset);

    return res;
}

//
// FileServer
//

struct ClientData {
    ClientData() {
        communicator = std::make_shared<Communicator>(false);
    }

    FileServer::ClientInfo info;

    bool isUpdating = false;
    bool needsNewChunk = false;
    bool sendFileInfos = false;
    bool wasConnected = false;

    std::deque<FileServer::FileChunkRequestData> fileChunkRequests;

    std::shared_ptr<Communicator> communicator;
};

struct FileServer::Impl {
    bool updateFileInfos() {
        fileInfos.clear();

        for (int i = 0; i < (int) files.size(); ++i) {
            if (fileUsed[i] == false) {
                continue;
            }
            const auto & file = files[i];
            fileInfos[i] = file.info;
        }

        return true;
    }

    bool updateClientInfos() {
        clientInfos.clear();

        for (int i = 0; i < (int) clients.size(); ++i) {
            const auto & client = clients[i];
            if (client.communicator->isConnected() == false) {
                continue;
            }

            clientInfos[i] = client.info;
        }

        return true;
    }

    bool exists(const FileInfo & info) const {
        for (int i = 0; i < (int) files.size(); ++i) {
            if (fileUsed[i] == false) {
                continue;
            }

            const auto & file = files[i];
            if (info.uri == file.info.uri &&
                info.filesize == file.info.filesize &&
                info.filename == file.info.filename) {
                return true;
            }
        }

        return false;
    }

    Parameters parameters;

    std::atomic<bool> isRunning { false };

    bool isListening = false;

    int currentFileUpdateId = 0;
    int currentClientUpdateId = 0;

    std::vector<bool> fileUsed;

    std::vector<FileData> files;
    std::vector<ClientData> clients;

    bool changedFileInfos = false;
    TFileInfos fileInfos;

    bool changedClientInfos = false;
    TClientInfos clientInfos;

    std::mutex mutex;
    std::vector<std::thread> workers;
};

FileServer::FileServer() : m_impl(new Impl()) {
}

FileServer::~FileServer() {
    m_impl->isRunning = false;

    for (auto & worker : m_impl->workers) {
        worker.join();
    }
}

bool FileServer::init(const Parameters & parameters) {
    if (m_impl->isRunning) {
        return false;
    }

    m_impl->parameters = parameters;

    m_impl->fileUsed.resize(m_impl->parameters.nMaxFiles);
    m_impl->files.resize(m_impl->parameters.nMaxFiles);
    m_impl->clients.resize(m_impl->parameters.nMaxClients);

    for (int i = 0; i <(int)  m_impl->clients.size(); ++i) {
        auto & client = m_impl->clients[i];

        client.communicator->setErrorCallback([i](Communicator::TErrorCode code) {
            printf("Client %d disconnected, code = %d\n", i, code);
        });

        client.communicator->setMessageCallback(MsgFileInfosRequest, [this, i](const char * , Communicator::TBufferSize ) {
            printf("Received message %d\n", MsgFileInfosRequest);
            {
                std::lock_guard<std::mutex> lock(m_impl->mutex);
                m_impl->clients[i].sendFileInfos = true;
            }

            return 0;
        });

        client.communicator->setMessageCallback(MsgFileChunkRequest, [this, i](const char * dataBuffer, Communicator::TBufferSize dataSize) {
            size_t offset = 0;
            FileChunkRequestData data;
            Unserialize()(data, dataBuffer, dataSize, offset);
            //printf("Received chunk request %d for file '%s'\n", data.chunkId, data.uri.c_str());

            {
                std::lock_guard<std::mutex> lock(m_impl->mutex);
                m_impl->clients[i].fileChunkRequests.emplace_back(std::move(data));
            }

            return 0;
        });
    }

    m_impl->isRunning = true;
    m_impl->workers.resize(m_impl->parameters.nWorkerThreads);
    for (auto & worker : m_impl->workers) {
        worker = std::thread([this]() {
            while (m_impl->isRunning) {
                update();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    return true;
}

bool FileServer::startListening() {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->isListening) {
            return false;
        }

        m_impl->isListening = true;
    }

    return true;
}

bool FileServer::stopListening() {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        if (m_impl->isListening == false) {
            return false;
        }

        m_impl->isListening = false;
    }

    return true;
}

bool FileServer::isListening() {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->isListening;
}

bool FileServer::update() {
    bool doListen = false;
    bool doSendFileInfos = false;
    bool doSendFileChunk = false;

    FileChunkResponseData fileChunkToSend;

    TClientId updateId = -1;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        if (m_impl->changedFileInfos) {
            m_impl->updateFileInfos();
            m_impl->changedFileInfos = false;
        }

        if (m_impl->changedClientInfos) {
            m_impl->updateClientInfos();
            m_impl->changedClientInfos = false;
        }

        updateId = m_impl->currentClientUpdateId;

        if (++m_impl->currentClientUpdateId == (int) m_impl->clients.size()) {
            m_impl->currentClientUpdateId = 0;
        }

        auto & client = m_impl->clients.at(updateId);

        if (client.isUpdating) {
            return false;
        }

        doListen = m_impl->isListening;

        doSendFileInfos = client.sendFileInfos;
        client.sendFileInfos = false;

        if (client.fileChunkRequests.size() > 0) {
            const auto & req = client.fileChunkRequests.front();

            // todo : data checks
            fileChunkToSend.uri = req.uri;
            fileChunkToSend.chunkId = req.chunkId;
            for (int i = 0; i < (int) m_impl->files.size(); ++i) {
                if (m_impl->fileUsed[i] == false) {
                    continue;
                }

                const auto & file = m_impl->files[i];
                if (file.info.uri != req.uri) {
                    continue;
                }

                auto chunkSize = file.data.size()/file.info.nChunks;

                int64_t pStart = req.chunkId*chunkSize;
                int64_t pLen = req.chunkId == (file.info.nChunks - 1) ? file.data.size() - pStart : chunkSize;

                fileChunkToSend.pStart = pStart;
                fileChunkToSend.pLen = pLen;
                fileChunkToSend.data.assign(file.data.begin() + pStart, file.data.begin() + pStart + pLen);

                doSendFileChunk = true;

                break;
            }

            client.fileChunkRequests.pop_front();
        }

        client.isUpdating = true;

        if (client.wasConnected && client.communicator->isConnected() == false) {
            printf("Client %d has disconnected\n", updateId);
            client.wasConnected = false;
            m_impl->updateClientInfos();
        }

        if (client.wasConnected == false && client.communicator->isConnected()) {
            printf("Client %d has connected\n", updateId);
            client.wasConnected = true;
            client.info.address = client.communicator->getPeerAddress();
            m_impl->updateClientInfos();
        }
    }

    {
        auto & client = m_impl->clients.at(updateId);
        if (doListen) {
            client.communicator->listen(m_impl->parameters.listenPort, 0);
        } else {
            client.communicator->stopListening();
        }
        if (doSendFileInfos) {
            SerializationBuffer buffer;
            Serialize()(m_impl->fileInfos, buffer);
            client.communicator->send(MsgFileInfosResponse, buffer.data(), (int) buffer.size());
            client.communicator->update();
        }
        if (doSendFileChunk) {
            SerializationBuffer buffer;
            Serialize()(fileChunkToSend, buffer);
            client.communicator->send(MsgFileChunkResponse, buffer.data(), (int) buffer.size());
            client.communicator->update();
        }
        client.communicator->update();
    }

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->clients[updateId].isUpdating = false;
    }

    return true;
}

bool FileServer::addFile(FileData && data) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        if (data.data.size() == 0) {
            return false;
        }

        if (m_impl->exists(data.info)) {
            return false;
        }

        // todo : handle max files reached
        if (m_impl->currentFileUpdateId == (int) m_impl->files.size()) {
            return false;
        }

        if (data.info.nChunks == 0) {
            data.info.nChunks = m_impl->parameters.nDefaultFileChunks;
        }

        if (data.info.nChunks > (int32_t) data.data.size()) {
            data.info.nChunks = (int32_t) data.data.size();
        }

        data.info.filesize = data.data.size();

        m_impl->fileUsed[m_impl->currentFileUpdateId] = true;
        m_impl->files[m_impl->currentFileUpdateId] = std::move(data);

        m_impl->currentFileUpdateId++;

        m_impl->changedFileInfos = true;
    }

    return true;
}

bool FileServer::clearAllFiles() {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        m_impl->currentFileUpdateId = 0;
        for (int i = 0; i < (int) m_impl->fileUsed.size(); ++i) {
            m_impl->fileUsed[i] = false;
        }

        for (auto & file : m_impl->files) {
            file = {};
        }

        m_impl->changedFileInfos = true;
    }

    return true;
}

bool FileServer::clearFile(const TURI & uri) {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        for (int i = 0; i < (int) m_impl->fileUsed.size(); ++i) {
            if (m_impl->files[i].info.uri != uri) {
                continue;
            }
            m_impl->fileUsed[i] = false;
            m_impl->files[i] = {};

            break;
        }

        m_impl->changedFileInfos = true;
    }

    return true;
}

FileServer::TFileInfos FileServer::getFileInfos() const {
    TFileInfos result;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        result = m_impl->fileInfos;
    }

    return result;
}

FileServer::TClientInfos FileServer::getClientInfos() const {
    TClientInfos result;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        result = m_impl->clientInfos;
    }

    return result;
}

const FileServer::Parameters & FileServer::getParameters() const {
    return m_impl->parameters;
}

const FileServer::FileData & FileServer::getFileData(const TURI & uri) const {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);

        for (int i = 0; i < (int) m_impl->files.size(); ++i) {
            if (m_impl->fileUsed[i] == false) {
                continue;
            }

            if (m_impl->files[i].info.uri != uri) {
                continue;
            }

            return m_impl->files[i];
        }
    }

    static FileData empty;
    return empty;
}

}
