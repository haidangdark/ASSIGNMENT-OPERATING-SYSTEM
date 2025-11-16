 /*================================================================================================================================================================================*/
/*=============================================================================ĐỌC TỪ ĐÂY NHA MN===================================================================================*/
 /*================================================================================================================================================================================*/
/*
File queue.c là một file cơ bản nhưng cực kỳ quan trọng, vì nó cung cấp cấu trúc dữ liệu nền tảng cho Bộ lập lịch (Scheduler).
dequeue được thiết kế để hoạt động như một Hàng đợi Ưu tiên (Priority Queue): khi dequeue được gọi, nó sẽ tìm và trả về tiến trình có độ ưu tiên cao nhất (tức là chỉ số prio nhỏ nhất).
Nó cung cấp 4 hàm cơ bản: empty (kiểm tra rỗng), enqueue (thêm vào cuối), dequeue (lấy ra theo ưu tiên), và purgequeue (xóa một tiến trình cụ thể).




Các thư viện và Header liên quan
1. #include <stdio.h>: Thư viện chuẩn C cho các hoạt động Input/Output. Mặc dù không được sử dụng trực tiếp trong code bạn cung cấp, nó thường được thêm vào cho các mục đích debug (ví dụ printf).

2. #include <stdlib.h>: Thư viện chuẩn C. Cần thiết vì nó định nghĩa NULL, được sử dụng rộng rãi trong các hàm để kiểm tra con trỏ.

3. #include "queue.h": File header của chính nó. File này định nghĩa struct queue_t và MAX_QUEUE_SIZE, cũng như khai báo (prototype) cho các hàm mà queue.c sẽ hiện thực.

4. Cấu trúc struct queue_t (từ queue.h) là trung tâm của file này:

struct queue_t {
    struct pcb_t * proc[MAX_QUEUE_SIZE]; // Một mảng các con trỏ trỏ đến PCB
    int size;                         // Số lượng phần tử hiện có trong hàng đợi
};
*/






#include <stdio.h>
#include <stdlib.h>
#include "queue.h"



/*
empty
Chức năng: Kiểm tra xem hàng đợi q có bị rỗng hay không.

Chi tiết Code:

        if (q == NULL) return 1;: Kiểm tra an toàn. Nếu con trỏ q là NULL, coi như là hàng đợi rỗng và trả về 1 (true).

        return (q->size == 0);: Trả về kết quả của phép so sánh q->size có bằng 0 hay không. Nếu size là 0, trả về 1 (true - rỗng); ngược lại trả về 0 (false - không rỗng).
*/
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


/*
enqueue
Chức năng: Thêm một tiến trình (proc) vào cuối hàng đợi q.

Chi tiết Code:

        1. Kiểm tra an toàn:

                if (q == NULL || proc == NULL || q->size >= MAX_QUEUE_SIZE): Kiểm tra xem q và proc có hợp lệ không, và quan trọng nhất là hàng đợi đã bị đầy chưa (q->size >= MAX_QUEUE_SIZE).

                Nếu một trong các điều kiện này xảy ra, hàm sẽ return và không làm gì cả.

        2. Thêm vào cuối:

                q->proc[q->size] = proc;: Gán con trỏ proc vào vị trí cuối cùng của mảng proc (vị trí q->size).

        3. Cập nhật kích thước:

                q->size++;: Tăng kích thước của hàng đợi lên 1.
*/
void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL || proc == NULL || q->size >= MAX_QUEUE_SIZE){
                return;
        }

        q->proc[q->size] = proc; // struct queue_t se có proc là struct pcb_t * proc[MAX_QUEUE_SIZE];
        q->size++;


}



/*
dequeue
Chức năng: Lấy ra và trả về tiến trình có độ ưu tiên cao nhất (chỉ số prio nhỏ nhất) từ hàng đợi q.

Chi tiết Code:

        1. Kiểm tra an toàn: if (q == NULL || q->size == 0): Nếu hàng đợi NULL hoặc rỗng, trả về NULL (không có gì để lấy).

        2. Logic MLQ_SCHED (Hàng đợi Ưu tiên):

                int highest_prio = 9999;: Khởi tạo mức ưu tiên cao nhất (thực ra là giá trị nhỏ nhất) bằng một số lớn.

                int idx = -1;: Khởi tạo chỉ số (index) của tiến trình được chọn.

                Tìm kiếm: Dùng vòng lặp for (i = 0; i < q->size; i++) để duyệt qua tất cả các tiến trình trong hàng đợi.

                int prio = q->proc[i]->prio;: Lấy prio (độ ưu tiên động) của tiến trình thứ i.

                if (prio < highest_prio): Nếu tìm thấy tiến trình có prio nhỏ hơn (ưu tiên cao hơn), cập nhật highest_prio và lưu lại idx = i.

        3. Xử lý kết quả tìm kiếm:

                if (idx == -1) return NULL;: Nếu không tìm thấy (ví dụ hàng đợi rỗng sau khi kiểm tra, hoặc prio bị lỗi), trả về NULL.

                struct pcb_t *selected = q->proc[idx];: Lưu lại con trỏ đến tiến trình được chọn.

        4. Xóa khỏi hàng đợi (Phần quan trọng):

                for (i = idx; i < q->size - 1; i++) { q->proc[i] = q->proc[i + 1]; }: Đây là thao tác "lấp lỗ hổng". Nó dịch chuyển tất cả các phần tử sau idx sang trái 1 vị trí, đè lên vị trí idx.

        5. Cập nhật kích thước: q->size--;.

        6. Trả về: return selected; (trả về tiến trình đã được chọn).
Lưu ý: Nếu #ifdef MLQ_SCHED không được định nghĩa, logic else sẽ thực hiện một hàng đợi FIFO đơn giản, tức là luôn lấy q->proc[0] và dịch chuyển phần còn lại sang trái. (đã định nghĩa lại r)
*/
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
        

}




/*
purgequeue
Chức năng: Xóa một tiến trình (proc) cụ thể ra khỏi hàng đợi q, bất kể nó nằm ở vị trí nào. Hàm này thường được dùng khi một tiến trình bị hủy (terminated).

Chi tiết Code:

        1. Kiểm tra an toàn: if (q == NULL || proc == NULL || q->size == 0): Kiểm tra q, proc và hàng đợi có rỗng không.

        2. Tìm kiếm:

                int found = 0; int idx = -1;: Khởi tạo biến.

                Dùng vòng lặp for (int i = 0; i < q->size; i++) để tìm proc trong mảng.

                if (q->proc[i] == proc): So sánh địa chỉ con trỏ. Nếu proc được tìm thấy tại vị trí i.

                idx = i; found = 1; break;: Ghi lại idx và thoát vòng lặp.

        3. Xử lý kết quả:

                if (!found) { return NULL; }: Nếu không tìm thấy proc trong hàng đợi, trả về NULL.

        4. Xóa khỏi hàng đợi:

                for (int i = idx; i < q->size-1; i++) { q->proc[i] = q->proc[i+1]; }: Logic "lấp lỗ hổng" tương tự như dequeue.

        5. Cập nhật kích thước: q->size--;.

        6. Trả về: return proc; (trả về con trỏ đến tiến trình vừa bị xóa).
*/
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