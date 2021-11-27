#include "ggsock/serialization.h"

#include <string>
#include <memory>
#include <cstring>

namespace {
    template <typename Operator>
        inline void advance(size_t & offset, size_t amount, Operator & op) noexcept {
            offset += amount;
            op.nBytesProcessed += amount;
        }

    // type - fundamental

    template <typename T>
        inline bool serialize_fundamental(const T & obj, GGSock::SerializationBuffer & buffer, size_t & offset, GGSock::Serialize & op) noexcept {
            static_assert(std::is_fundamental<T>::value, "Fundamental type required");

            auto osize = sizeof(obj);

            if (offset + osize > buffer.size()) {
                buffer.resize(offset + osize);
            }

            std::memcpy(buffer.data() + offset, &obj, osize);
            ::advance(offset, osize, op);

            return true;
        }

    template <typename T>
        inline bool unserialize_fundamental(T & obj, const char * bufferData, size_t bufferSize, size_t & offset, GGSock::Unserialize & op) noexcept {
            static_assert(std::is_fundamental<T>::value, "Fundamental type required");

            auto osize = sizeof(obj);

            if (offset + osize > bufferSize) return false;

            std::memcpy(reinterpret_cast<char *>(&obj), bufferData + offset, osize);
            ::advance(offset, osize, op);

            return true;
        }

    // type - vector

    template <typename T>
        inline bool serialize_vector(const T * objs, int32_t n, GGSock::SerializationBuffer & buffer, size_t & offset, GGSock::Serialize & op) noexcept {
            static_assert(std::is_fundamental<T>::value, "Fundamental type required");

            auto osize = sizeof(T)*n;

            if (offset + osize > buffer.size()) {
                buffer.resize(offset + osize);
            }

            std::memcpy(buffer.data() + offset, objs, osize);
            ::advance(offset, osize, op);

            return true;
        }

    template <typename T>
        inline bool unserialize_vector(T * objs, int32_t n, const char * bufferData, size_t bufferSize, size_t & offset, GGSock::Unserialize & op) noexcept {
            static_assert(std::is_fundamental<T>::value, "Fundamental type required");

            auto osize = sizeof(T)*n;

            if (offset + osize > bufferSize) {
                return false;
            }

            std::copy(
                bufferData + offset,
                bufferData + offset + osize, reinterpret_cast<char *>(objs));

            ::advance(offset, osize, op);

            return true;
        }
}

#define ADD_HELPER(T, type)                                                                                 \
    template <>                                                                                             \
    bool Serialize::operator()<T>(const T & obj, SerializationBuffer & buffer, size_t & offset) {           \
        return ::serialize_##type(obj, buffer, offset, *this);                                              \
    }                                                                                                       \
                                                                                                            \
    template <>                                                                                             \
    bool Unserialize::operator()<T>(T & obj, const char * bufferData, size_t bufferSize, size_t & offset) { \
        return ::unserialize_##type(obj, bufferData, bufferSize, offset, *this);                            \
    }

namespace GGSock {

// fundamental

ADD_HELPER(bool,        fundamental)
ADD_HELPER(char,        fundamental)

ADD_HELPER(int8_t,      fundamental)
ADD_HELPER(int16_t,     fundamental)
ADD_HELPER(int32_t,     fundamental)
ADD_HELPER(int64_t,     fundamental)

ADD_HELPER(uint8_t,     fundamental)
ADD_HELPER(uint16_t,    fundamental)
ADD_HELPER(uint32_t,    fundamental)
ADD_HELPER(uint64_t,    fundamental)

ADD_HELPER(float,       fundamental)
ADD_HELPER(double,      fundamental)

// std::string

template <> bool Serialize::operator()<std::string>(const std::string & t, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    int32_t n = (int32_t) t.size();
    res &= operator()(n, buffer, offset);
    res &= ::serialize_vector(t.data(), n, buffer, offset, *this);

    return res;
}

template <> bool Unserialize::operator()<std::string>(std::string & t, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    int32_t n = 0;
    res &= operator()(n, bufferData, bufferSize, offset);

    t.resize(n);
    res &= ::unserialize_vector(&t[0], n, bufferData, bufferSize, offset, *this);

    return res;
}


}
