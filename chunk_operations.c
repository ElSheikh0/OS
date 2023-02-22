/*
 * chunk_operations.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include <kern/trap/fault_handler.h>
#include <kern/disk/pagefile_manager.h>
#include "kheap.h"
#include "memory_manager.h"

/******************************/
/*[1] RAM CHUNKS MANIPULATION */
/******************************/

//===============================
// 1) CUT-PASTE PAGES IN RAM:
//===============================
//This function should cut-paste the given number of pages from source_va to dest_va
//if the page table at any destination page in the range is not exist, it should create it
//Hint: use ROUNDDOWN/ROUNDUP macros to align the addresses
int cut_paste_pages(uint32* page_directory, uint32 source_va, uint32 dest_va,
		uint32 num_of_pages) {
	uint32 src_va = ROUNDDOWN(source_va, PAGE_SIZE);
	uint32 dst_va = ROUNDDOWN(dest_va, PAGE_SIZE);
	uint32* ptr_page_table_src = NULL;
	uint32* ptr_page_table_dst = NULL;
	int prm;
	struct FrameInfo* frame;
	uint32 size = PAGE_SIZE * num_of_pages;
	struct FrameInfo *ptr_fram_info;
	for (uint32 i = dst_va; i < ROUNDUP(dest_va + size, PAGE_SIZE); i +=
			PAGE_SIZE) {
		uint32 *ptr_page_tabel = NULL;
		ptr_fram_info = get_frame_info(page_directory, i, &ptr_page_tabel);
		if (ptr_fram_info != NULL) {
			return -1;
		}
	}
	for (int i = 0; i < num_of_pages; i++) {
		if (get_page_table(page_directory, dst_va,
				&ptr_page_table_dst) == TABLE_NOT_EXIST) {
			ptr_page_table_dst = create_page_table(page_directory, dst_va);
		}
		frame = get_frame_info(page_directory, src_va, &ptr_page_table_src);
		prm = pt_get_page_permissions(page_directory, src_va);
		map_frame(page_directory, frame, dst_va, prm);
		unmap_frame(page_directory, src_va);
		src_va = src_va + PAGE_SIZE;
		dst_va = dst_va + PAGE_SIZE;
	}

	return 0;
}

//===============================
// 2) COPY-PASTE RANGE IN RAM:
//===============================
//This function should copy-paste the given size from source_va to dest_va
//if the page table at any destination page in the range is not exist, it should create it
//Hint: use ROUNDDOWN/ROUNDUP macros to align the addresses
int copy_paste_chunk(uint32* page_directory, uint32 source_va, uint32 dest_va,
		uint32 size) {
	uint32 start_va_src = ROUNDDOWN(source_va, PAGE_SIZE);
	uint32 end_va_src = ROUNDUP(source_va + size, PAGE_SIZE);
	uint32 start_va_dest = ROUNDDOWN(dest_va, PAGE_SIZE);
	uint32 end_va_dest = ROUNDUP(dest_va + size, PAGE_SIZE);

	struct FrameInfo* src_frame;
	struct FrameInfo* dst_frame;
	uint32 *ptr_page_table_src = NULL;
	uint32 *ptr_page_table_dest = NULL;
	for (uint32 i = start_va_dest; i < end_va_dest; i += PAGE_SIZE) {

		dst_frame = get_frame_info(page_directory, i, &ptr_page_table_dest);

		if (dst_frame != NULL) {
			uint32 permissions = pt_get_page_permissions(page_directory, i);
			int rw;

			permissions = permissions / 2;
			rw = permissions % 2;

			if (rw == 0) {
				return -1;
			}
		}

		if (get_page_table(page_directory, i,
				&ptr_page_table_dest) == TABLE_NOT_EXIST) {
			ptr_page_table_dest = create_page_table(page_directory, i);
		}
	}

	for (uint32 i = start_va_src; i < end_va_src; i += PAGE_SIZE) {
		dst_frame = get_frame_info(page_directory, start_va_dest,
				&ptr_page_table_src);
		src_frame = get_frame_info(page_directory, i, &ptr_page_table_src);
		if (dst_frame == NULL) {
			allocate_frame(&src_frame);
			map_frame(page_directory, src_frame, start_va_dest,
					pt_get_page_permissions(page_directory, i));
			pt_set_page_permissions(page_directory, start_va_dest,
			PERM_WRITEABLE, 0);
		}
		start_va_dest += PAGE_SIZE;
	}
	char *src_content, *dst_content;
	src_content = (char*) source_va;
	dst_content = (char*) dest_va;
	for (int i = 0; i < size; ++i) {
		*dst_content = *src_content;
		*src_content++;
		*dst_content++;
	}
	return 0;
}

