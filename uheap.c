#include <inc/lib.h>

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

int FirstTimeFlag = 1;
void InitializeUHeap() {
	if (FirstTimeFlag) {
		initialize_dyn_block_system();
		cprintf("DYNAMIC BLOCK SYSTEM IS INITIALIZED\n");
#if UHP_USE_BUDDY
		initialize_buddy();
		cprintf("BUDDY SYSTEM IS INITIALIZED\n");
#endif
		FirstTimeFlag = 0;
	}
}

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//=================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//=================================
void initialize_dyn_block_system() {
	//TODO: [PROJECT MS3] [USER HEAP - USER SIDE] initialize_dyn_block_system
	// your code is here, remove the panic and write your code
//	panic("initialize_dyn_block_system() is not implemented yet...!!");

	//[1] Initialize two lists (AllocMemBlocksList & FreeMemBlocksList) [Hint: use LIST_INIT()]
	LIST_INIT(&AllocMemBlocksList);
	LIST_INIT(&FreeMemBlocksList);

	MAX_MEM_BLOCK_CNT = (USER_HEAP_MAX - USER_HEAP_START) / PAGE_SIZE;
	uint32 sizealloc = ROUNDUP((sizeof(struct MemBlock) * MAX_MEM_BLOCK_CNT),
			PAGE_SIZE);
	//	allocate_chunk(ptr_page_directory, KERNEL_HEAP_START, sizealloc,
	//	PERM_WRITEABLE);

	sys_allocate_chunk(USER_DYN_BLKS_ARRAY, sizealloc,
	PERM_WRITEABLE | PERM_USER);
	MemBlockNodes = (struct MemBlock*) USER_DYN_BLKS_ARRAY;
	initialize_MemBlocksList(MAX_MEM_BLOCK_CNT);
	struct MemBlock *free_alloc = LIST_FIRST(&AvailableMemBlocksList);
	free_alloc->sva = USER_HEAP_START;
	free_alloc->size = (USER_HEAP_MAX - USER_HEAP_START);
	LIST_REMOVE(&AvailableMemBlocksList, free_alloc);
	LIST_INSERT_HEAD(&FreeMemBlocksList, free_alloc);
}

//=================================
// [2] ALLOCATE SPACE IN USER HEAP:
//=================================

void* malloc(uint32 size) {
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	InitializeUHeap();
	if (size == 0)
		return NULL;
	//==============================================================
	//==============================================================

	//TODO: [PROJECT MS3] [USER HEAP - USER SIDE] malloc
	// your code is here, remove the panic and write your code
	//panic("malloc() is not implemented yet...!!");

	// Steps:
	//	1) Implement FF strategy to search the heap for suitable space
	//		to the required allocation size (space should be on 4 KB BOUNDARY)
	//	2) if no suitable space found, return NULL
	// 	3) Return pointer containing the virtual address of allocated space,
	//
	//Use sys_isUHeapPlacementStrategyFIRSTFIT()... to check the current strategy

	size = ROUNDUP(size, PAGE_SIZE);
	struct MemBlock* alloc = NULL;

	if (sys_isUHeapPlacementStrategyFIRSTFIT() == 1) {
		alloc = alloc_block_FF(size);
		insert_sorted_allocList(alloc);
	}

	if (alloc != NULL) {

//			sys_allocate_chunk(USER_HEAP_START,ROUNDUP(alloc->size, PAGE_SIZE),
//					PERM_WRITEABLE );
		//allocate_chunk(ptr_page_directory, alloc->sva,
		//		ROUNDUP(alloc->size, PAGE_SIZE), (PERM_WRITEABLE));

		return (void*) ROUNDUP(alloc->sva, PAGE_SIZE);

	}
	return NULL;

}

