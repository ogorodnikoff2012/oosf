#pragma once

#define TYPE_INT8   'b'
#define TYPE_INT16  'w'
#define TYPE_INT32  'l'
#define TYPE_INT64  'q'
#define TYPE_FLOAT  'f'
#define TYPE_DOUBLE 'd'
#define TYPE_STRING 's'
#define TYPE_VECTOR 'v'
#define TYPE_MAP    'm'
// #define TYPE_TUPLE  't'
#define TYPE_BOOL   '?'
#define TYPE_BOOL_T '+'
#define TYPE_BOOL_F '-'
#define TYPE_STRUCT '!'

#include <cstdio>

class OutputDataStream;
class InputDataStream;

enum ReadStatus {
    kStatusOk,
    kStatusBadType,
    kStatusMalformedData,
    kStatusReadError,
    kStatusStringOutOfCache
};

class Serializable {
public:
    virtual ReadStatus TryRead(InputDataStream*) = 0;
    virtual void WriteValue(OutputDataStream*) const = 0;
    virtual ~Serializable() = default;
};
