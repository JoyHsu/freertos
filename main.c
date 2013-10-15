#define USE_STDPERIPH_DRIVER
#include "stm32f10x.h"

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <string.h>

/* Filesystem includes */
#include "filesystem.h"
#include "fio.h"

/* Shell parameter definition */
#define MAX_cmdname 10
#define MAX_function 50
#define MAX_inslength 100
#define BACKSPACE 127

extern const char _sromfs;

static void setup_hardware();

volatile xQueueHandle serial_str_queue = NULL;
volatile xSemaphoreHandle serial_tx_wait_sem = NULL;
volatile xQueueHandle serial_rx_queue = NULL;

/* Queue structure used for passing messages. */
typedef struct {
	char str[100];
} serial_str_msg;

/* Queue structure used for passing characters. */
typedef struct {
	char ch;
} serial_ch_msg;


/* IRQ handler to handle USART2 interruptss (both transmit and receive
 * interrupts). */
void USART2_IRQHandler()
{
	static signed portBASE_TYPE xHigherPriorityTaskWoken;
	serial_ch_msg rx_msg;

	/* If this interrupt is for a transmit... */
	if (USART_GetITStatus(USART2, USART_IT_TXE) != RESET) {
		/* "give" the serial_tx_wait_sem semaphore to notfiy processes
		 * that the buffer has a spot free for the next byte.
		 */
		xSemaphoreGiveFromISR(serial_tx_wait_sem, &xHigherPriorityTaskWoken);

		/* Diables the transmit interrupt. */
		USART_ITConfig(USART2, USART_IT_TXE, DISABLE);
		/* If this interrupt is for a receive... */
	}
	else if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET) {
		/* Receive the byte from the buffer. */
		rx_msg.ch = USART_ReceiveData(USART2);

		/* Queue the received byte. */
		if(!xQueueSendToBackFromISR(serial_rx_queue, &rx_msg, &xHigherPriorityTaskWoken)) {
			/* If there was an error queueing the received byte,
			 * freeze. */
			while(1);
		}
	}
	else {
		/* Only transmit and receive interrupts should be enabled.
		 * If this is another type of interrupt, freeze.
		 */
		while(1);
	}

	if (xHigherPriorityTaskWoken) {
		taskYIELD();
	}
}

void strprintf(char *instr)
{
	while (!xQueueSendToBack(serial_str_queue, instr,
		                       		portMAX_DELAY));
}

void send_byte(char ch)
{
	/* Wait until the RS232 port can receive another byte (this semaphore
	 * is "given" by the RS232 port interrupt when the buffer has room for
	 * another byte.
	 */
	while (!xSemaphoreTake(serial_tx_wait_sem, portMAX_DELAY));

	/* Send the byte and enable the transmit interrupt (it is disabled by
	 * the interrupt).
	 */
	USART_SendData(USART2, ch);
	USART_ITConfig(USART2, USART_IT_TXE, ENABLE);
}

char receive_byte()
{
	serial_ch_msg msg;

	/* Wait for a byte to be queued by the receive interrupts handler. */
	while (!xQueueReceive(serial_rx_queue, &msg, portMAX_DELAY));

	return msg.ch;
}

void rs232_xmit_msg_task(void *pvParameters)
{
	serial_str_msg msg;
	int curr_char;

	while (1) {
		/* Read from the queue.  Keep trying until a message is
		 * received.  This will block for a period of time (specified
		 * by portMAX_DELAY). */
		while (!xQueueReceive(serial_str_queue, &msg, portMAX_DELAY));

		/* Write each character of the message to the RS232 port. */
		curr_char = 0;
		while (msg.str[curr_char] != '\0') {
			send_byte(msg.str[curr_char]);
			curr_char++;
		}
	}
}

void read_romfs_task(void *pvParameters)
{
	char buf[128];
	size_t count;
	int fd = fs_open("/romfs/test.txt", 0, O_RDONLY);
	do {
		//Read from /romfs/test.txt to buffer
		count = fio_read(fd, buf, sizeof(buf));
		
		//Write buffer to fd 1 (stdout, through uart)
		fio_write(1, buf, count);
	} while (count);
	
	while (1);
}

