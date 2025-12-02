// /*
//  * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
//  */

// /* LamiaAtrium release
//  * Source Code License Grant: The authors hereby grant to Licensee
//  * personal permission to use and modify the Licensed Source Code
//  * for the sole purpose of studying while attending the course CO2018.
//  */

// // #ifdef MM_PAGING
// /*
//  * System Library
//  * Memory Module Library libmem.c 
//  */


//  /*================================================================================================================================================================================*/
// /*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
//  /*================================================================================================================================================================================*/
// /*
// File libmem.c là một trong những thành phần cốt lõi, đóng vai trò là "thư viện" (library) quản lý bộ nhớ ảo cấp cao.
// Nó là lớp trung gian giữa lệnh của CPU (cpu.c gọi liballoc, libfree...) và hệ thống bảng trang/bộ nhớ vật lý (mm64.c, mm-memphy.c).
// Chức năng chính của libmem.c là quản lý các vùng nhớ (regions - vm_rg_struct) bên trong một vùng ảo (area - vm_area_struct), cụ thể là vùng heap.
// Nó chịu trách nhiệm:

// 1. Xử lý lệnh ALLOC: Tìm một vùng nhớ ảo (vm_rg_struct) còn trống trong vm_freerg_list. Nếu không tìm thấy, nó sẽ yêu cầu Kernel (thông qua System Call) để mở rộng vùng heap (tăng con trỏ sbrk).

// 2. Xử lý lệnh FREE: Lấy một vùng nhớ (vm_rg_struct) đang sử dụng và trả nó về danh sách vm_freerg_list để tái sử dụng sau này.

// 3. Xử lý READ/WRITE: Dịch "chỉ số đăng ký" (register index) và "offset" thành một địa chỉ ảo tuyệt đối (virtual address).

// 4. Quản lý bộ nhớ trống: Implement thuật toán First-Fit để tìm kiếm và tái sử dụng các vùng nhớ đã được FREE.

// 5. Đảm bảo an toàn (Thread-Safety): Sử dụng một mutex (khóa) để đảm bảo rằng các cấu trúc dữ liệu (như vm_freerg_list) không bị hỏng khi nhiều CPU/tiến trình cùng truy cập một lúc.



// Các thư viện (Headers) liên quan
// 1. "mm.h" và "mm64.h": Cung cấp các định nghĩa cấu trúc quan trọng nhất (pcb_t, mm_struct, vm_area_struct, vm_rg_struct), các hằng số (PAGING_PAGESZ, PAGING_MAX_SYMTBL_SZ), và các macro thao tác địa chỉ/bit.

// 2. "syscall.h": Cần thiết để thực hiện lời gọi hệ thống (system call). Nó định nghĩa hàm syscall() và các hằng số như SYSMEM_INC_OP.

// 3. "libmem.h": File header của chính nó, khai báo các hàm liballoc, libfree...

// 4. <stdlib.h>: Thư viện chuẩn C, dùng cho malloc và free (ví dụ, khi tạo freerg_node trong hàm __free).

// 5. <stdio.h>: Dùng cho printf (chủ yếu cho các lệnh IODUMP và PAGETBL_DUMP).

// 6. <pthread.h>: Rất quan trọng, dùng để tạo và quản lý pthread_mutex_t mmvm_lock, đảm bảo an toàn cho các thao tác bộ nhớ.


// Biến và Cấu trúc dữ liệu chính
// static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

// Đây là một "ổ khóa" (mutex). Bất kỳ hàm nào muốn thay đổi các cấu trúc dữ liệu quản lý bộ nhớ (như vm_freerg_list hoặc symrgtbl) đều phải "khóa" (pthread_mutex_lock) trước khi bắt đầu và "mở khóa" (pthread_mutex_unlock) ngay sau khi hoàn thành.

// Điều này ngăn ngừa tình trạng "race condition" (khi 2 tiến trình cùng lúc alloc hoặc free, làm hỏng danh sách liên kết).

// */


// #include "string.h"
// #include "mm.h"
// #include "mm64.h"
// #include "syscall.h"
// #include "libmem.h"
// #include <stdlib.h>
// #include <stdio.h>
// #include <pthread.h>

// static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

// /*enlist_vm_freerg_list - add new rg to freerg_list
//  *@mm: memory region
//  *@rg_elmt: new region
//  *
//  */
// /*enlist_vm_freerg_list
// Chức năng: Thêm một vm_rg_struct (vùng nhớ) vào đầu danh sách các vùng nhớ trống (vm_freerg_list).

// Chi tiết code: Đây là thao tác "push" vào đầu một danh sách liên kết (linked list) đơn:

// Lấy con trỏ rg_node trỏ đến đầu danh sách free hiện tại.

// Kiểm tra rg_elmt (vùng nhớ mới) có hợp lệ không (kích thước > 0).

// Cho rg_elmt->rg_next trỏ vào rg_node (đầu cũ).

// Cập nhật mm->mmap->vm_freerg_list trỏ vào rg_elmt (đầu mới).
//  */

// int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)   // rg_elmt (vùng nhớ mới)
// {
//   struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list; // vùng nhớ cũ (danh sách free hiện tại)

//   if (rg_elmt->rg_start >= rg_elmt->rg_end)
//     return -1;

//   if (rg_node != NULL)
//     rg_elmt->rg_next = rg_node; // thêm mới vào đầu cũ

//   /* Enlist the new region */
//   mm->mmap->vm_freerg_list = rg_elmt; // gáng lại đầu danh sách vào đầu mới  // gióng gáng lại đầu cho link _ list

//   return 0;
// }

// /*get_symrg_byid - get mem region by region ID
//  *@mm: memory region
//  *@rgid: region ID act as symbol index of variable
//  *
//  */

//  /*
//  get_symrg_byid
// Chức năng: Lấy con trỏ đến một vm_rg_struct đang được sử dụng từ "bảng ký hiệu" (symrgtbl) dựa trên rgid (chỉ số đăng ký).

// Chi tiết code:

// Kiểm tra rgid có nằm trong giới hạn của mảng [0, PAGING_MAX_SYMTBL_SZ - 1] không.

// Nếu hợp lệ, trả về địa chỉ của phần tử &mm->symrgtbl[rgid].
//  */
// struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
// {
//   /*fix off-by-one: ngăn vượt biên khi rgid == PAGING_MAX_SYMTBL_SZ */
//   if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
//     return NULL;
  

//   return &mm->symrgtbl[rgid];
// }

// /*__alloc - allocate a region memory
//  *@caller: caller
//  *@vmaid: ID vm area to alloc memory region
//  *@rgid: memory region ID (used to identify variable in symbole table)
//  *@size: allocated size
//  *@alloc_addr: address of allocated memory region
//  *
//  */
// /*
// __alloc (Hàm logic chính của liballoc)
// Chức năng: Cấp phát một vùng nhớ ảo kích thước size và đăng ký nó với rgid.

// Chi tiết code:

// 1. pthread_mutex_lock(&mmvm_lock);: Khóa lại để bảo vệ.

// 2. struct vm_area_struct *cur_vma = get_vma_by_num(...): Lấy VMA 0 (heap).

// 3. Thử tìm trong danh sách free (First-Fit):

// Gọi get_free_vmrg_area(..., size, ...) (hàm này sẽ được giải thích ở dưới).

// Nếu thành công (== 0):

// Vùng rgnode (tham số đầu ra của get_free_vmrg_area) chứa thông tin (rg_start, rg_end) của vùng trống vừa tìm thấy.

// Cập nhật thông tin này vào mm->symrgtbl[rgid].

// Gán địa chỉ bắt đầu vào *alloc_addr (để liballoc sử dụng).

// pthread_mutex_unlock(...) và return 0 (thành công).

