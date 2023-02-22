/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include "../cpu/sched.h"
#include "../disk/pagefile_manager.h"
#include "../mem/memory_manager.h"

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	assert(
			LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE;
}
void setPageReplacmentAlgorithmCLOCK() {
	_PageRepAlgoType = PG_REP_CLOCK;
}
void setPageReplacmentAlgorithmFIFO() {
	_PageRepAlgoType = PG_REP_FIFO;
}
void setPageReplacmentAlgorithmModifiedCLOCK() {
	_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;
}
/*2018*/void setPageReplacmentAlgorithmDynamicLocal() {
	_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;
}
/*2021*/void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps) {
	_PageRepAlgoType = PG_REP_NchanceCLOCK;
	page_WS_max_sweeps = PageWSMaxSweeps;
}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE) {
	return _PageRepAlgoType == LRU_TYPE ? 1 : 0;
}
uint32 isPageReplacmentAlgorithmCLOCK() {
	if (_PageRepAlgoType == PG_REP_CLOCK)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmFIFO() {
	if (_PageRepAlgoType == PG_REP_FIFO)
		return 1;
	return 0;
}
uint32 isPageReplacmentAlgorithmModifiedCLOCK() {
	if (_PageRepAlgoType == PG_REP_MODIFIEDCLOCK)
		return 1;
	return 0;
}
/*2018*/uint32 isPageReplacmentAlgorithmDynamicLocal() {
	if (_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL)
		return 1;
	return 0;
}
/*2021*/uint32 isPageReplacmentAlgorithmNchanceCLOCK() {
	if (_PageRepAlgoType == PG_REP_NchanceCLOCK)
		return 1;
	return 0;
}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt) {
	_EnableModifiedBuffer = enableIt;
}
uint8 isModifiedBufferEnabled() {
	return _EnableModifiedBuffer;
}

void enableBuffering(uint32 enableIt) {
	_EnableBuffering = enableIt;
}
uint8 isBufferingEnabled() {
	return _EnableBuffering;
}

void setModifiedBufferLength(uint32 length) {
	_ModifiedBufferLength = length;
}
uint32 getModifiedBufferLength() {
	return _ModifiedBufferLength;
}

//===============================
// FAULT HANDLERS
//===============================

//Handle the table fault
void table_fault_handler(struct Env * curenv, uint32 fault_va) {
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory,
				(uint32) fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}

//Handle the page fault

void page_fault_handler(struct Env * curenv, uint32 fault_va) {
	struct FrameInfo *frame_info_ptr = NULL;
	int maxSize = curenv->page_WS_max_size;
	int sizeOfWs = env_page_ws_get_size(curenv);
	fault_va = ROUNDDOWN(fault_va, PAGE_SIZE);

	if (sizeOfWs < maxSize) {
		int ret = allocate_frame(&frame_info_ptr);
		if (ret != E_NO_MEM) {
			map_frame(curenv->env_page_directory, frame_info_ptr, fault_va,
			PERM_PRESENT | PERM_USER | PERM_WRITEABLE);

			ret = pf_read_env_page(curenv, (void *) fault_va);
			if (ret == E_PAGE_NOT_EXIST_IN_PF) {
				if ((fault_va < USTACKTOP && fault_va >= USTACKBOTTOM)
						|| (fault_va < USER_HEAP_MAX
								&& fault_va >= USER_HEAP_START)) {

				} else {

					panic("invaild access %x", fault_va);
				}
			}

			int size = curenv->page_WS_max_size;
			for (int i = 0; i < size; i++) {
				if (curenv->ptr_pageWorkingSet[curenv->page_last_WS_index].empty) {
					env_page_ws_set_entry(curenv, curenv->page_last_WS_index,
							ROUNDDOWN(fault_va, PAGE_SIZE));
					break;

				} else {
					curenv->page_last_WS_index =
							(curenv->page_last_WS_index + 1)
									% curenv->page_WS_max_size;

				}
			}

		}
	} else {

		while (1) {
			uint32 VA = env_page_ws_get_virtual_address(curenv,
					curenv->page_last_WS_index);
			uint32 Perms = pt_get_page_permissions(curenv->env_page_directory,
					VA);
			struct FrameInfo *ptr_frame_info;
			uint32 *ptr_page_table = NULL;
			get_page_table(curenv->env_page_directory, VA, &ptr_page_table);
			ptr_frame_info = get_frame_info(curenv->env_page_directory, VA,
					&ptr_page_table);
			uint32 perm = pt_get_page_permissions(curenv->env_page_directory,
					VA);

			if ((Perms & PERM_USED) != PERM_USED) {

				if ((perm & PERM_MODIFIED) == PERM_MODIFIED)
					pf_update_env_page(curenv, VA, ptr_frame_info);

				unmap_frame(curenv->env_page_directory, VA);
				env_page_ws_clear_entry(curenv, curenv->page_last_WS_index);
				break;
			} else {
				pt_set_page_permissions(curenv->env_page_directory, VA, 0,
				PERM_USED);

			}
			curenv->page_last_WS_index = ((curenv->page_last_WS_index) + 1)
					% (curenv->page_WS_max_size);
		}

		struct FrameInfo *ptr_frame_info;
		uint32 *ptr_page_table = NULL;

		int ret = allocate_frame(&ptr_frame_info);

		if (ret != E_NO_MEM) {
			map_frame(curenv->env_page_directory, ptr_frame_info, fault_va,
			PERM_PRESENT | PERM_USER | PERM_WRITEABLE);
			ret = pf_read_env_page(curenv, (void *) fault_va);
			if (ret == E_PAGE_NOT_EXIST_IN_PF) {
				if ((fault_va < USTACKTOP && fault_va >= USTACKBOTTOM)
						|| (fault_va < USER_HEAP_MAX
								&& fault_va >= USER_HEAP_START)) {
				} else
					panic("invaild access %x", fault_va);
			}

			int size = curenv->page_WS_max_size;
			for (int i = 0; i < size; i++) {
				if (curenv->ptr_pageWorkingSet[curenv->page_last_WS_index].empty) {
					env_page_ws_set_entry(curenv, curenv->page_last_WS_index,
							ROUNDDOWN(fault_va, PAGE_SIZE));
					break;

				} else {
					curenv->page_last_WS_index++;

				}
			}

			curenv->page_last_WS_index = (curenv->page_last_WS_index + 1)
					% curenv->page_WS_max_size;

		}

	}
}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va) {
	// Write your code here, remove the panic and write your code
	panic("__page_fault_handler_with_buffering() is not implemented yet...!!");

}
