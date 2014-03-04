#include "main.h"

USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber   = 0,
				.DataINEndpoint           =
					{
						.Address          = CDC_TX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.DataOUTEndpoint =
					{
						.Address          = CDC_RX_EPADDR,
						.Size             = CDC_TXRX_EPSIZE,
						.Banks            = 1,
					},
				.NotificationEndpoint =
					{
						.Address          = CDC_NOTIFICATION_EPADDR,
						.Size             = CDC_NOTIFICATION_EPSIZE,
						.Banks            = 1,
					},
			},
	};
	
void uart_put( unsigned char data )
{
	 while(!(UCSR1A & (1<<UDRE1)));
		UDR1=data;		        
}

void uart_puts(const char *s )
{
    while (*s)
      uart_put(*s++);
}

unsigned char uart_get( void ) 
{
	while ( !(UCSR1A & (1<<RXC1)) );
	return UDR1;
}
	
static FILE USBSerialStream;
volatile static bool ConfigSuccess = true;
static volatile int8_t phoneBuffer[100];
static volatile int8_t bufferLength = 1;
static volatile int8_t numbers = 0;
volatile static bool bDebug = false;
volatile int16_t iRead = 0;

static volatile int8_t tim_cnter = 0;
ISR(TIMER0_COMPA_vect)
{
	tim_cnter++;
	if(tim_cnter > 100)
	{
		PORTB ^= (1 << PB6);
		tim_cnter = 0;
	}
}

ISR(INT1_vect)
{
	uart_puts("ATI\r");
}

void openGate()
{
	PORTC |= (1 << PC7);
	_delay_ms(3000);
	PORTC &= ~(1 << PC7);
}


SIGNAL(USART1_RX_vect)
{
	int8_t c = UDR1;
	int16_t c2 = c;
	if(ConfigSuccess && bDebug)
		fputs(&c2, &USBSerialStream); // do debugu
	
		
	if((bufferLength == 1 && c != 0x0D) || bufferLength > 98)
		return;
	
	phoneBuffer[bufferLength] = c;
	bufferLength++;		
}

static volatile int8_t stringBuffer[100];
static volatile int8_t stringCnter = 0;
unsigned char stringCheck(char *s)
{
	int i = 1;
	
	while(*s != phoneBuffer[i])
	{
		i++;
		if(i > bufferLength-1)
			return 0;
	}
	while (*s)
	{		
			
		if(phoneBuffer[i] != *s++)
			return 0;
		i++;			
		
		if(i > bufferLength-1)
			return 0;
	}
	
	stringCnter = 0;
	while(i < bufferLength -1)
	{
		stringBuffer[stringCnter] = phoneBuffer[i];
		stringCnter++;
		i++;
	}
	
	return 1;
}

unsigned char findRinBuff()
{

	if(bufferLength < 2)
		return 0;
		
	for(unsigned char i = 2; i < bufferLength; i++)
	{
		if(phoneBuffer[i] == 0x0D)
			return i;
	}
	
	return 0;

}

void bufferCheck()
{

	if(bufferLength > 1 && phoneBuffer[1] != 0x0D)
		bufferLength = 1;

	if(findRinBuff() > 0)
	{
		if(stringCheck("RING") == 1)
		{
			uart_puts("ATH\r");
			_delay_ms(500);
		}
		
		if(stringCheck("+CLCC: 1,1,6,") == 1)
		{	
		
			if(stringBuffer[21] != '"') // jeśli jest wpisany opis
			{
				openGate();
			}
			
		}
		
		bufferLength = 1;
	}
}

void pb_clear(int from, int to)
{
	char buffer[5];
	for (int i = from; i < to+1; i++)
	{
		uart_puts("AT+CPBW=");
		itoa(i, buffer, 10);
		uart_puts(buffer);
		uart_puts("\r");
		_delay_ms(300);
	}
}


int main(void)
{	
	SetupHardware();    
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);	
	GlobalInterruptEnable();
	
	_delay_ms(5000);
	uart_puts("ATI\r");
	_delay_ms(500);

	numbers = eeprom_read_word(( uint16_t *)1);
	
	for (;;)
	{
		if(ConfigSuccess)
		{
			int16_t b = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
			
			if(b > -1)
			{
					
				if(b == '*')
				{
					iRead = 0;					
				}	
				
				if(b == '\r' || b == 0x1A)
				{
					uart_puts("\",129,\"aa\"\r");
					_delay_ms(300);
					bufferLength = 1;
					
					fputs("ok\r\n", &USBSerialStream);
				}
				
				if(b == 0x1A)
				{
					if(numbers > iRead)
						pb_clear(iRead +1, numbers);
					
					numbers = iRead;
					eeprom_write_word((uint16_t*)1, (uint16_t)numbers);
				}		
								
				if(b == '*' || b == '\r')
				{
					iRead++;
					uart_puts("AT+CPBW=");
					
					char buff[5];
					itoa(iRead, buff, 10);
					
					int korekcja = 0;				
					if(iRead > 99)
					{
						korekcja = 2;
					}
					else if(iRead > 9)
					{
						korekcja = 1;
					}
					
					for(int i = 0; i < korekcja+1; i++)
						uart_put(buff[i]);
						
					uart_puts(",\"");
				}

				if(b > 47 && b < 58)
				{
					uart_put(b);			
				}
				
				if(b == 0x1B)
					openGate();
					
				if(b == 'd')
					bDebug = !bDebug;
									
			}
			
			CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
			USB_USBTask();
		}
		bufferCheck();
		
	}
}


void USARTInit(unsigned int ubrr_value)
{
   
   UCSR1A |= (1 << U2X1);
   UCSR1B |= (1 << RXEN1) | (1 << TXEN1) | (1 << RXCIE1);
   UCSR1C |= (1 << UCSZ11) | (1 << UCSZ10);
   UBRR1 = ubrr_value;
   DDRD |= (1 << PD3);
}

void SetupHardware(void)
{
	MCUSR &= ~(1 << WDRF);
	wdt_disable();
	
	DDRB = (1 << PB5) | (1 << PB6) | (1 << PB4);	
	DDRC = (1 << PC7);
	PORTC |= (1 << PC2);
	
	
	clock_prescale_set(0);

	USB_Init();	
	USARTInit(25);
	
	TCCR0A |= (1 << WGM01);
	TCCR0B |= (1 << CS00) | (1 << CS02);
	TIMSK0 |= (1 << OCIE0A);
	OCR0A = 255;
	
	DDRD &= ~(1 << PD1);
	EICRA |= (1 << ISC11) | (1 << ISC10);
	EIMSK |= (1 << INT1);
}

void EVENT_USB_Device_Connect(void)
{

}

void EVENT_USB_Device_Disconnect(void)
{

}

void EVENT_USB_Device_ConfigurationChanged(void)
{
	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}