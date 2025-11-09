#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

// struct pcb_t
// {
// 	uint32_t pid;		 // PID
// 	uint32_t priority;	 // Default priority, this legacy process based (FIXED)
// 	char path[100];
// 	struct code_seg_t *code; // Code segment
// 	addr_t regs[10];	 // Registers, store address of allocated regions
// 	uint32_t pc;		 // Program pointer, point to the next instruction
// #ifdef MLQ_SCHED
// 	// Priority on execution (if supported), on-fly aka. changeable
// 	// and this vale overwrites the default priority when it existed
// 	uint32_t prio;
// #endif
// 	struct krnl_t *krnl;	
// 	struct page_table_t *page_table; // Page table
// 	uint32_t bp;			 // Break pointer
// };

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL || proc == NULL || q->size >= MAX_QUEUE_SIZE){
                return;
        }

        q->proc[q->size] = proc; // struct queue_t se có proc là struct pcb_t * proc[MAX_QUEUE_SIZE];
        q->size++;


}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */
        if (q == NULL || q->size == 0){
                return NULL;
        }
        // struct pcb_t *highest = q->proc[0];
        // int highest_prio = highest->priority;
        // int idx = 0;

        // // find the highest priority = cach duyet qua mang proc cua q, priority nao nho nhat la do uu tien cao nhat
        // for (int i = idx; i < q->size; i++){
        //         if (q->proc[i]->priority < highest_prio){
        //                 highest = q->proc[i];
        //                 highest_prio = q->proc[i]->priority;
        //                 idx = i;
        //         }
        // }
        // //remove it from q
        // for (int i = idx; i < q->size - 1; i++){
        //         q->proc[i] = q->proc[i+1];
        // }
        // q->size--;
        // return highest;
#ifdef MLQ_SCHED
        /* MLQ: Tìm process có priority cao nhất (chỉ số nhỏ nhất) */
        int highest_prio = 9999;
        int idx = -1;
        int i;

        for (i = 0; i < q->size; i++) {
                int prio = q->proc[i]->prio;
                if (prio < highest_prio) {
                        highest_prio = prio;
                        idx = i;
                }
        }

        if (idx == -1)
                return NULL;

        struct pcb_t *selected = q->proc[idx];

        /* Xóa phần tử tại idx */
        for (i = idx; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }
        q->size--;

        return selected;
#else
        /* FIFO: Lấy phần tử đầu tiên */
        struct pcb_t *first = q->proc[0];

        /* Dịch chuyển các phần tử */
        int i;
        for (i = 0; i < q->size - 1; i++) {
                q->proc[i] = q->proc[i + 1];
        }
        q->size--;

        return first;
#endif
        /* === KẾT THÚC === */

}

struct pcb_t *purgequeue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: remove a specific item from queue
         * */
        if (q == NULL || proc == NULL || q->size ==0){
                return NULL;
        }

        int found = 0;
        int idx = -1;

        for (int  i = 0; i < q->size; i++){
                if (q->proc[i] == proc){
                        idx = i;
                        found = 1;
                        break;
                }
        }
        if (!found){
                return NULL;
        }

        for (int i = idx; i < q->size-1; i++){
                q->proc[i] = q->proc[i+1];
        }
        q->size--;
        return proc;
        
}