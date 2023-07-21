#include "ggsock/communicator.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#pragma comment(lib, "Ws2_32.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <signal.h>
#endif
#include <sys/types.h>

#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <array>
#include <vector>
#include <condition_variable>

namespace {
    using TSocketDescriptor = int32_t;

    void closeAndReset(TSocketDescriptor & sock) {
        if (sock != -1) {
#ifndef __EMSCRIPTEN__
            shutdown(sock, 0);
#endif
            close(sock);
            sock = -1;
        }
    }

    void setNonBlocking(TSocketDescriptor & sock) {
#ifdef _WIN32
        unsigned long nonblocking = 1;
        ioctlsocket(sock, FIONBIO, &nonblocking);
#else
        int flags;
        flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);

        int flag = 1;
        int result = setsockopt(sock,            /* socket affected */
                                IPPROTO_TCP,     /* set option at TCP level */
                                TCP_NODELAY,     /* name of option */
                                (char *) &flag,  /* the cast is historical cruft */
                                sizeof(int));    /* length of option value */
        if (result != 0) {
            fprintf(stderr, "Failed to set non-blocking socket\n");
        }
#endif
    }

    bool e_wouldBlock() {
#ifdef _WIN32
        return errno == EWOULDBLOCK || WSAGetLastError() == WSAEWOULDBLOCK;
#else
        return errno == EWOULDBLOCK;
#endif
    }

    bool e_isConnected() {
#ifdef _WIN32
        return errno == EISCONN || WSAGetLastError() == WSAEISCONN;
#else
        return errno == EISCONN;
#endif
    }

    bool e_inProgress() {
#ifdef _WIN32
        return errno == EINPROGRESS || errno == EALREADY || WSAGetLastError() == WSAEINPROGRESS || WSAGetLastError() == WSAEALREADY;
#else
        return errno == EINPROGRESS || errno == EALREADY;
#endif
    }

    struct MessageHeader {
        ::GGSock::Communicator::TBufferSize  size;
        ::GGSock::Communicator::TMessageType type;

        static constexpr size_t getSizeInBytes() {
            return
                sizeof(::GGSock::Communicator::TBufferSize) +
                sizeof(::GGSock::Communicator::TMessageType);
        }
    };
}

namespace GGSock {
    struct Communicator::Data {
        Data(bool startOwnWorker) {
            // todo : maybe move this to a static method
            static bool isFirst = true;
            if (isFirst) {
#ifdef _WIN32
                // Initialize Winsock
                WSADATA wsaData;
                int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
                if (iResult != 0) {
                    printf("WSAStartup failed with error: %d\n", iResult);
                    return;
                }
#endif

#ifndef _WIN32
                // this is needed to avoid program crash upon sending data to disconnected clients
                signal(SIGPIPE, SIG_IGN);
#endif

                isFirst = false;
            }

            std::lock_guard<std::mutex> lock(mutex);

            bufferDataRecv.resize(256*1024, 0);

            if (startOwnWorker) {
                isRunning = true;
                worker = std::thread([this]() {
                    while (isRunning) {
                        update();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                });
            }
        }

        ~Data() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                isRunning = false;
                isConnected = false;
                isConnecting = false;
                isListening = false;
                ::closeAndReset(sdpeer);
                ::closeAndReset(sd);
            }

            if (worker.joinable()) {
                worker.join();
            }
        }

        bool update() {
            std::lock_guard<std::mutex> lock(mutex);
            if (isServer && isListening) {
                doListen();
            } else if (isServer && isListening == false && isConnected) {
                doRead();
            } else if (isServer == false && isConnecting) {
                doConnect();
            } else if (isServer == false && isConnecting == false && isConnected) {
                doRead();
            }
            {
                std::lock_guard<std::mutex> lock(mutexSend);
                if (isConnected && (rbHead != rbEnd)) {
                    doSend();
                }
                if (isConnected == false) {
                    rbHead = rbEnd;
                }
            }

            return true;
        }