// 4. Nếu thất bại (không có vùng trống nào đủ lớn):

// Phải mở rộng heap.

// inc_sz = PAGING_PAGE_ALIGNSZ(size);: Tính kích thước cần mở rộng, làm tròn lên cho vừa vặn với kích thước trang.

// old_sbrk = cur_vma->sbrk;: Lưu lại đỉnh heap cũ (đây sẽ là địa chỉ bắt đầu của vùng nhớ mới).

// Chuẩn bị và gọi System Call:

// regs.a1 = SYSMEM_INC_OP;: Đặt lệnh là "Tăng VMA".

// regs.a3 = ...size;: Đặt tham số là kích thước cần tăng.

// syscall(caller->krnl, caller->pid, 17, &regs);: Gọi kernel (syscall 17). Kernel sẽ chạy sys_memmap -> inc_vma_limit, làm tăng cur_vma->sbrk.

// Đăng ký vùng nhớ mới:

// caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;: Địa chỉ bắt đầu là đỉnh heap cũ.

// caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;: Địa chỉ kết thúc.

// *alloc_addr = old_sbrk;.

// 5. pthread_mutex_unlock(&mmvm_lock);: Mở khóa và return 0.
// */
// int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
// {
//   /*Allocate at the toproof */
//   pthread_mutex_lock(&mmvm_lock);

//   /*  guard NULL để tránh segfault sớm */
//   if (!caller || !caller->krnl || !caller->krnl->mm || !caller->krnl->mm->mmap) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }
  

//   struct vm_rg_struct rgnode;
//   struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid); //struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid); mm.h
//   int inc_sz=0;

//   if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) //int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg); mm.h
//   {
//     caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
//     caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
 
//     *alloc_addr = rgnode.rg_start;

//     pthread_mutex_unlock(&mmvm_lock);
//     return 0;
//   }

//   /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

//   /*Attempt to increate limit to get space */
// #ifdef MM64
//   inc_sz = (uint32_t)(size/(int)PAGING64_PAGESZ);
//   inc_sz = inc_sz + 1;
// #else
//   inc_sz = PAGING_PAGE_ALIGNSZ(size);
// #endif
//   int old_sbrk;
//   inc_sz = inc_sz + 1;

//   old_sbrk = cur_vma->sbrk;

//   /* TODO INCREASE THE LIMIT
//    * SYSCALL 1 sys_memmap
//    */
//   struct sc_regs regs;
//   regs.a1 = SYSMEM_INC_OP;
//   regs.a2 = vmaid;
// #ifdef MM64
//   regs.a3 = size;
// #else
//   regs.a3 = PAGING_PAGE_ALIGNSZ(size);
// #endif  
//   /* chỉ syscall khi kernel sẵn sàng */
//   if (caller && caller->krnl)
//     syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */


//   /*Successful increase limit */
//   caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;
//   caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

//   *alloc_addr = old_sbrk;

//   pthread_mutex_unlock(&mmvm_lock);
//   return 0;

// }

// /*__free - remove a region memory
//  *@caller: caller
//  *@vmaid: ID vm area to alloc memory region
//  *@rgid: memory region ID (used to identify variable in symbole table)
//  *@size: allocated size
//  *
//  */
// /*
// __free (Hàm logic chính của libfree)
// Chức năng: Trả một vùng nhớ (rgid) về danh sách free (vm_freerg_list).

// Chi tiết code:

// pthread_mutex_lock(&mmvm_lock);: Khóa.

// struct vm_rg_struct *rgnode = get_symrg_byid(...): Lấy thông tin vùng đang cấp phát từ symrgtbl.

// struct vm_rg_struct *freerg_node = malloc(...): Tạo một vm_rg_struct mới để đại diện cho vùng sẽ được giải phóng.

// freerg_node->rg_start = rgnode->rg_start; ...: Sao chép thông tin (start, end) từ rgnode sang freerg_node.

// rgnode->rg_start = rgnode->rg_end = 0;: Xóa thông tin trong symrgtbl. Điều này đánh dấu rgid này là "rỗng" (không còn hợp lệ).

// enlist_vm_freerg_list(caller->krnl->mm, freerg_node);: Đưa freerg_node vừa tạo vào đầu danh sách free.

// pthread_mutex_unlock(&mmvm_lock);: Mở khóa.
// */
// int __free(struct pcb_t *caller, int vmaid, int rgid)
// {
//   pthread_mutex_lock(&mmvm_lock);

  
//   if (!caller || !caller->krnl || !caller->krnl->mm) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }
  

//   if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) /*đồng bộ điều kiện với get_symrg_byid */
//   {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

//   /* TODO: Manage the collect freed region to freerg_list */
//   struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);

//   if (!rgnode || (rgnode->rg_start == 0 && rgnode->rg_end == 0)) /*an toàn hơn */
//   {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }
//   struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
//   freerg_node->rg_start = rgnode->rg_start;
//   freerg_node->rg_end = rgnode->rg_end;
//   freerg_node->rg_next = NULL;

//   rgnode->rg_start = rgnode->rg_end = 0;
//   rgnode->rg_next = NULL;

//   /*enlist the obsoleted memory region */
//   enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

//   pthread_mutex_unlock(&mmvm_lock);
//   return 0;
// }

// /*liballoc - PAGING-based allocate a region memory
//  *@proc:  Process executing the instruction
//  *@size: allocated size
//  *@reg_index: memory region ID (used to identify variable in symbole table)
//  */

//  /*
//  liballoc và libfree (Các hàm Wrapper)
// Đây là các hàm API "sạch" mà cpu.c gọi tới.

// Chúng chỉ đơn giản là gọi hàm logic __alloc hoặc __free với vmaid = 0 (mặc định là heap) và reg_index (chỉ số đăng ký).
//  */
// int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)
// {
//   addr_t  addr;

//   int val = __alloc(proc, 0, reg_index, size, &addr);
//   if (val == 0)
//   {
//     proc->regs[reg_index] = addr;
//   }
// #ifdef IODUMP
//   /* TODO dump IO content (if needed) */
//   printf("liballoc:178\n");
//   print_pgtbl(proc, 0, 0); 

// #endif

//   /* By default using vmaid = 0 */
//   return val;
// }

// /*libfree - PAGING-based free a region memory
//  *@proc: Process executing the instruction
//  *@size: allocated size
//  *@reg_index: memory region ID (used to identify variable in symbole table)
//  */

// int libfree(struct pcb_t *proc, uint32_t reg_index)
// {
//   if (proc->regs[reg_index] == 0) {
//       return -1;
//   }
//   int val = __free(proc, 0, reg_index);
// #ifdef IODUMP
//   /* TODO dump IO content (if needed) */
//   printf("libfree:218\n");
//   print_pgtbl(proc, 0, 0);
// #endif
//   return 0;//val;
// }

// /*pg_getpage - get the page in ram
//  *@mm: memory region
//  *@pagenum: PGN
//  *@framenum: return FPN
//  *@caller: caller
//  *
//  */
// int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
// {

//   if (!caller || !caller->krnl || !caller->krnl->mm) return -1;
//   if (!caller->krnl->mm->pgd) {
//     /* Chưa có bảng trang: map 1-1 tạm thời để tránh segfault */
//     *fpn = pgn;
//     return 0;
//   }

//   uint32_t pte = pte_get_entry(caller, pgn);

//   if (!PAGING_PAGE_PRESENT(pte))
//   { /* Page is not online, make it actively living */
//     addr_t vicpgn, swpfpn;
// //    addr_t vicfpn;
// //    addr_t vicpte;
// //  struct sc_regs regs;

