#pragma once

#include <ostream>
#include <vector>
#include <memory>
#include "types.h"
#include <string>
#include <unordered_map>
#include <map>
#include <string_view>
#include <queue>
#include <type_traits>
#include <typeinfo>
#include <unordered_set>
#include <typeindex>
#include <sstream>
#include <exception>
#include <iostream>

#define LOG(x) std::cerr << __PRETTY_FUNCTION__ << ':' << __LINE__ << ": " << #x << " = " << ( x ) << std::endl

class OutputDataStream {
public:
    explicit OutputDataStream(std::ostream* out, int string_cache_size = 0)
        : out_(out), string_counter_(0), string_cache_size_(string_cache_size) {
        last_occurence_.reserve(string_cache_size + 2);

        /* Signature */
        Write("OOSFv1");
        WriteMinimal(string_cache_size_);
    }

    ~OutputDataStream() {
    }

    OutputDataStream(const OutputDataStream&) = delete;
    void operator=(const OutputDataStream&) = delete;

    std::ostream* GetStream() {
        return out_;
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
    void Write(const T& value) {
        WriteType<T>();
        WriteValue(value);
    }

    void Write(bool value) {
        WriteByte(value ? '+' : '-');
    }

    template <class Iter>
    void WriteAsVector(Iter begin, Iter end, int size = -1) {
        if (size < 0) {
            size = std::distance(begin, end);
        }

        using ValueType = typename std::iterator_traits<Iter>::value_type;
        WriteType<std::vector<ValueType>>();
        WriteAsVectorInternal(begin, size);
    }

    template <class Iter>
    void WriteAsMap(Iter begin, Iter end, int size = -1) {
        if (size < 0) {
            size = std::distance(begin, end);
        }

        using Pair = typename std::iterator_traits<Iter>::value_type;
        using KeyType = typename Pair::first_type;
        using ValueType = typename Pair::second_type;
        WriteType<std::map<KeyType, ValueType>>();
        WriteAsMapInternal(begin, size);
    }
/*
    template <class... Args>
    void WriteAsTuple(const Args&... args) {
        WriteType<std::tuple<Args...>>();
        (WriteValue(args), ...);
    }
*/
    void WriteMinimal(int64_t value) {
#define TRY(size) if (value < (1LL << ( size - 1)) && -(1LL << (size - 1)) <= value) {   \
    Write(static_cast<int##size##_t>(value));                               \
} else
        TRY(8)
        TRY(16)
        TRY(32)
        Write(value);
#undef TRY
    }

private:
    template <class T>
    void WriteType() {
#define WRITE_TYPE(type, symbol)                                \
if constexpr (std::is_same_v<type, T> ||                        \
        IsSameIntegral<type, T>::value) {                       \
    WriteByte(symbol);                                          \
} else
        WRITE_TYPE(std::int8_t  , TYPE_INT8     )
        WRITE_TYPE(std::int16_t , TYPE_INT16    )
        WRITE_TYPE(std::int32_t , TYPE_INT32    )
        WRITE_TYPE(std::int64_t , TYPE_INT64    )
        WRITE_TYPE(float        , TYPE_FLOAT    )
        WRITE_TYPE(double       , TYPE_DOUBLE   )
        WRITE_TYPE(bool         , TYPE_BOOL     )
        WRITE_TYPE(std::string  , TYPE_STRING   )

        if constexpr (std::is_base_of_v<Serializable, std::decay_t<T>>) {
            auto type_index = std::type_index(typeid(std::decay_t<T>));
            auto iter = registered_classes_.find(type_index);
            if (iter == registered_classes_.end()) {
                throw std::runtime_error("Unsupported class");
            }

            WriteByte(TYPE_STRUCT);
            WriteValue(iter->second);
        } else if constexpr (std::is_same_v<std::decay_t<T>, char*> || std::is_same_v<std::decay_t<T>, std::string>) {
            WriteByte(TYPE_STRING);
 /*       } else if constexpr (IsTuple<T>::value) {
            WriteTupleType(static_cast<T*>(nullptr));*/
        } else if constexpr (IsVector<T>::value) {
            WriteByte(TYPE_VECTOR);
            WriteType<typename T::value_type>();
        } else if constexpr (IsMap<T>::value) {
            WriteByte(TYPE_MAP);
            WriteType<typename T::key_type>();
            WriteType<typename T::mapped_type>();
        } else {
            std::stringstream ss;
            ss << "Type " << std::type_info(typeid(std::decay_t<T>)).name() << " is not serializable";
            throw std::runtime_error(ss.str());
        }
#undef WRITE_TYPE
    }

#define TYPE_CHECKER(name, type) \
    template <class T> \
    class name : public std::false_type {}; \
    \
    template <class... Args> \
    class name<type<Args...>> : public std::true_type {};

//    TYPE_CHECKER(IsTuple,   std::tuple  )
    TYPE_CHECKER(IsVector,  std::vector )
    TYPE_CHECKER(IsMap,     std::map    )

#undef TYPE_CHECKER
/*
    template <class... Args>
    inline void WriteTupleType(std::tuple<Args...>*) {
        WriteByte(TYPE_TUPLE);
        WriteMinimal(sizeof...(Args));
        (WriteType<Args>(), ...);
    }
*/
    template <class T>
    inline void WriteValue(const T& value) {
        if constexpr (std::is_integral_v<T>) {
            WriteBytes(&value);
        } else if constexpr (std::is_floating_point_v<T>) {
            WriteBytes(&value);
        } else if constexpr (std::is_base_of_v<Serializable, std::decay_t<T>>) {
            value.WriteValue(this);
        } else {
            static_assert(std::disjunction_v<std::is_integral<T>, std::is_floating_point<T>, std::is_base_of<Serializable, std::decay_t<T>>>);
        }
    }

    void HonestWriteString(const std::string& str) {
        WriteMinimal(str.size());
        WriteBytes(str.c_str(), str.size());
    }

    inline void WriteValue(const char* str) {
        WriteValue(std::string(str));
    }

    inline void WriteValue(std::string str) {
        if (string_cache_size_ == 0) {
            HonestWriteString(str);
            ++string_counter_;
            return;
        }

        LOG(str);
        auto iter = last_occurence_.find(str);
        if (iter != last_occurence_.end()) {
            WriteMinimal(-iter->second - 1);
            iter->second = string_counter_;
        } else {
            HonestWriteString(str);
            iter = last_occurence_.emplace(std::move(str), string_counter_).first;
        }
        ++string_counter_;
        LOG(string_counter_);

        string_frame_.push(iter);
        if (static_cast<int>(string_frame_.size()) > string_cache_size_) {
            auto old_string = string_frame_.front();
            string_frame_.pop();
            if (old_string->second + string_cache_size_ < string_counter_) {
                last_occurence_.erase(old_string);
            }
        }
    }

    template <class T, class U>
    class IsSameIntegral : public std::conjunction<std::is_integral<T>, std::is_integral<U>,
                                                   std::integral_constant<bool, (sizeof(T) == sizeof(U))>> {};

    template <class T>
    inline void WriteValue(const std::vector<T>& vec) {
        WriteAsVectorInternal(vec.begin(), vec.size());
    }

    template <class K, class V>
    inline void WriteValue(const std::map<K, V>& map) {
        WriteAsMapInternal(map.begin(), map.size());
    }

    template <class Iter>
    void WriteAsVectorInternal(Iter iter, int64_t count) {
        WriteMinimal(count);
        while (count --> 0) {
            WriteValue(*iter);
            ++iter;
        }
    }

    template <class Iter>
    void WriteAsMapInternal(Iter iter, int64_t count) {
        WriteMinimal(count);
        while (count --> 0) {
            WriteValue(iter->first);
            WriteValue(iter->second);
            ++iter;
        }
    }

    inline void WriteByte(char byte) {
        out_->put(byte);
    }

    template <class T>
    inline void WriteBytes(const T* value, int count = 1) {
        out_->write(reinterpret_cast<const char*>(value), sizeof(T) * count);
    }

    std::ostream* out_;
    int string_counter_;
    int string_cache_size_;
    std::unordered_map<std::string, int> last_occurence_;
    std::queue<typename std::unordered_map<std::string, int>::iterator> string_frame_;

    std::unordered_map<std::type_index, std::string> registered_classes_;
};

#undef LOG
