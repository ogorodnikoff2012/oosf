#pragma once

#include <cstdio>
#include <vector>
#include <map>
#include <memory>
#include <deque>
#include <stack>
#include <cstring>
#include "types.h"
#include <iostream>
#include <unordered_map>
#include <typeinfo>
#include <typeindex>

#define LOG(x) std::cerr << __FILE__ << ':' << __LINE__ << ':' << #x << ": " << x << std::endl;

class InputDataStream {
public:
    using String = std::shared_ptr<const char[]>;

    explicit InputDataStream(std::FILE* file) : file_(file) {
        String signature;
        if (TryRead(&signature) != kStatusOk || std::strcmp("OOSFv1", signature.get()) != 0) {
            corrupted_ = true;
        }
        int64_t cache_size = 0;
        if (TryReadMinimal(&cache_size) != kStatusOk) {
            corrupted_ = true;
        }
        string_cache_size_ = cache_size;
    }

    ~InputDataStream() {}

    InputDataStream(const InputDataStream&) = delete;
    void operator=(const InputDataStream&) = delete;

    operator bool() const {
        return !corrupted_;
    }

    template <class T>
    bool RegisterClass(const std::string& str) {
        auto type_index = std::type_index(typeid(std::decay_t<T>));
        auto iter = registered_classes_.find(type_index);
        if (iter != registered_classes_.end()) {
            return false;
        }
        registered_classes_[type_index] = str;
        return true;
    }

    template <class T>
    ReadStatus TryRead(T* object) {
        if (corrupted_) {
            return kStatusReadError;
        }
        ReadStatus result = kStatusOk;
        if ((result = CheckType(object)) == kStatusOk) {
            result = ReadObject(object);
        }
        return result;
    }

    ReadStatus TryRead(bool* var) {
        if (corrupted_) {
            return kStatusReadError;
        }
        auto pos = std::ftell(file_);
        int sym1 = GetByte();
        if (sym1 < 0) {
            std::fseek(file_, pos, SEEK_SET);
            return kStatusReadError;
        }
        if (sym1 == '+' || sym1 == '-') {
            *var = sym1 == '+';
            return kStatusOk;
        }
        if (sym1 == '?') {
            int sym2 = GetByte();
            if (sym2 < 0) {
                std::fseek(file_, pos, SEEK_SET);
                return kStatusReadError;
            }
            *var = sym2;
            return kStatusOk;
        }

        std::fseek(file_, pos, SEEK_SET);
        return kStatusBadType;
    }

    ReadStatus TryReadMinimal(int64_t* value) {
#define TRY_READ(type) {                                \
    type var = 0;                                       \
    if ((result = TryRead(&var)) != kStatusBadType) {   \
        if (result == kStatusOk) {                      \
            *value = var;                               \
            return kStatusOk;                           \
        } else {                                        \
            return result;                              \
        }                                               \
    }                                                   \
}
        ReadStatus result = kStatusOk;
        TRY_READ(int8_t)
        TRY_READ(int16_t)
        TRY_READ(int32_t)
        TRY_READ(int64_t)
        return result;
#undef TRY_READ
    }

private:
#define CHECK_FIRST_LETTER(symbol) {                                    \
    char ch = GetByte();                                                \
    /*LOG(ch);*/                                                        \
    if (ch != symbol) {                                                 \
        if (ch < 0 || std::fseek(file_, pos, SEEK_SET) < 0) {           \
            corrupted_ = true;                                          \
            return kStatusReadError;                                    \
        }                                                               \
        return kStatusBadType;                                          \
    }                                                                   \
}
    template <class T>
    ReadStatus CheckType(T*) {
        if constexpr (std::is_base_of_v<Serializable, std::decay_t<T>>) {
            auto pos = std::ftell(file_);
            CHECK_FIRST_LETTER(TYPE_STRUCT);

            String str;
            ReadStatus string_check = ReadObject(&str);
            if (string_check != kStatusOk) {
                std::fseek(file_, pos, SEEK_SET);
                return string_check;
            }

            auto type_index = std::type_index(typeid(std::decay_t<T>));
            auto iter = registered_classes_.find(type_index);
            if (iter == registered_classes_.end() || iter->second != str.get()) {
                std::fseek(file_, pos, SEEK_SET);
                return kStatusBadType;
            }

            return kStatusOk;

        } else {
            return kStatusBadType;
        }
    }


#define CHECK_SIMPLE_TYPE(type, symbol)                                 \
        ReadStatus CheckType(type*) {                                   \
            auto pos = std::ftell(file_);                               \
            CHECK_FIRST_LETTER(symbol)                                  \
            return kStatusOk;                                           \
        }

#define READ_SIMPLE_TYPE(type)                                          \
        ReadStatus ReadObject(type* value) {                            \
            uint32_t items = std::fread(value, sizeof(type), 1, file_); \
            return items == 0 ?                                         \
                    (corrupted_ = true, kStatusReadError) : kStatusOk;  \
        }