//     /* TODO Initialize the target frame storing our variable */
// //  addr_t tgtfpn 
//     (void)mm; (void)vicpgn; (void)swpfpn;
//     *fpn = pgn;
//     return 0;
//     /* TODO: Play with your paging theory here */
//     /* Find victim page */
//     if (find_victim_page(caller->krnl->mm, &vicpgn) == -1)
//     {
//       return -1;
//     }

//     /* Get free frame in MEMSWP */
//     if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1)
//     {
//       return -1;
//     }

//     /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/

//     /* TODO copy victim frame to swap 
//      * SWP(vicfpn <--> swpfpn)
//      * SYSCALL 1 sys_memmap
//      */


//     /* Update page table */
//     //pte_set_swap(...);

//     /* Update its online status of the target page */
//     //pte_set_fpn(...);

//     enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
//   }

//   *fpn = PAGING_FPN(pte);

//   return 0;
// }

// /*pg_getval - read value at given offset
//  *@mm: memory region
//  *@addr: virtual address to acess
//  *@value: value
//  *
//  */
// int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
// {
//   int pgn = PAGING_PGN(addr);
// //  int off = PAGING_OFFST(addr);
//   int fpn;

//   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
//     return -1; /* invalid page access */

// //  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

//   /* TODO 
//    *  MEMPHY_read(caller->krnl->mram, phyaddr, data);
//    *  MEMPHY READ 
//    *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
//    */

//   /* guard mram NULL */
//   if (!caller || !caller->krnl || !caller->krnl->mram) return -1;
 
//   int off = PAGING_OFFST(addr);
//   int phyaddr = (fpn * PAGING_PAGESZ) + off;
//   if (MEMPHY_read(caller->krnl->mram, phyaddr, data) != 0)
//     return -1;
//   return 0;
// }

// /*pg_setval - write value to given offset
//  *@mm: memory region
//  *@addr: virtual address to acess
//  *@value: value
//  *
//  */
// int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
// {
//   int pgn = PAGING_PGN(addr);
// //  int off = PAGING_OFFST(addr);
//   int fpn;

//   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
//   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
//     return -1; /* invalid page access */

//   /*guard mram NULL */
//   if (!caller || !caller->krnl || !caller->krnl->mram) return -1;
  

//   /* TODO 
//    *  MEMPHY_write(caller->krnl->mram, phyaddr, value);
//    *  MEMPHY WRITE with SYSMEM_IO_WRITE 
//    * SYSCALL 17 sys_memmap
//    */
//   int off = PAGING_OFFST(addr);
//   int phyaddr = (fpn * PAGING_PAGESZ) + off;
//   if (MEMPHY_write(caller->krnl->mram, phyaddr, value) != 0)
//     return -1;

//   return 0;
// }

// /*__read - read value in region memory
//  *@caller: caller
//  *@vmaid: ID vm area to alloc memory region
//  *@offset: offset to acess in memory region
//  *@rgid: memory region ID (used to identify variable in symbole table)
//  *@size: allocated size
//  *
//  */
// /*
// __read và __write
// Chức năng: Xử lý logic đọc/ghi 1 byte từ một vùng nhớ ảo.

// Chi tiết code:

// struct vm_rg_struct *currg = get_symrg_byid(...): Lấy vm_rg_struct (vùng đã alloc) dựa trên rgid (chỉ số đăng ký).

// Kiểm tra truy cập hợp lệ (Bounds Checking):

// if (currg == NULL || offset >= (currg->rg_end - currg->rg_start)) return -1;

// Đây là bước kiểm tra bảo mật quan trọng: nó đảm bảo rgid là hợp lệ và offset không vượt ra ngoài kích thước (rg_end - rg_start) của vùng nhớ đã cấp phát.

// addr_t vaddr = currg->rg_start + offset;: Tính địa chỉ ảo tuyệt đối.

// Dịch địa chỉ (Đơn giản hóa):

// addr_t fpn = vaddr / PAGING_PAGESZ;

// addr_t off = vaddr % PAGING_PAGESZ;

// Code  ở đây đang thực hiện một ánh xạ 1-1 (identity mapping), tức là vaddr (địa chỉ ảo) được coi như phyaddr (địa chỉ vật lý).

// MEMPHY_read(...) hoặc MEMPHY_write(...): Gọi hàm cấp thấp để đọc/ghi trực tiếp lên mảng storage của mram.
// */
// int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
// {
 
//   pthread_mutex_lock(&mmvm_lock);

//   /*Kiểm tra con trỏ an toàn */
//   if (!caller || !caller->krnl || !caller->krnl->mm) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

  
//   struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);

 
//   if (currg == NULL || currg->rg_start >= currg->rg_end || offset >= (currg->rg_end - currg->rg_start)) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

//   /*TÍNH ĐỊA CHỈ ẢO TUYỆT ĐỐI */
//   addr_t vaddr = currg->rg_start + offset;

//   /* ĐỌC DỮ LIỆU QUA CƠ CHẾ PAGING  */
//   /* * Thay vì dùng MEMPHY_read trực tiếp, ta gọi pg_getval.
//    * pg_getval sẽ:
//    * - Dịch vaddr -> PGN (Page Number)
//    * - Tra cứu Page Table để lấy FPN (Frame Number)
//    * - Nếu trang bị Swap-out, nó sẽ Swap-in lại vào RAM
//    * - Sau đó mới đọc dữ liệu trả về biến 'data'
//    */
//   if (pg_getval(caller->krnl->mm, vaddr, data, caller) != 0) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

//   /* 7. MỞ KHÓA */
//   pthread_mutex_unlock(&mmvm_lock);
//   return 0;
// }

// /*libread - PAGING-based read a region memory */
// int libread(
//     struct pcb_t *proc, // Process executing the instruction
//     uint32_t source,    // Index of source register
//     addr_t offset,    // Source address = [source] + [offset]
//     uint32_t* destination)
// {
//   BYTE data;
//   int val = __read(proc, 0, source, offset, &data);

//   *destination = data;
// #ifdef IODUMP
//   printf("libread:426\n");
//   print_pgtbl(proc, 0, 0);
// #endif

//   return val;
// }

// /*__write - write a region memory
//  *@caller: caller
//  *@vmaid: ID vm area to alloc memory region
//  *@offset: offset to acess in memory region
//  *@rgid: memory region ID (used to identify variable in symbole table)
//  *@size: allocated size
//  *
//  */
// int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
// {
  
//   pthread_mutex_lock(&mmvm_lock);

 
//   if (!caller || !caller->krnl || !caller->krnl->mm) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

  
//   struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
  
//   // Kiểm tra bounds (Offset không được vượt quá kích thước vùng nhớ)
//   if (currg == NULL || currg->rg_start >= currg->rg_end || offset >= (currg->rg_end - currg->rg_start)) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

//   /*TÍNH ĐỊA CHỈ ẢO TUYỆT ĐỐI */
//   addr_t vaddr = currg->rg_start + offset;


//   /* pg_setval sẽ lo việc tìm Page Table và xử lý Swap */
//   if (pg_setval(caller->krnl->mm, vaddr, value, caller) != 0) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }

 
//   pthread_mutex_unlock(&mmvm_lock);
//   return 0;
// }

// /*libwrite - PAGING-based write a region memory */
// int libwrite(
//     struct pcb_t *proc,   // Process executing the instruction
//     BYTE data,            // Data to be wrttien into memory
//     uint32_t destination, // Index of destination register
//     addr_t offset)
// {
//   int val = __write(proc, 0, destination, offset, data);
//   if (val == -1)
//   {
//     return -1;
//   }
// #ifdef IODUMP
//   printf("libwrite:502\n");
//   print_pgtbl(proc, 0, 0);
// #endif

//   return val;
// }

