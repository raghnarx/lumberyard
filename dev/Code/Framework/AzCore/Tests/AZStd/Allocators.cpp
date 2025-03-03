/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "UserTypes.h"

#include <AzCore/std/allocator.h>
#include <AzCore/std/allocator_static.h>
#include <AzCore/std/allocator_ref.h>
#include <AzCore/std/allocator_stack.h>

#include <AzCore/Memory/SystemAllocator.h>


using namespace AZStd;
using namespace UnitTestInternal;

namespace UnitTest
{
    /**
     * Test for AZSTD provided default allocators.
     */

    /// Default allocator.
    class AllocatorDefaultTest
        : public AllocatorsTestFixture
    {
    public:
        void run()
        {
            const char name[] = "My test allocator";
            AZStd::allocator myalloc(name);
            AZ_TEST_ASSERT(strcmp(myalloc.get_name(), name) == 0);
            const char newName[] = "My new test allocator";
            myalloc.set_name(newName);
            AZ_TEST_ASSERT(strcmp(myalloc.get_name(), newName) == 0);

            AZStd::allocator::pointer_type data = myalloc.allocate(100, 1);
            AZ_TEST_ASSERT(data != 0);

            myalloc.deallocate(data, 100, 1);

            data = myalloc.allocate(50, 128);
            AZ_TEST_ASSERT(data != 0);

            myalloc.deallocate(data, 50, 128);

            AZStd::allocator myalloc2;
            AZ_TEST_ASSERT(myalloc == myalloc2); // always true
            AZ_TEST_ASSERT(!(myalloc != myalloc2)); // always false
        }
    };

    TEST_F(AllocatorDefaultTest, Test)
    {
        run();
    }

    /// Static buffer allocator.
    TEST(Allocator, StaticBuffer)
    {
        const int bufferSize = 500;
        typedef static_buffer_allocator<bufferSize, 4> buffer_alloc_type;

        const char name[] = "My test allocator";
        buffer_alloc_type myalloc(name);
        AZ_TEST_ASSERT(strcmp(myalloc.get_name(), name) == 0);
        const char newName[] = "My new test allocator";
        myalloc.set_name(newName);
        AZ_TEST_ASSERT(strcmp(myalloc.get_name(), newName) == 0);

        AZ_TEST_ASSERT(myalloc.get_max_size() == AZStd::size_t(bufferSize));
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        buffer_alloc_type::pointer_type data = myalloc.allocate(100, 1);
        AZ_TEST_ASSERT(data != 0);
        AZ_TEST_ASSERT(myalloc.get_max_size() == bufferSize - 100);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 100);

