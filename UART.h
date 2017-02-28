#ifndef __STM32L476G_DISCOVERY_UART_H
#define __STM32L476G_DISCOVERY_UART_H

#include "stm32l476xx.h"

#define BufferSize 32

typedef char BOOL;

#define FALSE 0x00;
#define TRUE	0xFF;
#define NULL	0

void UART2_Init(void);
void UART2_GPIO_Init(void);
void USART1_IRQHandler(void);
void USART2_IRQHandler(void);
void USART_Init(USART_TypeDef * USARTx);
void USART_Write(USART_TypeDef * USARTx, uint8_t *buffer, uint32_t nBytes);
uint8_t   USART_Read(USART_TypeDef * USARTx);
BOOL USART_ReadNB(USART_TypeDef * USARTx, char *readChar);
void USART_Delay(uint32_t us);
void USART_IRQHandler(USART_TypeDef * USARTx, uint8_t *buffer, uint32_t * pRx_counter);

#endif /* __STM32L476G_DISCOVERY_UART_H */
