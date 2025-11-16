/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */


 /*================================================================================================================================================================================*/
/*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
 /*================================================================================================================================================================================*/
/*
File libmem.c là một trong những thành phần cốt lõi, đóng vai trò là "thư viện" (library) quản lý bộ nhớ ảo cấp cao.
Nó là lớp trung gian giữa lệnh của CPU (cpu.c gọi liballoc, libfree...) và hệ thống bảng trang/bộ nhớ vật lý (mm64.c, mm-memphy.c).
Chức năng chính của libmem.c là quản lý các vùng nhớ (regions - vm_rg_struct) bên trong một vùng ảo (area - vm_area_struct), cụ thể là vùng heap.
Nó chịu trách nhiệm:

1. Xử lý lệnh ALLOC: Tìm một vùng nhớ ảo (vm_rg_struct) còn trống trong vm_freerg_list. Nếu không tìm thấy, nó sẽ yêu cầu Kernel (thông qua System Call) để mở rộng vùng heap (tăng con trỏ sbrk).

2. Xử lý lệnh FREE: Lấy một vùng nhớ (vm_rg_struct) đang sử dụng và trả nó về danh sách vm_freerg_list để tái sử dụng sau này.

3. Xử lý READ/WRITE: Dịch "chỉ số đăng ký" (register index) và "offset" thành một địa chỉ ảo tuyệt đối (virtual address).

4. Quản lý bộ nhớ trống: Implement thuật toán First-Fit để tìm kiếm và tái sử dụng các vùng nhớ đã được FREE.

5. Đảm bảo an toàn (Thread-Safety): Sử dụng một mutex (khóa) để đảm bảo rằng các cấu trúc dữ liệu (như vm_freerg_list) không bị hỏng khi nhiều CPU/tiến trình cùng truy cập một lúc.



Các thư viện (Headers) liên quan
1. "mm.h" và "mm64.h": Cung cấp các định nghĩa cấu trúc quan trọng nhất (pcb_t, mm_struct, vm_area_struct, vm_rg_struct), các hằng số (PAGING_PAGESZ, PAGING_MAX_SYMTBL_SZ), và các macro thao tác địa chỉ/bit.

2. "syscall.h": Cần thiết để thực hiện lời gọi hệ thống (system call). Nó định nghĩa hàm syscall() và các hằng số như SYSMEM_INC_OP.

3. "libmem.h": File header của chính nó, khai báo các hàm liballoc, libfree...

4. <stdlib.h>: Thư viện chuẩn C, dùng cho malloc và free (ví dụ, khi tạo freerg_node trong hàm __free).

5. <stdio.h>: Dùng cho printf (chủ yếu cho các lệnh IODUMP và PAGETBL_DUMP).

6. <pthread.h>: Rất quan trọng, dùng để tạo và quản lý pthread_mutex_t mmvm_lock, đảm bảo an toàn cho các thao tác bộ nhớ.


Biến và Cấu trúc dữ liệu chính
static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

Đây là một "ổ khóa" (mutex). Bất kỳ hàm nào muốn thay đổi các cấu trúc dữ liệu quản lý bộ nhớ (như vm_freerg_list hoặc symrgtbl) đều phải "khóa" (pthread_mutex_lock) trước khi bắt đầu và "mở khóa" (pthread_mutex_unlock) ngay sau khi hoàn thành.

Điều này ngăn ngừa tình trạng "race condition" (khi 2 tiến trình cùng lúc alloc hoặc free, làm hỏng danh sách liên kết).

*/


#include "string.h"
#include "mm.h"
#include "mm64.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
/*enlist_vm_freerg_list
Chức năng: Thêm một vm_rg_struct (vùng nhớ) vào đầu danh sách các vùng nhớ trống (vm_freerg_list).

Chi tiết code: Đây là thao tác "push" vào đầu một danh sách liên kết (linked list) đơn:

Lấy con trỏ rg_node trỏ đến đầu danh sách free hiện tại.

Kiểm tra rg_elmt (vùng nhớ mới) có hợp lệ không (kích thước > 0).

Cho rg_elmt->rg_next trỏ vào rg_node (đầu cũ).

Cập nhật mm->mmap->vm_freerg_list trỏ vào rg_elmt (đầu mới).
 */

