/*! \file Communicator.h
 *  \brief Enter description here.
 *  \author Georgi Gerganov
 */

#pragma once

#include <memory>
#include <string>
#include <functional>

namespace GGSock {
    using TPort = int32_t;
    using TAddress = std::string;
    using TErrorCode = int16_t;
    using TBufferSize = uint32_t;
    using TMessageType = uint16_t;

    using CBError = std::function<void(TErrorCode errorCode)>;
    using CBMessage = std::function<uint16_t(const char * dataBuffer, size_t dataSize)>;

    class Communicator {
        public:
            Communicator();
            ~Communicator();

            bool listen(TPort port, int32_t timeout_ms, int32_t maxConnections = 1);
            bool connect(const TAddress & address, TPort port, int32_t timeout_ms);

            bool disconnect();
            bool isConnected() const;

            bool send(TMessageType type);
            bool send(TMessageType type, const char * dataBuffer, size_t dataSize);

            bool setErrorCallback(CBError && callback);
            bool setMessageCallback(TMessageType type, CBMessage && callback);

            bool removeErrorCallback();
            bool removeMessageCallback(TMessageType type);

        private:
            struct Data;
            std::unique_ptr<Data> data_;
            Data & getData() { return *data_; }
            const Data & getData() const { return *data_; }
    };
}
