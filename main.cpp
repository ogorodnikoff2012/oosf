#include <output_data_stream.h>

int main() {
    std::FILE* file = fopen("file.bin", "w");
    OutputDataStream stream(file, 2);

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

    stream.WriteAsTuple(1, 2, "foo");

    std::map<std::string, int> map{{"foo", 3}, {"bar", 5}, {"baz", 17}};
    stream.Write(map);

    fclose(file);
    return 0;
}
