
#include "stm32l476xx.h"
#include "SysClock.h"
#include "LED.h"
#include "UART.h"
#include "recipe.h"


#include <string.h>
#include <stdio.h>

#define POSTERRORSTRING "ERROR: No pulse within 100ms...\r\nPOST FAILED: Please Check connections and retry...\r\n"
#define PERIODSTRING "Lower Limit: 950us, Upper Limit: 1050us\r\nWould you like to change this? (y/n):"

char RxComByte = 0;
uint8_t buffer[BufferSize];
char str[50] = "";
char str2[] = "My body is ready!!!!";
char str3[] = "It is a 1!!!";

uint8_t g_pendingInterrupt = 0;
uint32_t clockT = 0;

//Position status for servos
uint8_t servoPos[2] = {0, 0};

//Recipe instruction
uint8_t recPos[2] = {0, 0};

//Recipe operation status
uint8_t recStat[2] = {0, 0};

//Address to set registers
volatile uint32_t *servos[2] = {&(TIM2->CCR1), &(TIM2->CCR2)};

//Two commands
char inputBuf[2] = {'\0', '\0'};

void setupGPIO(){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	
	//Input from PA0 Alternate function mode
	GPIOA->MODER &= ((~GPIO_MODER_MODER0) & (~GPIO_MODER_MODER1));
	GPIOA->MODER |= GPIO_MODER_MODER0_1 | GPIO_MODER_MODER1_1;

	//Seturp Alternate Function Bologna for PA0 and PA1
	GPIOA->AFR[0] &= ~GPIO_AFRL_AFRL0;
	GPIOA->AFR[0] |= 0x0011;
	

}

void setupInterrupt(){
	//Enable the system Clock
	RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_SYSCFGEN;
	
	//Set PA0 to EXTI0
	SYSCFG->EXTICR[0] |= SYSCFG_EXTICR1_EXTI0_PA;
	
	//Setup EXTI
	EXTI->IMR1 |= EXTI_IMR1_IM0;
	EXTI->RTSR1 |= EXTI_RTSR1_RT0;
	
	//Setup NVIC
	NVIC_SetPriority(EXTI0_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 1, 0));
	NVIC_EnableIRQ(EXTI0_IRQn);
	
}

void setupTimer2(){
	//Enable clock for timer 2
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	
	//Timer Prescale 80MHz / 80 -> 1MHz
	TIM2->PSC = (80-1);
	
	
	//Turn off input capture
	TIM2->CCER &= ~TIM_CCER_CC1E;
	
	//Enable Input Capture channels
	TIM2->CCMR1 &= ~TIM_CCMR1_CC1S;
	TIM2->CCMR1 &= ~TIM_CCMR1_CC2S;
	
	//pwm enable with pre-load
	TIM2->CCMR1 |= TIM_CCMR1_OC1PE | TIM_CCMR1_OC2PE | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2;
	TIM2->CR1 |= TIM_CR1_ARPE;
	
	//Re-enable input capture
	TIM2->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E;
	
	//20ms Period
	TIM2->ARR = 20000;
	
	//Initial servo Positions
	TIM2->CCR1 = pwmDuty[0];
	TIM2->CCR2 = pwmDuty[0];
	
	//Input Caputure Filter 7 Cycles
	//TIM2->CCMR1 |= TIM_CCMR1_IC1F_2 | TIM_CCMR1_IC1F_1 | TIM_CCMR1_IC1F_0;
	
	//Generate a Flipping Event 
	TIM2->EGR |= TIM_EGR_UG;
	
	//Enable counter
	TIM2->CR1 = TIM_CR1_CEN;
}

//	void EXTI0_IRQHandler(void){
//		
//		g_pendingInterrupt = 1;
//		clockT = TIM2->CCR1;
//		TIM2->CNT = 0;
//		
//		if(started && started < 1001){
//			started++;
//			if((clockT - lPeriodMS)< 101){
//				buckets[(clockT - lPeriodMS)]++;
//			}
//		}
//		
//		EXTI->PR1 |= EXTI_PR1_PIF0;
//	}



void parseSingleCommand(char in, int servo){
	
	
	switch(in){
		//Go right
		case 'r':
		case 'R':
			if(servoPos[servo] != 0)
				*servos[servo] = pwmDuty[--servoPos[servo]];
			break;
		
		//Go Left
		case 'l':
		case 'L':
			if(servoPos[servo] != 5)
				*servos[servo] = pwmDuty[++servoPos[servo]];
			break;
		
		//Pause
		case 'p':
		case 'P':
			recStat[servo] = FALSE;
			break;
		
		//Continue
		case 'c':
		case 'C':
			recStat[servo] = TRUE;
			break;
		
		//No-Op
		case 'n':
		case 'N':
			break;
		
		//restart
		case 'b':
		case 'B':
			recStat[servo] = TRUE;
			recPos[servo] = 0;
			break;
		
		//Otherwise ignore
		default:
			break;
	}
}

int main(void){
	char rxByte = 0;
	System_Clock_Init(); // Switch System Clock = 80 MHz
	//LED_Init();
	UART2_Init();
	setupGPIO();
	//setupInterrupt();
	setupTimer2();
	
	// 	//this loop switches between the two for testing
	// 	while(1){
	// 		rxByte = USART_Read(USART2);
	// 		TIM2->CCR1 = pwmDuty[rxByte-48];
	// 		rxByte = 0;
	// 		rxByte = USART_Read(USART2);
	// 		TIM2->CCR2 = pwmDuty[rxByte-48];
	// 	}
	
	while (1){
		char charRead;
		while (USART_ReadNB(USART2, &charRead)){
			
			if(charRead == '\r'){
				parseSingleCommand(inputBuf[0], 0);
				parseSingleCommand(inputBuf[1], 1);
				//parseCommands();
			} else {
				inputBuf[0] = inputBuf[1];
				inputBuf[1] = charRead;
			}
		}
		
		if(recStat[0]){
			//TODO: Run Instruction
		}
		
		if(recStat[1]){
			//TODO: Run Instruction
		}
		
	}
	
	
}

