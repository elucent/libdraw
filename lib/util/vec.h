#ifndef BASIL_VECTOR_H
#define BASIL_VECTOR_H

#include "defs.h"
#include <new>
#include <initializer_list>

template<typename T>
class vector {
    u8* data;
    u32 _size, _capacity;

    void free(u8* array) {
        T* tptr = (T*)array;
        for (u32 i = 0; i < _size; i ++) tptr[i].~T();
        delete[] array;
    }

    void init(u32 size) {
        _size = 0, _capacity = size;
        data = new u8[_capacity * sizeof(T)];
    }

    void copy(const T* ts, u32 n) {
        _size = 0;
        T* tptr = (T*)data;
        for (u32 i = 0; i < n; i ++) {
            new(tptr + i) T(ts[i]);
            ++ _size;
        }
    }

    void destruct(u32 i) {
        T* tptr = (T*)data;
        tptr[i].~T();
    }

    void grow(u32 capacity) {
        u8* old = data;
        u32 oldsize = _size;
        init(capacity);
        copy((const T*)old, oldsize);
        free(old);
    }

public:
    vector() {
        init(16);
    }

    vector(const std::initializer_list<T>& init): vector() {
        for (const T& t : init) push(t);
    }

    ~vector() {
        free(data);
    }

    vector(const vector& other) {
        init(other._capacity);
        copy((const T*)other.data, other._size);
    }

    vector& operator=(const vector& other) {
        if (this != &other) {
            free(data);
            init(other._capacity);
            copy((const T*)other.data, other._size);
        }
        return *this;
    }

    void push(const T& t) {
        while (_size + 1 >= _capacity) grow(_capacity * 2);
        T* tptr = (T*)data;
        new(tptr + _size) T(t);
        ++ _size;
    }
    
    void pop() {
        -- _size;
        destruct(_size);
    }

    void reserve(u32 capacity) {
        if (capacity > _capacity) grow(capacity);
    }

    void clear() {
        for (u32 i = 0; i < _size; ++ i) destruct(i);
        _size = 0;
    }

    const T& operator[](u32 i) const {
        return ((T*)data)[i];
    }

    T& operator[](u32 i) {
        return ((T*)data)[i];
    }

    const T* begin() const {
        return (const T*)data;
    }

    T* begin() {
        return (T*)data;
    }

    const T* end() const {
        return (const T*)data + _size;
    }

    T* end() {
        return (T*)data + _size;
    }

    u32 size() const {
        return _size;
    }

    u32 capacity() const {
        return _capacity;
    }

    const T& front() const { 
        return *begin();
    }

    T& front() { 
        return *begin();
    }

    const T& back() const { 
        return *(end() - 1);
    }

    T& back() { 
        return *(end() - 1);
    }
};

#endif