        bool doListen() {
            FD_ZERO(&master_set);
            max_sd = sd;
            FD_SET(sd, &master_set);

            memcpy(&working_set, &master_set, sizeof(master_set));

            timeout.tv_sec  = timeoutListen_ms/1000;
            timeout.tv_usec = (timeoutListen_ms%1000)*1000;

            int rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
            if (rc < 0) {
                return false;
            }

            if (rc == 0) {
                return false;
            }

            int ndesc = rc;
            if (ndesc != 1) {
                printf("WARNING: ndesc = %d\n", ndesc);
            }

            if (FD_ISSET(sd, &working_set)) {
                --ndesc;

                do {
                    sdpeer = accept(sd, NULL, NULL);
                    if (sdpeer < 0) {
                        if (e_wouldBlock() == false) {
                            perror("  accept() failed");
                        }
                        return false;
                    }

                    socklen_t len;
                    len = sizeof(peeraddr);
                    getpeername(sdpeer, (struct sockaddr*)&peeraddr, &len);

                    printf("  New incoming connection - %d, %d, ip = %s\n", sd, sdpeer, inet_ntoa(peeraddr.sin_addr));

                    ::setNonBlocking(sdpeer);

                    isListening = false;
                    isConnected = true;

                    // stop listening for connections
                    ::closeAndReset(sd);

                    break;
                } while (sdpeer != -1);
            } else {
                return false;
            }

            return true;
        }

        bool doConnect() {
            auto tStart = std::chrono::high_resolution_clock::now();

            while (isConnecting) {
                auto rc = ::connect(sd, (struct sockaddr *) &addr, sizeof(addr));
                if (rc < 0 && e_isConnected() == false) {
                    if (e_inProgress() == false && e_wouldBlock() == false) {
                        ::closeAndReset(sd);
                        sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

                        int enable = 1;
                        if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(int)) < 0) {
                            fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
                        }

#ifndef _WIN32
                        if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (char *)&enable, sizeof(int)) < 0) {
                            fprintf(stderr, "setsockopt(SO_REUSEPORT) failed");
                        }
#endif

                        //{
                        //    linger lin;
                        //    lin.l_onoff = 0;
                        //    lin.l_linger = 0;
                        //    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(lin)) < 0) {
                        //        fprintf(stderr, "setsockopt(SO_LINGER) failed");
                        //    }
                        //}

