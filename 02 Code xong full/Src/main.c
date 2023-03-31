
#include <stdint.h>
#include <stdio.h>
#include "led.h"

// stack memory
#define SIZE_TASK_STACK     1024U
#define SIZE_SCHED_STACK    1024U

#define SRAM_START 0x20000000U
#define SIZE_SRAM  ((128) * (1024))
#define SRAM_END  (SRAM_START + SIZE_SRAM)

#define T1_STACK_START 				 SRAM_END
#define T2_STACK_START  			(SRAM_END - (1*SIZE_TASK_STACK))
#define T3_STACK_START 			 (SRAM_END - (2*SIZE_TASK_STACK))
#define T4_STACK_START  			(SRAM_END - (3*SIZE_TASK_STACK))
#define IDLE_STACK_START  		 ( (SRAM_END) - (4 * SIZE_TASK_STACK) )
#define SCHED_STACK_START 		 (SRAM_END - (5*SIZE_TASK_STACK))

#define TICK_HZ 1000U
#define HSI_CLOCK 16000000U
#define SYSTICK_TIM_CLK HSI_CLOCK


// task handle function
void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);
void idle_task(void);
void init_systick_timer(uint32_t tick_hz);
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);

void init_tasks_stack(void);
void enable_rpocessor_faults(void);
__attribute__((naked))  void switch_sp_to_psp(void);
void update_next_task(void);
void save_psp_value(uint32_t current_psp_value);
uint32_t get_psp_value(void);

void task_delay(uint32_t tick_count);

#define INTERRUPT_DISABLE()  do{__asm volatile ("MOV R0,#0x1"); asm volatile("MSR PRIMASK,R0"); } while(0)
#define INTERRUPT_ENABLE()  do{__asm volatile ("MOV R0,#0x0"); asm volatile("MSR PRIMASK,R0"); } while(0)

#define MAX_TASKS   5
uint32_t g_tick_count = 0;

uint8_t curent_task = 1 ; // task 1 đang chạy

typedef struct
{
	uint32_t psp_value;
	uint32_t block_count;
	uint8_t current_state;
	void (*task_handler)(void); // giữ địa chỉ của task
}TCB_t; // Task control block

TCB_t user_tasks[MAX_TASKS];

int main(void)
{
	enable_rpocessor_faults();

	init_scheduler_stack(SCHED_STACK_START);

	led_init_all();

	init_tasks_stack();

	init_systick_timer(TICK_HZ);

	// đang dung MSP nên chuyển qua PSP
	switch_sp_to_psp();

	task1_handler();


    /* Loop forever */
	for(;;);
}

void idle_task(void)
{
	while(1);
}

void task1_handler(void)
{
	while(1)
	{
		printf("Task  1\n");
		led_on(12);
		task_delay(1000);
		led_off(12);
		task_delay(1000);

	}
}
void task2_handler(void)
{
	while(1)
	{
		printf("Task  2\n");
		led_on(13);
		task_delay(500);
		led_off(13);
		task_delay(500);

	}
}
void task3_handler(void)
{
	while(1)
	{
		printf("Task  3\n");
		led_on(14);
		task_delay(250);
		led_off(14);
		task_delay(250);
	}
}
void task4_handler(void)
{
	while(1)
	{
		printf("Task  4\n");
		led_on(15);
		task_delay(125);
		led_off(15);
		task_delay(125);

	}
}

void init_systick_timer(uint32_t tick_hz)
{
	uint32_t *pSRVR = (uint32_t *)0xE000E014;
	uint32_t *pSCSR = (uint32_t *)0xE000E010;

	// reload value
	uint32_t count_value = (SYSTICK_TIM_CLK / TICK_HZ) -1 ;

	// xóa SVR 24 bit
	*pSRVR &=~ (0x00FFFFFFFF);

	// ghi value vào SVR
	*pSRVR |= count_value;

	// enable systick exception request
	*pSCSR |= ( 1<< 1);
	// clock source
	*pSCSR |= ( 1<< 2);
	// anable counter
	*pSCSR |= ( 1<< 0);

}

__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack)
{
     __asm volatile("MSR MSP,%0": :  "r" (sched_top_of_stack)  :   );
     __asm volatile("BX LR"); // sao chép giá trị của LR vào PC

}

#define DUMMY_XPSR 0x01000000;
#define TASK_RUNNING_STATE 0x00
#define TASK_BLOCKED_STATE 0xFF
void init_tasks_stack(void)
{

	user_tasks[0].current_state = TASK_RUNNING_STATE;
	user_tasks[1].current_state = TASK_RUNNING_STATE;
	user_tasks[2].current_state = TASK_RUNNING_STATE;
	user_tasks[3].current_state = TASK_RUNNING_STATE;
	user_tasks[4].current_state = TASK_RUNNING_STATE;

	user_tasks[0].psp_value = IDLE_STACK_START;
	user_tasks[1].psp_value = T1_STACK_START;
	user_tasks[2].psp_value = T2_STACK_START;
	user_tasks[3].psp_value = T3_STACK_START;
	user_tasks[4].psp_value = T4_STACK_START;

	user_tasks[0].task_handler = idle_task;
	user_tasks[1].task_handler = task1_handler;
	user_tasks[2].task_handler = task2_handler;
	user_tasks[3].task_handler = task3_handler;
	user_tasks[4].task_handler = task4_handler;

	uint32_t * pPSP;
	for(int i = 0;i< MAX_TASKS ;i ++)
	{
		pPSP = (uint32_t*) user_tasks[i].psp_value;

		pPSP--; // XPSR
		*pPSP = DUMMY_XPSR;

		pPSP--; // PC
		*pPSP = (uint32_t) user_tasks[i].task_handler;

		pPSP--; // LR
		*pPSP = 0xFFFFFFFD;
		for( int j =0;j< 13; j++)
		{

			pPSP--;
			*pPSP = 0;

		}

		// sau khi xong thì lưu lại sPSP
		user_tasks[i].psp_value = (uint32_t)pPSP;

	}
}

