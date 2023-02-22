#include "kheap.h"

#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

//==================================================================//
//==================================================================//
//NOTE: All kernel heap allocations are multiples of PAGE_SIZE (4KB)//
//==================================================================//
//==================================================================//
static uint32 arr_phy_to_vir[1024 * 1024];

void initialize_dyn_block_system() {

	//[1] Initialize two lists (AllocMemBlocksList & FreeMemBlocksList) [Hint: use LIST_INIT()]
	LIST_INIT(&AllocMemBlocksList);
	LIST_INIT(&FreeMemBlocksList);

#if STATIC_MEMBLOCK_ALLOC
	//DO NOTHING
#else

	MAX_MEM_BLOCK_CNT = (KERNEL_HEAP_MAX - KERNEL_HEAP_START) / PAGE_SIZE;
	uint32 sizealloc = ROUNDUP((sizeof(struct MemBlock) * MAX_MEM_BLOCK_CNT),
			PAGE_SIZE);
	allocate_chunk(ptr_page_directory, KERNEL_HEAP_START, sizealloc,
	PERM_WRITEABLE);

//
//	/*[2] Dynamically allocate the array of MemBlockNodes
//	 * 	remember to:
//	 * 		1. set MAX_MEM_BLOCK_CNT with the chosen size of the array
//	 * 		2. allocation should be aligned on PAGE boundary
//	 * 	HINT: can use alloc_chunk(...) function
//	 */

	MemBlockNodes = (struct MemBlock*) KERNEL_HEAP_START;
	initialize_MemBlocksList(MAX_MEM_BLOCK_CNT);
	struct MemBlock *free_alloc = LIST_FIRST(&AvailableMemBlocksList);
	free_alloc->sva = KERNEL_HEAP_START + sizealloc;
	free_alloc->size = (KERNEL_HEAP_MAX - KERNEL_HEAP_START) - sizealloc;
	LIST_REMOVE(&AvailableMemBlocksList, free_alloc);
	LIST_INSERT_HEAD(&FreeMemBlocksList, free_alloc);
#endif

	//[3] Initialize AvailableMemBlocksList by filling it with the MemBlockNodes
	//[4] Insert a new MemBlock with the remaining heap size into the FreeMemBlocksList
}

void* kmalloc(unsigned int size) {
	size = ROUNDUP(size, PAGE_SIZE);
	struct MemBlock* alloc = NULL;
	if (isKHeapPlacementStrategyNEXTFIT() == 1) {
		alloc = alloc_block_NF(size);
	}
	if (isKHeapPlacementStrategyFIRSTFIT() == 1) {
		alloc = alloc_block_FF(size);

	}
	if (isKHeapPlacementStrategyBESTFIT() == 1) {
		alloc = alloc_block_BF(size);
	}

	if (alloc != NULL) {
		insert_sorted_allocList(alloc);

		allocate_chunk(ptr_page_directory, alloc->sva,
				ROUNDUP(alloc->size, PAGE_SIZE), (PERM_WRITEABLE));

		struct FrameInfo* frame_info_ptr;
		// you can delete loop if you choose second option in kheap_virtual_address()
		for (uint32 i = alloc->sva; i < ROUNDUP((size + alloc->sva), PAGE_SIZE);
				i += PAGE_SIZE)
				{
			uint32 *ptr_page_table = NULL;
			get_page_table(ptr_page_directory, i, &ptr_page_table);
			frame_info_ptr = get_frame_info(ptr_page_directory, i,
					&ptr_page_table);
			arr_phy_to_vir[to_physical_address(frame_info_ptr) / PAGE_SIZE] = i;
		}

		return (void*) (ROUNDUP(alloc->sva, PAGE_SIZE));

	}
	return NULL;
}