                        ::setNonBlocking(sd);
                    }
                    if (timeoutConnect_ms > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));

                        auto tEnd = std::chrono::high_resolution_clock::now();
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count() >= timeoutConnect_ms) {
                            ::closeAndReset(sd);
                            isConnecting = false;
                        }
                        continue;
                    }

                    return false;
                }

                sdpeer = sd;

                printf("Connected successfully, sd = %d\n", sd);

                isConnecting = false;
                isConnected = true;

                return true;
            }

            return false;
        }

        void doRead() {
            int rc = (int) recv(sdpeer, bufferHeaderRecv.data(), bufferHeaderRecv.size(), 0);
            if (rc < 0) {
                if (e_wouldBlock() == false) {
                    isConnected = false;
                    ::closeAndReset(sdpeer);
                    ::closeAndReset(sd);

                    TErrorCode errorCode = errno;
                    if (errorCallback) {
                        errorCallback(errorCode);
                    }
                }
                return;
            }

            if (rc == 0) {
                isConnected = false;
                isListening = false;
                ::closeAndReset(sdpeer);
                ::closeAndReset(sd);

                TErrorCode errorCode = errno;
                if (errorCallback) {
                    errorCallback(errorCode);
                }

                return;
            }

            //printf("  %d bytes received, %d %d\n", rc,
            //       (int)(*reinterpret_cast<int32_t *>(bufferHeaderRecv.data())),
            //       (int)(*reinterpret_cast<int16_t *>(bufferHeaderRecv.data() + sizeof(int32_t)))
            //       );

            if (rc == (int) bufferHeaderRecv.size()) {
                TBufferSize size = 0;
                TMessageType type = 0;
                memcpy(&size, bufferHeaderRecv.data(), sizeof(size));
                memcpy(&type, bufferHeaderRecv.data() + sizeof(size), sizeof(type));
                if (size == ::MessageHeader::getSizeInBytes()) {
                    if (const auto & cb = messageCallback[type]) {
                        cb(bufferDataRecv.data(), 0);
                    }
                } else if (size > ::MessageHeader::getSizeInBytes()) {
                    lastMessageType = type;
                    leftToReceive = size - ::MessageHeader::getSizeInBytes();
                    offsetReceive = 0;
                    if (leftToReceive > (int) bufferDataRecv.size()) {
                        printf("Extend receive buffer to %d bytes\n", (int) bufferDataRecv.size());
                        bufferDataRecv.resize(leftToReceive);
                    }

                    while (leftToReceive > 0) {
                        //printf("left = %d\n", (int) leftToReceive);
                        size_t curSize = (std::min)(65536, (int) leftToReceive);

                        int rc = (int) recv(sdpeer, bufferDataRecv.data() + offsetReceive, curSize, 0);
                        if (rc < 0) {
                            if (e_wouldBlock() == false) {
                                isConnected = false;
                                ::closeAndReset(sdpeer);
                                ::closeAndReset(sd);

                                if (errorCallback) {
                                    TErrorCode errorCode = errno;
                                    errorCallback(errorCode);
                                }

                                break;
                            }

                            continue;
                        }

                        if (rc == 0) {
                            isConnected = false;
                            isListening = false;
                            ::closeAndReset(sdpeer);
                            ::closeAndReset(sd);

                            TErrorCode errorCode = errno;
                            if (errorCallback) errorCallback(errorCode);

                            return;
                        }

                        leftToReceive -= rc;
                        offsetReceive += rc;
                    }

                    if (const auto & cb = messageCallback[type]) {
                        cb(bufferDataRecv.data(), size - ::MessageHeader::getSizeInBytes());
                    }
                } else {
                    // error
                }
            }
        }

        void doSend() {
            const auto & curMessage = ringBufferSend[rbHead];
            TBufferSize size = (TBufferSize) curMessage.size();

            int offset = 0;
            while (size > 0) {
                int rc = (int) ::send(sdpeer, curMessage.data() + offset, size, 0);
                if (rc < 0) {
                    if (e_wouldBlock() == false) {
                        isConnected = false;
                        ::closeAndReset(sdpeer);
                        ::closeAndReset(sd);

                        if (errorCallback) {
                            TErrorCode errorCode = errno;
                            errorCallback(errorCode);
                        }

                        break;
                    }
                    continue;
                }
                size -= rc;
                offset += rc;
            }

            if (++rbHead >= (int) ringBufferSend.size()) {
                rbHead = 0;
            }
        }

        bool addMessageToSend(std::string && msg) {
            ringBufferSend[rbEnd] = std::move(msg);

            if (++rbEnd >= (int) ringBufferSend.size()) {
                rbEnd = 0;
            }

            return rbEnd != rbHead;
        }

        bool isServer = true;
        bool isListening = false;
        bool isConnected = false;
        bool isConnecting = false;
        bool isRunning = false;

        int32_t timeoutListen_ms = 0;
        int32_t timeoutConnect_ms = 0;

        TSocketDescriptor sd = -1;
        TSocketDescriptor sdpeer = -1;
        TSocketDescriptor max_sd = -1;

        struct timeval timeout;
        struct sockaddr_in addr;
        struct sockaddr_in peeraddr;

        fd_set master_set;
        fd_set working_set;

        int32_t offsetReceive = 0;
        int32_t leftToReceive = 0;
        TMessageType lastMessageType = -1;

        std::int32_t rbHead = 0;
        std::int32_t rbEnd = 0;
        std::array<std::string, 128> ringBufferSend;

        std::vector<char> bufferDataRecv;

        std::array<char, ::MessageHeader::getSizeInBytes()> bufferHeaderRecv;
        std::array<char, ::MessageHeader::getSizeInBytes()> bufferHeaderSend;

        mutable std::mutex mutex;
        mutable std::mutex mutexSend;
        std::thread worker;

        CBError errorCallback = nullptr;
        std::map<TMessageType, CBMessage> messageCallback;
    };

    Communicator::Communicator(bool startOwnWorker) : data_(new Data(startOwnWorker)) {}
    Communicator::~Communicator() {}

    bool Communicator::update() {
        return getData().update();
    }

    bool Communicator::listen(TPort port, int32_t timeout_ms, int32_t maxConnections) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        if (data.isConnected) return false;
        if (data.isListening) return false;

        ::closeAndReset(data.sd);

        data.sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (data.sd < 0) {
            ::closeAndReset(data.sd);
            fprintf(stderr, "Error creating socket (%d %s)\n", errno, strerror(errno));
            return false;
        }

        int enable = 1;
        if (setsockopt(data.sd, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(int)) < 0) {
            fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
        }