#define READ_CHECK(type, symbol) CHECK_SIMPLE_TYPE(type, symbol) READ_SIMPLE_TYPE(type)

    READ_CHECK(int8_t   , TYPE_INT8     )
    READ_CHECK(int16_t  , TYPE_INT16    )
    READ_CHECK(int32_t  , TYPE_INT32    )
    READ_CHECK(int64_t  , TYPE_INT64    )
    READ_CHECK(double   , TYPE_DOUBLE   )
    READ_CHECK(float    , TYPE_FLOAT    )
    READ_CHECK(bool     , TYPE_BOOL     )

    CHECK_SIMPLE_TYPE(String, TYPE_STRING)

#undef READ_CHECK
#undef CHECK_SIMPLE_TYPE
#undef READ_SIMPLE_TYPE

#define CHECK_SUBTYPE(type) {                           \
    ReadStatus inner_check = CheckType((type*)nullptr); \
    if (inner_check != kStatusOk) {                     \
        std::fseek(file_, pos, SEEK_SET);               \
        return inner_check;                             \
    }                                                   \
}
    template <class T, class... Args>
    ReadStatus CheckType(std::vector<T, Args...>*) {
        auto pos = std::ftell(file_);
        CHECK_FIRST_LETTER(TYPE_VECTOR)
        CHECK_SUBTYPE(T)
        return kStatusOk;
    }

    template <class K, class V, class... Args>
    ReadStatus CheckType(std::map<K, V, Args...>*) {
        auto pos = std::ftell(file_);
        CHECK_FIRST_LETTER(TYPE_MAP)
        CHECK_SUBTYPE(K)
        CHECK_SUBTYPE(V)
        return kStatusOk;
    }

    template <class K, class V, class... Args>
    ReadStatus CheckType(std::unordered_map<K, V, Args...>*) {
        return CheckType(static_cast<std::map<K, V>*>(nullptr));
    }

#define READ_LENGTH                                                                 \
int64_t length = 0;                                                                 \
ReadStatus status = kStatusOk;                                                      \
if ((status = TryReadMinimal(&length)) != kStatusOk) {                              \
    corrupted_ = true;                                                              \
    return status == kStatusReadError ? kStatusReadError : kStatusMalformedData;    \
}


//         template <class... Args>
//         ReadStatus CheckType(std::tuple<Args...>*) {
//             return kStatusBadType; /* TODO fix */
//             auto pos = std::ftell(file_);
//             CHECK_FIRST_LETTER(TYPE_TUPLE)
//             READ_LENGTH
//             if (length != sizeof...(Args)) {
//                 corrupted_ = true;
//                 return kStatusMalformedData;
//             }
//
//             return kStatusOk;
//         }

#undef CHECK_SUBTYPE
#undef CHECK_FIRST_LETTER

    template <class T, class=std::enable_if_t<std::is_base_of_v<Serializable, std::decay_t<T>>>>
    ReadStatus ReadObject(T* obj) {
        return obj->TryRead(this);
    }

    template <class T, class... Args>
    ReadStatus ReadObject(std::vector<T, Args...>* vec) {
        READ_LENGTH
        if (length < 0) {
            corrupted_ = true;
            return kStatusMalformedData;
        }
        vec->resize(length);

        for (int i = 0; i < length; ++i) {
            if (ReadStatus status = ReadObject(&vec->at(i)); status != kStatusOk) {
                corrupted_ = true;
                return status;
            }
        }
        return kStatusOk;
    }

    template <class K, class V, class... Args>
    ReadStatus ReadObject(std::map<K, V, Args...>* map) {
        return ReadAsMap<K, V>(map);
    }

    template <class K, class V, class... Args>
    ReadStatus ReadObject(std::unordered_map<K, V, Args...>* map) {
        return ReadAsMap<K, V>(map);
    }

    template <class Map, class K, class V>
    ReadStatus ReadAsMap(Map* map) {
        READ_LENGTH
        if (length < 0) {
            corrupted_ = true;
            return kStatusMalformedData;
        }

        for (int i = 0; i < length; ++i) {
            K key;
            V value;
            ReadStatus status = kStatusOk;
            if ((status = ReadObject(&key)) != kStatusOk || (status = ReadObject(&value)) != kStatusOk) {
                corrupted_ = true;
                return status;
            }
            map->emplace(std::make_pair(key, value));
        }
        return kStatusOk;
    }

    ReadStatus ReadObject(String* str) {
        READ_LENGTH
        if (length < 0) {
            int64_t index = -length - 1;
            index -= (string_counter_ - string_cache_size_);
            if (index < 0 || index >= string_cache_size_) {
                corrupted_ = true;
                return kStatusStringOutOfCache;
            }

            *str = string_cache_[index];
        } else {
            char* data = new char[length + 1];
            data[length] = '\0';
            uint32_t bytes = std::fread(data, 1, length, file_);
            if (bytes < length) {
                delete[] data;
                corrupted_ = true;
                return kStatusReadError;
            }
            *str = String(data);
        }

        string_cache_.push_back(*str);
        while (static_cast<int>(string_cache_.size()) > string_cache_size_) {
            string_cache_.pop_front();
        }
        ++string_counter_;
        return kStatusOk;
    }

    inline int GetByte() {
        return std::fgetc(file_);
    }

    std::FILE* file_;
    std::deque<String> string_cache_;
    int string_cache_size_ = 1;
    int string_counter_ = 0;
    bool corrupted_ = false;

    std::unordered_map<std::type_index, std::string> registered_classes_;
};

#undef LOG