void kfree(void* virtual_address) {

	uint32 va = (uint32) virtual_address;
	struct MemBlock* toFree = find_block(&AllocMemBlocksList, va);
	if (toFree != NULL) {
		uint32 end = toFree->sva + toFree->size;
		uint32 start = toFree->sva;
		struct FrameInfo* frame_info_ptr;
		LIST_REMOVE(&AllocMemBlocksList, toFree);
		for (uint32 i = start; i < end; i += PAGE_SIZE) {
			uint32 *ptr_page_table = NULL;
			get_page_table(ptr_page_directory, i, &ptr_page_table);
			frame_info_ptr = get_frame_info(ptr_page_directory, i,
					&ptr_page_table);
			arr_phy_to_vir[to_physical_address(frame_info_ptr) / PAGE_SIZE] = 0;
			unmap_frame(ptr_page_directory, i);
		}
		insert_sorted_with_merge_freeList(toFree);
	}
}

unsigned int kheap_virtual_address(unsigned int physical_address) {
	//TODO: [PROJECT MS2] [KERNEL HEAP] kheap_virtual_address
	// Write your code here, remove the panic and write your code

// option 1

	return arr_phy_to_vir[physical_address / PAGE_SIZE];

	//OR you can Run ..

// option 2

	//return to_frame_info(physical_address)->va;

	//return the virtual address corresponding to given physical_address
	//refer to the project presentation and documentation for details
	//EFFICIENT IMPLEMENTATION ~O(1) IS REQUIRED ==================

}

unsigned int kheap_physical_address(unsigned int virtual_address) {
	//TODO: [PROJECT MS2] [KERNEL HEAP] kheap_physical_address
	// Write your code here, remove the panic and write your code
	//panic("kheap_physical_address() is not implemented yet...!!");

	uint32 physical_address;
	uint32* ptr_page_table = NULL;
	struct FrameInfo* ptr_frame_info = get_frame_info(ptr_page_directory,
			virtual_address, &ptr_page_table);

	physical_address = (ptr_frame_info - frames_info) << 12;
	uint32 presentbit = ptr_page_table[PTX(virtual_address)] & PERM_PRESENT;
	if (presentbit == 0) {
		return 0;
	}
	return physical_address;

	//return the physical address corresponding to given virtual_address
	//refer to the project presentation and documentation for details
}

void kfreeall() {
	panic("Not implemented!");

}

void kshrink(uint32 newSize) {
	panic("Not implemented!");
}

void kexpand(uint32 newSize) {
	panic("Not implemented!");
}

//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().

void *krealloc(void *virtual_address, uint32 new_size) {
	//TODO: [PROJECT MS2 - BONUS] [KERNEL HEAP] krealloc
	// Write your code here, remove the panic and write your code
	//panic("krealloc() is not implemented yet...!!");

	uint32 va = (uint32) virtual_address;

	if (virtual_address == NULL) {

		return kmalloc(new_size);
	} else if (new_size == 0) {

		kfree(virtual_address);
		return NULL;
	} else {

		struct MemBlock* block = find_block(&AllocMemBlocksList, va);
		if (block->size >= new_size)
			return (void*) block->sva;
		else if (new_size > block->size) {
			if (block->prev_next_info.le_next != NULL
					&& block->size + block->prev_next_info.le_next->size
							>= new_size) {

				int oldsize = block->size;
				uint32 num_pages = ROUNDUP((new_size - oldsize),
						PAGE_SIZE)/PAGE_SIZE;
				allocate_chunk(ptr_page_directory, block->sva + oldsize,
						num_pages * PAGE_SIZE, PERM_WRITEABLE);
				return (void*) block->sva;
			}

			else{

			new_size = ROUNDUP(new_size, PAGE_SIZE);
			struct MemBlock* temp = alloc_block_BF(new_size);
			int oldsize = block->size;

			uint32 num_pages = ROUNDUP((new_size-oldsize),PAGE_SIZE) / PAGE_SIZE;
			uint32 num_pages_old = ROUNDUP(oldsize,PAGE_SIZE) / PAGE_SIZE;
			uint32 num_pages_new = ROUNDUP(new_size,PAGE_SIZE) / PAGE_SIZE;

			if (temp != NULL) {

				allocate_chunk(ptr_page_directory, temp->sva,
						(num_pages * PAGE_SIZE) - (num_pages_old * PAGE_SIZE),
						PERM_WRITEABLE);


				return (void*) temp->sva;
			} else
				return NULL;
			}
		}
	}

	return NULL;
}
