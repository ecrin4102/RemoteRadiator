/*
Description : Exemple d'un buffer tournant sous STM32.
							Le programme commence par initialiser la clock et les p�riphs essentiels
							Ensuite on initialise les variables sytemes
							Puis on configure l'USART1 relier � un PC avec les Interruptions sur RX.
	Le programme montre une assez bonne robustesse. Par contre L'ajout d'une tempo trop longue
*/



#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//====================================================================
#if !defined HSE_VALUE
	#define HSE_VALUE 8000000
#endif

#define MAX_USART_BUFF 200
//====================================================================
typedef enum {FALSE, TRUE}BOOL;

typedef struct CMD_T{
	int lenght;
	int id;
	BOOL cmd_available;
	char data[MAX_USART_BUFF];
  struct CMD_T *next;
	struct CMD_T *prev;
}Cmd_t;

typedef struct BUFF_T{
	int nb_Cmd;
	Cmd_t *first;
	Cmd_t *last;
}Buff_t;

//====================================================================
void initSystem(void);
void initApp(void);
void initUSART1(void);
void initUSART2(void);

Cmd_t* createCmdStruct(Cmd_t* prev);

void usartSendChar(USART_TypeDef *usart, char c);
void usartSendString(USART_TypeDef *usart, char *s);
void usartSendUint32(USART_TypeDef *usart, uint32_t data);
char* usartGetString(Buff_t *buff);

//=====================================================================


static Buff_t _usart1Buff;
static Buff_t _usart2Buff;

int main(void){
	//int i;
	//char* rep;
	RCC_ClocksTypeDef clk;
	
	initSystem();
	initApp();
	initUSART1();
	initUSART2();
	
	
	usartSendString(USART1, "Configuration du module wifi:\r\n");
	//usartSendString(USART2, "AT+CWMODE_CUR=1\r\n");
	//usartSendString(USART2, "AT+CWJAP_CUR=\"SFR-d178\", \"DELAE5LW7U4A\"\r\n");
	//usartSendString(USART2, "AT+CWDHCP_CUR=3\r\n");
	//usartSendString(USART2, "AT+CIPAP_CUR?\r\n");
	
	
	
	while(1){
		
		while(_usart1Buff.nb_Cmd > 0){
			if(_usart1Buff.first->cmd_available == TRUE){
				usartSendString(USART1, _usart1Buff.first->data);
				if(_usart1Buff.first->next != NULL){
					_usart1Buff.first = _usart1Buff.first->next;
					free(_usart1Buff.first->prev);
					_usart1Buff.first->prev = NULL;			
				}else{
					usartSendString(USART1, "nouveau\r\n");
					free(_usart1Buff.first);
					_usart1Buff.first = _usart1Buff.last = createCmdStruct(NULL);
				}
				_usart1Buff.nb_Cmd --;
			}
		}
		
	}
	
	
	return(0);
}


void initSystem(void){
	//Activation de la clock sur le gestionnaire de flash et de la ram
	//Normalement fait au reset mais on le fait dans le code pour �tre sure.
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FLITF | RCC_AHBPeriph_SRAM, ENABLE);

	//On utilise le quartz de 8Mhz associ� � la PLL avec un multiplieur de 9, soit 72Mhz
	RCC_HSEConfig(RCC_HSE_ON); //On active le mode quartz externe haute vitesse
	RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9); //Freq = 8*9=72Mhz
	RCC_PLLCmd(ENABLE);
	while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
	
	//On utilise la fr�quence max soit 72Mhz pour les p�riph�riques
	RCC_HCLKConfig(RCC_HCLK_Div1);
  RCC_PCLK2Config (RCC_HCLK_Div1) ; 	
	RCC_PCLK1Config(RCC_HCLK_Div1);
}

void initApp(void){
	_usart1Buff.nb_Cmd = 0;
	_usart1Buff.first = NULL;
	_usart1Buff.last = NULL;
	_usart1Buff.first = _usart1Buff.last = createCmdStruct(NULL);

	
	_usart2Buff.nb_Cmd = 0;
	_usart2Buff.first = NULL;
	_usart2Buff.last = NULL;
	_usart2Buff.first = _usart2Buff.last = createCmdStruct(NULL);
}

