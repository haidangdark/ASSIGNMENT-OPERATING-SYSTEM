/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* === ĐÃ THÊM === */
/* sys_xxxhandler: syscall demo + thống kê lượt gọi theo PID và tổng lượt gọi.
 *
 * Prototype phải khớp cơ chế dispatch trong syscall.c:
 *   extern int __sym(struct krnl_t*, uint32_t pid, struct sc_regs*);
 * (syscall() dùng switch + __SYSCALL macro để gọi theo đúng prototype này)
 */

int __sys_xxxhandler(struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
    (void)krnl; /* hiện tại không cần dùng đến kernel pointer */

    /* Thống kê đơn giản, lưu nội bộ ở handler (demo):
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
/* === ĐÃ THÊM === */
