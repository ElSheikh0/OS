/*
 * dyn_block_management.c
 *
 *  Created on: Sep 21, 2022
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"

//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//===========================
// PRINT MEM BLOCK LISTS:
//===========================
void print_mem_block_lists() {
	cprintf("\n=========================================\n");
	struct MemBlock* blk;
	struct MemBlock* lastBlk = NULL;
	cprintf("\nFreeMemBlocksList:\n");
	uint8 sorted = 1;
	LIST_FOREACH(blk, &FreeMemBlocksList)
	{
		if (lastBlk && blk->sva < lastBlk->sva + lastBlk->size)
			sorted = 0;
		cprintf("[%x, %x)-->", blk->sva, blk->sva + blk->size);
		lastBlk = blk;
	}
	if (!sorted)
		cprintf("\nFreeMemBlocksList is NOT SORTED!!\n");

	lastBlk = NULL;
	cprintf("\nAllocMemBlocksList:\n");
	sorted = 1;
	LIST_FOREACH(blk, &AllocMemBlocksList)
	{
		if (lastBlk && blk->sva < lastBlk->sva + lastBlk->size)
			sorted = 0;
		cprintf("[%x, %x)-->", blk->sva, blk->sva + blk->size);
		lastBlk = blk;
	}
	if (!sorted)
		cprintf("\nAllocMemBlocksList is NOT SORTED!!\n");
	cprintf("\n=========================================\n");

}

//********************************************************************************//
//********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//

//===============================
// [1] INITIALIZE AVAILABLE LIST:
//===============================

void initialize_MemBlocksList(uint32 numOfBlocks) {
	LIST_INIT(&AvailableMemBlocksList);
	int i;
	for (i = 0; i < numOfBlocks; i++) {
		LIST_INSERT_HEAD(&AvailableMemBlocksList, &(MemBlockNodes[i]));
	}
}

//===============================
// [2] FIND BLOCK:
//===============================
struct MemBlock* find_block(struct MemBlock_List* blockList, uint32 va) {
	int bolck_found = -1;
	struct MemBlock* it;
	LIST_FOREACH(it, blockList)
	{
		if (it->sva == va) {
			bolck_found = 1;
			break;
		}
	}
	if (bolck_found == 1)
		return it;
	else
		return NULL;
}

//=========================================
// [3] INSERT BLOCK IN ALLOC LIST [SORTED]:
//=========================================
void insert_sorted_allocList(struct MemBlock* blockToInsert) {
	struct MemBlock* head;
	struct MemBlock* tail;
	struct MemBlock* temp;
	if (LIST_SIZE(&(AllocMemBlocksList)) == 0) {
		LIST_INSERT_HEAD(&(AllocMemBlocksList), blockToInsert);
		tail = blockToInsert;
		head = blockToInsert;
	} else {
		LIST_FOREACH(temp, &(AllocMemBlocksList))
		{
			if (temp == LIST_LAST(&(AllocMemBlocksList))
					&& blockToInsert->sva > temp->sva) {
				LIST_INSERT_TAIL(&(AllocMemBlocksList), blockToInsert);
				tail = blockToInsert;
				break;
			} else if (temp == LIST_FIRST(&(AllocMemBlocksList))
					&& blockToInsert->sva < temp->sva) {
				LIST_INSERT_HEAD(&(AllocMemBlocksList), blockToInsert);
				head = blockToInsert;
				break;
			} else if (temp->sva < blockToInsert->sva
					&& temp->prev_next_info.le_next->sva > blockToInsert->sva) {
				blockToInsert->prev_next_info.le_next =
						temp->prev_next_info.le_next;
				temp->prev_next_info.le_next = blockToInsert;
				blockToInsert->prev_next_info.le_prev = temp;
				blockToInsert->prev_next_info.le_next->prev_next_info.le_prev =
						blockToInsert;
				LIST_SIZE(&(AllocMemBlocksList)) =
				LIST_SIZE(&(AllocMemBlocksList)) + 1;
				break;
			}
		}
	}
}

