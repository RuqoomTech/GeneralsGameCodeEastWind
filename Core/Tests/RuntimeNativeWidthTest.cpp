/*
** Native-width runtime regression test.
**
** Exercises the production WWLib object-pool template with enough allocations
** to force multiple backing blocks. The historical implementation stored the
** block-list link through uint32*, which was only pointer-sized on Win32. On a
** real 64-bit Windows ABI that made the block header four bytes too short and
** allowed the first free-list pointer to overlap the allocation-chain link.
*/

#include "Precompiled/CppMacros.h"

#include <cstddef>
#include <cstdint>
#include <cwctype>
#include <set>

#include <gtest/gtest.h>

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

static_assert(sizeof(std::uintptr_t) == sizeof(void *), "uintptr_t must match native pointer width");
static_assert(alignof(PoolProbe) <= alignof(void *),
    "native-width pool probe must not require stronger alignment than a pointer");
static_assert(sizeof(PoolProbe) >= sizeof(PoolProbe *), "pool free-list link must fit in an object slot");
}


TEST(RuntimeNativeWidth, FastAllocators)
{
    FastFixedAllocator fixed(9);
    void *fixedItems[96] = {};
    for (std::size_t i = 0; i < 96; ++i)
    {
        fixedItems[i] = fixed.Alloc();
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(fixedItems[i]) % alignof(void*), 0U)
            << "FastFixedAllocator returned misaligned storage";
    }
    for (void *item : fixedItems)
        fixed.Free(item);

    FastAllocatorGeneral general;
    const unsigned int sizes[] = {1, 7, 8, 9, 15, 16, 17, 31, 33, 127, 511, 2049};
    for (unsigned int size : sizes)
    {
        void *storage = general.Alloc(size);
        ASSERT_NE(storage, nullptr) << "FastAllocatorGeneral returned null for " << size << " bytes";
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(storage) % alignof(void*), 0U)
            << "FastAllocatorGeneral returned misaligned storage";
        general.Free(storage);
    }

    unsigned char *grow = static_cast<unsigned char*>(general.Alloc(8));
    for (unsigned int i = 0; i < 8; ++i)
        grow[i] = static_cast<unsigned char>(0xA0u + i);
    grow = static_cast<unsigned char*>(general.Realloc(grow, 64));
    for (unsigned int i = 0; i < 8; ++i)
    {
        ASSERT_EQ(grow[i], static_cast<unsigned char>(0xA0u + i))
            << "FastAllocatorGeneral grow realloc lost payload data";
    }

    for (unsigned int i = 0; i < 64; ++i)
        grow[i] = static_cast<unsigned char>(i);
    unsigned char *shrink = static_cast<unsigned char*>(general.Realloc(grow, 5));
    for (unsigned int i = 0; i < 5; ++i)
    {
        ASSERT_EQ(shrink[i], static_cast<unsigned char>(i))
            << "FastAllocatorGeneral shrink realloc lost payload data";
    }
    general.Free(shrink);
}

TEST(RuntimeNativeWidth, ObjectPoolBlocks)
{
    // 35 entries force five backing allocations with BLOCK_SIZE=8, exercising
    // both the native-width block chain and free-list traversal.
    ObjectPoolClass<PoolProbe, 8> pool;
    PoolProbe *items[35] = {};
    std::set<PoolProbe *> addresses;

    for (std::size_t i = 0; i < 35; ++i)
    {
        items[i] = pool.Allocate_Object_Memory();
        ASSERT_NE(items[i], nullptr) << "ObjectPoolClass returned null";
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(items[i]) % alignof(PoolProbe), 0U)
            << "ObjectPoolClass returned misaligned storage";
        ASSERT_TRUE(addresses.insert(items[i]).second) << "ObjectPoolClass returned duplicate live storage";

        items[i]->nativePointer = items[i];
        items[i]->marker = kMarker;
        items[i]->index = static_cast<std::uint32_t>(i);
    }

    for (std::size_t i = 0; i < 35; ++i)
    {
        ASSERT_TRUE(items[i]->nativePointer == items[i] && items[i]->marker == kMarker && items[i]->index == i)
            << "ObjectPoolClass storage was corrupted at item " << i;
    }

    for (std::size_t i = 0; i < 35; ++i)
    {
        pool.Free_Object_Memory(items[i]);
    }
}