#ifndef _WIN32
        if (setsockopt(data.sd, SOL_SOCKET, SO_REUSEPORT, (char *)&enable, sizeof(int)) < 0) {
            fprintf(stderr, "setsockopt(SO_REUSEPORT) failed");
        }
#endif

        //{
        //    linger lin;
        //    lin.l_onoff = 0;
        //    lin.l_linger = 0;
        //    if (setsockopt(data.sd, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(lin)) < 0) {
        //        fprintf(stderr, "setsockopt(SO_LINGER) failed");
        //    }
        //}

        auto & addr = data.addr;

        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        int res = bind(data.sd, (struct sockaddr *)&addr, sizeof(addr));
        if (res == -1) {
            perror("Bind failed");
            fprintf(stderr, "Error: bind failed");
            return false;
        }

        res = ::listen(data.sd, maxConnections);
        if (res == -1) {
            fprintf(stderr, "Error: listen failed");
            return false;
        }

        ::setNonBlocking(data.sd);

        data.isServer = true;
        data.isListening = true;

        data.timeoutListen_ms = (std::max)(0, timeout_ms);
        if (timeout_ms > 0) {
            bool success = data.doListen();

            data.isListening = false;
            return success;
        } else if (timeout_ms < 0) {
            data.timeoutListen_ms = 1;
            while (data.isListening) {
                bool success = data.doListen();
                if (success) {
                    data.isListening = false;
                    return true;
                }
            }
            return false;
        }

        return true;
    }

    bool Communicator::connect(const TAddress & address, TPort port, int32_t timeout_ms) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        if (data.isConnected) return false;
        if (data.isConnecting) return false;

        struct hostent *server;

        data.sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (data.sd < 0) {
            ::closeAndReset(data.sd);
            fprintf(stderr, "Error creating socket (%d %s)\n", errno, strerror(errno));
            return false;
        }

        int enable = 1;
        if (setsockopt(data.sd, SOL_SOCKET, SO_REUSEADDR, (char *)&enable, sizeof(int)) < 0) {
            fprintf(stderr, "setsockopt(SO_REUSEADDR) failed");
        }

#ifndef _WIN32
        if (setsockopt(data.sd, SOL_SOCKET, SO_REUSEPORT, (char *)&enable, sizeof(int)) < 0) {
            fprintf(stderr, "setsockopt(SO_REUSEPORT) failed");
        }