// /*free_pcb_memphy - collect all memphy of pcb
//  *@caller: caller
//  *@vmaid: ID vm area to alloc memory region
//  *@incpgnum: number of page
//  */
// int free_pcb_memph(struct pcb_t *caller)
// {
//   pthread_mutex_lock(&mmvm_lock);

//   /* guard NULL */
//   if (!caller || !caller->krnl || !caller->krnl->mm) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return -1;
//   }
//     /* Nếu pgd chưa cấp phát thì không có gì để trả frame → return 0 an toàn */
//   if (!caller->krnl->mm->pgd) {
//     pthread_mutex_unlock(&mmvm_lock);
//     return 0;
//   }
  

//   int pagenum, fpn;
//   uint32_t pte;

//   for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
//   {
//     pte = caller->krnl->mm->pgd[pagenum];

//     if (PAGING_PAGE_PRESENT(pte))
//     {
//       fpn = PAGING_FPN(pte);
//       if (caller->krnl->mram)                 /* guard */
//         MEMPHY_put_freefp(caller->krnl->mram, fpn);
//     }
//     else
//     {
//       fpn = PAGING_SWP(pte);
//       if (caller->krnl->active_mswp)          /* guard */
//         MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
//     }
//   }

//   pthread_mutex_unlock(&mmvm_lock);
//   return 0;
// }


// /*find_victim_page - find victim page
//  *@caller: caller
//  *@pgn: return page number
//  *
//  */
// /*
// find_victim_page
// Chức năng: Tìm một trang (page) để "hi sinh" (swap-out) khi RAM đầy.

// Thuật toán: FIFO (First-In, First-Out).

// Chi tiết code:

// struct pgn_t *pg = mm->fifo_pgn;: fifo_pgn là con trỏ đầu của danh sách liên kết các PGN đang nằm trong RAM.

// while (pg->pg_next) ...: Duyệt danh sách để tìm node cuối cùng (node này là node được nạp vào RAM đầu tiên).

// *retpgn = pg->pgn;: Lấy PGN của trang "nạn nhân".

// Xóa node đó khỏi danh sách (bằng cách cho node prev trỏ next = NULL) và free(pg).
// */
// int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
// {
//   struct pgn_t *pg = mm->fifo_pgn;

//   /* TODO: Implement the theorical mechanism to find the victim page */
//   if (!pg)
//   {
//     return -1;
//   }
//   struct pgn_t *prev = NULL;
//   while (pg->pg_next)
//   {
//     prev = pg;
//     pg = pg->pg_next;
//   }
//   *retpgn = pg->pgn;

//   /* xử lý đúng trường hợp chỉ còn 1 node (prev == NULL) */
//   if (prev == NULL) {
//     mm->fifo_pgn = NULL;
//   } else {
//     prev->pg_next = NULL;
//   }
  

//   free(pg);

//   return 0;
// }

// /*get_free_vmrg_area - get a free vm region
//  *@caller: caller
//  *@vmaid: ID vm area to alloc memory region
//  *@size: allocated size
//  *
//  */
// /*
// get_free_vmrg_area
// Chức năng: Tìm một vùng trống (vm_rg_struct) trong vm_freerg_list đủ lớn (>= size).

// Thuật toán: First-Fit.

// Chi tiết code:

// struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;: Lấy con trỏ rgit (iterator) trỏ vào đầu danh sách free.

// while (rgit != NULL): Duyệt danh sách.

// if (rgit->rg_start + size <= rgit->rg_end): (First-Fit) Kiểm tra xem vùng rgit hiện tại có đủ size không.

// Nếu đủ:

// newrg->rg_start = rgit->rg_start; ...: Ghi lại thông tin vùng sẽ cấp phát vào newrg (để __alloc dùng).

// Xử lý phần còn lại (Fragment):

// if (rgit->rg_start + size < rgit->rg_end): Nếu vùng rgit lớn hơn size -> chỉ "cắt" size bytes từ đầu, cập nhật lại rgit->rg_start (dịch nó lên size bytes).

// else: Nếu rgit vừa bằng size -> xóa rgit khỏi danh sách (bằng cách copy nextrg đè lên rgit và free(nextrg)).

// break;: Đã tìm thấy, thoát vòng lặp.

// Nếu không đủ: rgit = rgit->rg_next;: Đi đến vùng trống tiếp theo.

// if (newrg->rg_start == -1) return -1;: Nếu duyệt hết mà newrg->rg_start vẫn là -1 (chưa tìm thấy), trả về -1 (báo __alloc phải mở rộng heap).
// */
// int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
// {
//   struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

//   struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

//   if (rgit == NULL)
//     return -1;

//   /* Probe unintialized newrg */
//   newrg->rg_start = newrg->rg_end = -1;

//   /* Traverse on list of free vm region to find a fit space */
//   while (rgit != NULL)
//   {
//     if (rgit->rg_start + size <= rgit->rg_end)
//     { /* Current region has enough space */
//       newrg->rg_start = rgit->rg_start;
//       newrg->rg_end = rgit->rg_start + size;

//       /* Update left space in chosen region */
//       if (rgit->rg_start + size < rgit->rg_end)
//       {
//         rgit->rg_start = rgit->rg_start + size;
//       }
//       else
//       { /*Use up all space, remove current node */
//         /*Clone next rg node */
//         struct vm_rg_struct *nextrg = rgit->rg_next;

//         /*Cloning */
//         if (nextrg != NULL)
//         {
//           rgit->rg_start = nextrg->rg_start;
//           rgit->rg_end = nextrg->rg_end;

//           rgit->rg_next = nextrg->rg_next;

//           free(nextrg);
//         }
//         else
//         {                                /*End of free list */
//           rgit->rg_start = rgit->rg_end; // dummy, size 0 region
//           rgit->rg_next = NULL;
//         }
//       }
//       break;
//     }
//     else
//     {
//       rgit = rgit->rg_next; // Traverse next rg
//     }
//   }

//   if (newrg->rg_start == -1) // new region not found
//     return -1;

//   return 0;
// }

// // #endif


/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
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

//struct vm_rg_struct *currg = &caller->krnl->mm->symrgtbl[rgid];

/*enlist_vm_freerg_list - add new rg to freerg_list */
//Trong cái file libmem.c này thì tui nên đọc gì trước


//Trong file này thì tui nên đọc hàm nào trước
//Hàm inc_vma_limit nên được đọc trước vì nó là hàm chính trong file này và thực hiện chức năng quan trọng là mở rộng giới hạn vùng nhớ ảo (VMA) cho tiến trình.
//kế tiếp là tới hàm gì
//Sau khi hiểu hàm inc_vma_limit, bạn nên đọc hàm __alloc vì nó sử dụng inc_vma_limit để cấp phát bộ nhớ cho tiến trình.
//Cuối cùng, bạn có thể đọc hàm __free để hiểu cách giải phóng bộ nhớ đã cấp phát.
//Bằng cách đọc các hàm này theo thứ tự, bạn sẽ có cái nhìn tổng quan về cách quản lý bộ nhớ trong hệ thống, từ việc cấp phát đến việc giải phóng bộ nhớ.
//Vậy hàm inc_vma_limit này là hàm gì?
//Hàm inc_vma_limit là một hàm trong hệ thống quản lý bộ nhớ ảo (virtual memory management) của hệ điều hành.
//Nó được sử dụng để mở rộng giới hạn vùng nhớ ảo (VMA - Virtual Memory Area) cho một tiến trình cụ thể.
//VMA đại diện cho một vùng liên tục trong không gian địa chỉ ảo của tiến trình     
//Hàm inc_vma_limit nhận vào ba tham số:
//1. caller: con trỏ đến cấu trúc pcb_t đại diện cho tiến trình gọi hàm.
//2. vmaid: ID của vùng nhớ ảo (VMA) mà tiến trình muốn mở rộng.
//3. inc_sz: kích thước cần mở rộng cho vùng nhớ ảo.
//Hàm này sẽ tìm vùng nhớ ảo tương ứng với vmaid trong cấu trúc mm_struct của tiến trình.
//Nếu tìm thấy, nó sẽ kiểm tra xem việc mở rộng có vượt quá giới hạn bộ nhớ vật lý hay không.
//Nếu không vượt quá, nó sẽ cập nhật giới hạn vùng nhớ ảo và trả về 0 để báo hiệu thành công.
//Nếu không tìm thấy vùng nhớ ảo hoặc việc mở rộng vượt quá giới hạn bộ nhớ vật lý, hàm sẽ trả về -1 để báo hiệu lỗi.
//Hàm này rất quan trọng trong việc quản lý bộ nhớ ảo, giúp đảm bảo rằng tiến trình có đủ bộ nhớ để thực thi các tác vụ của nó một cách hiệu quả.

