/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i ++) {
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i; 
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	running_list.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
//get_mlq_proc() : duyệt từ prio = 0 -> tìm hàng đợi có slot[prio]>0 và có tiến trình -> dequeue -> giảm slot
//put_mlq_proc() : nếu slot[prio] <= 0 -> chuyển xuống hàng đợi thấp hơn -> reset slot mới bằng MAX_PRIO - prio (hiện tại)
//add_mlq_proc() : dùng proc->prority ->đặt proc->prio->enqueue vào hàng đợi đúng
//prio là cái mà qua xử lý sẽ được gán lại và sẽ ghi đè lên priority, priority là độ ưu tiên mà lệnh có từ ban đầu (chưa qua xử lý)


struct pcb_t * get_mlq_proc(void) { // ham nay dung de lay tu hang doi (queue.h) uu tien cao nhat co tien trinh
	struct pcb_t * proc = NULL;

	pthread_mutex_lock(&queue_lock);
	/*TODO: get a process from PRIORITY [ready_queue].
	 *      It worth to protect by a mechanism.
	 * */

	for (int prio = 0; prio < MAX_PRIO; prio++){
		if (slot[prio] > 0 && !empty(&mlq_ready_queue[prio])){
			proc = dequeue(&mlq_ready_queue[prio]);
			if (proc != NULL){
				slot[prio]--; //dung mot slot
				break;
			}
		}
	}




	//cua thay
	if (proc != NULL)
		enqueue(&running_list, proc);

	pthread_mutex_unlock(&queue_lock);
	return proc;	
}

void put_mlq_proc(struct pcb_t * proc) { // dua lai vao hang doi dung prio, giam slot[prio]
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list 
	 *       It worth to protect by a mechanism.
	 * 
	 */
	pthread_mutex_lock(&queue_lock);


	int prio = proc->prio;
	if (prio < 0) prio = 0;
	if (prio >= MAX_PRIO) prio = MAX_PRIO - 1;
	// neu het slot, chuyen xuong hang doi thap hon (co do uu tien tap hon)
	if (slot[prio] <= 0){
		prio++;
		if (prio >= MAX_PRIO){
			prio = MAX_PRIO - 1;
		}
		slot[prio] = MAX_PRIO - prio; // o tren co de cap (dong 57)
	}
	
	proc->prio = prio;


	//cua thay
	
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {//them moi vao hang doi dung prio
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->mlq_ready_queue = mlq_ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list
	 *       It worth to protect by a mechanism.
	 * 
	 */
       
	pthread_mutex_lock(&queue_lock);

	int prio = proc->priority; // proc->prio la cai ma sau khi qua xu ly dc gan lai, con priority se la cai ma minh tu gan ban dau
	if (prio < 0) prio = 0;
	if (prio >= MAX_PRIO) prio = MAX_PRIO - 1;

	proc->prio = prio;


	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else // này là phần của 32bit, muốn thì làm thêm
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;

	pthread_mutex_lock(&queue_lock);
	/*TODO: get a process from [ready_queue].
	 *       It worth to protect by a mechanism.
	 * 
	 */
	if (!empty(&ready_queue)) {
		proc = dequeue(&ready_queue);
		if (proc) enqueue(&running_list, proc);
	}

	pthread_mutex_unlock(&queue_lock);

	return proc;
}

void put_proc(struct pcb_t * proc) {
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list 
	 *       It worth to protect by a mechanism.
	 * 
	 */

	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	proc->krnl->ready_queue = &ready_queue;
	proc->krnl->running_list = &running_list;

	/* TODO: put running proc to running_list 
	 *       It worth to protect by a mechanism.
	 * 
	 */

	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


