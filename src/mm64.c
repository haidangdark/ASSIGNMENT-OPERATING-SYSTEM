/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

 /*================================================================================================================================================================================*/
/*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
 /*================================================================================================================================================================================*/
 /*
 File mm64.c là file cốt lõi của hệ thống quản lý bộ nhớ, vì nó là nơi hiện thực hóa logic cho kiến trúc 64-bit và 5-level paging.
 File này cung cấp các hàm cấp thấp để thao tác trực tiếp với các bảng trang (Page Tables) và các mục bảng trang (PTEs).


 Chức năng tổng quan của mm64.c
1. Hiện thực 5-Level Paging: Cung cấp logic để "tách" một địa chỉ ảo 64-bit thành 5 chỉ số (index) cho 5 cấp bảng trang.

2. Khởi tạo Bộ nhớ Tiến trình: Định nghĩa hàm init_mm, hàm quan trọng nhất khi nạp một tiến trình mới. Nó tạo ra Bảng Trang Gốc (PGD) và VMA (Heap) đầu tiên.

3. Thao tác với PTE: Cung cấp các hàm "setter" (như init_pte, pte_set_fpn) để cấu hình các bit của một Page Table Entry (PTE), ví dụ như set bit PRESENT hoặc gán số khung trang vật lý (FPN).

4. Hỗ trợ Swap: Cung cấp hàm __swap_cp_page, là hàm "công nhân" thực hiện việc sao chép nội dung của một trang từ RAM ra SWAP hoặc ngược lại.

5. Cung cấp Hàm Stub (Giả lập): Do sự phức tạp của hệ thống 64-bit, file này cung cấp nhiều hàm "dummy" (hàm giả lập, chưa có logic đầy đủ) như vmap_page_range, alloc_pages_range. 
Các hàm này cho phép hệ thống chạy ở chế độ đơn giản (ánh xạ 1-1) trong khi vẫn giữ đúng cấu trúc của một hệ thống 64-bit.


Các thư viện và Header liên quan
1. "mm64.h": Rất quan trọng. Cung cấp các macro 64-bit (như PAGING64_ADDR_PGD_MASK, PAGING64_ADDR_PT_LOBIT...) để "tách" địa chỉ ảo.

2. "mm.h": Cung cấp các macro PTE chung (như PAGING_PTE_PRESENT_MASK, PAGING_PTE_FPN_MASK) và các prototype hàm mà mm64.c phải hiện thực.

3. <stdlib.h>: Dùng cho malloc, calloc (để cấp phát PGD, VMA, v.v.).

4. <stdio.h>: Dùng cho các hàm printf (chủ yếu trong các hàm print_... để debug).
 */

#include "mm64.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

/* --- [OVERRIDE] GHI ĐÈ CẤU HÌNH CHO CHẾ ĐỘ 64-BIT --- */
#ifdef MM64
  /* 1. Ghi đè kích thước trang (4096 thay vì 256) */
  #undef PAGING_PAGESZ
  #define PAGING_PAGESZ PAGING64_PAGESZ 

  /* 2. Ghi đè cách tính Offset (Lấy 12 bit cuối thay vì 8 bit) */
  #undef PAGING_OFFST
  #define PAGING_OFFST(addr)  ((addr) & PAGING64_ADDR_OFFST_MASK)

  /* 3. Ghi đè cách tính số trang (Dịch 12 bit thay vì 8 bit) */
  #undef PAGING_PGN
  #define PAGING_PGN(addr)    ((addr) >> PAGING64_ADDR_PT_SHIFT)
  
  /* 4. Ghi đè các macro bit */
  #undef PAGING_PTE_FPN
  #define PAGING_PTE_FPN(pte) PAGING64_PTE_FPN(pte)
  
  #undef PAGING_PTE_PRESENT
  #define PAGING_PTE_PRESENT(pte) PAGING64_PTE_PRESENT(pte)

  #undef PAGING_PTE_SWAPPED
  #define PAGING_PTE_SWAPPED(pte) PAGING64_PTE_SWAPPED(pte)
#endif
/* --------------------------------------------------- */

