/*
** Step 04C native-width runtime regression.
**
** Exercises the production WWLib object-pool template with enough allocations
** to force multiple backing blocks. The historical implementation stored the
** block-list link through uint32*, which was only pointer-sized on Win32. On a
** real 64-bit Windows ABI that made the block header four bytes too short and
** allowed the first free-list pointer to overlap the allocation-chain link.
*/

#include "Utility/CppMacros.h"

#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <iostream>
#include <set>

#include "WWLib/FastAllocator.h"
#include "WWLib/mempool.h"

namespace
{
struct PoolProbe
{
    void *nativePointer;
    std::uintptr_t marker;
    std::uint32_t index;
};

constexpr std::uintptr_t kMarker = static_cast<std::uintptr_t>(UINT32_C(0x5A17C0DE));
}


int testFastAllocator()
{
    FastFixedAllocator fixed(9);
    void *fixedItems[96] = {};
    for (std::size_t i = 0; i < 96; ++i)
    {
        fixedItems[i] = fixed.Alloc();
        if ((reinterpret_cast<std::uintptr_t>(fixedItems[i]) % alignof(void*)) != 0)
        {
            std::cerr << "FastFixedAllocator returned misaligned storage\n";
            return 10;
        }
    }
    for (void *item : fixedItems)
        fixed.Free(item);

    FastAllocatorGeneral general;
    const unsigned int sizes[] = {1, 7, 8, 9, 15, 16, 17, 31, 33, 127, 511, 2049};
    for (unsigned int size : sizes)
    {
        void *storage = general.Alloc(size);
        if (storage == nullptr || (reinterpret_cast<std::uintptr_t>(storage) % alignof(void*)) != 0)
        {
            std::cerr << "FastAllocatorGeneral returned misaligned storage\n";
            return 11;
        }
        general.Free(storage);
    }

    unsigned char *grow = static_cast<unsigned char*>(general.Alloc(8));
    for (unsigned int i = 0; i < 8; ++i)
        grow[i] = static_cast<unsigned char>(0xA0u + i);
    grow = static_cast<unsigned char*>(general.Realloc(grow, 64));
    for (unsigned int i = 0; i < 8; ++i)
    {
        if (grow[i] != static_cast<unsigned char>(0xA0u + i))
        {
            std::cerr << "FastAllocatorGeneral grow realloc lost payload data\n";
            return 12;
        }
    }

    for (unsigned int i = 0; i < 64; ++i)
        grow[i] = static_cast<unsigned char>(i);
    unsigned char *shrink = static_cast<unsigned char*>(general.Realloc(grow, 5));
    for (unsigned int i = 0; i < 5; ++i)
    {
        if (shrink[i] != static_cast<unsigned char>(i))
        {
            std::cerr << "FastAllocatorGeneral shrink realloc lost payload data\n";
            return 13;
        }
    }
    general.Free(shrink);
    return 0;
}

int main()
{
    static_assert(sizeof(std::uintptr_t) == sizeof(void *), "uintptr_t must match native pointer width");
    static_assert(alignof(PoolProbe) <= alignof(void *),
        "native-width pool probe must not require stronger alignment than a pointer");
    static_assert(sizeof(PoolProbe) >= sizeof(PoolProbe *), "pool free-list link must fit in an object slot");

    if (const int fastAllocatorResult = testFastAllocator(); fastAllocatorResult != 0)
        return fastAllocatorResult;

    // 35 entries force five backing allocations with BLOCK_SIZE=8, exercising
    // both the native-width block chain and free-list traversal.
    ObjectPoolClass<PoolProbe, 8> pool;
    PoolProbe *items[35] = {};
    std::set<PoolProbe *> addresses;

    for (std::size_t i = 0; i < 35; ++i)
    {
        items[i] = pool.Allocate_Object_Memory();
        if (items[i] == nullptr)
        {
            std::cerr << "ObjectPoolClass returned null\n";
            return 1;
        }

        if ((reinterpret_cast<std::uintptr_t>(items[i]) % alignof(PoolProbe)) != 0)
        {
            std::cerr << "ObjectPoolClass returned misaligned storage\n";
            return 2;
        }

        if (!addresses.insert(items[i]).second)
        {
            std::cerr << "ObjectPoolClass returned duplicate live storage\n";
            return 3;
        }

        items[i]->nativePointer = items[i];
        items[i]->marker = kMarker;
        items[i]->index = static_cast<std::uint32_t>(i);
    }

    for (std::size_t i = 0; i < 35; ++i)
    {
        if (items[i]->nativePointer != items[i] || items[i]->marker != kMarker || items[i]->index != i)
        {
            std::cerr << "ObjectPoolClass storage was corrupted\n";
            return 4;
        }
    }

    for (std::size_t i = 0; i < 35; ++i)
    {
        pool.Free_Object_Memory(items[i]);
    }

    std::cout << "Step 04C native-width runtime allocator guard passed on "
              << (sizeof(void *) * 8) << "-bit pointers.\n";
    return 0;
}
