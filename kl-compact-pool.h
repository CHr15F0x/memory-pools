#ifndef KL_COMPACT_POOL_H_
#define KL_COMPACT_POOL_H_

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <type_traits>

#define KL_COMPACT_POOL_RUNTIME_CHECKS 1

#if KL_COMPACT_POOL_RUNTIME_CHECKS
#define KL_COMPACT_POOL_CHECK_POINTER(OFFSET, CAPACITY, PTR)\
    assert(((OFFSET) >= 0u) || ("Pointer does not belong to pool." == 0));\
    assert(((OFFSET) < static_cast<ptrdiff_t>(CAPACITY)) || ("Pointer does not belong to pool." == 0));\
    assert((reinterpret_cast<uintptr_t>(PTR) % alignof(aligned_storage_t) == 0) || ("Pointer is not aligned properly" == 0))
#define KL_COMPACT_POOL_CHECK_CAPACITY(CAPACITY)\
    assert((capacity > 0u) || (KL_MIN_CAPACITY_MSG == 0));\
    assert((capacity <= private_internal::MAX_SUPPORTED_CAPACITY) || (KL_MAX_CAPACITY_MSG == 0));
#else
#define KL_COMPACT_POOL_CHECK_POINTER(OFFSET, CAPACITY, PTR)
#define KL_COMPACT_POOL_CHECK_CAPACITY(CAPACITY)
#endif // KL_COMPACT_POOL_RUNTIME_CHECKS

