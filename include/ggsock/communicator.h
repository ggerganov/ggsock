#pragma once

#include "ggsock/common.h"

#include <memory>
#include <functional>

namespace GGSock {
    class Communicator {
        public:
            using TErrorCode = int16_t;
            using TBufferSize = uint32_t;
            using TMessageType = uint16_t;

            using CBError = std::function<void(TErrorCode errorCode)>;
            using CBMessage = std::function<uint16_t(const char * dataBuffer, TBufferSize dataSize)>;

            Communicator(bool startOwnWorker);
            ~Communicator();

            bool update();

            bool listen(TPort port, int32_t timeout_ms, int32_t maxConnections = 1);
            bool connect(const TAddress & address, TPort port, int32_t timeout_ms);

            bool disconnect();
            bool stopListening();
            bool isConnected() const;
            bool isConnecting() const;
            TAddress getPeerAddress() const;

            bool send(TMessageType type);
            bool send(TMessageType type, const char * dataBuffer, TBufferSize dataSize);

            bool setErrorCallback(CBError && callback);
            bool setMessageCallback(TMessageType type, CBMessage && callback);

            bool removeErrorCallback();
            bool removeMessageCallback(TMessageType type);

            static TAddress getLocalAddress();

        private:
            struct Data;
            std::unique_ptr<Data> data_;
            Data & getData() { return *data_; }
            const Data & getData() const { return *data_; }
    };
}