        myalloc.deallocate(data, 100, 1); // we can free the last allocation only
        AZ_TEST_ASSERT(myalloc.get_max_size() == bufferSize);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        data = myalloc.allocate(100, 1);
        myalloc.allocate(3, 1);
        myalloc.deallocate(data); // can't free allocation which is not the last.
        AZ_TEST_ASSERT(myalloc.get_max_size() == bufferSize - 103);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 103);

        myalloc.reset();
        AZ_TEST_ASSERT(myalloc.get_max_size() == AZStd::size_t(bufferSize));
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        data = myalloc.allocate(50, 64);
        AZ_TEST_ASSERT(data != 0);
        AZ_TEST_ASSERT(((AZStd::size_t)data & 63) == 0);
        AZ_TEST_ASSERT(myalloc.get_max_size() <= bufferSize - 50);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() >= 50);

        buffer_alloc_type myalloc2;
        AZ_TEST_ASSERT(myalloc == myalloc);
        AZ_TEST_ASSERT((myalloc2 != myalloc));
    }

    /// Static pool allocator.
    TEST(Allocator, StaticPool)
    {
        const int numNodes = 100;
        const char name[] = "My test allocator";
        const char newName[] = "My new test allocator";
        typedef static_pool_allocator<int, numNodes> int_node_pool_type;
        int_node_pool_type myalloc(name);
        AZ_TEST_ASSERT(strcmp(myalloc.get_name(), name) == 0);
        myalloc.set_name(newName);
        AZ_TEST_ASSERT(strcmp(myalloc.get_name(), newName) == 0);

        AZ_TEST_ASSERT(myalloc.get_max_size() == sizeof(int) * numNodes);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        int* data = reinterpret_cast<int*>(myalloc.allocate(sizeof(int), 1));
        AZ_TEST_ASSERT(data != 0);
        AZ_TEST_ASSERT(myalloc.get_max_size() == (numNodes - 1) * sizeof(int));
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == sizeof(int));

        myalloc.deallocate(data, sizeof(int), 1);
        AZ_TEST_ASSERT(myalloc.get_max_size() == sizeof(int) * numNodes);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        for (int i = 0; i < numNodes; ++i)
        {
            data = reinterpret_cast<int*>(myalloc.allocate(sizeof(int), 1));
            AZ_TEST_ASSERT(data != 0);
            AZ_TEST_ASSERT(myalloc.get_max_size() == (numNodes - (i + 1)) * sizeof(int));
            AZ_TEST_ASSERT(myalloc.get_allocated_size() == (i + 1) * sizeof(int));
        }

        myalloc.reset();
        AZ_TEST_ASSERT(myalloc.get_max_size() == numNodes * sizeof(int));
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        AZ_TEST_ASSERT(myalloc == myalloc);

        //////////////////////////////////////////////////////////////////////////
        // static pool allocator
        // Generally we can't use more then 16 byte alignment on the stack.
        // Some platforms might fail. Which is ok, higher alignment should be handled by US. Or not on the stack.
        const int dataAlignment = 16;

        typedef aligned_storage<sizeof(int), dataAlignment>::type aligned_int_type;
        typedef static_pool_allocator<aligned_int_type, numNodes> aligned_int_node_pool_type;
        aligned_int_node_pool_type myaligned_pool;
        aligned_int_type* aligned_data = reinterpret_cast<aligned_int_type*>(myaligned_pool.allocate(sizeof(aligned_int_type), dataAlignment));

        AZ_TEST_ASSERT(aligned_data != 0);
        AZ_TEST_ASSERT(((AZStd::size_t)aligned_data & (dataAlignment - 1)) == 0);
        AZ_TEST_ASSERT(myaligned_pool.get_max_size() == (numNodes - 1) * sizeof(aligned_int_type));
        AZ_TEST_ASSERT(myaligned_pool.get_allocated_size() == sizeof(aligned_int_type));

        myaligned_pool.deallocate(aligned_data, sizeof(aligned_int_type), dataAlignment); // Make sure we free what we have allocated.
        //////////////////////////////////////////////////////////////////////////
    }

    /// Reference allocator.
    TEST(Allocator, Reference)
    {
        const int bufferSize = 500;
        typedef static_buffer_allocator<bufferSize, 4> buffer_alloc_type;
        buffer_alloc_type shared_allocator("Shared allocator");

        typedef allocator_ref<buffer_alloc_type> ref_allocator_type;

        const char name1[] = "Ref allocator1";
        ref_allocator_type ref_allocator1(shared_allocator, name1);
        const char name2[] = "Ref allocator2";
        ref_allocator_type ref_allocator2(shared_allocator, name2);

        AZ_TEST_ASSERT(strcmp(ref_allocator1.get_name(), name1) == 0);
        AZ_TEST_ASSERT(strcmp(ref_allocator2.get_name(), name2) == 0);

        const char newName1[] = "Ref new allocator1";
        ref_allocator1.set_name(newName1);
        AZ_TEST_ASSERT(strcmp(ref_allocator1.get_name(), newName1) == 0);
        const char newName2[] = "Ref new allocator2";
        ref_allocator2.set_name(newName2);
        AZ_TEST_ASSERT(strcmp(ref_allocator2.get_name(), newName2) == 0);

        AZ_TEST_ASSERT(ref_allocator2.get_allocator() == ref_allocator1.get_allocator());

        ref_allocator_type::pointer_type data1 = ref_allocator1.allocate(10, 1);
        AZ_TEST_ASSERT(data1 != 0);
        AZ_TEST_ASSERT(ref_allocator1.get_max_size() == bufferSize - 10);
        AZ_TEST_ASSERT(ref_allocator1.get_allocated_size() == 10);
        AZ_TEST_ASSERT(shared_allocator.get_max_size() == bufferSize - 10);
        AZ_TEST_ASSERT(shared_allocator.get_allocated_size() == 10);

        ref_allocator_type::pointer_type data2 = ref_allocator2.allocate(10, 1);
        AZ_TEST_ASSERT(data2 != 0);
        AZ_TEST_ASSERT(ref_allocator2.get_max_size() <= bufferSize - 20);
        AZ_TEST_ASSERT(ref_allocator2.get_allocated_size() >= 20);
        AZ_TEST_ASSERT(shared_allocator.get_max_size() <= bufferSize - 20);
        AZ_TEST_ASSERT(shared_allocator.get_allocated_size() >= 20);

        shared_allocator.reset();

        data1 = ref_allocator1.allocate(10, 32);
        AZ_TEST_ASSERT(data1 != 0);
        AZ_TEST_ASSERT(ref_allocator1.get_max_size() <= bufferSize - 10);
        AZ_TEST_ASSERT(ref_allocator1.get_allocated_size() >= 10);
        AZ_TEST_ASSERT(shared_allocator.get_max_size() <= bufferSize - 10);
        AZ_TEST_ASSERT(shared_allocator.get_allocated_size() >= 10);

        data2 = ref_allocator2.allocate(10, 32);
        AZ_TEST_ASSERT(data2 != 0);
        AZ_TEST_ASSERT(ref_allocator1.get_max_size() <= bufferSize - 20);
        AZ_TEST_ASSERT(ref_allocator1.get_allocated_size() >= 20);
        AZ_TEST_ASSERT(shared_allocator.get_max_size() <= bufferSize - 20);
        AZ_TEST_ASSERT(shared_allocator.get_allocated_size() >= 20);

        AZ_TEST_ASSERT(ref_allocator1 == ref_allocator2);
        AZ_TEST_ASSERT(!(ref_allocator1 != ref_allocator2));
    }

    /// Stack buffer allocator.
    TEST(Allocator, Stack)
    {
        size_t bufferSize = 500;

        const char name[] = "My test allocator";
        stack_allocator myalloc(alloca(bufferSize), bufferSize, name);
        AZ_TEST_ASSERT(strcmp(myalloc.get_name(), name) == 0);
        const char newName[] = "My new test allocator";
        myalloc.set_name(newName);
        AZ_TEST_ASSERT(strcmp(myalloc.get_name(), newName) == 0);

        AZ_TEST_ASSERT(myalloc.get_max_size() == AZStd::size_t(bufferSize));
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        stack_allocator::pointer_type data = myalloc.allocate(100, 1);
        AZ_TEST_ASSERT(data != 0);
        AZ_TEST_ASSERT(myalloc.get_max_size() == bufferSize - 100);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 100);

        myalloc.deallocate(data, 100, 1); // this allocator doesn't free data
        AZ_TEST_ASSERT(myalloc.get_max_size() == bufferSize - 100);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 100);

        myalloc.reset();
        AZ_TEST_ASSERT(myalloc.get_max_size() == AZStd::size_t(bufferSize));
        AZ_TEST_ASSERT(myalloc.get_allocated_size() == 0);

        data = myalloc.allocate(50, 64);
        AZ_TEST_ASSERT(data != 0);
        AZ_TEST_ASSERT(((AZStd::size_t)data & 63) == 0);
        AZ_TEST_ASSERT(myalloc.get_max_size() <= bufferSize - 50);
        AZ_TEST_ASSERT(myalloc.get_allocated_size() >= 50);

        AZ_STACK_ALLOCATOR(myalloc2, 200); // test the macro declaration

        AZ_TEST_ASSERT(myalloc2.get_max_size() == 200);

        AZ_TEST_ASSERT(myalloc == myalloc);
        AZ_TEST_ASSERT((myalloc2 != myalloc));
    }
}
