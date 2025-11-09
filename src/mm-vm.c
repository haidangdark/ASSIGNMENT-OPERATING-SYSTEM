/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  /* === FIX: guard NULL tuyệt đối trước khi deref === */
  if (!mm) return NULL;
  if (!mm->mmap) return NULL;

  struct vm_area_struct *pvma = mm->mmap;

  /* vm_id đầu tiên chỉ đọc khi pvma khác NULL */
  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (!pvma) return NULL;

    pvma = pvma->vm_next;
    if (!pvma) return NULL;

    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  //struct vm_area_struct *cur_vma = get_vma_by_num(caller->kernl->mm, vmaid);

  //newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO: update the newrg boundary
  // newrg->rg_start = ...
  // newrg->rg_end = ...
  */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
   if (!cur_vma) return NULL;


  newrg = (struct vm_rg_struct*)malloc(sizeof(struct vm_rg_struct));
  if (!newrg) return NULL;
  addr_t eff_sz = (alignedsz > 0) ? alignedsz : size;
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + eff_sz;
  /* END TODO */

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
    /* === FIX: guard caller/krnl/mm/mmap === */
  if (!caller || !caller->krnl || !caller->krnl->mm || !caller->krnl->mm->mmap)
    return -1;
  //struct vm_area_struct *vma = caller->krnl->mm->mmap;

  /* TODO validate the planned memory area is not overlapped */
  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->krnl->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TODO validate the planned memory area is not overlapped */

  struct vm_area_struct *cur_area = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  /* Kiểm tra phạm vi đề xuất nằm trong giới hạn của VMA hiện tại */
  if (vmastart < cur_area->vm_start || vmaend > cur_area->vm_end) {
    /* Với mô hình mở rộng vm_end dần, bước này có thể cho phép vượt vm_end cũ.
       Kiểm tra lỏng: chỉ cần vmastart <= vmaend; việc nâng vm_end sẽ được xử lý ở inc_vma_limit. */
    /* Không trả -1 ở đây để tránh chặn hợp lệ khi đang mở rộng; tiếp tục các kiểm tra chồng lắp với VMA khác. */
  }

  /* Kiểm tra chồng lắp với các VMA khác (nếu có nhiều VMA) */
  while (vma != NULL)
  {
    if (vma != cur_area )
    {
      /* Tự tính overlap (không dựa vào macro OVERLAP vì hiện macro là 0)
         overlap nếu max(start) < min(end) */
      addr_t max_st = (vmastart > vma->vm_start) ? vmastart : vma->vm_start;
      addr_t min_en = (vmaend   < vma->vm_end  ) ? vmaend   : vma->vm_end;
      if (max_st < min_en) {
        return -1; /* Overlap với VMA khác */
      }
    }
    vma = vma->vm_next;
  }
  /* End TODO*/

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  //struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));

  /* TOTO with new address scheme, the size need tobe aligned 
   *      the raw inc_sz maybe not fit pagesize
   */ 
  //addr_t inc_amt;

//  int incnumpage =  inc_amt / PAGING_PAGESZ;

  /* TODO Validate overlap of obtained region */
  //if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
  //  return -1; /*Overlap and failed allocation */

  /* TODO: Obtain the new vm area based on vmaid */
  //cur_vma->vm_end... 
  // inc_limit_ret...
  /* The obtained vm area (only)
   * now will be alloc real ram region */

//  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
//                   old_end, incnumpage , newrg) < 0)
//    return -1; /* Map the memory to MEMRAM */

  if (inc_sz == 0) return 0;
  if (!caller || !caller->krnl || !caller->krnl->mm) {
    return -1;
  }

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (!cur_vma) return -1;

  /* Căn hàng theo kích thước trang của hệ (giữ dùng macro chung của dự án) */
  addr_t inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  if (inc_amt == 0) inc_amt = PAGING_PAGESZ;

  addr_t old_end = cur_vma->vm_end;
  addr_t new_end = old_end + inc_amt;

  /* Kiểm tra chồng lắp với các VMA khác khi mở rộng ranh giới (old_end -> new_end) */
  if (validate_overlap_vm_area(caller, vmaid, old_end, new_end) < 0)
    return -1;

  /* Ánh xạ “dummy” RAM cho vùng tăng thêm để hợp lệ hoá không gian usable */
  int incnumpage = (int)(inc_amt / PAGING_PAGESZ);
  struct vm_rg_struct mapped_rg;
  if (vm_map_ram(caller, cur_vma->vm_start, new_end, old_end, incnumpage, &mapped_rg) < 0)
    return -1;

  /* Nâng giới hạn vùng và sbrk (đưa usable top lên đầu mới) */
  cur_vma->vm_end = new_end;
  cur_vma->sbrk   = new_end;

  return 0;
}

// #endif