void enable_rpocessor_faults(void)
{
	// 1 enable all config exception like usage foult, mem manager fault and bus fault
	uint32_t *pSHCSR = (uint32_t*)0xE000ED24;

	*pSHCSR |= ( 1 << 16); //mem manage
	*pSHCSR |= ( 1 << 17); //bus fault
	*pSHCSR |= ( 1 << 18); //usage fault
}

void HardFault_Handler(void)
{
	printf("Exception : Hardfault\n");
	while(1);
}


void MemManage_Handler(void)
{
	printf("Exception : MemManage\n");
	while(1);
}

void BusFault_Handler(void)
{
	printf("Exception : BusFault\n");
	while(1);
}

uint32_t get_psp_value(void)
{

	return user_tasks[curent_task].psp_value;
}

void save_psp_value(uint32_t current_psp_value)
{
	user_tasks[curent_task].psp_value = current_psp_value;
}

void update_next_task(void)
{
	int state = TASK_BLOCKED_STATE;

	for(int i= 0 ; i < (MAX_TASKS) ; i++)
	{
		curent_task++;
		curent_task %= MAX_TASKS;
		state = user_tasks[curent_task].current_state;
		if( (state == TASK_RUNNING_STATE) && (curent_task != 0) )
			break;
	}

	if(state != TASK_RUNNING_STATE)
	curent_task = 0;
}

__attribute__((naked))  void switch_sp_to_psp(void)
{
	//1. initialize the PSP with TASK1 stack start address
	//get the value of psp of current_task
	__asm volatile ("PUSH {LR}"); //Lưu LR kết nối trở lại main ()
	__asm volatile ("BL get_psp_value"); // Giá trị trả về lưu trong R0
	__asm volatile ("MSR PSP,R0"); //initialize psp
	__asm volatile ("POP {LR}");  //trả về giá trị LR

	//2. change SP to PSP using CONTROL register
	__asm volatile ("MOV R0,#0X02");
	__asm volatile ("MSR CONTROL,R0");
	__asm volatile ("BX LR");
}
void schedule()
{
	uint32_t *pICSR = (uint32_t*)0xE000ED04;
	//pend the pendsv exception
	*pICSR |= ( 1 << 28);

}
void task_delay(uint32_t tick_count)
{
		//disable interrupt
		INTERRUPT_DISABLE();

	   if(curent_task)
	   {
		   user_tasks[curent_task].block_count = g_tick_count + tick_count;
		   user_tasks[curent_task].current_state = TASK_BLOCKED_STATE;
		   schedule();
	   }

	   //enable interrupt
	   	INTERRUPT_ENABLE();

}

__attribute__((naked))  void PendSV_Handler(void)
{
	//* lưu trạng thái cho task current
	//1 get giá trị PSP của task đang chạy hiện tại
	__asm volatile("MRS R0, PSP");

	//2 sử dụng giá trị PSP lưu SF2 ( R4 - R11)
	__asm volatile ("STMDB R0!, {R4-R11}");

	__asm volatile("PUSH {LR}");

	//3. lưu giá trị hienj tại của PSP
	__asm volatile ("BL save_psp_value");


	//* truy xuất ngữ cảnh của Task tiếp theo
	//1 quyết định task nào chạy
	__asm volatile("BL update_next_task");

	//2. get PSP value
	__asm volatile ("BL get_psp_value");

	//3 sử dụng PSP value để truy xuát SF2 (R4 - R11)
	__asm volatile ("LDMIA R0!, {R4-R11}");

	//4. update PSP và thoát
	__asm volatile ("MSR PSP,R0");

	__asm volatile("POP {LR}");
	__asm volatile("BX LR");
}

void update_global_tick_count(void)
{
	g_tick_count++;
}

void unblock_tasks(void)
{
	for(int i = 1 ; i < MAX_TASKS ; i++)
	{
		if(user_tasks[i].current_state != TASK_RUNNING_STATE)
		{
			if(user_tasks[i].block_count == g_tick_count)
			{
				user_tasks[i].current_state =  TASK_RUNNING_STATE;
			}
		}
	}
}

void SysTick_Handler()
{
	uint32_t *pICSR = (uint32_t*)0xE000ED04;

	update_global_tick_count();

	unblock_tasks();

	//pend the pendsv exception
	*pICSR |= ( 1 << 28);
}