int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)   // rg_elmt (vùng nhớ mới)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list; // vùng nhớ cũ (danh sách free hiện tại)

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node; // thêm mới vào đầu cũ

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt; // gáng lại đầu danh sách vào đầu mới  // gióng gáng lại đầu cho link _ list

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */

 /*
 get_symrg_byid
Chức năng: Lấy con trỏ đến một vm_rg_struct đang được sử dụng từ "bảng ký hiệu" (symrgtbl) dựa trên rgid (chỉ số đăng ký).

Chi tiết code:

Kiểm tra rgid có nằm trong giới hạn của mảng [0, PAGING_MAX_SYMTBL_SZ - 1] không.

Nếu hợp lệ, trả về địa chỉ của phần tử &mm->symrgtbl[rgid].
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  /*fix off-by-one: ngăn vượt biên khi rgid == PAGING_MAX_SYMTBL_SZ */
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return NULL;
  

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
/*
__alloc (Hàm logic chính của liballoc)
Chức năng: Cấp phát một vùng nhớ ảo kích thước size và đăng ký nó với rgid.

Chi tiết code:

1. pthread_mutex_lock(&mmvm_lock);: Khóa lại để bảo vệ.

2. struct vm_area_struct *cur_vma = get_vma_by_num(...): Lấy VMA 0 (heap).

3. Thử tìm trong danh sách free (First-Fit):

Gọi get_free_vmrg_area(..., size, ...) (hàm này sẽ được giải thích ở dưới).

Nếu thành công (== 0):

Vùng rgnode (tham số đầu ra của get_free_vmrg_area) chứa thông tin (rg_start, rg_end) của vùng trống vừa tìm thấy.

Cập nhật thông tin này vào mm->symrgtbl[rgid].

Gán địa chỉ bắt đầu vào *alloc_addr (để liballoc sử dụng).

pthread_mutex_unlock(...) và return 0 (thành công).

4. Nếu thất bại (không có vùng trống nào đủ lớn):

Phải mở rộng heap.

inc_sz = PAGING_PAGE_ALIGNSZ(size);: Tính kích thước cần mở rộng, làm tròn lên cho vừa vặn với kích thước trang.

old_sbrk = cur_vma->sbrk;: Lưu lại đỉnh heap cũ (đây sẽ là địa chỉ bắt đầu của vùng nhớ mới).

Chuẩn bị và gọi System Call:

regs.a1 = SYSMEM_INC_OP;: Đặt lệnh là "Tăng VMA".

regs.a3 = ...size;: Đặt tham số là kích thước cần tăng.

syscall(caller->krnl, caller->pid, 17, &regs);: Gọi kernel (syscall 17). Kernel sẽ chạy sys_memmap -> inc_vma_limit, làm tăng cur_vma->sbrk.

Đăng ký vùng nhớ mới:

caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;: Địa chỉ bắt đầu là đỉnh heap cũ.

caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;: Địa chỉ kết thúc.

*alloc_addr = old_sbrk;.

5. pthread_mutex_unlock(&mmvm_lock);: Mở khóa và return 0.
*/
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
{
  /*Allocate at the toproof */
  pthread_mutex_lock(&mmvm_lock);

  /*  guard NULL để tránh segfault sớm */
  if (!caller || !caller->krnl || !caller->krnl->mm || !caller->krnl->mm->mmap) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  

  struct vm_rg_struct rgnode;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid); //struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid); mm.h
  int inc_sz=0;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) //int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg); mm.h
  {
    caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
    *alloc_addr = rgnode.rg_start;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /*Attempt to increate limit to get space */
#ifdef MM64
  inc_sz = (uint32_t)(size/(int)PAGING64_PAGESZ);
  inc_sz = inc_sz + 1;
#else
  inc_sz = PAGING_PAGE_ALIGNSZ(size);
#endif
  int old_sbrk;
  inc_sz = inc_sz + 1;

  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * SYSCALL 1 sys_memmap
   */
  struct sc_regs regs;
  regs.a1 = SYSMEM_INC_OP;
  regs.a2 = vmaid;
#ifdef MM64
  regs.a3 = size;
#else
  regs.a3 = PAGING_PAGE_ALIGNSZ(size);
#endif  
  /* chỉ syscall khi kernel sẵn sàng */
  if (caller && caller->krnl)
    syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */


  /*Successful increase limit */
  caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;

  pthread_mutex_unlock(&mmvm_lock);
  return 0;

}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
/*
__free (Hàm logic chính của libfree)
Chức năng: Trả một vùng nhớ (rgid) về danh sách free (vm_freerg_list).

Chi tiết code:

pthread_mutex_lock(&mmvm_lock);: Khóa.

struct vm_rg_struct *rgnode = get_symrg_byid(...): Lấy thông tin vùng đang cấp phát từ symrgtbl.

struct vm_rg_struct *freerg_node = malloc(...): Tạo một vm_rg_struct mới để đại diện cho vùng sẽ được giải phóng.

freerg_node->rg_start = rgnode->rg_start; ...: Sao chép thông tin (start, end) từ rgnode sang freerg_node.

rgnode->rg_start = rgnode->rg_end = 0;: Xóa thông tin trong symrgtbl. Điều này đánh dấu rgid này là "rỗng" (không còn hợp lệ).

enlist_vm_freerg_list(caller->krnl->mm, freerg_node);: Đưa freerg_node vừa tạo vào đầu danh sách free.

pthread_mutex_unlock(&mmvm_lock);: Mở khóa.
*/
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  
  if (!caller || !caller->krnl || !caller->krnl->mm) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  

  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) /*đồng bộ điều kiện với get_symrg_byid */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);

  if (!rgnode || (rgnode->rg_start == 0 && rgnode->rg_end == 0)) /*an toàn hơn */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

 /*
 liballoc và libfree (Các hàm Wrapper)
Đây là các hàm API "sạch" mà cpu.c gọi tới.

Chúng chỉ đơn giản là gọi hàm logic __alloc hoặc __free với vmaid = 0 (mặc định là heap) và reg_index (chỉ số đăng ký).
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
{
  addr_t  addr;

  int val = __alloc(proc, 0, reg_index, size, &addr);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  printf("IODUMP: PID %d ALLOC vaddr=" FORMAT_ADDR " size=%llu\n",
         proc->pid, addr, (unsigned long long)size);
#ifdef PAGETBL_DUMP
  printf("PAGETBL_DUMP: PID %d ALLOC region [rgid=%d] [" FORMAT_ADDR " -> " FORMAT_ADDR "]\n",
         proc->pid, reg_index, addr, addr + size);
#endif
#endif

  /* By default using vmaid = 0 */
  return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  int val = __free(proc, 0, reg_index);
  if (val == -1)
  {
    return -1;
  }