//hàm inc_vma_limit được định nghĩa ở đâu? và trong file nào?
//Hàm inc_vma_limit được định nghĩa trong file src/libmem.c.
//Nó là một phần của hệ thống quản lý bộ nhớ ảo trong hệ điều hành giả lập LamiaAtrium.
//File libmem.c chứa các hàm liên quan đến quản lý bộ nhớ, bao gồm cả việc cấp phát và giải phóng bộ nhớ cho các tiến trình.
//Hàm inc_vma_limit cụ thể được sử dụng để mở rộng giới hạn vùng nhớ ảo (VMA) cho tiến trình khi cần thiết.
//Bạn có thể tìm thấy định nghĩa của hàm này trong mã nguồn của file src/libmem.c để hiểu rõ hơn về cách nó hoạt động.

//hàm nhớ đc tạo ra mà chưa dùng thì gọi thk này để cho thuê 
//inc_vma_limit mở rộng nó xin đc một lô đất to mà chưa dùng hết thì sẽ đưa lô đất đó vào ds sẵn sàng cấp phát 
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;
  /*
  Input:
        mm: Cấu trúc quản lý bộ nhớ (cái bảng tin).
        rg_elmt: Vùng nhớ cần đăng ký (cái biển báo phòng trống).
  */
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
  //vm_freerg_list là một Danh sách liên kết (Linked List) chứa các vùng nhớ trống.
  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;
    //insert at head

  mm->mmap->vm_freerg_list = rg_elmt;
  return 0;
}

/*get_symrg_byid - get mem region by region ID */
//hàm get_symrg_byid dùng để làm gì?
//cuốn sổ 
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
    return NULL;
  }
  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory */
// TRONG __alloc() - libmem.c
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr)
//caller: process
//vmaid: ID vùng nhớ ảo , trong bài tập này khu heap sẽ đc gán là 0
//rgid: ID thanh ghi, vùng nhớ , lô số mấy ? -> quy đinnhj khu thuộc về cái tạo ra
//size: kích thước
//alloc_addr: tờ giấy trắng ghi kết quả 
/*
Trong C, hàm __alloc trả về int (0 hoặc -1) chỉ để báo là "Thành công" hay "Thất bại". Nó không thể trả về 2 giá trị cùng lúc được.
Vậy làm sao trả về Địa chỉ bắt đầu của vùng nhớ vừa cấp?
Người gọi hàm phải đưa sẵn một con trỏ (alloc_addr).
Hàm __alloc tính toán xong, sẽ viết địa chỉ nhà mới (ví dụ: 256) vào nơi mà con trỏ này trỏ tới.
Khi hàm kết thúc, người gọi mở phong bì ra sẽ thấy số 256.
*/
{
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) { //check id có nằm trong giới hạn cho phép hay không
        return -1;// hệ thống chỉ quản lí 1000 biến nếu đứa vào số -1 hoặc 2000 là lỗi
    }

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
    if (cur_vma == NULL) {
        return -1;
    }

    struct vm_rg_struct *rgnode = &caller->krnl->mm->symrgtbl[rgid];

    if (rgnode->rg_start != 0 || rgnode->rg_end != 0) {
        return -1;
    }

    addr_t alloc_start = cur_vma->sbrk;//đánh dấu đã dùng với đất hoang
    addr_t alloc_end = alloc_start + size;
    
    if (alloc_end > cur_vma->vm_end) {
        addr_t needed_size = alloc_end - cur_vma->sbrk;
        //nếu yc cấp phát vượt quá giới hạn của vùng nhớ thì sẽ gọi inc ra để nông heap ra 
        if (inc_vma_limit(caller, vmaid, needed_size) != 0) {
            printf("[ERROR] __alloc: Failed to expand VMA\n");
            return -1;
        }
        // Sau khi expand, sbrk đã được cập nhật
        alloc_start = cur_vma->sbrk;
        alloc_end = alloc_start + size;
    }
    
    if (alloc_start == 0) { //nếu =0 thì heap chưa đc khởi tạo 
        alloc_start = PAGING_PAGESZ; // bắt đầu heap từ trang đầu tiên 
        alloc_end = alloc_start + size; //từ địa chỉ khởi tạo mới 
    }
    
    rgnode->rg_start = alloc_start;
    rgnode->rg_end = alloc_end;
    *alloc_addr = alloc_start;
    
    cur_vma->sbrk = alloc_end;


    return 0;
}

/*__free - remove a region memory */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
    pthread_mutex_lock(&mmvm_lock);
    //đảm bảo chỉ có 1 ng đc sửa tại 1 thời điểm 
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        //pagingmax30 có nằm trg phạm vi trả 
        pthread_mutex_unlock(&mmvm_lock);
        return -1;
    }
    
    struct vm_rg_struct *rgnode = &caller->krnl->mm->symrgtbl[rgid];
    //truy xuất vào hồ sơ vùng nhớ 
    //mở sổ symrgtbl tìm đến dòng rgid 
    //phía dưới là ngăn chặn vc trả vùng nhớ k tồn tại 
    if (rgnode->rg_start == 0 && rgnode->rg_end == 0) {
        printf("[WARNING] __free: Process %d trying to free unallocated region %d\n", 
               caller->pid, rgid);
        pthread_mutex_unlock(&mmvm_lock);
        return -1;  // Không free region chưa được allocate
    }

    if (rgnode->rg_start >= rgnode->rg_end) {
        printf("[WARNING] __free: Process %d region %d has invalid range [%lu-%lu]\n",
               caller->pid, rgid, rgnode->rg_start, rgnode->rg_end);
        pthread_mutex_unlock(&mmvm_lock);
        return -1;//start phải lun bé hơn end ngược lại là sai 
    }
    
    // Reset region
    rgnode->rg_start = 0;
    rgnode->rg_end = 0;

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
}

/*liballoc - PAGING-based allocate a region memory */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index)//syntax tiến trình , kích thước , số thứ tự của thanh ghi đó 
{
  addr_t addr;
  int val = __alloc(proc, 0, reg_index, size, &addr); //gọi hàm alloc trước đó để tính toán và mở rộng heap
  
  if (val == 0) {
    proc->regs[reg_index] = addr;
  }
  
#ifdef IODUMP
  printf("liballoc:178\n");
  print_pgtbl(proc, 0, 0);  
#endif

  return val;
}

/*libfree - PAGING-based free a region memory */
/*libfree - PAGING-based free a region memory */
int libfree(struct pcb_t *proc, uint32_t reg_index)// syntax ai đang trả và trả trong vùng nhớ số mấy
{
  if (proc->regs[reg_index] == 0) {
      printf("[WARNING] libfree: Process %d reg_index %d contains address 0, skipping free\n",
             proc->pid, reg_index);
      return -1;
  }//ko thể trả 1 phòng mà chua tung thue 
  
  int val = __free(proc, 0, reg_index);//địa chỉ hợp lệ thì 
  
#ifdef IODUMP
  printf("libfree:218\n");
  print_pgtbl(proc, 0, 0);
#endif
  
  return val;
}