//=================================
// [3] FREE SPACE FROM USER HEAP:
//=================================
// free():
//	This function frees the allocation of the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	FROM main memory AND free pages from page file then switch back to the user again.
//
//	We can use sys_free_user_mem(uint32 virtual_address, uint32 size); which
//		switches to the kernel mode, calls free_user_mem() in
//		"kern/mem/chunk_operations.c", then switch back to the user mode here
//	the free_user_mem function is empty, make sure to implement it.
void free(void* virtual_address) {
	//TODO: [PROJECT MS3] [USER HEAP - USER SIDE] free
		// your code is here, remove the panic and write your code
		//panic("free() is not implemented yet...!!");
		//print_mem_block_lists();
		uint32 va = (uint32) virtual_address;
		struct MemBlock* temp = find_block(&AllocMemBlocksList,ROUNDDOWN(va, PAGE_SIZE));
		struct MemBlock* toFree = temp;
		if (temp != NULL) {
			sys_free_user_mem(temp->sva, temp->size);
			LIST_REMOVE(&AllocMemBlocksList, toFree);
			insert_sorted_with_merge_freeList(temp);
		}

		//you should get the size of the given allocation using its address
		//you need to call sys_free_user_mem()
		//refer to the project presentation and documentation for details
}

//=================================
// [4] ALLOCATE SHARED VARIABLE:
//=================================
void* smalloc(char *sharedVarName, uint32 size, uint8 isWritable) {
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	InitializeUHeap();
	if (size == 0)
		return NULL;
	//==============================================================
	size = ROUNDUP(size, PAGE_SIZE);

	struct MemBlock* ff_retuen = NULL;

	if (sys_isUHeapPlacementStrategyFIRSTFIT() == 1)
		ff_retuen = alloc_block_FF(size);

	if (ff_retuen != NULL && sys_createSharedObject(sharedVarName, size, isWritable,
			(void*) ff_retuen->sva) >= 0) {
			return (void*) ff_retuen->sva;//roundup
	}
	return NULL;

}

//========================================
// [5] SHARE ON ALLOCATED SHARED VARIABLE:
//========================================
void* sget(int32 ownerEnvID, char *sharedVarName) {
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	InitializeUHeap();
	//==============================================================



	uint32 size = sys_getSizeOfSharedObject(ownerEnvID, sharedVarName);

	if (size == E_SHARED_MEM_NOT_EXISTS)
		return NULL;

	size = ROUNDUP(size, PAGE_SIZE);
	struct MemBlock* alloc = NULL;

	if (sys_isUHeapPlacementStrategyFIRSTFIT() == 1)
		alloc = alloc_block_FF(size);

	if (alloc != NULL && sys_getSharedObject(ownerEnvID, sharedVarName,
			(void*) alloc->sva) >= 0)
			return (void*) alloc->sva;


	return NULL;

}

//==================================================================================//
//============================== BONUS FUNCTIONS ===================================//
//==================================================================================//

//=================================
// REALLOC USER SPACE:
//=================================
//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to malloc().
//	A call with new_size = zero is equivalent to free().

//  Hint: you may need to use the sys_move_user_mem(...)
//		which switches to the kernel mode, calls move_user_mem(...)
//		in "kern/mem/chunk_operations.c", then switch back to the user mode here
//	the move_user_mem() function is empty, make sure to implement it.
void *realloc(void *virtual_address, uint32 new_size) {
	//==============================================================
	//DON'T CHANGE THIS CODE========================================
	InitializeUHeap();
	//==============================================================
	// [USER HEAP - USER SIDE] realloc
	// Write your code here, remove the panic and write your code
	panic("realloc() is not implemented yet...!!");
}

//=================================
// FREE SHARED VARIABLE:
//=================================
//	This function frees the shared variable at the given virtual_address
//	To do this, we need to switch to the kernel, free the pages AND "EMPTY" PAGE TABLES
//	from main memory then switch back to the user again.
//
//	use sys_freeSharedObject(...); which switches to the kernel mode,
//	calls freeSharedObject(...) in "shared_memory_manager.c", then switch back to the user mode here
//	the freeSharedObject() function is empty, make sure to implement it.

void sfree(void* virtual_address) {
	//TODO: [PROJECT MS3 - BONUS] [SHARING - USER SIDE] sfree()

	// Write your code here, remove the panic and write your code
	panic("sfree() is not implemented yet...!!");
}

//==================================================================================//
//========================== MODIFICATION FUNCTIONS ================================//
//==================================================================================//
void expand(uint32 newSize) {
	panic("Not Implemented");

}
void shrink(uint32 newSize) {
	panic("Not Implemented");

}
void freeHeap(void* virtual_address) {
	panic("Not Implemented");
}
