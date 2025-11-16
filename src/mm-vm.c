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

 /*================================================================================================================================================================================*/
/*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
 /*================================================================================================================================================================================*/
 /*
 File mm-vm.c là module chịu trách nhiệm quản lý Bộ nhớ Ảo (Virtual Memory - VM) ở mức cấu trúc vùng (VMA).
 Nếu libmem.c lo việc quản lý các "căn nhà" nhỏ (vm_rg_struct) bên trong một "khu phố", thì mm-vm.c chính là kiến trúc sư lo việc quản lý, xây dựng và mở rộng chính các "khu phố" (vm_area_struct) đó. (trong pdf của mình có minh họa)


 Chức năng tổng quan của mm-vm.c
1. Quản lý Danh sách VMA: Tìm kiếm và truy xuất các cấu trúc vm_area_struct (VMA) dựa trên ID.

2. Kiểm tra Chồng lấn (Overlap Validation): Đảm bảo rằng khi một vùng nhớ mới được cấp phát hoặc mở rộng, nó không xâm phạm vào địa chỉ của các vùng nhớ khác đã tồn tại.

3. Mở rộng Giới hạn VMA (Expand VMA Limit): Đây là chức năng quan trọng nhất (inc_vma_limit), cho phép nới rộng không gian địa chỉ (tăng sbrk) khi tiến trình cần thêm bộ nhớ mà vùng heap hiện tại đã đầy.
Nó sẽ ánh xạ thêm các trang vật lý mới vào vùng mở rộng này.

4. Hỗ trợ Swap (Swap Helper): Cung cấp hàm bọc (wrapper) để gọi thao tác swap trang ở cấp thấp hơn.


Các thư viện liên quan
1. "mm.h": Thư viện cốt lõi, chứa định nghĩa của struct mm_struct, struct vm_area_struct, struct vm_rg_struct và các hàm prototype.

2. "string.h": Dùng cho các hàm xử lý chuỗi/bộ nhớ (như memset nếu cần).

3. <stdlib.h>: Dùng cho hàm malloc (cấp phát động các cấu trúc dữ liệu) và free.

4. <stdio.h>: Dùng cho việc in ấn, debug (nếu có).

5. <pthread.h>: Mặc dù được include, nhưng trong các hàm hiện tại của file này chưa sử dụng mutex trực tiếp (việc đồng bộ hóa chủ yếu được xử lý ở lớp libmem.c gọi xuống).
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
/*
get_vma_by_num
Chức năng: Tìm kiếm một vm_area_struct cụ thể trong danh sách liên kết các VMA của tiến trình dựa trên vmaid (ID của vùng).

Chi tiết Code:

Kiểm tra an toàn (Guard check): Nếu con trỏ mm hoặc mm->mmap (danh sách VMA) là NULL, trả về NULL ngay lập tức để tránh lỗi truy cập bộ nhớ (Segmentation Fault).

Duyệt danh sách:

Bắt đầu từ pvma = mm->mmap.

Dùng vòng lặp while để so sánh pvma->vm_id với vmaid cần tìm.

Nếu vm_id hiện tại nhỏ hơn vmaid cần tìm, chuyển sang node tiếp theo (pvma = pvma->vm_next).

Kết quả: Trả về con trỏ pvma tìm được (hoặc NULL nếu duyệt hết danh sách mà không thấy).
*/
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  /* guard NULL tuyệt đối trước khi deref === */
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