printf("%s:%d\n",__func__,__LINE__);
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  struct vm_rg_struct *rg = (proc && proc->krnl && proc->krnl->mm)
                            ? get_symrg_byid(proc->krnl->mm, reg_index) : NULL; /* === ĐÃ THÊM === guard */
  if (rg && rg->rg_start < rg->rg_end) {
    printf("IODUMP: PID %d FREE vaddr=" FORMAT_ADDR " size=%llu\n",
           proc->pid, rg->rg_start, (unsigned long long)(rg->rg_end - rg->rg_start));
  }
#ifdef PAGETBL_DUMP
  printf("PAGETBL_DUMP: PID %d FREE region [rgid=%d]\n", proc->pid, reg_index);
#endif
#endif
  return 0;//val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{

  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  if (!caller->krnl->mm->pgd) {
    /* Chưa có bảng trang: map 1-1 tạm thời để tránh segfault */
    *fpn = pgn;
    return 0;
  }

  uint32_t pte = pte_get_entry(caller, pgn);

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    addr_t vicpgn, swpfpn;
//    addr_t vicfpn;
//    addr_t vicpte;
//  struct sc_regs regs;

    /* TODO Initialize the target frame storing our variable */
//  addr_t tgtfpn 
    (void)mm; (void)vicpgn; (void)swpfpn;
    *fpn = pgn;
    return 0;
    /* TODO: Play with your paging theory here */
    /* Find victim page */
    if (find_victim_page(caller->krnl->mm, &vicpgn) == -1)
    {
      return -1;
    }

    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1)
    {
      return -1;
    }

    /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/

    /* TODO copy victim frame to swap 
     * SWP(vicfpn <--> swpfpn)
     * SYSCALL 1 sys_memmap
     */


    /* Update page table */
    //pte_set_swap(...);

    /* Update its online status of the target page */
    //pte_set_fpn(...);

    enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(pte);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
//  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