#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
/*
Nhóm 1: Thao tác Page Table Entry (PTE)
init_pte(addr_t *pte, int pre, ...)

  Chức năng: Cấu hình các bit của một PTE (một addr_t).

  Chi tiết Code:

      Nếu pre=1 (present) và swp=0 (trong RAM):

          SETBIT(*pte, PAGING_PTE_PRESENT_MASK);

          CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

          SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, ...);: Gán FPN vào các bit 0-12.

      Nếu pre=1 (present) và swp=1 (trong SWAP):

          SETBIT(*pte, PAGING_PTE_PRESENT_MASK);

          SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

          SETVAL(*pte, swptyp, ...) và SETVAL(*pte, swpoff, ...): Gán thông tin vị trí SWAP.

pte_set_swap, pte_set_fpn, pte_get_entry, pte_set_entry

  Chức năng: Đây là các hàm "setter/getter" cho PTE.

  Lưu ý quan trọng (Trong code): Các hàm này hiện tại chỉ là hàm giả lập (stub).

    Ví dụ: addr_t *pte = (addr_t *)&pgn;.

    Logic này không thực hiện duyệt 5-level page table để tìm PTE thật. Nó chỉ lấy địa chỉ của biến pgn (nằm trên stack) và sửa nó.

    pte_get_entry chỉ đơn giản trả về pgn.

Trong một hệ thống đầy đủ, các hàm này phải dùng get_pd_from_pagenum để duyệt cây bảng trang, tìm đến địa chỉ của PTE cuối cùng (cấp 1) và đọc/ghi giá trị tại đó.
*/

/* * lookup_and_alloc_pte - Logic Cấp phát lười (Lazy Allocation)
 * Duyệt từ PGD -> PT. Nếu thiếu ở đâu, xin RAM đắp vào đó.
 */
static addr_t *lookup_and_alloc_pte(struct pcb_t *proc, 
                                  addr_t idx_pgd, addr_t idx_p4d, addr_t idx_pud, 
                                  addr_t idx_pmd, addr_t idx_pt)
{
  struct krnl_t *sys_kern = proc->krnl;
  struct mm_struct *proc_mm = sys_kern->mm;
  
  /* 1. Kiểm tra cấp PGD (Level 5) */
  if (!PAGING64_PTE_PRESENT(proc_mm->pgd[idx_pgd])) {
    addr_t fpn_p4d;
    // Xin frame mới cho bảng P4D
    if (MEMPHY_get_freefp(sys_kern->mram, &fpn_p4d) != 0) return NULL;
    
    // Liên kết PGD entry trỏ tới frame vừa xin
    init_pte(&proc_mm->pgd[idx_pgd], 1, fpn_p4d, 0, 0, 0, 0);
    
    // Xóa trắng bảng P4D mới tạo
    addr_t *tbl_p4d = (addr_t*)(sys_kern->mram->storage + (fpn_p4d * PAGING_PAGESZ));
    for (int k = 0; k < PAGING64_P4D_CNT; k++) tbl_p4d[k] = 0;
  }
  
  /* 2. Kiểm tra cấp P4D (Level 4) */
  addr_t fpn_p4d = PAGING64_PTE_FPN(proc_mm->pgd[idx_pgd]);
  addr_t *tbl_p4d = (addr_t*)(sys_kern->mram->storage + (fpn_p4d * PAGING_PAGESZ));
  
  if (!PAGING64_PTE_PRESENT(tbl_p4d[idx_p4d])) {
    addr_t fpn_pud;
    if (MEMPHY_get_freefp(sys_kern->mram, &fpn_pud) != 0) return NULL;
    
    init_pte(&tbl_p4d[idx_p4d], 1, fpn_pud, 0, 0, 0, 0);
    
    addr_t *tbl_pud = (addr_t*)(sys_kern->mram->storage + (fpn_pud * PAGING_PAGESZ));
    for (int k = 0; k < PAGING64_PUD_CNT; k++) tbl_pud[k] = 0;
  }
  
  /* 3. Kiểm tra cấp PUD (Level 3) */
  addr_t fpn_pud = PAGING64_PTE_FPN(tbl_p4d[idx_p4d]);
  addr_t *tbl_pud = (addr_t*)(sys_kern->mram->storage + (fpn_pud * PAGING_PAGESZ));
  
  if (!PAGING64_PTE_PRESENT(tbl_pud[idx_pud])) {
    addr_t fpn_pmd;
    if (MEMPHY_get_freefp(sys_kern->mram, &fpn_pmd) != 0) return NULL;
    
    init_pte(&tbl_pud[idx_pud], 1, fpn_pmd, 0, 0, 0, 0);
    
    addr_t *tbl_pmd = (addr_t*)(sys_kern->mram->storage + (fpn_pmd * PAGING_PAGESZ));
    for (int k = 0; k < PAGING64_PMD_CNT; k++) tbl_pmd[k] = 0;
  }
  
  /* 4. Kiểm tra cấp PMD (Level 2) */
  addr_t fpn_pmd = PAGING64_PTE_FPN(tbl_pud[idx_pud]);
  addr_t *tbl_pmd = (addr_t*)(sys_kern->mram->storage + (fpn_pmd * PAGING_PAGESZ));
  
  if (!PAGING64_PTE_PRESENT(tbl_pmd[idx_pmd])) {
    addr_t fpn_pt;
    if (MEMPHY_get_freefp(sys_kern->mram, &fpn_pt) != 0) return NULL;
    
    init_pte(&tbl_pmd[idx_pmd], 1, fpn_pt, 0, 0, 0, 0);
    
    addr_t *tbl_pt = (addr_t*)(sys_kern->mram->storage + (fpn_pt * PAGING_PAGESZ));
    for (int k = 0; k < PAGING64_PT_CNT; k++) tbl_pt[k] = 0;
  }
  
  /* 5. Trả về địa chỉ của PTE trong bảng PT (Level 1) */
  addr_t fpn_pt = PAGING64_PTE_FPN(tbl_pmd[idx_pmd]);
  addr_t *tbl_pt = (addr_t*)(sys_kern->mram->storage + (fpn_pt * PAGING_PAGESZ));
  
  return &tbl_pt[idx_pt];
}





