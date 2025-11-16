/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */



  /*================================================================================================================================================================================*/
/*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
 /*================================================================================================================================================================================*/
 /*
 File sched.c là "bộ não" điều phối của hệ điều hành, chịu trách nhiệm hiện thực Bộ lập lịch (Scheduler). Nó quyết định tiến trình nào được chạy, khi nào chạy, và xử lý tiến trình sau khi chạy xong.


 Chức năng tổng quan của sched.c
	Chức năng chính của file này là triển khai thuật toán lập lịch Đa cấp Hàng đợi (Multi-Level Queue - MLQ) như được mô tả trong file PDF.

	Nó quản lý một mảng các hàng đợi, mỗi hàng đợi ứng với một mức ưu tiên. Nó phải thực hiện 3 tác vụ chính:

		1. add_proc: Thêm một tiến trình mới (từ loader) vào hàng đợi ưu tiên chính xác của nó.

		2. get_proc: Chọn một tiến trình từ hàng đợi có ưu tiên cao nhất (và còn "lượt" - slot) để CPU thực thi.

		3. put_proc: Nhận lại một tiến trình vừa chạy hết thời gian (time_slice), kiểm tra "lượt" của nó, và có thể hạ mức ưu tiên (demotion) của nó trước khi đưa trở lại hàng đợi.


Các thư viện và Header liên quan
1. "queue.h": Thư viện quan trọng nhất. Nó cung cấp cấu trúc struct queue_t và các hàm enqueue, dequeue, empty để thao tác trên các hàng đợi.

2. "sched.h": File header của chính nó, định nghĩa hằng số MAX_PRIO và các prototype hàm.

3. <pthread.h>: Cực kỳ quan trọng. Cung cấp pthread_mutex_t (queue_lock) và các hàm pthread_mutex_init, pthread_mutex_lock, pthread_mutex_unlock. Điều này là bắt buộc để đảm bảo an toàn (thread-safe), ngăn chặn việc nhiều CPU cùng lúc thay đổi các hàng đợi gây hỏng dữ liệu.

4. "common.h" (được gọi qua queue.h/sched.h): Cung cấp định nghĩa cốt lõi của struct pcb_t.

5. <stdlib.h> và <stdio.h>: Thư viện chuẩn C, thường dùng cho NULL hoặc printf (debug).



Cấu trúc dữ liệu chính (Biến static)
Đây là các cấu trúc dữ liệu trung tâm được quản lý bởi sched.c:

	static pthread_mutex_t queue_lock;:

		Một "ổ khóa" (mutex) toàn cục. Bất kỳ hàm nào muốn đọc/ghi vào các hàng đợi đều phải khóa mutex này trước và mở sau khi xong.

	static struct queue_t running_list;:

		Một hàng đợi dùng để lưu các tiến trình đang chạy (hoặc vừa chạy xong).

	static struct queue_t mlq_ready_queue[MAX_PRIO];:

		Cốt lõi của MLQ. Đây là một mảng gồm MAX_PRIO (140) hàng đợi.

		mlq_ready_queue[0] là hàng đợi ưu tiên cao nhất.

		mlq_ready_queue[139] là hàng đợi ưu tiên thấp nhất.

	static int slot[MAX_PRIO];:

		Một mảng số nguyên lưu số "lượt" (slot) còn lại cho mỗi mức ưu tiên.

		Điều này hiện thực chính sách trong PDF: một mức ưu tiên chỉ được chạy một số lần nhất định trước khi phải nhường cho mức thấp hơn.
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


/*
init_scheduler
Chức năng: Khởi tạo tất cả các cấu trúc dữ liệu của bộ lập lịch khi OS khởi động.

Chi tiết Code:

	1. Dùng vòng lặp for (i = 0; i < MAX_PRIO; i++).

	2. Đặt kích thước của tất cả MAX_PRIO hàng đợi về 0: mlq_ready_queue[i].size = 0;.

	3. Khởi tạo Slot: slot[i] = MAX_PRIO - i;. Đây là logic quan trọng:

		Hàng đợi 0 (cao nhất) có 140 - 0 = 140 slot.

		Hàng đợi 1 có 140 - 1 = 139 slot.

		...

		Hàng đợi 139 (thấp nhất) có 140 - 139 = 1 slot.

	4. Khởi tạo running_list và queue_lock.
*/
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

