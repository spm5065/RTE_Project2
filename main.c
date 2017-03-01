
#include "stm32l476xx.h"
#include "SysClock.h"
#include "LED.h"
#include "UART.h"
#include "recipe.h"


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG_PRINT

char RxComByte = 0;
uint8_t buffer[BufferSize];


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

//Loop stuff
BOOL 		inLoop[2];
uint8_t loopStart[2];
uint8_t loopsRemaining[2];

//Wait Timing
uint8_t waitTimings[2] = {0, 0};

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
			Green_LED_Off();
			break;
		
		//Continue
		case 'c':
		case 'C':
			if( recPos[servo] != 0){
				recStat[servo] = TRUE;
				Green_LED_On();
			}
			
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
			inLoop[servo] = FALSE;
			Green_LED_On();
			Red_LED_Off();
			break;
		
		//Otherwise ignore
		default:
			USART_Write(USART2, (uint8_t *)"User Command Error\r\n>", strlen("User Command Error\r\n>"));
			break;
	}
}



//run a single instruction 
void runInstruction( int servo ){
	
	//Get instruction only portion
	uint8_t instruction = recipeTest[recPos[servo]] & 0xE0;
	uint8_t argument		= recipeTest[recPos[servo]] & 0x1F;
	
	//if we need to wait, decrement the counter, then wait
	if(waitTimings[servo] > 0) {
		
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)".\r\n", strlen(".\r\n"));
#endif
		
		waitTimings[servo]--;
		return;
	}
	
	//Do instruction parsing
	switch (instruction){
		
		//Move
		case MOV:
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"MOV\r\n", strlen("MOV\r\n"));
#endif
		
			//Check if out of range
			if(5 < argument){
				USART_Write(USART2, (uint8_t *)"MOV Argument error\r\n Stopping Execution\r\n>", strlen("MOV Argument error\r\n Stopping Execution\r\n>"));
				
				//reset statuses
				recStat[servo] = FALSE;
				recPos[servo] = 0;
				inLoop[servo] = FALSE;
				
				//reset waits
				waitTimings[servo] = 0;
				
			} else {
				
				//Calculate waits for this move
				waitTimings[servo] = abs((int)servoPos[servo] - argument) << 1 ;
				
				//Set Position
				servoPos[servo] = argument;
				
				//Send the servo there
				*servos[servo] = pwmDuty[argument];
			}
			break;
			
			//Custom Command moves servo to the right if possible
		case RIGHT:
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"RIGHT\r\n", strlen("RIGHT\r\n"));
#endif
		if(servoPos[servo] != 0)
				//Calculate waits for this move
				waitTimings[servo] = abs((int)servoPos[servo] - argument) << 1 ;
				//Set Position
				servoPos[servo] = argument;
				//Send the servo there
				*servos[servo] = pwmDuty[--servoPos[servo]];	
		break;
		
		//Custom Command moves servo to the left if possible
		case LEFT:
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"LEFT\r\n", strlen("LEFT\r\n"));
#endif
		if(servoPos[servo] != 5)
				//Calculate waits for this move
				waitTimings[servo] = abs((int)servoPos[servo] - argument) << 1 ;
				//Set Position
				servoPos[servo] = argument;
				//Send the servo there
				*servos[servo] = pwmDuty[++servoPos[servo]];
		break;
		
		
		//Start a loop
		case LOOP:
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"LOOP\r\n", strlen("LOOP\r\n"));
#endif
		
			//check for nested loop
			if( inLoop[servo] ) {
				recPos[servo] = 0;
				recStat[servo] = FALSE;
				Red_LED_On();
				Green_LED_On();
				return;
			} //Loop Error
			else{
				
				//Setup Loop
				inLoop[servo] = TRUE;
				loopStart[servo] = recPos[servo];
				loopsRemaining[servo] = argument;
			}
			break;
		
		//End a loop
		case ENDLOOP:
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"ENDLOOP\r\n", strlen("ENDLOOP\r\n"));
#endif			
		
			//check for no loop
			if( !inLoop[servo] ) {
				Red_LED_On();
			} //TODO Loop Error
			else{
				
				//check to see if finished
				if(loopsRemaining[servo] == 0){
					
					//Set loop to off
					inLoop[servo] = FALSE;
				
				//if need to iterate again
				} else {
					
					//start loop again
					loopsRemaining[servo]--;
					recPos[servo] = loopStart[servo];
				}
			}
			break;
		
		//Wait for some amount of time
		case WAIT:
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"WAIT\r\n", strlen("WAIT\r\n"));
#endif
		
			//Set number of times to wait
			waitTimings[servo] = argument;
			break;
		
		//End the recipe
		case RECIPEEND:
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"RECIPEEND\r\n", strlen("RECIPEEND\r\n"));
#endif
		
			//Cleanup and set running to false
			recPos[servo] = 0;
			recStat[servo] = FALSE;
			return;
		default:
			Red_LED_On();
	}
	
	//Set next instruction
	recPos[servo]++;
	
}

int main(void){
	//char rxByte = 0;
	System_Clock_Init(); // Switch System Clock = 80 MHz
	LED_Init();
	UART2_Init();
	setupGPIO();
	//setupInterrupt();
	setupTimer2();
	
	//Init that we aren't in a loop
	inLoop[0] = FALSE;
	inLoop[1] = FALSE;
	
	// 	//this loop switches between the two for testing
	// 	while(1){
	// 		rxByte = USART_Read(USART2);
	// 		TIM2->CCR1 = pwmDuty[rxByte-48];
	// 		rxByte = 0;
	// 		rxByte = USART_Read(USART2);
	// 		TIM2->CCR2 = pwmDuty[rxByte-48];
	// 	}
	
	USART_Write(USART2, (uint8_t *)"Setup Complete\r\n>", strlen("Setup Complete\r\n>"));
	
	while (1){
		char charRead;
		while (USART_ReadNB(USART2, &charRead)){
						USART_Write(USART2,(uint8_t *)&charRead, sizeof(char));
			
			if(charRead == '\r'){
				USART_Write(USART2, (uint8_t *)"\r\n>", strlen("\r\n>"));
				parseSingleCommand(inputBuf[0], 0);
				parseSingleCommand(inputBuf[1], 1);
			} else {
				inputBuf[0] = inputBuf[1];
				inputBuf[1] = charRead;
			}
		}
		
		//If running run an instruction on one servo
		if(recStat[0]){
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"1:", strlen("1:"));
#endif
			
			runInstruction(0);
		}
		
		//If running run an instruction on the other servo
		if(recStat[1]){
			
#ifdef DEBUG_PRINT
			USART_Write(USART2, (uint8_t *)"2:", strlen("2:"));
#endif
			
			runInstruction(1);
		}
		
		//Sit and wait
		for( int i = 0; i < 8000000; i++){}
		
	}
}