void initUSART1(void){
	USART_InitTypeDef usart1InitStruct;
	GPIO_InitTypeDef gpioaInitStruct;
	
	//On active la clock sur le p�riph�rique
	//A faire avant la config
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	
	//Configuration de Tx
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_9;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);
	
	//Configuration de RX
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_10;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);

	USART_Cmd(USART1, ENABLE);
	//Config
	usart1InitStruct.USART_BaudRate = 115200;
	usart1InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart1InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	usart1InitStruct.USART_Parity = USART_Parity_No;
	usart1InitStruct.USART_StopBits = USART_StopBits_1;
	usart1InitStruct.USART_WordLength = USART_WordLength_8b;

	
	USART_Init(USART1, &usart1InitStruct);
	
	//Autorisation des Interruptions
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
	NVIC_EnableIRQ(USART1_IRQn);
}

void initUSART2(void){
	USART_InitTypeDef usart2InitStruct;
	GPIO_InitTypeDef gpioaInitStruct;
	
	//On active la clock sur le p�riph�rique
	//A faire avant la config
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
	//Configuration de Tx
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_2;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);
	
	//Configuration de RX
	gpioaInitStruct.GPIO_Pin = GPIO_Pin_3;
	gpioaInitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpioaInitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &gpioaInitStruct);

	USART_Cmd(USART2, ENABLE);
	//Config
	usart2InitStruct.USART_BaudRate = 115200;
	usart2InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart2InitStruct.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	usart2InitStruct.USART_Parity = USART_Parity_No;
	usart2InitStruct.USART_StopBits = USART_StopBits_1;
	usart2InitStruct.USART_WordLength = USART_WordLength_8b;

	
	USART_Init(USART2, &usart2InitStruct);
	
	//Autorisation des Interruptions
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
	NVIC_EnableIRQ(USART2_IRQn);
}


void usartSendChar(USART_TypeDef *usart, char c){
		while(USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET);
		USART_SendData(usart, c);
	
}

void usartSendString(USART_TypeDef *usart, char *s){
	while(*s != '\0'){
		usartSendChar(usart, *s++);
	}
}

void  usartSendUint32(USART_TypeDef *usart, uint32_t data){
	char buffer[11];
	sprintf(buffer, "%zu",data);
	usartSendString(usart, buffer);
}

char* usartGetString(Buff_t *buff){
	return(NULL);
}

void USART1_IRQHandler(void){
	if(USART_GetITStatus(USART1, USART_IT_RXNE) == SET){
		
		if(_usart1Buff.first == NULL){
			_usart1Buff.first = _usart1Buff.last = createCmdStruct(NULL);
		}
		
		if(_usart1Buff.last->cmd_available == TRUE){
				//Create a new cmd
			_usart1Buff.last->next = createCmdStruct(_usart1Buff.last);
			_usart1Buff.last->next->prev = _usart1Buff.last;
			_usart1Buff.last = _usart1Buff.last->next;
		}
		
		//Save char in the buffer
		_usart1Buff.last->data[_usart1Buff.last->id] = USART_ReceiveData(USART1);
		if(_usart1Buff.last->data[_usart1Buff.last->id] == '\n'){
			_usart1Buff.last->data[_usart1Buff.last->id+1] = '\0';
			_usart1Buff.last->cmd_available = TRUE;
			_usart1Buff.nb_Cmd++;
		}
		_usart1Buff.last->id++;
		
		USART_ClearITPendingBit(USART1, USART_IT_RXNE);
	}
}

void USART2_IRQHandler(void){
	if(USART_GetITStatus(USART2, USART_IT_RXNE) == SET){
		if(_usart2Buff.first == NULL ){
			_usart2Buff.first = _usart2Buff.last = createCmdStruct(NULL);
		}
		
		//Create a new cmd
		if(_usart2Buff.last->cmd_available == TRUE){
			
			_usart2Buff.last->next = createCmdStruct(_usart2Buff.last);
			_usart2Buff.last->next->prev = _usart2Buff.last;
			_usart2Buff.last = _usart2Buff.last->next;
		}
		
		//Save char in the buffer
		_usart2Buff.last->data[_usart2Buff.last->id] = USART_ReceiveData(USART2);
		if(_usart2Buff.last->data[_usart2Buff.last->id] == '\n'){
			
			_usart2Buff.last->data[_usart2Buff.last->id+1] = '\0';
			_usart2Buff.last->cmd_available = TRUE;
			_usart2Buff.nb_Cmd++;
		}
		_usart2Buff.last->id++;
		
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}

Cmd_t* createCmdStruct(Cmd_t* prev){
	Cmd_t* temp = malloc(sizeof(Cmd_t));
	temp->id = 0;
	temp->cmd_available = FALSE;
	temp->lenght = 0;
	temp->next = NULL;
	temp->prev = prev;
	return(temp);
}
