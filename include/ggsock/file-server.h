#pragma once

#include "ggsock/common.h"

#include <memory>
#include <map>
#include <vector>

namespace GGSock {
    class FileServer {
        public:
            using TURI = std::string;
            using TFilesize = int64_t;
            using TFilename = std::string;

            using TFileId = int32_t;
            using TClientId = int32_t;
            using TChunkId = int32_t;
            using TProgress = float;

            struct FileInfo;
            struct ClientInfo;

            using TFileInfos = std::map<TFileId, FileInfo>;
            using TClientInfos = std::map<TClientId, ClientInfo>;

            using TBinaryBlob = std::vector<char>;

            enum MessageType {
                MsgFileInfosRequest = 100,
                MsgFileInfosResponse,
                MsgFileChunkRequest,
                MsgFileChunkResponse,
            };

            struct FileChunkRequestData {
                TURI uri = "";
                TChunkId chunkId = 0;
                int32_t nChunksHave = 0;
                int32_t nChunksExpected = 0;
            };

            struct FileChunkResponseData {
                TURI uri = "";
                TChunkId chunkId = 0;
                TBinaryBlob data;
                int64_t pStart = 0;
                int64_t pLen = 0;
            };

            struct Parameters {
                int32_t nWorkerThreads = 4;
                int32_t nMaxClients = 8;
                int32_t nMaxFiles = 128;
                int32_t nDefaultFileChunks = 128;

                TPort listenPort = 22765;
            };

            struct FileInfo {
                TURI uri = "?";
                TFilesize filesize = 0;
                TFilename filename = "?";

                int32_t nChunks = 0;
            };

            struct FileData {
                FileInfo info;
                TBinaryBlob data;
            };

            struct ClientInfo {
                TPort port = 0;
                TAddress address = "0.0.0.0";

                int64_t dataTx_bytes = 0;
                int64_t dataRx_bytes = 0;

                TFileId currentFileId = -1;
                TProgress currentProgress = 0.0f;
            };

            FileServer();
            ~FileServer();

            bool init(const Parameters & parameters);

            bool startListening();
            bool stopListening();
            bool isListening();

            bool update();

            bool addFile(FileData && data);
            bool clearAllFiles();
            bool clearFile(const TURI & uri);

            TFileInfos getFileInfos() const;
            TClientInfos getClientInfos() const;

            const Parameters & getParameters() const;
            const FileData & getFileData(const TURI & uri) const;

        private:
            struct Impl;
            std::unique_ptr<Impl> m_impl;
    };

}