int init_pte(addr_t *pte,    // NHÓM 1
             int pre,    // present
             addr_t fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             addr_t swpoff) // swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0)
        return -1;  // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    }
    else
    { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;
}


/*
 * get_pd_from_address - Parse address to 5 page directory level
 */
/*
Nhóm 2: Logic 5-Level Paging (Cốt lõi)
get_pd_from_address(addr_t addr, ...)

  Chức năng: "Tách" một địa chỉ ảo addr thành 5 chỉ số (index) cho 5 cấp bảng trang: PGD, P4D, PUD, PMD, và PT.

  Chi tiết Code: Đây là một chuỗi các phép toán bit (AND và SHIFT). Nó dùng các macro từ mm64.h:

    *pgd = (addr & PAGING64_ADDR_PGD_MASK) >> PAGING64_ADDR_PGD_LOBIT;

    ... và tương tự cho 4 cấp còn lại.

  Đây là bước đầu tiên và quan trọng nhất trong quá trình "Page Table Walk" (duyệt cây bảng trang) để dịch địa chỉ.

get_pd_from_pagenum(addr_t pgn, ...)

  Chức năng: Tương tự như hàm trên, nhưng nhận đầu vào là Số Trang Ảo (PGN) thay vì địa chỉ đầy đủ.

  Chi tiết Code: Nó chỉ đơn giản là dịch trái pgn (thêm các bit 0 của OFFSET) để tạo thành một địa chỉ ảo, sau đó gọi get_pd_from_address.
*/
int get_pd_from_address(addr_t addr, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt)   //NHÓM 2
{
	/* Extract page direactories */
	*pgd = (addr&PAGING64_ADDR_PGD_MASK)>>PAGING64_ADDR_PGD_LOBIT;
	*p4d = (addr&PAGING64_ADDR_P4D_MASK)>>PAGING64_ADDR_P4D_LOBIT;
	*pud = (addr&PAGING64_ADDR_PUD_MASK)>>PAGING64_ADDR_PUD_LOBIT;
	*pmd = (addr&PAGING64_ADDR_PMD_MASK)>>PAGING64_ADDR_PMD_LOBIT;
	*pt = (addr&PAGING64_ADDR_PT_MASK)>>PAGING64_ADDR_PT_LOBIT;

	/* TODO: implement the page direactories mapping */

  if (*pgd >= PAGING64_PGD_CNT || *p4d >= PAGING64_P4D_CNT || 
      *pud >= PAGING64_PUD_CNT || *pmd >= PAGING64_PMD_CNT || 
      *pt >= PAGING64_PT_CNT) {
      return -1;
  }

	return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 */
int get_pd_from_pagenum(addr_t pgn, addr_t* pgd, addr_t* p4d, addr_t* pud, addr_t* pmd, addr_t* pt) //NHÓM 2
{
	/* Shift the address to get page num and perform the mapping*/
	return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT,
                         pgd,p4d,pud,pmd,pt);
}


