#ifndef KL_COMPACT_POOL_H_
#define KL_COMPACT_POOL_H_

#include <cassert>
#include <cstdint>
#include <cstddef>

namespace kl
{
namespace internal
{
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFUL
typedef uint_fast64_t cpu_word_t;
constexpr size_t BITS = 64;
#elif UINTPTR_MAX == 0xFFFFFFFFUL
typedef uint_fast32_t cpu_word_t;
constexpr size_t BITS = 32;
#else
#error "Unsupported platform!"
#endif

constexpr cpu_word_t ONE = static_cast<cpu_word_t>(1);
constexpr size_t FULL = UINTPTR_MAX;

inline cpu_word_t LeastBitClear(cpu_word_t word)
{
    cpu_word_t out;
    asm("bsf %1, %0"
        : "=rm" (out)
        : "rm" (~word));
    return out;
}

template <size_t Level>
size_t OffsetAndMark(cpu_word_t *allocated)
{ 
    assert("Only specialised instances of this function make sense." == 0);
    return 0;
}

template<>
size_t OffsetAndMark<0>(cpu_word_t *allocated)
{
    size_t offset = LeastBitClear(allocated[0]);
    allocated[0] |= (ONE << offset);
    assert(offset < BITS);
    return offset;
}

template<>
size_t OffsetAndMark<1>(cpu_word_t *allocated)
{
    size_t offset_on_level0 = LeastBitClear(allocated[0]);
    size_t offset_on_level1 = LeastBitClear(allocated[1 + offset_on_level0]);
    size_t offset = offset_on_level0 * BITS + offset_on_level1;
    
    allocated[1 + offset_on_level0] |= (ONE << offset_on_level1);

    if (allocated[1 + offset_on_level0] == FULL)
    {
        allocated[0] |= (ONE << offset_on_level0);
    }
    
    assert(offset < (BITS * BITS));
    return offset;
}
} // namespace internal
} // namespace kl
#endif // KL_COMPACT_POOL_H_