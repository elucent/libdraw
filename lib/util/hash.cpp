#include "hash.h"

u64 rotl(u64 u, u64 n) {
	return (u << n) | ((u >> (64 - n)) & ~(-1 << n));
}

u64 rotr(u64 u, u64 n) {
	return (u >> n) | ((u << (64 - n)) & (-1 & ~(-1 << n)));
}

u64 raw_hash(const void* t, uint64_t size) {
	u64 h = 13830991727719723691ul;

	const u64* words = (const u64*)t;
    u32 i = 0;
	for (; i < size / 8; ++ i) {
	    uint64_t u = *words * 4695878395758459391ul + 1337216223865348961ul;
	    h ^= rotl(u, 7);
	    h ^= rotl(u, 61);
	    h *= 10646162605368431489ul;
	    ++ words;
	}
	
	const u8* bytes = (const u8*)words;
	i *= 8;
	for (; i < size; ++ i) {
	    uint64_t u = *bytes * 4695878395758459391ul + 1337216223865348961ul;
	    h ^= rotl(u, 7);
	    h ^= rotl(u, 61);
	    h *= 10646162605368431489ul;
	    ++ bytes;
	}
	double d = h;
	return *(u64*)&d;
}

template<>
u64 hash(const char* const& s) {
    u32 size = 0;
    const char* sptr = s;
    while (*sptr) ++ sptr, ++ size;
    return raw_hash((const u8*)s, size);
}

template<>
u64 hash(const string& s) {
    return raw_hash((const u8*)s.raw(), s.size());
}