/*
 * pte_set_swap - Set PTE entry for swapped page
 */
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff)  //NHÓM 1
{


    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    
    // Tách địa chỉ
    if (get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx) != 0) 
        return -1;

    // Tìm đường & Cấp phát (nếu cần)
    addr_t *pte = lookup_and_alloc_pte(caller, pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx);
    if (pte == NULL) return -1;

    // Ghi dữ liệu Swap vào PTE tìm được
    *pte = 0; // Xóa cũ
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
  return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 */
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn)   //NHÓM 1
{


    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    
    if (get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx) != 0) 
        return -1;

    addr_t *pte = lookup_and_alloc_pte(caller, pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx);
    if (pte == NULL) return -1; 

    // Ghi dữ liệu FPN vào PTE tìm được
    *pte = 0;
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
  return 0;
}




/* Get PTE page table entry */
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn)     //NHÓM 1
{

    if (!caller || !caller->krnl || !caller->krnl->mm) return 0;

    addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;
    if (get_pd_from_pagenum(pgn, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx) != 0) return 0;

    struct krnl_t *krnl = caller->krnl;
    struct mm_struct *mm = krnl->mm;
    
    if (!PAGING64_PTE_PRESENT(mm->pgd[pgd_idx])) return 0;
    
    addr_t p4d_fpn = PAGING64_PTE_FPN(mm->pgd[pgd_idx]);
    if (p4d_fpn * PAGING_PAGESZ >= krnl->mram->maxsz) return 0; 
    addr_t *p4d_table = (addr_t*)(krnl->mram->storage + (p4d_fpn * PAGING_PAGESZ));
    
    if (!PAGING64_PTE_PRESENT(p4d_table[p4d_idx])) return 0;
    
    addr_t pud_fpn = PAGING64_PTE_FPN(p4d_table[p4d_idx]);
    if (pud_fpn * PAGING_PAGESZ >= krnl->mram->maxsz) return 0;
    addr_t *pud_table = (addr_t*)(krnl->mram->storage + (pud_fpn * PAGING_PAGESZ));
    
    if (!PAGING64_PTE_PRESENT(pud_table[pud_idx])) return 0;
    
    addr_t pmd_fpn = PAGING64_PTE_FPN(pud_table[pud_idx]);
    if (pmd_fpn * PAGING_PAGESZ >= krnl->mram->maxsz) return 0;
    addr_t *pmd_table = (addr_t*)(krnl->mram->storage + (pmd_fpn * PAGING_PAGESZ));
    
    if (!PAGING64_PTE_PRESENT(pmd_table[pmd_idx])) return 0;
    
    addr_t pt_fpn = PAGING64_PTE_FPN(pmd_table[pmd_idx]);
    if (pt_fpn * PAGING_PAGESZ >= krnl->mram->maxsz) return 0;
    addr_t *pt_table = (addr_t*)(krnl->mram->storage + (pt_fpn * PAGING_PAGESZ));
    
    return pt_table[pt_idx];
}

/* Set PTE page table entry */
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val)   //NHÓM 1
{
    /* Lấy pointer tới kernel từ process caller */
    struct krnl_t *sys_kernel = caller->krnl;
    
    /* Gán trực tiếp giá trị vào bảng PGD tại chỉ số trang pgn */
    sys_kernel->mm->pgd[pgn] = pte_val;
    
    return 0;
}


/*
 * vmap_pgd_memset - map a range of page at aligned address
 */

 /*
 Nhóm 3: Các hàm Stub (Giả lập)
vmap_pgd_memset: Hàm rỗng. Dùng để giả lập việc "đánh dấu" một vùng nhớ (theo yêu cầu PDF) mà không cần cấp phát thật.

vmap_page_range: Hàm giả lập, chỉ gán ret_rg->rg_start và ret_rg->rg_end theo địa chỉ ảo. Nó không thực hiện ánh xạ thật.

alloc_pages_range: Hàm giả lập, trả về NULL và không cấp phát khung trang nào.

vm_map_ram: Hàm giả lập, chỉ gán ret_rg->rg_start và ret_rg->rg_end. 
Hàm này được inc_vma_limit (trong mm-vm.c) gọi, và vì nó là hàm giả lập, nên hệ thống của bạn đang chạy ở chế độ ánh xạ 1-1 (Địa chỉ ảo = Địa chỉ vật lý).
 */
