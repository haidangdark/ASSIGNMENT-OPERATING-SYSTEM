 /*================================================================================================================================================================================*/
/*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
 /*================================================================================================================================================================================*/

/*
Chức năng tổng quan của sys_xxxhandler.c
Đây là Trình xử lý (Handler) cho lời gọi hệ thống (system call) tùy chỉnh . Dựa trên syscall.tbl, nó được đăng ký với số hiệu 440.
Khi một tiến trình (Process) ở chế độ người dùng (userspace) thực thi lệnh syscall 440, kernel sẽ:
    1. Ngắt (trap) vào chế độ kernel.

    2. Tra cứu bảng syscall_table.

    3. Tìm thấy số 440 và gọi hàm __sys_xxxhandler (trong file này) để thực thi.

Chức năng cụ thể đã triển khai trong hàm này là:

    In tham số: In ra giá trị của tham số đầu tiên mà tiến trình truyền vào.

    Thống kê (Stats): Đếm và báo cáo số lần syscall này được gọi, bao gồm tổng số lần và số lần gọi bởi từng PID (Process ID) riêng biệt.

Các thư viện và Header liên quan
1. "common.h": Cung cấp các định nghĩa cơ bản nhất như struct krnl_t, kiểu arg_t và quan trọng là macro FORMAT_ARG. Macro này đảm bảo printf có thể in đúng kiểu dữ liệu 32-bit (%u) hay 64-bit (%lu) tùy theo cấu hình biên dịch.

2. "syscall.h": Cung cấp định nghĩa của struct sc_regs. Đây là cấu trúc dùng để truyền các tham số (giả lập thanh ghi) từ userspace vào kernel.

3. <stdio.h>: Thư viện chuẩn C, cần thiết cho hàm printf để in thông báo ra console.

4. <stdint.h>: Cung cấp các kiểu dữ liệu có kích thước cố định, ví dụ uint64_t được dùng cho total_calls để đảm bảo bộ đếm không bị tràn số sớm.

*/
#include "common.h"
#include "syscall.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>


/* sys_xxxhandler: syscall demo + thống kê lượt gọi theo PID và tổng lượt gọi.
 *
 * Prototype phải khớp cơ chế dispatch trong syscall.c:
 *   extern int __sym(struct krnl_t*, uint32_t pid, struct sc_regs*);
 * (syscall() dùng switch + __SYSCALL macro để gọi theo đúng prototype này)
 */


 /*
 Tham số:

    struct krnl_t *krnl: Con trỏ tới cấu trúc kernel chính. Hàm này không dùng đến nó, nên có lệnh (void)krnl; để tránh cảnh báo (warning) của trình biên dịch.

    uint32_t pid: ID của tiến trình đã gọi syscall này.

    struct sc_regs *regs: Con trỏ tới cấu trúc chứa các tham số. regs->a1 là tham số thứ nhất, regs->a2 là thứ hai, v.v..

Logic Thống kê (Phần quan trọng nhất):

    Để theo dõi số lần gọi, hàm sử dụng các biến static. Biến static có nghĩa là giá trị của chúng được lưu giữ vĩnh viễn qua các lần gọi hàm (chúng không bị mất đi khi hàm kết thúc).

        static unsigned long long total_calls = 0ULL;
        enum { MAX_SLOTS = 64 };
        static struct { ... } slots[MAX_SLOTS];


    1. static unsigned long long total_calls = 0ULL;: Khai báo biến đếm tổng số lần gọi. Biến này được khởi tạo bằng 0 ở lần chạy đầu tiên và tăng dần sau mỗi lần gọi syscall 440.

    2. static struct { ... } slots[MAX_SLOTS];: Khai báo một mảng "cache" gồm 64 slot để đếm số lần gọi theo từng pid.

    3. Vòng lặp for (Tìm kiếm slot):

        Hàm duyệt qua 64 slot.

        Nó tìm xem pid hiện tại đã được lưu trong slot nào chưa (if (slots[i].used && slots[i].pid == pid)).

        Đồng thời, nó cũng tìm một slot trống (if (!slots[i].used && free_idx < 0)) để dành, phòng trường hợp đây là pid mới.

    4. Xử lý Slot:

        Nếu pid mới (found_idx < 0):

            Nếu còn slot trống (free_idx >= 0), nó sẽ dùng slot trống đó.

            Nếu hết slot trống (free_idx < 0), nó sẽ tái sử dụng (ghi đè) slot 0. Đây là một chiến lược thay thế cache đơn giản.

            Sau đó, nó khởi tạo slot (.pid = pid, .cnt = 0, .used = 1).

        Cuối cùng: slots[found_idx].cnt++;: Tăng bộ đếm cho pid này.

Logic Xử lý chính:

    1. printf("The first system call parameter " FORMAT_ARG "\n", (arg_t)regs->a1);

        Đây là hành động chính của syscall: nó lấy tham số đầu tiên (regs->a1) từ tiến trình và in ra màn hình.

    2. printf("[sys_xxxhandler] pid=%u | pid_calls=%llu | total_calls=%llu\n", ...);

        In ra thông tin thống kê: PID nào vừa gọi, PID đó đã gọi (syscall 440) bao nhiêu lần, và tổng số lần (syscall 440) đã được gọi bởi tất cả các PID.

    3. return 0;

        Trả về 0 để báo cho kernel biết rằng syscall đã thực thi thành công.
    
 */
int __sys_xxxhandler(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
    (void)krnl; /* hiện tại không cần dùng đến kernel pointer */

    /* Thống kê đơn giản, lưu nội bộ ở handler :
       - total_calls: tổng số lần syscall này được gọi
       - per-pid: đếm số lần theo từng PID (bảng nhỏ, linear search) */
    static unsigned long long total_calls = 0ULL;

    enum { MAX_SLOTS = 64 };
    static struct {
        uint32_t pid;
        unsigned long long cnt;
        int used;
    } slots[MAX_SLOTS];

    /* Tăng tổng lượt gọi */
    total_calls++;

    /* Tìm/ghi nhận slot theo PID */
    int i, free_idx = -1, found_idx = -1;
    for (i = 0; i < MAX_SLOTS; ++i) {
        if (slots[i].used && slots[i].pid == pid) {
            found_idx = i;
            break;
        }
        if (!slots[i].used && free_idx < 0)
            free_idx = i;
    }
    if (found_idx < 0) {
        if (free_idx < 0) {
            /* Hết slot -> tái sử dụng slot 0 (đơn giản) */
            found_idx = 0;
            slots[0].pid = pid;
            slots[0].cnt = 0;
            slots[0].used = 1;
        } else {
            found_idx = free_idx;
            slots[found_idx].pid = pid;
            slots[found_idx].cnt = 0;
            slots[found_idx].used = 1;
        }
    }
    slots[found_idx].cnt++;

    /* In tham số thứ nhất (regs->a1) theo định dạng kiến trúc (32/64-bit) */
    printf("The first system call parameter " FORMAT_ARG "\n", (arg_t)regs->a1);

    /* In thống kê */
    printf("[sys_xxxhandler] pid=%u | pid_calls=%llu | total_calls=%llu\n",
           pid,
           (unsigned long long)slots[found_idx].cnt,
           (unsigned long long)total_calls);

    return 0;
}

