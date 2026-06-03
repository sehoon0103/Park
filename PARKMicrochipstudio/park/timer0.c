#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

volatile uint32_t timer0_overflow_count = 0;
volatile uint32_t timer0_millis = 0;
static uint8_t timer0_fract = 0;

// 타이머0 오버플로우 시간 = 256틱 × (64/16000000) = 1.024ms
#define MICROS_PER_TIMER0_OVERFLOW (64 * 256 / (F_CPU / 1000000UL))
#define MILLIS_INC  (MICROS_PER_TIMER0_OVERFLOW / 1000)         // 1
#define FRACT_INC   ((MICROS_PER_TIMER0_OVERFLOW % 1000) >> 3)  // 3
#define FRACT_MAX   (1000 >> 3)                                  // 125

ISR(TIMER0_OVF_vect)
{
	uint32_t m = timer0_millis;
	uint8_t f = timer0_fract;

	m += MILLIS_INC;
	f += FRACT_INC;

	// 1.024ms overflow를 1ms로 근사하는 오차를 분수 누적으로 보정
	if (f >= FRACT_MAX)
	{
		f -= FRACT_MAX;
		m += 1;
	}

	timer0_fract = f;
	timer0_millis = m;
	timer0_overflow_count++;
}

void timer0_init(void)
{
	cli();

	TCCR0A = 0;
	// prescaler 64: 16MHz → 타이머 클럭 250kHz (4μs/tick)
	TCCR0B = (1 << CS01) | (1 << CS00);
	TIMSK0 |= (1 << TOIE0);

	timer0_overflow_count = 0;
	timer0_millis = 0;
	timer0_fract = 0;

	sei();
}

uint32_t millis(void)
{
	uint32_t m;
	// ISR과 공유하는 4바이트 변수 — 원자적으로 읽어야 함
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		m = timer0_millis;
	}
	return m;
}

uint32_t micros(void)
{
	uint32_t m;
	uint8_t t;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		m = timer0_overflow_count;
		t = TCNT0;
		// overflow 발생 직후 ISR이 아직 처리되지 않은 경우 수동 보정
		if ((TIFR0 & (1 << TOV0)) && (t < 255))
		{
			m++;
		}
	}
	// (overflow횟수 × 256 + 현재 카운터) × 4μs/tick
	return ((m << 8) + t) * (64 / (F_CPU / 1000000UL));
}

void delay(uint32_t ms)
{
	uint32_t start = millis();
	while ((millis() - start) < ms) {}
}