int vmap_pgd_memset(struct pcb_t *caller, addr_t addr, int pgnum) //NHÓM 3
{
  
  uint64_t pattern = 0xdeadbeef;
  for (int i = 0; i < pgnum; i++) {
    addr_t current_addr = addr + (i * PAGING_PAGESZ);
    addr_t pgn = PAGING_PGN(current_addr);
    pte_set_fpn(caller, pgn, (pattern + i) & 0xFFFFF);
  }
  return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller, addr_t addr, int pgnum,     //NHÓM 3
                    struct framephy_struct *frames, struct vm_rg_struct *ret_rg)
{
  

  struct framephy_struct *phy_node = frames;
  addr_t pgn;

  /* Cập nhật thông tin vùng nhớ được trả về */
  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + (pgnum * PAGING_PAGESZ);

  /* Duyệt qua danh sách frames và map vào bảng trang */
  for (int i = 0; i < pgnum && phy_node != NULL; i++) {
      pgn = PAGING_PGN((addr + (i * PAGING_PAGESZ)));
      /* Gọi hàm set FPN để ánh xạ */
      pte_set_fpn(caller, pgn, phy_node->fpn);
      /* Thêm vào danh sách quản lý trang (để tìm Victim Page sau này nếu cần) */
      enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
      /* Chuyển sang frame tiếp theo */
      phy_node = phy_node->fp_next;
  }
  return 0;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 */
addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)   //NHÓM 3
{
 
    addr_t phys_idx; // FPN lấy được từ RAM
    struct framephy_struct *head = NULL;
    struct framephy_struct *tail = NULL;
    int got_frames = 0;

    /* Vòng lặp xin cấp phát số lượng frame yêu cầu */
    for (int i = 0; i < req_pgnum; i++) {
        /* Xin một frame trống từ thiết bị RAM */
        if (MEMPHY_get_freefp(caller->krnl->mram, &phys_idx) == 0) {
            
            /* Tạo node mới chứa thông tin frame vừa xin được */
            struct framephy_struct *node = malloc(sizeof(struct framephy_struct));
            node->fpn = phys_idx;
            node->fp_next = NULL;

            /* Nối node vào danh sách liên kết */
            if (head == NULL) {
                head = node; // Node đầu tiên
            } else {
                tail->fp_next = node; // Nối vào đuôi
            }
            tail = node; // Cập nhật con trỏ hiện tại
            
            got_frames++;
        } else {
            /* Hết RAM (Out of memory) -> dừng lại */
            break; 
        }
    }

    /* Trả về danh sách frame đã xin được */
    *frm_lst = head;

    /* Nếu xin đủ số lượng thì trả về 0 (Thành công), ngược lại trả về số lượng đã xin */
    if (got_frames == req_pgnum) return 0;
    else return got_frames;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum, struct vm_rg_struct *ret_rg)   //NHÓM 3
{

  struct framephy_struct *phys_frames = NULL;

    /* Bước 1: Xin cấp phát các frame vật lý */
    if (alloc_pages_range(caller, incpgnum, &phys_frames) != 0) {
        return -1; /* Lỗi: Không đủ bộ nhớ */
    }

    /* Bước 2: Ánh xạ các frame vừa xin được vào địa chỉ ảo */
    vmap_page_range(caller, mapstart, incpgnum, phys_frames, ret_rg);

  return 0;
}

/* Swap copy content page */

/*
Nhóm 4: Hỗ trợ Swap (Hàm chức năng đầy đủ)
__swap_cp_page(mpsrc, srcfpn, mpdst, dstfpn)

Chức năng: Đây là hàm đầy đủ chức năng duy nhất trong nhóm cấp thấp. Nó sao chép từng byte của một trang từ khung nguồn sang khung đích.

Chi tiết Code:

    Dùng vòng lặp for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++).

    Tính địa chỉ vật lý của byte nguồn: addrsrc = srcfpn * PAGING_PAGESZ + cellidx;.

    Tính địa chỉ vật lý của byte đích: addrdst = dstfpn * PAGING_PAGESZ + cellidx;.

    MEMPHY_read(mpsrc, addrsrc, &data);: Đọc 1 byte từ nguồn (có thể là RAM).

    MEMPHY_write(mpdst, addrdst, data);: Ghi 1 byte vào đích (có thể là SWAP).
*/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn,  //NHÓM 4
                   struct memphy_struct *mpdst, addr_t dstfpn)
{
  int cellidx;
  addr_t addrsrc, addrdst;
  for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 */