/*
Hàm pg_getpage là một trong những hàm quan trọng nhất (trái tim) của tầng giả lập phần cứng (MMU - Memory Management Unit).
Chức năng chính của nó là: Chuyển đổi từ địa chỉ Ảo (Page Number) sang địa chỉ Vật lý (Frame Number).
Nó thực hiện cơ chế Demand Paging (Cấp phát theo nhu cầu): Nếu trang chưa có trong RAM, nó sẽ đi xin RAM ngay lập tức.

mm: Cấu trúc quản lý bộ nhớ của tiến trình.
pgn (Page Number): Số trang ảo mà CPU/Người dùng muốn truy cập.
fpn (Frame Page Number): Đầu ra quan trọng nhất. Đây là con trỏ, hàm sẽ ghi số khung vật lý tìm được vào đây.
caller: Tiến trình đang gọi.

*/
/*pg_getpage - get the page in ram */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
//struct mm_struct *mm: quản lí bộ nhớ
//pgn: page global number: trang định danh
//struct pcb_t *caller: process control block  - processs
//fpn: frame page number: số khung vật lí 
//pte: page table entry
//new_fpn trường hợp page fault 
//thiết lập lun newnode để tính khoảng tg vào và ra để bit kick khi có thk mới vào mà hết chỗ
{
  if (pgn < 0 || pgn >= PAGING_MAX_PGN) {
    return -1;
  }
  //pgn: page number = 1000/256 =3 -> trang số 3 
  //pg_getpage(mm, pgn=3, &fpn, P1)
  // Kiểm tra page đã được map chưa
  uint32_t pte = pte_get_entry(caller, pgn);
  /*
  Kịch bản 1: Trường hợp "Sách đã có trên kệ" (Page Hit)
  Đây là trường hợp trang số 3 ĐÃ được nạp vào RAM từ trước rồi.
  Tra cứu (pte_get_entry):
  Hàm kiểm tra Bảng phân trang (Page Table) tại dòng số 3.
  Kết quả trả về (pte): Thấy có đánh dấu PRESENT (Đang hiện diện).
  Giá trị trong sổ ghi: "Trang 3 đang nằm ở Khung (Frame) số 8".
  Lấy kết quả (PAGING_FPN):
  Hàm gán *fpn = 8.
  Kết thúc:
  Trả về 0 (Thành công).
  Code không chạy vào đoạn if bên dưới (đoạn xin cấp phát).
  -> Kết quả: Địa chỉ ảo 1000 (Trang 3) ứng với địa chỉ vật lý của Khung 8.
  */
  
  /*
  Kịch bản 2: Trường hợp "Sách chưa có, phải vào kho lấy" (Page Fault)
  Đây là trường hợp quan trọng nhất mà đoạn code này xử lý. Trang số 3 CHƯA hề có trong RAM.

  Tra cứu (pte_get_entry):
  Kiểm tra Page Table tại dòng số 3.
  Kết quả: Không thấy đánh dấu PRESENT.
  -> Kết luận: Page Fault! Phải đi xin RAM mới.
  Xin khung RAM trống (MEMPHY_get_freefp):
  Hàm chạy xuống dòng MEMPHY_get_freefp.
  Nó lục trong danh sách các khung RAM vật lý. Giả sử nó tìm thấy Khung số 50 đang rảnh.
  Biến new_fpn được gán giá trị 50.
  Cập nhật sổ sách (pte_set_fpn):
  Ghi vào Bảng phân trang: "Từ nay, Trang ảo số 3 tương ứng với Khung vật lý số 50".
  Ghi danh vào danh sách quản lý (fifo_pgn):
  Hệ thống tạo một cái node mới (malloc new_node).
  Ghi thông tin: pgn = 3.
  Chèn node này vào đầu danh sách fifo_pgn.
  Mục đích: Để sau này nếu RAM đầy, hệ thống biết trang số 3 này vào RAM lúc nào để có thể chọn làm "nạn nhân" đuổi ra.
  Trả kết quả:
  Gán *fpn = 50.
  Trả về 0 (Thành công).
  */

  /*
  Hàm pg_getpage giống như một người Thủ kho tháo vát:
  Khách hỏi đồ (Trang 3).
  Nếu có sẵn trên kệ -> Chỉ ngay chỗ lấy.
  Nếu chưa có -> Tự động chạy đi tìm chỗ trống, lấy đồ đặt vào, ghi chép sổ sách, rồi mới chỉ cho khách. Khách (người dùng) không cần biết quá trình vất vả đó, chỉ cần biết là có đồ dùng.
  */

  /*
  CPU --> Check_PageTable{Có trong Page Table?}
  Check_PageTable -- YES (Hit/Present) --> Return_FPN[Trả về FPN luôn]
  Check_PageTable -- NO (Miss/Page Fault) --> Xin_RAM[MEMPHY_get_freefp: Xin khung mới]
  Xin_RAM --> Map[Ghi vào sổ Page Table] --> Return_FPN
  */
  if (PAGING_PAGE_PRESENT(pte)) {
    *fpn = PAGING_FPN(pte);
    return 0;
  }
  /*
  Trường hợp 1 (Page Hit): "À, anh đã nhận phòng rồi. Phòng anh nằm ở Tầng 5 (Frame) nhé."
  Hàm lấy số khung (FPN) từ sổ, ghi vào biến trả về *fpn.
  Trả về 0 (Thành công).
  Trường hợp 2 (Page Miss/Fault): "Anh chưa có phòng. Để tôi đi kiếm phòng trống cho anh." (Code chạy tiếp xuống dưới).
  */
  // Page is not present, allocate and map it
  addr_t new_fpn;
  if (MEMPHY_get_freefp(caller->krnl->mram, &new_fpn) == 0) {
    // Map the page
    if (pte_set_fpn(caller, pgn, new_fpn) == 0) {
      // Thêm vào FIFO list cho page replacement
      if (caller->krnl->mm->fifo_pgn == NULL) {
        caller->krnl->mm->fifo_pgn = malloc(sizeof(struct pgn_t));
        caller->krnl->mm->fifo_pgn->pgn = pgn;
        caller->krnl->mm->fifo_pgn->pg_next = NULL;
      } else {
        struct pgn_t *new_node = malloc(sizeof(struct pgn_t));
        new_node->pgn = pgn;
        new_node->pg_next = caller->krnl->mm->fifo_pgn;
        caller->krnl->mm->fifo_pgn = new_node;
      }
      
      *fpn = new_fpn;
      return 0;
    } else {
      MEMPHY_put_freefp(caller->krnl->mram, new_fpn);
      return -1;
    }
  } else {
    return -1;
  }
}
/*
    Hàm này giải quyết vấn đề cốt lõi: Sự lười biếng thông minh (Lazy Allocation).
    Khi bạn dùng malloc (__alloc), hệ thống chỉ hứa mồm (cấp địa chỉ ảo).
    Chỉ khi bạn thực sự đụng vào (gọi read/write), hàm pg_getpage này mới chạy.
    Lúc đó nó mới cuống cuồng đi lấy RAM thật đắp vào chỗ đó.
    
    Nếu không có hàm này:
    Chương trình sẽ phải xin RAM thật ngay từ đầu -> Rất lãng phí (vì nhiều khi xin mà không dùng).
    Chương trình phải tự quản lý địa chỉ vật lý -> Rất phức tạp và không an toàn.
*

/*pg_getval - read value at given offset */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
    return -1;
  }

  // Calculate physical address directly
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  
  // Read directly from physical memory
  if (MEMPHY_read(caller->krnl->mram, phyaddr, data) != 0) {
    return -1;
  }
  
  return 0;
}