//=========================================
// [4] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
struct MemBlock* alloc_block_FF(uint32 size) {
	int bolck_found = -1;
	struct MemBlock* it;
	LIST_FOREACH(it, &FreeMemBlocksList)
	{
		if (it != NULL && it->size == size) {
			LIST_REMOVE(&FreeMemBlocksList, it);
			return it;
		} else if (it != NULL && it->size > size) {

			struct MemBlock* temp_it = LIST_FIRST(&AvailableMemBlocksList);
			LIST_REMOVE(&AvailableMemBlocksList, temp_it);
			temp_it->size = size;
			temp_it->sva = it->sva;
			it->size = it->size - size;
			it->sva = it->sva + size;
			return temp_it;
		}
	}
	return NULL;
}

//=========================================
// [5] ALLOCATE BLOCK BY BEST FIT:
//=========================================
struct MemBlock* alloc_block_BF(uint32 size) {
	int bolck_found = -1;
	struct MemBlock* it;
	struct MemBlock* i;
	uint32 fitSize = 4294967294;
	uint32 tempva = 0;
	LIST_FOREACH(it, &FreeMemBlocksList)
	{
		if (it != NULL && it->size == size) {
			LIST_REMOVE(&FreeMemBlocksList, it);
			return it;
		}
	}
	LIST_FOREACH(i, &FreeMemBlocksList)
	{
		if (i != NULL && i->size > size && i->size <= fitSize) {
			fitSize = i->size;
			tempva = i->sva;
		}
	}
	it = find_block(&FreeMemBlocksList, tempva);
	if (it != NULL && it->size > size) {

		struct MemBlock* temp_it = LIST_FIRST(&AvailableMemBlocksList);

		temp_it->size = size;
		temp_it->sva = it->sva;
		it->size = it->size - size;
		it->sva = it->sva + size;
		LIST_REMOVE(&AvailableMemBlocksList, temp_it);

		return temp_it;
	}
	return NULL;
}

//=========================================
// [7] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
struct MemBlock* lastFit = NULL;
struct MemBlock* alloc_block_NF(uint32 size) {
	struct MemBlock* it=NULL;
	if (lastFit == it) {
		LIST_FOREACH(it, &FreeMemBlocksList)
		{
			if (it->size == size) {
				if (LIST_LAST(&FreeMemBlocksList) != it)
					lastFit = it->prev_next_info.le_next;
				else
					lastFit = NULL;
				LIST_REMOVE(&FreeMemBlocksList, it);
				return it;
			} else if (it->size > size) {
				struct MemBlock* temp_it = LIST_FIRST(&AvailableMemBlocksList);
				temp_it->size = size;
				temp_it->sva = it->sva;
				it->size = it->size - size;
				it->sva = it->sva + size;
				lastFit = temp_it;
				LIST_REMOVE(&AvailableMemBlocksList, temp_it);
				return temp_it;
			}
		}
	}

	if(  lastFit != it ) {


		LIST_FOREACH(it, &FreeMemBlocksList)
		{
			if (it->sva > lastFit->sva) {
				if (it->size == size) {
					if (LIST_LAST(&FreeMemBlocksList) != it)
						lastFit = it;
					else
						lastFit = NULL;
					LIST_REMOVE(&FreeMemBlocksList, it);
					return it;
				} else if (it->size > size) {
					struct MemBlock* temp_it = LIST_FIRST(
							&AvailableMemBlocksList);
					temp_it->size = size;
					temp_it->sva = it->sva;
					it->size = it->size - size;
					it->sva = it->sva + size;
					lastFit = temp_it;
					LIST_REMOVE(&AvailableMemBlocksList, temp_it);
					return temp_it;

				}
			}
		}
		LIST_FOREACH(it, &FreeMemBlocksList)
				{
					if (it->sva < lastFit->sva) {
						if (it->size == size) {
							if (LIST_LAST(&FreeMemBlocksList) != it)
								lastFit = it;
							else
								lastFit = NULL;
							LIST_REMOVE(&FreeMemBlocksList, it);
							return it;
						} else if (it->size > size) {

							struct MemBlock* temp_it = LIST_FIRST(
									&AvailableMemBlocksList);
							temp_it->size = size;
							temp_it->sva = it->sva;
							it->size = it->size - size;
							it->sva = it->sva + size;
							lastFit = temp_it;
							LIST_REMOVE(&AvailableMemBlocksList, temp_it);
							return temp_it;
						}
					}

				}
	}

