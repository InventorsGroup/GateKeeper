#include "main.h"


ISR(TIMER0_COMPA_vect)
{
	PORTB ^= (1 << PB6);
}



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

static FILE USBSerialStream;

void USARTWriteChar(unsigned char data)
{
   while(!(UCSR1A & (1<<UDRE1)));
   UDR1=data;
}

void uart_puts(const char *s )
{
	while (*s) 
		USARTWriteChar(*s++);
}

unsigned char USARTReadChar( void ) 
{
	PORTD &= ~(1<<2);
	while ( !(UCSR1A & (1<<RXC1)) );
	return UDR1;
}

SIGNAL(USART1_RX_vect)
{
	int16_t c = UDR1;
	fputs(&c, &USBSerialStream);
}

void pb_clear()
{
	char buffer[5];
	for (int i =1; i < 11; i++)
	{
		uart_puts("AT+CPBW=");
		itoa(i, buffer, 10);
		uart_puts(buffer);
		uart_puts("\r");
		_delay_ms(100);
	}
}

int main(void)
{
	int16_t b;
	
	SetupHardware();
    
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	
	GlobalInterruptEnable();
	b = "GateKeeper init: OK \n\r";
	fputs(&b, &USBSerialStream);
	
	_delay_ms(1000);
	PORTB |= (1 << PB4);
	_delay_ms(1500);
	PORTB &= ~(1 << PB4);
	_delay_ms(3000);
	
	PORTB |= (1 << PB5);
	

	for (;;)
	{
		b = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		
		if(b > -1)
		{
			fputs(&b, &USBSerialStream);
			USARTWriteChar(b);	
			if (b == 'c') pb_clear();; 			
		}
		
		CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
		USB_USBTask();
		
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
	
	clock_prescale_set(0);

	USB_Init();
	
	USARTInit(51);
	
	TCCR0A |= (1 << WGM01);
	TCCR0B |= (1 << CS00) | (1 << CS02);
	TIMSK0 |= (1 << OCIE0A);
	OCR0A = 255;
}

void EVENT_USB_Device_Connect(void)
{

}

void EVENT_USB_Device_Disconnect(void)
{

}

void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

void EVENT_USB_Device_ControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}