//===============================
// 3) SHARE RANGE IN RAM:
//===============================
//This function should share the given size from dest_va with the source_va
//Hint: use ROUNDDOWN/ROUNDUP macros to align the addresses
int share_chunk(uint32* page_directory, uint32 source_va, uint32 dest_va,
		uint32 size, uint32 perms) {
	uint32 *ptr_page_table_dest = NULL;
	uint32 *ptr_page_table_src = NULL;
	uint32 start_src = ROUNDDOWN(source_va, PAGE_SIZE);
	uint32 end_src = ROUNDUP(source_va + size, PAGE_SIZE);
	uint32 start_dest = ROUNDDOWN(dest_va, PAGE_SIZE);
	uint32 end_dest = ROUNDUP(dest_va + size, PAGE_SIZE);
	int ret_dest, ret_src;
	struct FrameInfo *frame_j_info, *frame_i_info;
	for (uint32 j = start_dest; j < end_dest; j += PAGE_SIZE) {
		frame_j_info = get_frame_info(page_directory, j, &ptr_page_table_dest);
		if (frame_j_info != NULL) {
			return -1;
		}
		ret_dest = get_page_table(page_directory, j, &ptr_page_table_dest);
		if (ret_dest == 1) {
			ptr_page_table_dest = create_page_table(page_directory, j);
		}
	}
	for (uint32 i = start_src, j = start_dest; i < end_src; j += PAGE_SIZE, i +=
	PAGE_SIZE) {
		get_page_table(page_directory, i, &ptr_page_table_src); // page table for src
		get_page_table(page_directory, j, &ptr_page_table_dest); // page table for dest
		frame_i_info = get_frame_info(page_directory, i, &ptr_page_table_src); // frame of src_va
		ptr_page_table_dest[PTX(j)] = ptr_page_table_src[PTX(i)]; // share
		frame_j_info = get_frame_info(page_directory, j, &ptr_page_table_dest);
		frame_i_info->references++;
		ptr_page_table_dest[PTX(j)] = to_physical_address(frame_j_info)
				| (perms | PERM_PRESENT);
	}
	return 0;
}

//===============================
// 4) ALLOCATE CHUNK IN RAM:
//===============================
//This function should allocate in RAM the given range [va, va+size)
//Hint: use ROUNDDOWN/ROUNDUP macros to align the addresses
int allocate_chunk(uint32* page_directory, uint32 va, uint32 size, uint32 perms) {
	//TODO: [PROJECT MS2] [CHUNK OPERATIONS] allocate_chunk
	// Write your code here, remove the panic and write your code
	struct FrameInfo* frame_info_ptr;
	for (uint32 i = ROUNDDOWN(va, PAGE_SIZE); i < ROUNDUP(va + size, PAGE_SIZE);
			i += PAGE_SIZE)
			{
		uint32 *ptr_page_table = NULL;
		int ret = get_page_table(page_directory, i, &ptr_page_table);
		if (ret == TABLE_NOT_EXIST) {
			ptr_page_table = create_page_table(page_directory, i);
		}
		frame_info_ptr = get_frame_info(page_directory, i, &ptr_page_table);
		if (frame_info_ptr != NULL) {
			return -1;
		}
	}

	for (uint32 i = ROUNDDOWN(va, PAGE_SIZE); i < ROUNDUP(va + size, PAGE_SIZE);
			i += PAGE_SIZE)
			{
		int ret1 = allocate_frame(&frame_info_ptr);
		if (ret1 == E_NO_MEM) {
			return -1;
		}
		int ret2 = map_frame(page_directory, frame_info_ptr, i, perms);
		frame_info_ptr->va = i;	//set va in frame
		if (ret2 == E_NO_MEM) {
			return -1;
		}
	}
	return 0;
}

/*BONUS*/
//=====================================
// 5) CALCULATE ALLOCATED SPACE IN RAM:
//=====================================
void calculate_allocated_space(uint32* page_directory, uint32 sva, uint32 eva,
		uint32 *num_tables, uint32 *num_pages) {
	uint32 *ptr_page_table = NULL;
	for (uint32 i = ROUNDDOWN(sva, PAGE_SIZE); i < ROUNDUP(eva, PAGE_SIZE); i +=
	PAGE_SIZE)
	{

		get_page_table(page_directory, i, &ptr_page_table);
		struct FrameInfo* frame_info = get_frame_info(page_directory, i,
				&ptr_page_table);
		if (frame_info != NULL) {
			(*num_pages)++;
		}
		if (ptr_page_table != NULL) {
			(*num_tables)++;
		}
	}
	if (*num_tables != 0) {
		*num_tables = ROUNDUP(*num_tables,1024) / 1024;
	}
}