/*
__mm_swap_page
Chức năng: Một hàm "cầu nối" (wrapper) để thực hiện việc tráo đổi nội dung trang (swap).

Tham số:

vicfpn: Frame Page Number của trang "nạn nhân" (trong RAM, cần đẩy ra SWAP).

swpfpn: Frame Page Number trên thiết bị SWAP (nơi sẽ chứa dữ liệu).

Chi tiết Code:

Gọi hàm cấp thấp __swap_cp_page (nằm trong mm64.c) để thực hiện copy dữ liệu vật lý từ mram sang active_mswp.
*/
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

 /*
 get_vm_area_node_at_brk
Chức năng: Tạo ra một cấu trúc vm_rg_struct (region) mới nằm ngay tại đỉnh sbrk (program break) của VMA hiện tại. Hàm này thường được dùng để chuẩn bị cho việc cấp phát mới ở cuối heap.

Chi tiết Code:

Lấy VMA hiện tại bằng get_vma_by_num. Nếu không tồn tại, trả về NULL.

Cấp phát bộ nhớ cho newrg bằng malloc.

Tính kích thước hiệu dụng: eff_sz là kích thước đã căn chỉnh (alignedsz) hoặc kích thước gốc (size).

Thiết lập địa chỉ:

newrg->rg_start = cur_vma->sbrk;: Bắt đầu ngay tại đỉnh heap hiện tại.

newrg->rg_end = newrg->rg_start + eff_sz;: Kết thúc sau khi cộng thêm kích thước.

Trả về newrg.
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
/*
validate_overlap_vm_area
Chức năng: Kiểm tra xem một vùng nhớ dự kiến (vmastart đến vmaend) có bị chồng lấn lên bất kỳ VMA nào khác của tiến trình hay không. Đây là chốt chặn an toàn quan trọng.

Chi tiết Code:

1. Kiểm tra cơ bản: Nếu vmastart >= vmaend, vùng nhớ không hợp lệ -> Trả về -1.

2. Lấy VMA hiện tại: Dùng get_vma_by_num để biết ta đang thao tác trên vùng nào (ví dụ Heap).

3. Kiểm tra nội bộ VMA: Code kiểm tra xem vùng đề xuất có nằm gọn trong giới hạn [vm_start, vm_end] của VMA hiện tại không. Tuy nhiên, logic mở rộng (expand) cho phép vượt qua vm_end cũ, nên bước này thường chỉ cảnh báo hoặc bỏ qua (như comment trong code mô tả).

4. Duyệt danh sách VMA khác: Vòng lặp while (vma != NULL) duyệt qua tất cả các VMA khác của tiến trình.

Nếu vma khác với cur_area (VMA đang thao tác):

Thực hiện kiểm tra chồng lấn (Overlap Check):


addr_t max_st = (vmastart > vma->vm_start) ? vmastart : vma->vm_start;
addr_t min_en = (vmaend   < vma->vm_end  ) ? vmaend   : vma->vm_end;
if (max_st < min_en) return -1; // Có chồng lấn
Logic này tương đương với toán học giao của hai đoạn thẳng: Nếu max(start1, start2) < min(end1, end2), thì hai đoạn đó giao nhau.
*/
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
    /* guard caller/krnl/mm/mmap === */
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

 /*
 inc_vma_limit
Chức năng: Tăng giới hạn (vm_end và sbrk) của một VMA để có thêm không gian nhớ. Hàm này được gọi khi syscall SYSMEM_INC_OP được kích hoạt (từ liballoc khi hết chỗ).

Chi tiết Code:

1. Kiểm tra đầu vào: Đảm bảo caller, mm, inc_sz hợp lệ.

2. Lấy VMA cần tăng: Gọi get_vma_by_num.

3. Căn chỉnh kích thước (Alignment):

inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);: Kích thước tăng thêm phải là bội số của trang (4096 bytes trong chế độ 64-bit). Ví dụ, cần tăng 100 bytes thì thực tế hệ thống sẽ tăng 4096 bytes.

4. Tính toán giới hạn mới:

old_end = cur_vma->vm_end;

new_end = old_end + inc_amt;

5. Kiểm tra an toàn: Gọi validate_overlap_vm_area với giới hạn mới (old_end -> new_end) để đảm bảo việc mở rộng không đè lên VMA kế tiếp (ví dụ Heap không đè lên Stack).

6. Ánh xạ vào RAM (Quan trọng):

Gọi vm_map_ram(...): Hàm này (trong mm64.c) sẽ thực hiện việc gán các khung trang vật lý thực sự cho dải địa chỉ ảo mới vừa được mở rộng.

Nếu không map được (hết RAM, lỗi...), trả về -1.

7. Cập nhật VMA:

cur_vma->vm_end = new_end;: Cập nhật giới hạn vùng.

cur_vma->sbrk = new_end;: Đẩy đỉnh heap lên mức mới.

8. Trả về 0 (Thành công).
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

/*
Tóm tắt quy trình liên quan
Khi libmem.c gọi liballoc mà hết chỗ:

libmem.c gọi syscall SYSMEM_INC_OP.

Kernel chuyển tới sys_memmap (trong sys_mem.c ).

sys_memmap gọi inc_vma_limit (trong mm-vm.c).

inc_vma_limit:

Kiểm tra xem mở rộng có an toàn không (validate_overlap_vm_area).

Xin trang vật lý mới (vm_map_ram).

Cập nhật sbrk lên cao hơn.

Quay lại libmem.c, vùng nhớ mới đã sẵn sàng để cấp phát.
*/