/*pg_setval - write value to given offset */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
    return -1;
  }

  // Calculate physical address directly
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  
  // Write directly to physical memory
  if (MEMPHY_write(caller->krnl->mram, phyaddr, value) != 0) {
    return -1;
  }
  
  return 0;
}

/*__read - read value in region memory */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data)
{
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1;
    }
      //đọc đúng vị trí sai thì cook trả về
    // SỬ DỤNG GLOBAL STORAGE
    struct vm_rg_struct *currg = &caller->krnl->mm->symrgtbl[rgid];
    
    // Kiểm tra region hợp lệ - NẾU CHƯA ALLOCATE, TỰ ĐỘNG ALLOCATE
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        // Tự động allocate region với size mặc định 100 bytes
        //giả sử ng dùng chưa gọi alloc thì code tự gọi và xin dùm 100 bytes
        addr_t alloc_addr;
        if (__alloc(caller, vmaid, rgid, 100, &alloc_addr) != 0) {
            return -1;
        }
    }

    if (currg->rg_start >= currg->rg_end) {
        return -1;
    }

    addr_t region_size = currg->rg_end - currg->rg_start;
    if (offset >= region_size) {
        return -1;
    }
    /*
    Ý nghĩa: Ngăn chặn hành vi "lấn sân".
    Ví dụ: Bạn được cấp 100 byte (region_size = 100). Bạn đòi đọc ở vị trí thứ 150 (offset = 150).
    Hành động: 150 >= 100 -> Trả về -1 (Lỗi). Không cho phép đọc trộm sang vùng nhớ của người khác.
    */
    addr_t target_addr = currg->rg_start + offset;
    /*
    Ý nghĩa: Chuyển từ "địa chỉ tương đối" sang "địa chỉ tuyệt đối (ảo)".
    Ví dụ:
    Vùng nhớ của bạn bắt đầu tại: 1000 (rg_start).
    Bạn muốn đọc vị trí thứ: 5 (offset).
    -> Địa chỉ cần đọc là: 1005 (target_addr).
    */
    // Thực hiện read
    return pg_getval(caller->krnl->mm, target_addr, data, caller);
    /*
    Ý nghĩa: Gọi xuống tầng giả lập phần cứng (MMU).
    Hành động: "Này MMU (pg_getval), hãy lấy giá trị tại địa chỉ ảo target_addr và bỏ vào biến data cho tôi".
    */
}

/*libread - PAGING-based read a region memory */
int libread(struct pcb_t *proc, uint32_t source, addr_t offset, uint32_t* destination)//yc lấy dữ liệu 
//proc: tiến trình nào 
//source: đọc ở ngăn nào, stt mà thanh ghi/biến mà bạn đã alloc trước đó 
//offset: đọc ở ngăn nhỏ thứ mấy trong tủ đó ( vị trí byte cần đọc)
//destination: cái rổ để hứng dữ liệu con trỏ tro tới biến dữ liệu người dùng và quăng dữ liệu đó vào đây 

{
  BYTE data;
  int val = __read(proc, 0, source, offset, &data);

  if (val == 0) {
    *destination = data;//copy dữ liệu từ data sang destination
  }

#ifdef IODUMP
  printf("libread:426\n");
  print_pgtbl(proc, 0, 0);
#endif

  return val;
}

/*__write - write a region memory */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value)
{
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1;
    }
    
    // SỬ DỤNG GLOBAL STORAGE
    struct vm_rg_struct *currg = &caller->krnl->mm->symrgtbl[rgid];
    
    // Kiểm tra region hợp lệ - NẾU CHƯA ALLOCATE, TỰ ĐỘNG ALLOCATE
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        // Tự động allocate region với size mặc định 100 bytes
        addr_t alloc_addr;
        if (__alloc(caller, vmaid, rgid, 100, &alloc_addr) != 0) {
            return -1;
        }
    }

    if (currg->rg_start >= currg->rg_end) {
        return -1;
    }

    addr_t region_size = currg->rg_end - currg->rg_start;
    if (offset >= region_size) {
        return -1;
    }

    addr_t target_addr = currg->rg_start + offset;
    
    // Thực hiện write
    return pg_setval(caller->krnl->mm, target_addr, value, caller);
    /*
    Ý nghĩa: Thay vì lấy dữ liệu ra, nó gọi pg_setval để đẩy dữ liệu vào.
    Hành động: "Này MMU, hãy ghi cái giá trị value này vào địa chỉ ảo target_addr cho tôi".
    */
}

/*libwrite - PAGING-based write a region memory */
int libwrite(struct pcb_t *proc, BYTE data, uint32_t destination, addr_t offset)
{
  int val = __write(proc, 0, destination, offset, data);

#ifdef IODUMP
  printf("libwrite:502\n");
  print_pgtbl(proc, 0, 0);
#endif

  return val;
}

/*find_victim_page - find victim page */
//quản lí ram 
int find_victim_page(struct mm_struct *mm, addr_t *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  if (pg == NULL) {
    return -1;
  }

  *retpgn = pg->pgn;
  mm->fifo_pgn = pg->pg_next;
  free(pg);
  //FIFO , ng đến sớm nhất sẽ bị đuổi cho người đến tiêos theo 
  //thằng đến sớm nhất hay là thằng clean nhất tức là dirty bit 
  return 0;
}

/*get_free_vmrg_area - get a free vm region */
//quản lí bộ nhớ ảo, tìm vùng nhớ để tái sử dụng 
//xài lại những gì mà người khác đã xài rồi 
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
  if (cur_vma == NULL) {
    return -1;
  }

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL) {
    return -1;
  }

  newrg->rg_start = newrg->rg_end = -1;

  while (rgit != NULL) {
    if (rgit->rg_start + size <= rgit->rg_end) {
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      if (rgit->rg_start + size < rgit->rg_end) {
        rgit->rg_start = rgit->rg_start + size;
      } else {
        struct vm_rg_struct *nextrg = rgit->rg_next;
        if (nextrg != NULL) {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;
          rgit->rg_next = nextrg->rg_next;
          free(nextrg);
        } else {
          rgit->rg_start = rgit->rg_end;
          rgit->rg_next = NULL;
        }
      }
      return 0;
    }
    rgit = rgit->rg_next;
  }

  return -1;
}

/*
Điểm cần làm rõ: get_symrg_byid vs get_free_vmrg_area
Ở ý này bạn hiểu logic "cắt đất" là đúng, nhưng tên hàm cần phân biệt rõ hơn một chút:
get_symrg_byid: Hàm này giống như "Cuốn sổ đỏ". Nó dùng để tra cứu xem cái reg_index (ví dụ: biến số 0, biến số 1) đang sở hữu vùng đất nào.
Ví dụ: Biến rgid=0 đang nằm ở địa chỉ ảo 1000 đến 1050.
get_free_vmrg_area: Hàm này giống như "Bảng tin rao vặt đất trống". Nó duyệt danh sách vm_freerg_list (danh sách các mảnh đất chưa ai dùng).
Cơ chế cắt đất: Nếu bạn cần 50MB, mà nó tìm thấy một mảnh trống 100MB, nó sẽ:
Lấy 50MB đầu đưa cho bạn.
Cập nhật lại mảnh trống đó: "Giờ chỉ còn dư 50MB phía sau thôi nhé" (dòng rgit->rg_start = rgit->rg_start + size ).
*/

