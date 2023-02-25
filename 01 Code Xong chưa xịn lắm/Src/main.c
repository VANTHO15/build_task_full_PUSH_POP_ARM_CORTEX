
#include <stdint.h>
#include <stdio.h>
#include "led.h"

// stack memory
#define SIZE_TASK_STACK     1024U
#define SIZE_SCHED_STACK    1024U

#define SRAM_START 0x20000000U
#define SIZE_SRAM  ((128) * (1024))
#define SRAM_END  (SRAM_START + SIZE_SRAM)

#define T1_STACK_START  SRAM_END
#define T2_STACK_START  (SRAM_END - (1*SIZE_TASK_STACK))
#define T3_STACK_START  (SRAM_END - (2*SIZE_TASK_STACK))
#define T4_STACK_START  (SRAM_END - (3*SIZE_TASK_STACK))
#define SCHED_STACK_START  (SRAM_END - (4*SIZE_TASK_STACK))

#define TICK_HZ 1000U
#define HSI_CLOCK 16000000U
#define SYSTICK_TIM_CLK HSI_CLOCK


// task handle function
void task1_handler(void);
void task2_handler(void);
void task3_handler(void);
void task4_handler(void);
void init_systick_timer(uint32_t tick_hz);
__attribute__((naked)) void init_scheduler_stack(uint32_t sched_top_of_stack);

void init_tasks_stack(void);
void enable_rpocessor_faults(void);
__attribute__((naked))  void switch_sp_to_psp(void);
void update_next_task(void);
void save_psp_value(uint32_t current_psp_value);
uint32_t get_psp_value(void);

#define MAX_STACKS 4
uint32_t psp_of_stacks[MAX_STACKS] = {T1_STACK_START, T2_STACK_START, T3_STACK_START, T4_STACK_START};
uint32_t task_handlers[MAX_STACKS];

uint8_t curent_task = 0 ; // task 1 đang chạy

int main(void)
{
	enable_rpocessor_faults();
	init_scheduler_stack(SCHED_STACK_START);

	task_handlers[0]= (uint32_t)task1_handler;
	task_handlers[1]= (uint32_t)task2_handler;
	task_handlers[2]= (uint32_t)task3_handler;
	task_handlers[3]= (uint32_t)task4_handler;

	led_init_all();

	init_tasks_stack();

	init_systick_timer(TICK_HZ);

	// đang dung MSP nên chuyển qua PSP
	switch_sp_to_psp();

	task1_handler();


    /* Loop forever */
	for(;;);
}

void task1_handler(void)
{
	while(1)
	{
		printf("Task  1\n");
		led_on(12);
		delay(DELAY_COUNT_1S);
		led_off(12);
		delay(DELAY_COUNT_1S);

	}
}
void task2_handler(void)
{
	while(1)
	{
		printf("Task  2\n");
		led_on(13);
		delay(DELAY_COUNT_500MS);
		led_off(13);
		delay(DELAY_COUNT_500MS);

	}
}
void task3_handler(void)
{
	while(1)
	{
		printf("Task  3\n");
		led_on(14);
		delay(DELAY_COUNT_250MS);
		led_off(14);
		delay(DELAY_COUNT_250MS);

	}
}
void task4_handler(void)
{
	while(1)
	{
		printf("Task  4\n");
		led_on(15);
		delay(DELAY_COUNT_125MS);
		led_off(15);
		delay(DELAY_COUNT_125MS);

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
void init_tasks_stack(void)
{
	uint32_t * pPSP;
	for(int i = 0;i< MAX_STACKS ;i ++)
	{
		pPSP = (uint32_t*)psp_of_stacks[i];

		pPSP--; // XPSR
		*pPSP = DUMMY_XPSR;

		pPSP--; // PC
		*pPSP = task_handlers[i];

		pPSP--; // LR
		*pPSP = 0xFFFFFFFD;
		for( int j =0;j< 13; j++)
		{

			pPSP--;
			*pPSP = 0;

		}

		// sau khi xong thì lưu lại sPSP
		psp_of_stacks[i] = (uint32_t)pPSP;

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

	return psp_of_stacks[curent_task];
}

void save_psp_value(uint32_t current_psp_value)
{
	psp_of_stacks[curent_task]= current_psp_value;
}

void update_next_task(void)
{
	curent_task ++;
	curent_task = curent_task % MAX_STACKS;
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

__attribute__((naked)) void SysTick_Handler()
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