/*
get_mlq_proc (Chọn tiến trình để chạy)
Chức năng: Đây là hàm "trái tim" của scheduler. Nó quyết định tiến trình nào sẽ chạy tiếp theo.

Chi tiết Code:

	1. pthread_mutex_lock(&queue_lock);: Khóa.

	2. for (int prio = 0; prio < MAX_PRIO; prio++): Duyệt từ ưu tiên cao nhất (0) xuống thấp nhất.

	3. if (slot[prio] > 0 && !empty(&mlq_ready_queue[prio])): Kiểm tra 2 điều kiện:

		Mức ưu tiên prio này có còn "lượt" chạy không (slot[prio] > 0)?

		Hàng đợi prio này có tiến trình nào đang chờ không (!empty)?

	4. Nếu cả 2 là đúng:

		proc = dequeue(&mlq_ready_queue[prio]);: Lấy tiến trình ra khỏi hàng đợi.

		slot[prio]--;: Trừ đi một lượt của mức ưu tiên này.

		break;: Thoát khỏi vòng lặp for ngay lập tức (vì đã tìm được tiến trình).

	5. if (proc != NULL) enqueue(&running_list, proc);: Nếu tìm được proc, thêm nó vào running_list (để theo dõi).

	6. pthread_mutex_unlock(&queue_lock);: Mở khóa.

	7. return proc;: Trả proc về cho CPU (hoặc trả NULL nếu không có tiến trình nào sẵn sàng).
*/
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

/*
put_mlq_proc (Đưa tiến trình về hàng đợi)
Chức năng: Xử lý một tiến trình vừa chạy xong 1 time_slice. Đây là nơi logic hạ ưu tiên (demotion) xảy ra.

Chi tiết Code:

	1. pthread_mutex_lock(&queue_lock);: Khóa.

	2. int prio = proc->prio;: Lấy mức ưu tiên hiện tại của tiến trình.

	3. if (slot[prio] <= 0): Kiểm tra xem mức ưu tiên này đã hết lượt chưa?

	4. Nếu đã hết lượt:

		prio++;: Hạ ưu tiên! (Tăng prio lên, ví dụ từ 0 -> 1).

		Đảm bảo prio không vượt quá MAX_PRIO - 1.

		slot[prio] = MAX_PRIO - prio;: Reset lại số lượt cho mức ưu tiên mới mà tiến trình vừa bị rớt xuống.

	5. proc->prio = prio;: Cập nhật prio mới (có thể đã bị hạ) cho tiến trình.

	6. enqueue(&mlq_ready_queue[proc->prio], proc);: Đưa tiến trình trở lại hàng đợi ở mức prio (mới hoặc cũ) của nó.

	7. pthread_mutex_unlock(&queue_lock);: Mở khóa.
*/
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


/*
add_mlq_proc (Dùng cho tiến trình mới)
Chức năng: Thêm một tiến trình (proc) vừa được loader nạp vào hệ thống.

Chi tiết Code:

	1. pthread_mutex_lock(&queue_lock);: Khóa hàng đợi.

	2. int prio = proc->priority;: Lấy mức ưu tiên mặc định (default priority) được gán khi load.

	3. Đảm bảo prio nằm trong khoảng [0, MAX_PRIO - 1].

	4. proc->prio = prio;: Gán mức ưu tiên mặc định thành mức ưu tiên động (dynamic prio). Đây là prio sẽ thay đổi trong quá trình chạy.

	5. enqueue(&mlq_ready_queue[proc->prio], proc);: Thêm tiến trình vào hàng đợi mlq_ready_queue tương ứng với prio của nó.

	6. pthread_mutex_unlock(&queue_lock);: Mở khóa.
*/
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




/*
get_proc, put_proc, add_proc
Chức năng: Đây là các hàm "wrapper" (hàm bọc) công khai.

Chi tiết Code: Chúng chỉ đơn giản là gọi các hàm _mlq_ tương ứng. Việc này giúp che giấu logic MLQ bên trong và cho phép dễ dàng thay đổi thuật toán lập lịch (ví dụ, thay bằng FIFO) mà không cần sửa code ở cpu.c.
*/
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