/*
Đây là sơ đồ tóm tắt lại toàn bộ quy trình bạn vừa học trong libmem.c:
Quy trình 3 bước:Bước 1: Xin đất (Alloc) - "Hứa mồm"User gọi: liballoc3.
Hệ thống: Gọi __alloc44.Hành động: Kiểm tra vm_freerg_list hoặc nới rộng inc_vma_limit5555.
Kết quả: Trả về một địa chỉ ảo (ví dụ: 1000). 
Chưa có tí RAM nào được cấp ở đây cả.
Bước 2: Sử dụng (Read/Write) - "Đụng chuyện mới làm" (Lazy Allocation)User gọi: libwrite (ghi vào địa chỉ 1000)66.
Hệ thống: Gọi __write $\to$ pg_setval777.Hành động: pg_setval gọi pg_getpage để dịch địa chỉ 1000 sang RAM8.Sự cố: pg_getpage phát hiện trang này chưa có trong RAM (Page Fault)9.
Bước 3: Cấp phát vật lý (Mapping) - "Làm thật"Hành động:Xin một khung RAM trống (MEMPHY_get_freefp)10.Ghi vào sổ (Page Table): "Trang ảo chứa địa chỉ 1000 nằm ở khung RAM số 50" (pte_set_fpn)11.Đăng ký vào danh sách FIFO (fifo_pgn) để sau này RAM đầy thì biết đường mà đuổi (find_victim_page)12.
Kết quả: Dữ liệu được ghi vào RAM thật.

Một lưu ý nhỏ về FIFO (find_victim_page)Trong code hiện tại của libmem.c13, hàm find_victim_page có nhiệm vụ chỉ điểm xem ai là người vào sớm nhất để chuẩn bị đuổi đi.Tuy nhiên, việc thực sự đuổi (swap out ra ổ cứng) sẽ nằm ở tầng cao hơn hoặc trong hàm xử lý Page Fault đầy đủ (như hàm __mm_swap_page trong mm-vm.c 14 hoặc syscall). Trong file libmem.c, chúng ta thấy nó quản lý danh sách fifo_pgn 15 để phục vụ cho mục đích này.
*/

/*Câu hỏi nếu như mà kêu là vì sao ở đây em lại chọn cách fifo thì nên chỉnh lại cách tối ưu
nhất là kết hợp cả 2

Nếu thầy hỏi "Muốn code tốt hơn thì nên làm thế nào?",
câu trả lời ăn điểm nhất không phải là chọn 1 trong 2, mà là kết hợp cả hai.
Tuy nhiên, nếu bắt buộc phải chọn hướng tối ưu hóa, bạn hãy trả lời:
 "Nên ưu tiên xóa theo trạng thái Dirty/Clean (kết hợp với FIFO) sẽ tốt hơn là FIFO thuần túy."Dưới đây là cách bạn nên trả lời thầy để thể hiện sự hiểu biết sâu sắc về Hệ điều hành:

 1. Câu trả lời ngắn gọn (The Hook)"Thưa thầy, thuật toán FIFO hiện tại dễ cài đặt nhưng chưa tối ưu về hiệu năng I/O. 
Để code tốt hơn, em sẽ cải tiến hàm find_victim_page để ưu tiên chọn trang Clean (Sạch) làm nạn nhân trước,
thay vì đuổi một trang Dirty (Bẩn) ngay lập tức."

2. Giải thích lý do (The "Why") 
- Phần này quan trọng để lấy điểm caoBạn hãy giải thích dựa trên chi phí đắt đỏ của việc ghi đĩa (I/O Cost):
Nếu đuổi trang Clean: Vì dữ liệu trong RAM y hệt trong ổ cứng (Swap),
    ta không cần ghi lại. Chỉ cần xóa tham chiếu và lấy khung RAM đó dùng luôn. 
Tốn 0 lần ghi đĩa (Rất nhanh).Nếu đuổi trang Dirty: Vì dữ liệu trong RAM đã bị thay đổi, 
    ta bắt buộc phải ghi (swap-out) nội dung đó xuống ổ cứng trước khi xóa. 
Tốn 1 lần ghi đĩa (Rất chậm).
Kết luận: Nếu dùng FIFO thuần túy (như code hiện tại), 
ta có thể vô tình đuổi một trang Dirty (tốn công ghi) trong khi có rất nhiều trang Clean (miễn phí) đang nằm lù lù ngay đó. 
Điều này làm chậm hệ thống.

3. Đề xuất giải pháp cụ thể (The Solution)Bạn có thể đề xuất thuật toán "Second Chance" (Cơ hội thứ hai) hoặc "Enhanced Second Chance" mà sách giáo khoa Hệ điều hành hay dạy.
Quy trình tìm nạn nhân mới sẽ như sau:Duyệt danh sách fifo_pgn.
Nếu gặp trang là Clean: Chọn làm nạn nhân ngay lập tức (Kick!).
Nếu gặp trang là Dirty:Đừng kick ngay.Hãy cho nó một cơ hội (bỏ qua nó, đẩy nó xuống cuối hàng đợi).
Hy vọng lần sau quay lại, nó đã được hệ thống lưu xuống đĩa (thành Clean) 
    hoặc ta tìm được trang khác Clean hơn.
Nếu duyệt hết vòng mà ai cũng Dirty thì đành phải chọn trang Dirty đầu tiên (quay về FIFO).

4. "Vũ khí bí mật" từ Source Code của bạnHãy chỉ cho thầy thấy là bạn đã nghiên cứu kỹ code:
"Thưa thầy, trong file mm.h, em thấy đã có định nghĩa sẵn PAGING_PTE_DIRTY_MASK.Code hiện tại của libmem.c mới chỉ quản lý danh sách fifo_pgn đơn giản
.Việc cải tiến là hoàn toàn khả thi vì hạ tầng để nhận biết Dirty/Clean đã có sẵn, 
chỉ cần sửa logic trong find_victim_page để kiểm tra bit này trước khi quyết định free là xong.
"Tóm lại câu trả lời mẫu:"Hiện tại code đang chạy FIFO thuần túy, ưu điểm là công bằng và dễ code. Nhưng nhược điểm là hiệu suất I/O kém (hiện tượng Belady). Để tối ưu, em sẽ sửa find_victim_page để ưu tiên đuổi các trang Clean trước. Việc này giảm bớt số lần phải ghi xuống Swap (MEMPHY_write), giúp hệ thống chạy nhanh hơn đáng kể."Trả lời như vậy chắc chắn bạn sẽ được đánh giá là hiểu bài sâu sắc!


*/

/*
1. Clean Page và Dirty Page là gì?Hãy tưởng tượng RAM là cái Bàn làm việc, còn Ổ cứng (Swap) là cái Tủ sách.
Clean Page (Trang Sạch):Là trang giấy bạn lấy từ Tủ sách ra để lên Bàn đọc, và bạn CHỈ ĐỌC nó, không viết vẽ bậy gì lên đó cả.
Đặc điểm: Nội dung trên Bàn (RAM) y hệt nội dung trong Tủ (Swap).
Khi bị đuổi (Evict): Bạn chỉ cần vứt tờ giấy trên bàn vào sọt rác. 
Không cần cất lại vào tủ, vì trong tủ đã có bản gốc y hệt rồi. 
Xử lý rất nhanh.

Dirty Page (Trang Bẩn):Là trang giấy bạn lấy ra, và bạn đã DÙNG BÚT VIẾT (lệnh write)
thay đổi nội dung của nó.Đặc điểm: Nội dung trên Bàn (RAM) KHÁC nội dung gốc trong Tủ (Swap).
Tờ trên bàn là bản mới nhất.Khi bị đuổi (Evict): Bạn KHÔNG ĐƯỢC vứt đi ngay.
Bạn phải LƯU (Copy) tờ giấy trên bàn ngược vào Tủ sách để cập nhật dữ liệu mới, sau đó mới được dọn bàn.
Xử lý chậm hơn (tốn công ghi đĩa).

*/