namespace kl
{
// Internal constants and functions, not a part of the public API
namespace private_internal
{
// 64 bit arch
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFul
typedef uint_fast64_t cpu_word_t;
constexpr size_t BITS = 64u;
constexpr size_t SHIFT = 6u;
constexpr cpu_word_t MASK = 0x3Fu;
// 32 bit arch
#elif UINTPTR_MAX == 0xFFFFFFFFul
typedef uint_fast32_t cpu_word_t;
constexpr size_t BITS = 32u;
constexpr size_t SHIFT = 5u;
constexpr cpu_word_t MASK = 0x1Fu;
#else
#error "Unsupported platform!"
#endif

// Width of this constant must be the same as word to achieve proper shifts
constexpr cpu_word_t ONE = static_cast<cpu_word_t>(1);
constexpr cpu_word_t FULL = UINTPTR_MAX;
constexpr size_t MAX_SUPPORTED_DEPTH = 3u;
constexpr size_t MAX_SUPPORTED_CAPACITY = ONE << (SHIFT * (MAX_SUPPORTED_DEPTH + 1u));
#define KL_MIN_CAPACITY_MSG "Pool has to be able to hold at least one object."
#define KL_MAX_CAPACITY_MSG "Maximum capacity is elements (64|32)^4 depending on architecture."

template<class T>
inline constexpr T Pow(const T base, cpu_word_t const exponent)
{
    return (exponent == 0) ? 1 : (base * Pow(base, exponent - 1));
}

template<class T>
inline constexpr size_t GetLevel(const T capacity, const size_t level = MAX_SUPPORTED_DEPTH)
{
    return level == 0 ? 0 : (capacity - 1) & (private_internal::MASK << (private_internal::SHIFT * (level))) ? level : GetLevel(capacity, level - 1u);
}

template<class T>
inline constexpr T MaxTreeSize(const T level)
{
    return (level == 0) ? 1 : Pow(BITS, level) + MaxTreeSize(level - 1u);
}

template<class T>
inline constexpr T OptimalAllocatedSize2(const T capacity, const size_t level)
{
    return (level == 0) ? 1 : (capacity - 1) / BITS + 1 + MaxTreeSize(level - 1u);
}

constexpr size_t BITS2 = Pow(BITS, 2);
constexpr size_t BITS3 = Pow(BITS, 3);
constexpr size_t BITS4 = Pow(BITS, 4);

inline cpu_word_t LeastBitClear(cpu_word_t word)
{
    cpu_word_t out = 0u;
    asm("bsf %1, %0"
        : "=r" (out)
        : "r" (~word));
    return out;
}

template <size_t Level>
size_t OffsetAndMark(cpu_word_t *allocated)
{
    (void)allocated;
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
    
    assert(offset < BITS2);
    return offset;
}

template<>
size_t OffsetAndMark<2>(cpu_word_t *allocated)
{
    size_t offset_on_level0 = LeastBitClear(allocated[0]);
    size_t offset_on_level1 = LeastBitClear(allocated[1 + offset_on_level0]);
    size_t offset_on_level2 = LeastBitClear(allocated[1 + BITS + offset_on_level0 * BITS + offset_on_level1]);
    size_t offset = offset_on_level0 * BITS2 + offset_on_level1 * BITS + offset_on_level2;

    allocated[1 + BITS + offset_on_level0 * BITS + offset_on_level1] |= (ONE << offset_on_level2);

    if (allocated[1 + BITS + offset_on_level0 * BITS + offset_on_level1] == FULL)
    {
        allocated[1 + offset_on_level0] |= (ONE << offset_on_level1);

        if (allocated[1 + offset_on_level0] == FULL)
        {
            allocated[0] |= (ONE << offset_on_level0);
        }
    }

    assert(offset < BITS3);
    return offset;
}

template<>
size_t OffsetAndMark<3>(cpu_word_t *allocated)
{
    size_t offset_on_level0 = LeastBitClear(allocated[0]);
    size_t offset_on_level1 = LeastBitClear(allocated[1 + offset_on_level0]);
    size_t offset_on_level2 = LeastBitClear(allocated[1 + BITS + offset_on_level0 * BITS + offset_on_level1]);
    size_t offset_on_level3 = LeastBitClear(allocated[1 + BITS + BITS2 + BITS2 * offset_on_level0 + BITS * offset_on_level1 + offset_on_level2]);
    size_t offset = offset_on_level0 * BITS3 + offset_on_level1 * BITS2 + offset_on_level2 * BITS + offset_on_level3;

    allocated[1 + BITS + BITS2 + BITS2 * offset_on_level0 + BITS * offset_on_level1 + offset_on_level2] |= (ONE << offset_on_level3);

    if (allocated[1 + BITS + BITS2 + BITS2 * offset_on_level0 + BITS * offset_on_level1 + offset_on_level2] == FULL)
    {
        allocated[1 + BITS + offset_on_level0 * BITS + offset_on_level1] |= (ONE << offset_on_level2);

        if (allocated[1 + BITS + offset_on_level0 * BITS + offset_on_level1] == FULL)
        {
            allocated[1 + offset_on_level0] |= (ONE << offset_on_level1);

            if (allocated[1 + offset_on_level0] == FULL)
            {
                allocated[0] |= (ONE << offset_on_level0);
            }
        }
    }

    assert(offset < BITS4);
    return offset;
}

template <size_t Level>
void Unmark(cpu_word_t *allocated, size_t offset)
{
    (void)allocated;
    (void)offset;
    assert("Only specialised instances of this function make sense." == 0);
}

template <>
void Unmark<0>(cpu_word_t *allocated, size_t offset)
{
    allocated[0] &= ~(ONE << offset);
}

// TODO check if shift & bithacks for division and modulo are faster

template <>
void Unmark<1>(cpu_word_t *allocated, size_t offset)
{
    allocated[offset / BITS + 1] &= ~(ONE << (offset % BITS));

    if (allocated[offset / BITS + 1] < FULL)
    {
        allocated[0] &= ~(ONE << (offset / BITS));
    }
}

template <>
void Unmark<2>(cpu_word_t *allocated, size_t offset)
{
    allocated[offset / BITS + BITS + 1] &= ~(ONE << (offset % BITS));

    if (allocated[offset / BITS + BITS + 1] < FULL)
    {
        allocated[offset / BITS2 + 1] &= ~(ONE << ((offset / BITS) % BITS));

        if (allocated[offset / BITS2 + 1] < FULL)
        {
            allocated[0] &= ~(ONE << (offset / BITS2));
        }
    }
}

template <>
void Unmark<3>(cpu_word_t *allocated, size_t offset)
{
    allocated[offset / BITS + BITS2 + BITS + 1] &= ~(ONE << (offset % BITS));

    if (allocated[offset / BITS + BITS2 + BITS + 1] < FULL)
    {
        allocated[offset / BITS2 + BITS + 1] &= ~(ONE << ((offset / BITS) % BITS));

        if (allocated[offset / BITS2 + BITS + 1] < FULL)
        {
            allocated[offset / BITS3 + 1] &= ~(ONE << ((offset / BITS2) % BITS));

            if (allocated[offset / BITS3 + 1] < FULL)
            {
                allocated[0] &= ~(ONE << (offset / BITS3));
            }
        }
    }
}
} // namespace private_internal

// Capacity set at compile time
template<typename T, size_t capacity>
class CompactPool
{
private:

    static_assert(capacity > 0, KL_MIN_CAPACITY_MSG);
    static_assert(capacity <= (private_internal::MAX_SUPPORTED_CAPACITY), KL_MAX_CAPACITY_MSG);

    static constexpr size_t level = private_internal::GetLevel(capacity);
    typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type aligned_storage_t;
    uint_fast32_t size_;
    uint_fast32_t max_utilisation_;
    aligned_storage_t buffer[capacity];
    private_internal::cpu_word_t allocated[private_internal::OptimalAllocatedSize2(capacity, level)];

public:

