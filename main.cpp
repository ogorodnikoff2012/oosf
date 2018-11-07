#include <output_data_stream.h>
#include <input_data_stream.h>
#include <iostream>
#include <fstream>

#define LOG(x) std::cerr << #x << ": " << (x) << std::endl;

template <class T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v) {
    out << '[';
    if (!v.empty()) {
        out << v.front();
        for (size_t i = 1; i < v.size(); ++i) {
            out << ", " << v[i];
        }
    }
    return out << ']';
}

template <class K, class V>
std::ostream& operator<<(std::ostream& out, const std::map<K, V>& m) {
    out << '{';
    if (!m.empty()) {
        bool first = true;
        for (auto iter = m.begin(); iter != m.end(); ++iter) {
            if (!first) {
                out << ", ";
            }
            out << iter->first << " -> " << iter->second;
            first = false;
        }
    }
    return out << '}';
}

struct Foo : public Serializable {
    int x;
    Foo(int value) : x(value) {
    }

    void WriteValue(OutputDataStream* out) const {
        out->Write(x);
    }
};

void WriteTest() {
    std::ofstream file("file.bin", std::ios_base::binary);
    OutputDataStream stream(&file, 2);

    stream.Write(1);
    stream.Write(1000LL);
    stream.Write(1.0f);
    stream.Write(3.14159);

    stream.Write('a');
    stream.Write(true);

    std::vector<int> v{1, 2, 3};
    stream.Write(v);

    for (int i = 0; i < 3; ++i) {
        stream.Write("Hello OOSF!");
        stream.Write("Hello Vova!");
    }

    for (int i = 0; i < 3; ++i) {
        stream.Write("String 1");
        stream.Write("String 2");
        stream.Write("String 3");
    }

    stream.WriteAsVector(v.begin(), v.end());

    std::map<std::string, int> map{{"foo", 3}, {"bar", 5}, {"baz", 17}};
    stream.Write(map);

    //stream.WriteAsTuple(1, 2, "foo");
    Foo f{42};
    stream.RegisterClass<Foo>("Foo");
    stream.Write(f);

    file.close();
}

void ReadTest() {
    std::FILE* file = std::fopen("file.bin", "r");
    InputDataStream stream(file);

    std::boolalpha(std::cerr);
    LOG((bool)(stream));

#define CHECK_TYPE(type)                    \
    {                                       \
        type x;                             \
        auto status = stream.TryRead(&x);   \
        LOG(status);                        \
        LOG(x);                             \
    }

    CHECK_TYPE(int32_t          )
    CHECK_TYPE(int64_t          )
    CHECK_TYPE(float            )
    CHECK_TYPE(double           )

    CHECK_TYPE(int8_t           )
    CHECK_TYPE(bool             )

    CHECK_TYPE(std::vector<int> )

    for (int i = 0; i < 15; ++i) {
        CHECK_TYPE(InputDataStream::String);
    }

    CHECK_TYPE(std::vector<int> )
    using Map = std::map<InputDataStream::String, int>;
    CHECK_TYPE(Map              )

    std::fclose(file);
}

int main() {
    WriteTest();
    ReadTest();

    return 0;
}