#endif

        //{
        //    linger lin;
        //    lin.l_onoff = 0;
        //    lin.l_linger = 0;
        //    if (setsockopt(data.sd, SOL_SOCKET, SO_LINGER, (const char *)&lin, sizeof(lin)) < 0) {
        //        fprintf(stderr, "setsockopt(SO_LINGER) failed");
        //    }
        //}

        ::setNonBlocking(data.sd);

        server = gethostbyname(address.c_str());
        if (server == NULL) {
            fprintf(stderr,"ERROR, no such host\n");
            return false;
        }

        auto & addr = data.addr;
        memset((char *) &addr, '\0', sizeof(addr));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr(address.c_str());

        data.isServer = false;
        data.isConnecting = true;

        data.timeoutConnect_ms = (std::max)(0, timeout_ms);
        if (timeout_ms > 0) {
            bool res = data.doConnect();

            data.isConnecting = false;
            return res;
        } else if (timeout_ms < 0) {
            data.timeoutConnect_ms = 1;
            while (data.isConnecting) {
                bool success = data.doConnect();
                if (success) {
                    data.isConnecting = false;
                    return true;
                }
            }
            return false;
        }

        return true;
    }

    bool Communicator::disconnect() {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        data.isListening = false;
        data.isConnecting = false;
        data.isConnected = false;

        ::closeAndReset(data.sdpeer);
        ::closeAndReset(data.sd);

        return true;
    }

    bool Communicator::stopListening() {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        if (data.isListening) {
            data.isListening = false;

            ::closeAndReset(data.sdpeer);
            ::closeAndReset(data.sd);

            return true;
        }

        return false;
    }

    bool Communicator::isConnected() const {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        return data.isConnected;
    }

    bool Communicator::isConnecting() const {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        return data.isConnecting;
    }

    TAddress Communicator::getPeerAddress() const {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        return inet_ntoa(data.peeraddr.sin_addr);
    }

    bool Communicator::send(TMessageType type) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutexSend);

        if (data.isConnected == false) return false;

        {
            std::string msg;
            TBufferSize size = ::MessageHeader::getSizeInBytes();

            msg.reserve(size);

            msg.append(reinterpret_cast<const char*>(&size), reinterpret_cast<const char*>(&size)+sizeof(size));
            msg.append(reinterpret_cast<const char*>(&type), reinterpret_cast<const char*>(&type)+sizeof(type));

            if (data.addMessageToSend(std::move(msg)) == false) {
                // error, send buffer is full
                return false;
            }
        }

        return true;
    }

    bool Communicator::send(TMessageType type, const char * dataBuffer, TBufferSize dataSize) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutexSend);

        if (data.isConnected == false) return false;

        {
            std::string msg;
            TBufferSize size = ::MessageHeader::getSizeInBytes() + dataSize;

            msg.reserve(size);

            msg.append(reinterpret_cast<const char*>(&size), reinterpret_cast<const char*>(&size)+sizeof(size));
            msg.append(reinterpret_cast<const char*>(&type), reinterpret_cast<const char*>(&type)+sizeof(type));
            msg.append(dataBuffer, dataBuffer + dataSize);

            if (data.addMessageToSend(std::move(msg)) == false) {
                // error, send buffer is full
                return false;
            }
        }

        return true;
    }

    bool Communicator::setErrorCallback(CBError && callback) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        data.errorCallback = std::move(callback);

        return true;
    }

    bool Communicator::setMessageCallback(TMessageType type, CBMessage && callback) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        data.messageCallback[type] = std::move(callback);

        return true;
    }

    bool Communicator::removeErrorCallback() {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        if (data.errorCallback) {
            data.errorCallback = nullptr;
            return true;
        }

        return false;
    }

    bool Communicator::removeMessageCallback(TMessageType type) {
        auto & data = getData();

        std::lock_guard<std::mutex> lock(data.mutex);

        if (data.messageCallback[type]) {
            data.messageCallback[type] = nullptr;
            return true;
        }

        return false;
    }

    TAddress Communicator::getLocalAddress() {
        int sock = socket(PF_INET, SOCK_DGRAM, 0);
        sockaddr_in loopback;

        if (sock == -1) {
            return "";
        }

        std::memset(&loopback, 0, sizeof(loopback));
        loopback.sin_family = AF_INET;
        loopback.sin_addr.s_addr = INADDR_LOOPBACK;   // using loopback ip address
        loopback.sin_port = htons(9);                 // using debug port

        if (::connect(sock, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback)) == -1) {
            close(sock);
            return "";
        }

        socklen_t addrlen = sizeof(loopback);
        if (getsockname(sock, reinterpret_cast<sockaddr*>(&loopback), &addrlen) == -1) {
            close(sock);
            return "";
        }

        close(sock);

        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &loopback.sin_addr, buf, INET_ADDRSTRLEN) == 0x0) {
            return "";
        }

        return buf;
    }
}
