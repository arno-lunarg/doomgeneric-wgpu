// Prebuilt Dawn is compiled with MSVC 14.50+ (VS 2026). Its STL calls vectorized helpers
// that the VS 2022 toolset (14.44) msvcprt.lib lacks, e.g.:
//   webgpu_dawn.lib : error LNK2019: unresolved external symbol __std_rotate
// Scalar versions below, signatures from microsoft/STL stl/inc/xutility and stl/inc/algorithm.
// Compiled out on 14.50+, where msvcprt.lib provides them.

#if defined(_MSC_VER) && _MSC_VER < 1950

#include <stdint.h>

// Byte-wise std::rotate of [first, last) so that mid becomes first. Callers only pass trivially
// copyable ranges. Example: "abcde", mid at 'c' -> "cdeab".
static void ReverseBytes(unsigned char* first, unsigned char* last)
{
    while (first < last)
    {
        unsigned char tmp = *first;
        *first++          = *--last;
        *last             = tmp;
    }
}

void __stdcall __std_rotate(void* first, void* mid, void* last)
{
    ReverseBytes(first, mid);
    ReverseBytes(mid, last);
    ReverseBytes(first, last);
}

// std::min_element / std::max_element: first occurrence, `last` when empty.
const void* __stdcall __std_min_element_4u(const void* first, const void* last)
{
    const uint32_t* best = first;
    for (const uint32_t* p = first; p != last; p++)
        if (*p < *best)
            best = p;
    return best;
}

const void* __stdcall __std_min_element_8u(const void* first, const void* last)
{
    const uint64_t* best = first;
    for (const uint64_t* p = first; p != last; p++)
        if (*p < *best)
            best = p;
    return best;
}

const void* __stdcall __std_max_element_4u(const void* first, const void* last)
{
    const uint32_t* best = first;
    for (const uint32_t* p = first; p != last; p++)
        if (*p > *best)
            best = p;
    return best;
}

// std::unique: collapses runs of equal values, returns the new end. Example: 1 1 2 2 1 -> 1 2 1.
void* __stdcall __std_unique_4(void* first, void* last)
{
    uint32_t* out = first;
    if (first == last)
        return last;
    for (uint32_t* p = (uint32_t*)first + 1; p != last; p++)
        if (*p != *out)
            *++out = *p;
    return out + 1;
}

void* __stdcall __std_unique_8(void* first, void* last)
{
    uint64_t* out = first;
    if (first == last)
        return last;
    for (uint64_t* p = (uint64_t*)first + 1; p != last; p++)
        if (*p != *out)
            *++out = *p;
    return out + 1;
}

#endif