 /*
 Nhóm 5: Khởi tạo và Quản lý Cấu trúc
init_mm(struct mm_struct *mm, struct pcb_t *caller)

    Chức năng: Hàm quan trọng nhất khi một tiến trình mới được nạp. Nó khởi tạo "bộ não" quản lý bộ nhớ (mm_struct) cho tiến trình đó.

    Chi tiết Code:

        1. Cấp phát struct vm_area_struct *vma0 (đây sẽ là vùng HEAP).

        2. Cấp phát Bảng Trang Gốc (PGD):

            mm->pgd = (typeof(mm->pgd))calloc(PAGING_MAX_PGN, sizeof(mm->pgd[0]));

            Đây là bước tối quan trọng. calloc cấp phát một mảng lớn cho PGD và khởi tạo tất cả bằng 0 (nghĩa là "not present").

        3. Đặt các cấp con (P4D, PUD, PMD, PT) bằng NULL. Đây là chiến lược "demand paging" (chỉ cấp phát khi cần).

        4. Khởi tạo vma0 (Heap) với kích thước ban đầu là PAGING_SBRK_INIT_SZ.

        5. Khởi tạo danh sách vùng trống (vm_freerg_list).

        6. Liên kết các cấu trúc lại với nhau: mm->mmap = vma0;.

        7. Khởi tạo bảng ký hiệu (biến): mm->symrgtbl về 0.

        8. Khởi tạo danh sách FIFO (dùng cho swap): mm->fifo_pgn = NULL;.

init_vm_rg, enlist_vm_rg_node, enlist_pgn_node

    Chức năng: Các hàm tiện ích (utility) để quản lý danh sách liên kết.

        init_vm_rg: malloc và khởi tạo một vm_rg_struct.

        enlist_..._node: Thêm một node vào đầu danh sách liên kết (thao tác push).
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller) //NHÓM 5

{
  //(void)caller;
  struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
  if (vma0 == NULL) {
    return -1;
  }

  /* TODO init page table directory */
   //mm->pgd = ...
   //mm->p4d = ...
   //mm->pud = ...
   //mm->pmd = ...
   //mm->pt = ...
  mm->pgd = (typeof(mm->pgd))calloc(PAGING_MAX_PGN, sizeof(mm->pgd[0]));
  if (!mm->pgd) { free(vma0); return -1; }

  /*project có các cấp p4d/pud/pmd/pt là con trỏ, 
     có thể set NULL ở đây; khi nào cần mới cấp phát thật sự. */
  mm->p4d = NULL;
  mm->pud = NULL;
  mm->pmd = NULL;
  mm->pt  = NULL;
  // mm->p4d = (typeof(mm->p4d))calloc(PAGING_MAX_PGN, sizeof(uint64_t));
  // mm->pud = (typeof(mm->pud))calloc(PAGING_MAX_PGN, sizeof(uint64_t));
  // mm->pmd = (typeof(mm->pmd))calloc(PAGING_MAX_PGN, sizeof(uint64_t));
  // mm->pt  = (typeof(mm->pt))calloc(PAGING_MAX_PGN, sizeof(uint64_t));

  /* By default the owner comes with at least one vma */
  vma0->vm_id = 0;
  vma0->vm_start = 0;

  /* khởi tạo heap ban đầu và free-list an toàn */
  vma0->vm_end = vma0->vm_start + PAGING_SBRK_INIT_SZ;
  vma0->sbrk = vma0->vm_end;
  vma0->vm_freerg_list = NULL;  /* <— rất quan trọng: tránh nối vào rác */
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
  if (first_rg == NULL) {
      free(vma0);
      return -1;
  }
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);
  

  /* TODO update VMA0 next */
  // vma0->next = ...
  vma0->vm_next = NULL;
  vma0->vm_mm = mm;

  /* TODO: update mmap */
  //mm->mmap = ...
  //mm->symrgtbl = ...
  mm->mmap = vma0;

  for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
  {
    mm->symrgtbl[i].rg_start = 0;
    mm->symrgtbl[i].rg_end = 0;
    mm->symrgtbl[i].rg_next = NULL;
  }

  /* zero hoá bảng pgd để tránh rác bit PRESENT */
  for (int i = 0; i < PAGING_MAX_PGN; i++) {
    mm->pgd[i] = 0;
  }
  

  mm->fifo_pgn = NULL;
  return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end)//NHÓM 5
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)//NHÓM 5
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn) //NHÓM 5
{
  struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}