void Shell(void *pvParameters)
{
	serial_str_msg msg;
	char ch;
	int curr_char = 0;
	int curr_ins = 0;
	char ins[MAX_inslength];
	char JoyShell[]="JoyShell>>";
	char next_line[3] = {'\n','\r','\0'};

	enum{
		
		Echo= 0, //Excute Echo command
		Hello,	 //Excute Hello command
		Ps,	 //Excute help command
		Help,	 //Excute ps command
		CMD_NUM,
		Wait ,//Wait command that be inserted
		Check,	 //Excute Check command
		Error
	}State;
	
	State = Wait;


	typedef struct {
		char name[MAX_cmdname + 1];
		char function[MAX_function + 1];
		int length;
		}Cmd_entry;
	
	const Cmd_entry cmd_data[CMD_NUM] = {
		[Echo] = {.name = "echo ", .function = "Show words that you enter a moment ago.", .length = 5},
		[Hello] = {.name = "hello ", .function = "Show words that you want to listen.", .length = 5},
		[Ps] = {.name = "ps ", .function = "Show runing process.", .length = 2},
		[Help] = {.name = "help ", .function = "Show commands that you can use.", .length = 4}
		};

	while(1)
	{
		switch(State)
		{
			case Wait:
				{

					strprintf("JoyShell>>");
					curr_ins =0;
					
					while(State==Wait)
					{
						curr_char = 0;
						ch = receive_byte();
						if((ch==BACKSPACE)&&(curr_ins>0))//Backspace 
							{
							msg.str[curr_char++]='\b';
							msg.str[curr_char++]=' ';
							msg.str[curr_char++]='\b';
							msg.str[curr_char++]='\0';
							curr_ins--;
							}
						else if((ch==BACKSPACE)&&(curr_ins<=0))//It is limit(at first), so can not to do backspace 
							{
							msg.str[curr_char++]=' ';
							msg.str[curr_char++]='\b';
							msg.str[curr_char++]='\0';
							}
						else if(ch=='\r')//Enter
							{
							State=Check;
							msg.str[curr_char++]='\0';
							}
						else//Universal word
							{
							ins[curr_ins++]=ch;
							msg.str[curr_char++] = ch;
							msg.str[curr_char++]='\0';
							}
						strprintf(msg.str);
						
					}
				}break;
			case Check:
				{
				int i,j,k;
					for(i = 0 ; i < CMD_NUM ; i++)
					{
						k=0;
						for(j = 0; j < cmd_data[i].length ; j++)
						{
							if( ins[j] == cmd_data[i].name[j] )
								k++;
							
							if( k >= cmd_data[i].length)
							{
								State=(i);
								break;
							}
							
						}

						if(State != Check)
							break;
					}
					if(State == Check)
						State=Error;
					strprintf(next_line);
										
				}break;
			case Echo:
				{
				curr_char = 0;
				int i;
				for( i=5 ; i < curr_ins ; i++)
					msg.str[curr_char++] = ins[i];
				msg.str[curr_char++]='\0';
				strprintf(msg.str);
				strprintf(next_line);
				State = Wait;
				}break;
			case Hello:
				{
				strprintf("You are a cool guy.");
				strprintf(next_line);
				State = Wait;
				}break;
			case Help:
				{
				int i;
				for ( i=0 ; i < Help ; i++)
				{
				strprintf(cmd_data[i].name);
				strprintf(cmd_data[i].function);
				strprintf(next_line);
				}
				State = Wait;
				}break;
			case Error:
				{
				strprintf("No server");
				strprintf(next_line);
				State = Wait;
				}break;			
		}	
	}

	
}

int main()
{
	init_rs232();
	enable_rs232_interrupts();
	enable_rs232();
# if 0	
	fs_init();
	fio_init();
	
	register_romfs("romfs", &_sromfs);
	
	/* Create the queue used by the serial task.  Messages for write to
	 * the RS232. */
	vSemaphoreCreateBinary(serial_tx_wait_sem);

	/* Create a task to output text read from romfs. */
	xTaskCreate(read_romfs_task,
	            (signed portCHAR *) "Read romfs",
	            512 /* stack size */, NULL, tskIDLE_PRIORITY + 2, NULL);
# endif

	/* Create the queue used by the serial task.  Messages for write to
	 * the RS232. */
	serial_str_queue = xQueueCreate(10, sizeof(serial_str_msg));
	vSemaphoreCreateBinary(serial_tx_wait_sem);
	serial_rx_queue = xQueueCreate(1, sizeof(serial_ch_msg));


	/* Create a task to write messages from the queue to the RS232 port. */
	xTaskCreate(rs232_xmit_msg_task,
	            (signed portCHAR *) "Serial Xmit Str",
	            512 /* stack size */, NULL, tskIDLE_PRIORITY + 2, NULL);
	
	/* Create a task to realize shell function. */
	xTaskCreate(Shell,
	            (signed portCHAR *) "Shell",
	            512 /* stack size */, NULL,
	            tskIDLE_PRIORITY + 5, NULL);

	/* Start running the tasks. */
	vTaskStartScheduler();

	return 0;
}

void vApplicationTickHook()
{
}