	return NULL;

}

//===================================================
// [8] INSERT BLOCK (SORTED WITH MERGE) IN FREE LIST:
//===================================================
void insert_sorted_with_merge_freeList(struct MemBlock* blockToInsert) {
	uint32 totalSizePrevious = 0, totalSizeBlock = 0;
	struct MemBlock* tail = LIST_LAST(&FreeMemBlocksList);
	if (LIST_SIZE(&FreeMemBlocksList) == 0) {
		LIST_INSERT_HEAD(&FreeMemBlocksList, blockToInsert);
	} else {
		tail = LIST_LAST(&FreeMemBlocksList);
		if (blockToInsert->sva > tail->sva) {
			LIST_INSERT_TAIL(&FreeMemBlocksList, blockToInsert);
		} else {
			struct MemBlock* temp;
			LIST_FOREACH(temp, &FreeMemBlocksList)
			{
				if (temp->sva
						> blockToInsert->sva&& temp == LIST_FIRST(&FreeMemBlocksList)) {
					LIST_INSERT_HEAD(&FreeMemBlocksList, blockToInsert);
					break;
				} else if (temp->sva < blockToInsert->sva
						&& temp->prev_next_info.le_next->sva
								> blockToInsert->sva) {
					blockToInsert->prev_next_info.le_next =
							temp->prev_next_info.le_next;
					temp->prev_next_info.le_next = blockToInsert;
					blockToInsert->prev_next_info.le_prev = temp;
					blockToInsert->prev_next_info.le_next->prev_next_info.le_prev =
							blockToInsert;			//edited
					LIST_SIZE(&(FreeMemBlocksList)) =
					LIST_SIZE(&(FreeMemBlocksList)) + 1;
					break;
				}
			}
		}
	}
	struct MemBlock* prevb = blockToInsert->prev_next_info.le_prev;
	struct MemBlock* nextb = blockToInsert->prev_next_info.le_next;
	if (LIST_FIRST(&FreeMemBlocksList) != blockToInsert) {
		totalSizePrevious = prevb->sva + prevb->size;
	}
	totalSizeBlock = blockToInsert->sva + blockToInsert->size;
	if (LIST_LAST(&FreeMemBlocksList) != blockToInsert
			&& LIST_FIRST(&FreeMemBlocksList) != blockToInsert
			&& totalSizePrevious == blockToInsert->sva
			&& totalSizeBlock == blockToInsert->prev_next_info.le_next->sva) {
		prevb->size = prevb->size + nextb->size + blockToInsert->size;
		LIST_REMOVE(&FreeMemBlocksList, nextb);
		LIST_REMOVE(&FreeMemBlocksList, blockToInsert);
		nextb->sva = 0;
		nextb->size = 0;
		blockToInsert->sva = 0;
		blockToInsert->size = 0;
		LIST_INSERT_TAIL(&(AvailableMemBlocksList), nextb);
		LIST_INSERT_TAIL(&(AvailableMemBlocksList), blockToInsert);
	} else if (LIST_FIRST(&FreeMemBlocksList) != blockToInsert
			&& totalSizePrevious == blockToInsert->sva) {	//merge with prev
		prevb->size = prevb->size + blockToInsert->size;
		LIST_REMOVE(&FreeMemBlocksList, blockToInsert);
		blockToInsert->sva = 0;
		blockToInsert->size = 0;
		LIST_INSERT_TAIL(&(AvailableMemBlocksList), blockToInsert);
	} else if (LIST_LAST(&FreeMemBlocksList) != blockToInsert
			&& totalSizeBlock == blockToInsert->prev_next_info.le_next->sva) {
		blockToInsert->prev_next_info.le_next->sva = blockToInsert->sva;
		blockToInsert->prev_next_info.le_next->size = blockToInsert->size
				+ nextb->size;
		LIST_REMOVE(&FreeMemBlocksList, blockToInsert);
		blockToInsert->sva = 0;
		blockToInsert->size = 0;
		LIST_INSERT_TAIL(&(AvailableMemBlocksList), blockToInsert);
	}
}