/*
Nhóm 6: Hàm Debug
print_list_fp, print_list_rg, print_list_vma, print_list_pgn, print_pgtbl

Chức năng: Các hàm print để debug, giúp nhìn vào bên trong các cấu trúc dữ liệu (danh sách liên kết) khi chạy.

TẤT CẢ CÁC HÀM BÊN DƯỚI 
*/
int print_list_fp(struct framephy_struct *ifp)
{
  struct framephy_struct *fp = ifp;

  printf("print_list_fp: ");
  if (fp == NULL) { printf("NULL list\n"); return -1;}
  printf("\n");
  while (fp != NULL)
  {
    printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
    fp = fp->fp_next;
  }
  printf("\n");
  return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
  struct vm_rg_struct *rg = irg;

  printf("print_list_rg: ");
  if (rg == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (rg != NULL)
  {
    printf("rg[" FORMAT_ADDR "->"  FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
    rg = rg->rg_next;
  }
  printf("\n");
  return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
  struct vm_area_struct *vma = ivma;

  printf("print_list_vma: ");
  if (vma == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (vma != NULL)
  {
    printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
    vma = vma->vm_next;
  }
  printf("\n");
  return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
  printf("print_list_pgn: ");
  if (ip == NULL) { printf("NULL list\n"); return -1; }
  printf("\n");
  while (ip != NULL)
  {
    printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
    ip = ip->pg_next;
  }
  printf("n");
  return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end)
{
  // if (caller->krnl && caller->krnl->mm) {
  //   printf("print_pgtbl:\n PDG=%lx P4g=%lx PUD=%lx PMD=%lx\n", 
  //          (addr_t)caller->krnl->mm->pgd,
  //          (addr_t)caller->krnl->mm->p4d, 
  //          (addr_t)caller->krnl->mm->pud,
  //          (addr_t)caller->krnl->mm->pmd);
  // }
  // return 0;
  /* Guard check */
  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;

  struct krnl_t *krnl = caller->krnl;
  struct mm_struct *mm = krnl->mm;
  
  
  addr_t pgd_idx, p4d_idx, pud_idx, pmd_idx, pt_idx;

 
  if (get_pd_from_address(start, &pgd_idx, &p4d_idx, &pud_idx, &pmd_idx, &pt_idx) != 0) {
      return 0;
  }

  printf("print_pgtbl (Virtual Address: %016lx):\n", (uint64_t)start);


  printf("  PGD Table: %016lx\n", (uint64_t)mm->pgd);

  if (!PAGING64_PTE_PRESENT(mm->pgd[pgd_idx])) {
      printf("  P4D Table: [Not Allocated / Not Present]\n");
      return 0;
  }


  addr_t fpn_p4d = PAGING64_PTE_FPN(mm->pgd[pgd_idx]);
 
  addr_t *tbl_p4d = (addr_t *)(krnl->mram->storage + (fpn_p4d * PAGING_PAGESZ));
  
  printf("  P4D Table: %016lx (Frame: %ld)\n", (uint64_t)tbl_p4d, (uint64_t)fpn_p4d);

  if (!PAGING64_PTE_PRESENT(tbl_p4d[p4d_idx])) {
      printf("  PUD Table: [Not Allocated / Not Present]\n");
      return 0;
  }

  
  addr_t fpn_pud = PAGING64_PTE_FPN(tbl_p4d[p4d_idx]);
  addr_t *tbl_pud = (addr_t *)(krnl->mram->storage + (fpn_pud * PAGING_PAGESZ));
  
  printf("  PUD Table: %016lx (Frame: %ld)\n", (uint64_t)tbl_pud, (uint64_t)fpn_pud);

  if (!PAGING64_PTE_PRESENT(tbl_pud[pud_idx])) {
      printf("  PMD Table: [Not Allocated / Not Present]\n");
      return 0;
  }


  addr_t fpn_pmd = PAGING64_PTE_FPN(tbl_pud[pud_idx]);
  addr_t *tbl_pmd = (addr_t *)(krnl->mram->storage + (fpn_pmd * PAGING_PAGESZ));
  
  printf("  PMD Table: %016lx (Frame: %ld)\n", (uint64_t)tbl_pmd, (uint64_t)fpn_pmd);

 
  
  return 0;
}

#endif  //def MM64