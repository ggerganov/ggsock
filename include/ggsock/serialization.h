#pragma once

#include <cstddef>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace GGSock {

struct SerializationBuffer : public std::vector<char> { using vector::vector; };

// if the following macro is defined, include the STL serialization overloads
struct Serialize {
    template <typename T>                       bool operator()(const T & obj,                        SerializationBuffer & buffer, size_t & offset);

    // helpers
    template <typename T>                       bool operator()(const T & obj,                        SerializationBuffer & buffer);

    // STL
    template <typename T>                       bool operator()(const std::vector<T> & obj,           SerializationBuffer & buffer, size_t & offset);
    template <typename T>                       bool operator()(const std::shared_ptr<T> & obj,       SerializationBuffer & buffer, size_t & offset);
    template <typename First, typename Second>  bool operator()(const std::pair<First, Second> & obj, SerializationBuffer & buffer, size_t & offset);
    template <typename Key, typename Value>     bool operator()(const std::map<Key, Value> & obj,     SerializationBuffer & buffer, size_t & offset);

    uint64_t nBytesProcessed = 0;
};

struct Unserialize {
    template <typename T>                       bool operator()(T & obj,                        const char * bufferData, size_t bufferSize, size_t & offset);

    // helpers
    template <typename T>                       bool operator()(T & obj,                        const SerializationBuffer & buffer);
    template <typename T>                       bool operator()(T & obj,                        const SerializationBuffer & buffer, size_t & offset);

    // STL
    template <typename T>                       bool operator()(std::vector<T> & obj,           const char * bufferData, size_t bufferSize, size_t & offset);
    template <typename T>                       bool operator()(std::shared_ptr<T> & obj,       const char * bufferData, size_t bufferSize, size_t & offset);
    template <typename First, typename Second>  bool operator()(std::pair<First, Second> & obj, const char * bufferData, size_t bufferSize, size_t & offset);
    template <typename Key, typename Value>     bool operator()(std::map<Key, Value> & obj,     const char * bufferData, size_t bufferSize, size_t & offset);

    // bool deepCopy = true;

    uint64_t nBytesProcessed = 0;
};

//
// Serialize helpers
//

template <typename T>bool Serialize::operator()(const T & obj, SerializationBuffer & buffer) {
    size_t offset = 0;
    return operator()(obj, buffer, offset);
}

//
// Unserialize helpers
//

template <typename T>bool Unserialize::operator()(T & obj, const SerializationBuffer & buffer) {
    size_t offset = 0;
    return operator()(obj, buffer, offset);
}

template <typename T>bool Unserialize::operator()(T & obj, const SerializationBuffer & buffer, size_t & offset) {
    return operator()(obj, buffer.data(), buffer.size(), offset);
}

//
// STL overloads
//

// std::vector

template <typename T> bool Serialize::operator()(const std::vector<T> & t, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    int32_t n = (int32_t) t.size();
    res &= operator()(n, buffer, offset);
    for (const auto & p : t) {
        res &= operator()(p, buffer, offset);
    }

    return res;
}

template <typename T> bool Unserialize::operator()(std::vector<T> & t, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    int32_t n = 0;
    res &= operator()(n, bufferData, bufferSize, offset);

    t.resize(n);
    for (int i = 0; i < n; ++i) {
        res &= operator()(t[i], bufferData, bufferSize, offset);
    }

    return true;
}

// std::shared_ptr

template <typename T> bool Serialize::operator()(const std::shared_ptr<T> & t, SerializationBuffer & buffer, size_t & offset) {
    return t ? operator()(*t, buffer, offset) : false;
}

template <typename T> bool Unserialize::operator()(std::shared_ptr<T> & t, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    auto tmp = std::make_shared<typename std::remove_cv<T>::type>();
    res &= operator()(*tmp, bufferData, bufferSize, offset);
    t = tmp;

    return true;
}

// std::pair

template <typename First, typename Second> bool Serialize::operator()(const std::pair<First, Second> & t, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    res &= operator()(t.first, buffer, offset);
    res &= operator()(t.second, buffer, offset);

    return res;
}

template <typename First, typename Second> bool Unserialize::operator()( std::pair<First, Second> & t, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    res &= operator()(t.first, bufferData, bufferSize, offset);
    res &= operator()(t.second, bufferData, bufferSize, offset);

    return res;
}

// std::map

template <typename Key, typename Value> bool Serialize::operator()(const std::map<Key, Value> & t, SerializationBuffer & buffer, size_t & offset) {
    bool res = true;

    int32_t n = (int32_t) t.size();
    res &= operator()(n, buffer, offset);
    for (const auto & p : t) {
        res &= operator()(p.first, buffer, offset);
        res &= operator()(p.second, buffer, offset);
    }

    return res;
}

template <typename Key, typename Value> bool Unserialize::operator()(std::map<Key, Value> & t, const char * bufferData, size_t bufferSize, size_t & offset) {
    bool res = true;

    int32_t n = 0;
    res &= operator()(n, bufferData, bufferSize, offset);
    for (int i = 0; i < n; ++i) {
        Key k;
        Value v;
        res &= operator()(k, bufferData, bufferSize, offset);
        res &= operator()(v, bufferData, bufferSize, offset);
        t[k] = std::move(v);
    }

    return res;
}

}