//  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  /* TODO 
   *  MEMPHY_read(caller->krnl->mram, phyaddr, data);
   *  MEMPHY READ 
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */

  /* guard mram NULL */
  if (!caller || !caller->krnl || !caller->krnl->mram) return -1;
 
  int off = PAGING_OFFST(addr);
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  if (MEMPHY_read(caller->krnl->mram, phyaddr, data) != 0)
    return -1;
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
//  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /*guard mram NULL */
  if (!caller || !caller->krnl || !caller->krnl->mram) return -1;
  

  /* TODO 
   *  MEMPHY_write(caller->krnl->mram, phyaddr, value);
   *  MEMPHY WRITE with SYSMEM_IO_WRITE 
   * SYSCALL 17 sys_memmap
   */
  int off = PAGING_OFFST(addr);
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  if (MEMPHY_write(caller->krnl->mram, phyaddr, value) != 0)
    return -1;

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
/*
__read và __write
Chức năng: Xử lý logic đọc/ghi 1 byte từ một vùng nhớ ảo.

Chi tiết code:

struct vm_rg_struct *currg = get_symrg_byid(...): Lấy vm_rg_struct (vùng đã alloc) dựa trên rgid (chỉ số đăng ký).

Kiểm tra truy cập hợp lệ (Bounds Checking):

if (currg == NULL || offset >= (currg->rg_end - currg->rg_start)) return -1;

Đây là bước kiểm tra bảo mật quan trọng: nó đảm bảo rgid là hợp lệ và offset không vượt ra ngoài kích thước (rg_end - rg_start) của vùng nhớ đã cấp phát.

addr_t vaddr = currg->rg_start + offset;: Tính địa chỉ ảo tuyệt đối.

Dịch địa chỉ (Đơn giản hóa):

addr_t fpn = vaddr / PAGING_PAGESZ;

addr_t off = vaddr % PAGING_PAGESZ;

Code  ở đây đang thực hiện một ánh xạ 1-1 (identity mapping), tức là vaddr (địa chỉ ảo) được coi như phyaddr (địa chỉ vật lý).

MEMPHY_read(...) hoặc MEMPHY_write(...): Gọi hàm cấp thấp để đọc/ghi trực tiếp lên mảng storage của mram.
*/
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
  /* guard NULL an toàn */
  if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
  

  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

