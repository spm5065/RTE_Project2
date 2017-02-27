
#include "stm32l476xx.h"
#include "SysClock.h"
#include "LED.h"
#include "UART.h"

#include <string.h>
#include <stdio.h>

#define POSTERRORSTRING "ERROR: No pulse within 100ms...\r\nPOST FAILED: Please Check connections and retry...\r\n"
#define PERIODSTRING "Lower Limit: 950us, Upper Limit: 1050us\r\nWould you like to change this? (y/n):"

char RxComByte = 0;
uint8_t buffer[BufferSize];
char str[50] = "";
char str2[] = "My body is ready!!!!";
char str3[] = "It is a 1!!!";

int g_pendingInterrupt = 0;
uint32_t clockT = 0;
unsigned int lPeriodMS = 950;
unsigned int hPeriodMS = 1050;
uint16_t buckets[101];

uint16_t started = 0;

void setupGPIO(){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
	
	//Input from PA0 Alternate function mode
	GPIOA->MODER &= ~GPIO_MODER_MODER0;
	GPIOA->MODER |= GPIO_MODER_MODER0_1|GPIO_MODER_MODER1_1;

	//Seturp Alternate Function Bologna
	GPIOA->AFR[0] &= ~GPIO_AFRL_AFRL0;
	GPIOA->AFR[0] |= 0x0001;
	
	// //Set Speed
	// GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR0;
	// //Pulldown
	// GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR0_0;
	// GPIOA->PUPDR |= GPIO_PUPDR_PUPDR0_1;

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
	//todo
	
	//Input Caputure Filter 7 Cycles
	//TIM2->CCMR1 |= TIM_CCMR1_IC1F_2 | TIM_CCMR1_IC1F_1 | TIM_CCMR1_IC1F_0;
	
	//Generate a Flipping Event 
	TIM2->EGR |= TIM_EGR_UG;
	
	//Enable counter
	TIM2->CR1 = TIM_CR1_CEN;
}

void EXTI0_IRQHandler(void){
	
	g_pendingInterrupt = 1;
	clockT = TIM2->CCR1;
	TIM2->CNT = 0;
	
	if(started && started < 1001){
		started++;
		if((clockT - lPeriodMS)< 101){
			buckets[(clockT - lPeriodMS)]++;
		}
	}
	
	EXTI->PR1 |= EXTI_PR1_PIF0;
}

int post(){
	while(!g_pendingInterrupt){
		if(TIM2->CNT > 100000){
			USART_Write(USART2, (uint8_t *)POSTERRORSTRING, strlen(POSTERRORSTRING));
			
			//Wait for input
			while(1)
				if('\r' == USART_Read(USART2)) return 0;
		}
	}
	
	return 1;
}

int setupPeriod(){
	char rxByte = 0;
	char buf[6];
	int i = 0;
	
	USART_Write(USART2, (uint8_t *) PERIODSTRING, strlen(PERIODSTRING));
	
	memset( buf, 0, 6); //Null the buffer
	lPeriodMS = 950;
	hPeriodMS = lPeriodMS + 100;
	while(1){
		rxByte = USART_Read(USART2);
		if(rxByte == 'y'){ //Set Limits
			USART_Write(USART2, (uint8_t *) "\r\nEnter new Limits: ", strlen("\r\nEnter new Limits: "));
			char rxByte = 0;
			do{
				if(rxByte) buf[i++] = rxByte;
				rxByte = USART_Read(USART2);
				if(i>5) {
					USART_Write(USART2, (uint8_t *) "Too many characters entered...resetting...\r\n", strlen("Too many characters entered...resetting...\r\n"));
					return 0;
				}

			}while(rxByte != '\r');
				
			if(!sscanf( buf, "%u", &lPeriodMS)){
				USART_Write(USART2, (uint8_t *) "\r\nInput Invalid\r\n", strlen("\r\nInput Invalid\r\n"));
				return 0;
			}
			
			if( lPeriodMS < 50 || lPeriodMS > 9950) {
				USART_Write(USART2, (uint8_t *) "Period out of range 50 to 9950ms... Retry\r\n", strlen("Period out of range 50 to 9950ms... Retry\r\n"));
				return 0;
			}
			hPeriodMS = lPeriodMS + 100;
			break;
		} if( rxByte == 'n' ) break; //Break 
		else USART_Write(USART2, (uint8_t *) "\r\nInput invalid", strlen("\r\nInput invalid"));
		
	}
	
	return 1;
}

void printHistogram(){
		USART_Write(USART2, (uint8_t *) "\r\n", strlen("\r\n"));
		for(int i = 0; i < 101; i++){
			if( buckets[i] > 0) {
				char bucketStr[25];
				sprintf(bucketStr, "%d us: %d\r\n", lPeriodMS + i, buckets[i]);
				USART_Write(USART2, (uint8_t *) bucketStr, strlen(bucketStr));
			}
		}
}

int main(void){
	
	System_Clock_Init(); // Switch System Clock = 80 MHz
	LED_Init();
	UART2_Init();
	setupGPIO();
	setupInterrupt();
	setupTimer2();
	
	//Trigger initial interrupt
	TIM2->EGR |= TIM_EGR_UG;
	
	//POST
	while(!post());
	USART_Write(USART2, (uint8_t *) "\r\nPOST SUCCEEDED\r\n", strlen("\r\nPOST SUCCEEDED\r\n"));
	
	
	//Run the thing
	while (1){
		
		while(!setupPeriod());
		
		char periodStr[25];
		sprintf(periodStr, "Range is %d to %d\r\n", lPeriodMS, hPeriodMS);
		USART_Write(USART2, (uint8_t *) periodStr, strlen(periodStr));
		
		for(int i = 0; i < 101; i++){
			buckets[i] = 0;
		}
		started = 1;
		while(started < 1001);
		
		printHistogram();
		
		USART_Write(USART2, (uint8_t *) "Restarting...\r\n", strlen("Restarting...\r\n"));
	}
}