/*BONUS*/
//=====================================
// 6) CALCULATE REQUIRED FRAMES IN RAM:
//=====================================
// calculate_required_frames:
// calculates the new allocation size required for given address+size,
// we are not interested in knowing if pages or tables actually exist in memory or the page file,
// we are interested in knowing whether they are allocated or not.
uint32 calculate_required_frames(uint32* page_directory, uint32 sva,
		uint32 size) {
	uint32 start = ROUNDDOWN(sva, PAGE_SIZE);
	uint32 end = ROUNDUP(sva + size, PAGE_SIZE);
	uint32 start_table = ROUNDDOWN(sva, PAGE_SIZE*1024);
	uint32 end_teble = ROUNDUP(sva + size, PAGE_SIZE*1024);

	uint32 num_of_free_frames = 0, table_entry = 0;
	uint32* ptr_page_table = NULL;
	struct FrameInfo* frame_info_ptr;
	for (uint32 i = start_table; i < end_teble; i += PAGE_SIZE * 1024) {
		frame_info_ptr = get_frame_info(page_directory, i, &ptr_page_table);
		if (PDX(i) != PDX(i-PAGE_SIZE) && ptr_page_table == NULL) {
			table_entry++;
		}
	}
	for (uint32 i = start; i < end; i += PAGE_SIZE)
	{
		frame_info_ptr = get_frame_info(page_directory, i, &ptr_page_table);

		if (frame_info_ptr == NULL) {
			num_of_free_frames++;
		}
	}
	return num_of_free_frames + table_entry;
}

//=================================================================================//
//===========================END RAM CHUNKS MANIPULATION ==========================//
//=================================================================================//

/*******************************/
/*[2] USER CHUNKS MANIPULATION */
/*******************************/

//======================================================
/// functions used for USER HEAP (malloc, free, ...)
//======================================================
//=====================================
// 1) ALLOCATE USER MEMORY:
//=====================================
void allocate_user_mem(struct Env* e, uint32 virtual_address, uint32 size) {
	// Write your code here, remove the panic and write your code
	panic("allocate_user_mem() is not implemented yet...!!");
}

//=====================================
// 2) FREE USER MEMORY:
//=====================================
void free_user_mem(struct Env* e, uint32 virtual_address, uint32 size) {
	//TODO: [PROJECT MS3] [USER HEAP - KERNEL SIDE] free_user_mem
	// Write your code here, remove the panic and write your code
	panic("free_user_mem() is not implemented yet...!!");

	//This function should:
	//1. Free ALL pages of the given range from the Page File
	//2. Free ONLY pages that are resident in the working set from the memory
	//3. Removes ONLY the empty page tables (i.e. not used) (no pages are mapped in the table)
}

//=====================================
// 2) FREE USER MEMORY (BUFFERING):
//=====================================
void __free_user_mem_with_buffering(struct Env* e, uint32 virtual_address,
		uint32 size) {
	// your code is here, remove the panic and write your code
	panic("__free_user_mem_with_buffering() is not implemented yet...!!");

	//This function should:
	//1. Free ALL pages of the given range from the Page File
	//2. Free ONLY pages that are resident in the working set from the memory
	//3. Free any BUFFERED pages in the given range
	//4. Removes ONLY the empty page tables (i.e. not used) (no pages are mapped in the table)
}

//=====================================
// 3) MOVE USER MEMORY:
//=====================================
void move_user_mem(struct Env* e, uint32 src_virtual_address,
		uint32 dst_virtual_address, uint32 size) {
	//TODO: [PROJECT MS3 - BONUS] [USER HEAP - KERNEL SIDE] move_user_mem
	//your code is here, remove the panic and write your code
	panic("move_user_mem() is not implemented yet...!!");

	// This function should move all pages from "src_virtual_address" to "dst_virtual_address"
	// with the given size
	// After finished, the src_virtual_address must no longer be accessed/exist in either page file
	// or main memory

	/**/
}

//=================================================================================//
//========================== END USER CHUNKS MANIPULATION =========================//
//=================================================================================//