    CompactPool() :
        size_(0u),
        max_utilisation_(0u),
        buffer(),
        allocated()
    {}

    ~CompactPool()
    {}

    T *Alloc()
    {
        if (size_ == capacity)
        {
            // Full
            return nullptr;
        }

        ++size_;
        max_utilisation_ = std::max(size_, max_utilisation_);
        size_t offset = private_internal::OffsetAndMark<level>(allocated);
        return reinterpret_cast<T *>(&buffer[offset]);
    }

    void Free(T *p)
    {
        ptrdiff_t offset = reinterpret_cast<aligned_storage_t *>(p) - buffer;
        KL_COMPACT_POOL_CHECK_POINTER(offset, capacity, p);
        private_internal::Unmark<level>(allocated, offset);
        --size_;
    }

    size_t Size() const
    {
        return size_;
    }

    size_t MaxUtilisation() const
    {
        return max_utilisation_;
    }

    constexpr size_t Capacity()
    {
        return capacity;
    }

    constexpr size_t MaxCapacity()
    {
        return private_internal::MAX_SUPPORTED_CAPACITY;
    }
}; // class CompactPool

// Capacity set in runtime
template<typename T>
class CompactPool2
{
private:

    typedef typename std::aligned_storage<sizeof(T), alignof(T)>::type aligned_storage_t;
    typedef size_t (* OffsetAndMarkFn)(private_internal::cpu_word_t *allocated);
    typedef void (* UnmarkFn)(private_internal::cpu_word_t *allocated, size_t offset);
    uint_fast32_t capacity_;
    uint_fast32_t size_;
    uint_fast32_t max_utilisation_;
    uint_fast32_t depth_;
    aligned_storage_t *buffer_;
    private_internal::cpu_word_t *allocated_;
    OffsetAndMarkFn offset_and_mark_;
    UnmarkFn unmark_;

    OffsetAndMarkFn ChooseOffsetAndMarkFn()
    {
        switch(depth_)
        {
            case 0u: return private_internal::OffsetAndMark<0>;
            case 1u: return private_internal::OffsetAndMark<1>;
            case 2u: return private_internal::OffsetAndMark<2>;
            case 3u: return private_internal::OffsetAndMark<3>;
            default:
                assert("Invalid depth." == 0);
                return nullptr;
        }
    }

    UnmarkFn ChooseUnmarkFn()
    {
        switch(depth_)
        {
            case 0u: return private_internal::Unmark<0>;
            case 1u: return private_internal::Unmark<1>;
            case 2u: return private_internal::Unmark<2>;
            case 3u: return private_internal::Unmark<3>;
            default:
                assert("Invalid depth." == 0);
                return nullptr;
        }
    }

public:

    CompactPool2(size_t capacity) :
        capacity_(capacity),
        size_(0u),
        max_utilisation_(0u),
        depth_(private_internal::GetLevel(capacity)),
        buffer_(reinterpret_cast<aligned_storage_t *>(std::malloc(capacity_ * sizeof(aligned_storage_t)))),
        allocated_(reinterpret_cast<private_internal::cpu_word_t *>(std::calloc(private_internal::OptimalAllocatedSize2(capacity_, depth_), sizeof(private_internal::cpu_word_t)))),
        offset_and_mark_(ChooseOffsetAndMarkFn()),
        unmark_(ChooseUnmarkFn())
    {
        KL_COMPACT_POOL_CHECK_CAPACITY(capacity_);
    }

    ~CompactPool2()
    {
        std::free(buffer_);
        std::free(allocated_);
    }

    T *Alloc()
    {
        if (size_ == capacity_)
        {
            // Full
            return nullptr;
        }

        ++size_;
        max_utilisation_ = std::max(size_, max_utilisation_);
        size_t offset = offset_and_mark_(allocated_);
        return reinterpret_cast<T *>(&buffer_[offset]);
    }

    void Free(T *p)
    {
        ptrdiff_t offset = reinterpret_cast<aligned_storage_t *>(p) - buffer_;
        KL_COMPACT_POOL_CHECK_POINTER(offset, capacity_, p);
        unmark_(allocated_, offset);
        --size_;
    }

    size_t Size() const
    {
        return size_;
    }

    size_t MaxUtilisation() const
    {
        return max_utilisation_;
    }

    size_t Capacity() const
    {
        return capacity_;
    }

    constexpr size_t MaxCapacity()
    {
        return private_internal::MAX_SUPPORTED_CAPACITY;
    }
}; // class CompactPool2

} // namespace kl
#endif // KL_COMPACT_POOL_H_
