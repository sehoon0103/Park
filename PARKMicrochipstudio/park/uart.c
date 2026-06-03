#include "board.h"
#include <avr/io.h>

#define USART_TX_BUFFER_SIZE 64
#define USART_RX_BUFFER_SIZE 64

// RS-485 송수신 방향 제어
#define RS485_TR_DDR   DDRC
#define RS485_TR_PORT  PORTC
#define RS485_TR_PIN   (7)

#define RS485_TR_TX()  (RS485_TR_PORT |=  (1 << RS485_TR_PIN))
#define RS485_TR_RX()  (RS485_TR_PORT &= ~(1 << RS485_TR_PIN))

// ISR ↔ main 공유 링 버퍼
volatile uint8_t tx_buffer[USART_TX_BUFFER_SIZE];
volatile uint8_t tx_head = 0;
volatile uint8_t tx_tail = 0;
volatile uint8_t rx_buffer[USART_RX_BUFFER_SIZE];
volatile uint8_t rx_head = 0;
volatile uint8_t rx_tail = 0;

ISR(USART_RX_vect)
{
	uint8_t data = UDR0;
	uint8_t next = rx_head + 1;
	if (next >= USART_RX_BUFFER_SIZE) next = 0;
	if (next == rx_tail) return;  // buffer full
	rx_buffer[rx_head] = data;
	rx_head = next;
}

ISR(USART_UDRE_vect)
{
	if (tx_head == tx_tail)
	{
		UCSR0B &= ~(1 << UDRIE0);
		UCSR0A |=  (1 << TXC0);
		UCSR0B |=  (1 << TXCIE0);  // 마지막 바이트 전송 완료 후 TR 전환을 위해
		return;
	}
	UDR0 = tx_buffer[tx_tail];
	uint8_t next = tx_tail + 1;
	if (next >= USART_TX_BUFFER_SIZE) next = 0;
	tx_tail = next;
}

// TXC ISR에서 TR 전환 — UDRE 시점에 하면 마지막 바이트가 깨짐
ISR(USART_TX_vect)
{
	RS485_TR_RX();
	UCSR0B &= ~(1 << TXCIE0);
}

void uart_init(uint32_t baud)
{
	UBRR0 = (F_CPU / 16 / baud) - 1;
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // 8-bit, no parity, 1 stop
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	RS485_TR_DDR |= (1 << RS485_TR_PIN);
	RS485_TR_RX();
	sei();
}

void uart_tx(uint8_t data)
{
	uint8_t next = tx_head + 1;
	if (next >= USART_TX_BUFFER_SIZE) next = 0;
	while (next == tx_tail) {}  // buffer full, wait
	tx_buffer[tx_head] = data;
	tx_head = next;
	RS485_TR_TX();
	UCSR0B |= (1 << UDRIE0);
}

uint8_t uart_rx()
{
	while (rx_head == rx_tail) {}  // blocking
	uint8_t data = rx_buffer[rx_tail];
	uint8_t next = rx_tail + 1;
	if (next >= USART_RX_BUFFER_SIZE) next = 0;
	rx_tail = next;
	return data;
}

uint8_t uart_is_available()
{
	return (rx_head != rx_tail);
}

void uart_print(const char *s)
{
	char c;
	for (; (c = *s); s++)
		uart_tx(c);
}

void uart_test()
{
	uart_init(9600);
	while (1)
	{
		RED_LED_TOGGLE();
		uart_print("hello\n");
		_delay_ms(1000);
	}
}