//  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  /* TODO Invalid memory identify */
  if (currg == NULL || offset >= (currg->rg_end - currg->rg_start))
    return -1;

  /* guard mram NULL */
  if (!caller->krnl->mram) return -1;
 

  addr_t vaddr = currg->rg_start + offset;
  addr_t fpn = vaddr / PAGING_PAGESZ;
  addr_t off = vaddr % PAGING_PAGESZ;

  if (MEMPHY_read(caller->krnl->mram, fpn * PAGING_PAGESZ + off, data) != 0)
    return -1;

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    addr_t offset,    // Source address = [source] + [offset]
    uint32_t* destination)
{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  *destination = data;
#ifdef IODUMP
  /* TODO dump IO content (if needed) */
  struct vm_rg_struct *rg = (proc && proc->krnl && proc->krnl->mm)
                            ? get_symrg_byid(proc->krnl->mm, source) : NULL; /* guard */
  if (rg) {
    addr_t vaddr = rg->rg_start + offset;
    addr_t fpn = vaddr / PAGING_PAGESZ;
    addr_t off = vaddr % PAGING_PAGESZ;
    printf("IODUMP: PID %d READ  vaddr=" FORMAT_ADDR " fpn=%llu offset=%llu value=0x%02x\n",
           proc->pid, vaddr, (unsigned long long)fpn, (unsigned long long)off, data);
  }
#ifdef PAGETBL_DUMP
 printf("PAGETBL_DUMP: PID %d READ  rgid=%d offset=%llu (NO PAGE TABLE)\n",
         proc->pid, source, (unsigned long long)offset);
#endif
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
  pthread_mutex_lock(&mmvm_lock);

  /* guard NULL an toàn */
  if (!caller || !caller->krnl || !caller->krnl->mm) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  

  struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

  if (currg == NULL || offset >= (currg->rg_end - currg->rg_start)){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* guard mram NULL */
  if (!caller->krnl->mram) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  

  addr_t vaddr = currg->rg_start + offset;
  addr_t fpn = vaddr / PAGING_PAGESZ;
  addr_t off = vaddr % PAGING_PAGESZ;

  if (MEMPHY_write(caller->krnl->mram, fpn * PAGING_PAGESZ + off, value) != 0){
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);
  if (val == -1)
  {
    return -1;
  }
#ifdef IODUMP
#ifdef PAGETBL_DUMP
  struct vm_rg_struct *rg = (proc && proc->krnl && proc->krnl->mm)
                            ? get_symrg_byid(proc->krnl->mm, destination) : NULL; /*guard */
  if (rg) {
    addr_t vaddr = rg->rg_start + offset;
    addr_t fpn = vaddr / PAGING_PAGESZ;
    addr_t off = vaddr % PAGING_PAGESZ;
    printf("IODUMP: PID %d WRITE vaddr=" FORMAT_ADDR " fpn=%llu offset=%llu value=0x%02x\n",
           proc->pid, vaddr, (unsigned long long)fpn, (unsigned long long)off, data);
  }
#endif
  printf("PAGETBL_DUMP: PID %d WRITE rgid=%d offset=%llu (NO PAGE TABLE)\n",
         proc->pid, destination, (unsigned long long)offset);
#endif

  return val;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  pthread_mutex_lock(&mmvm_lock);

  /* guard NULL */
  if (!caller || !caller->krnl || !caller->krnl->mm) {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
    /* Nếu pgd chưa cấp phát thì không có gì để trả frame → return 0 an toàn */
  if (!caller->krnl->mm->pgd) {
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }
  

  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->krnl->mm->pgd[pagenum];

    if (PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      if (caller->krnl->mram)                 /* guard */
        MEMPHY_put_freefp(caller->krnl->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      if (caller->krnl->active_mswp)          /* guard */
        MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
    }
  }

  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}


/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
/*
find_victim_page
Chức năng: Tìm một trang (page) để "hi sinh" (swap-out) khi RAM đầy.

Thuật toán: FIFO (First-In, First-Out).

Chi tiết code:

struct pgn_t *pg = mm->fifo_pgn;: fifo_pgn là con trỏ đầu của danh sách liên kết các PGN đang nằm trong RAM.

while (pg->pg_next) ...: Duyệt danh sách để tìm node cuối cùng (node này là node được nạp vào RAM đầu tiên).

*retpgn = pg->pgn;: Lấy PGN của trang "nạn nhân".

Xóa node đó khỏi danh sách (bằng cách cho node prev trỏ next = NULL) và free(pg).
*/
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (!pg)
  {
    return -1;
  }
  struct pgn_t *prev = NULL;
  while (pg->pg_next)
  {
    prev = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;

  /* xử lý đúng trường hợp chỉ còn 1 node (prev == NULL) */
  if (prev == NULL) {
    mm->fifo_pgn = NULL;
  } else {
    prev->pg_next = NULL;
  }
  

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
/*
get_free_vmrg_area
Chức năng: Tìm một vùng trống (vm_rg_struct) trong vm_freerg_list đủ lớn (>= size).

Thuật toán: First-Fit.

Chi tiết code:

struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;: Lấy con trỏ rgit (iterator) trỏ vào đầu danh sách free.

while (rgit != NULL): Duyệt danh sách.

if (rgit->rg_start + size <= rgit->rg_end): (First-Fit) Kiểm tra xem vùng rgit hiện tại có đủ size không.

Nếu đủ:

newrg->rg_start = rgit->rg_start; ...: Ghi lại thông tin vùng sẽ cấp phát vào newrg (để __alloc dùng).

Xử lý phần còn lại (Fragment):

if (rgit->rg_start + size < rgit->rg_end): Nếu vùng rgit lớn hơn size -> chỉ "cắt" size bytes từ đầu, cập nhật lại rgit->rg_start (dịch nó lên size bytes).

else: Nếu rgit vừa bằng size -> xóa rgit khỏi danh sách (bằng cách copy nextrg đè lên rgit và free(nextrg)).

break;: Đã tìm thấy, thoát vòng lặp.

Nếu không đủ: rgit = rgit->rg_next;: Đi đến vùng trống tiếp theo.

if (newrg->rg_start == -1) return -1;: Nếu duyệt hết mà newrg->rg_start vẫn là -1 (chưa tìm thấy), trả về -1 (báo __alloc phải mở rộng heap).
*/
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        {                                /*End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

// #endif
