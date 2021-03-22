#ifndef BASIL_DEFS_H
#define BASIL_DEFS_H

#include <cstdint>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// util/hash.h

template<typename T>
class set;

template<typename K, typename V>
class map;

// util/io.h

class stream;
class file;
class buffer;

// util/str.h

class string;

// util/vec.h

template<typename T>
class vector;

#endif
