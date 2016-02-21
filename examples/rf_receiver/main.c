/*
 * rf_receiver.c
 *
 * Created: 20.02.2016 13:42:04
 * Author : Christian Wagner
 */


#define F_CPU 1000000UL
#define _SAMPLES 4


#include <avr/io.h>
#include <avr/interrupt.h>

#include <rf_rx.h>


ISR(TIMER0_COMPA_vect)
{
    rf_rx_irq();
}

int main(void)
{
    DDRB &= !(1 << DDB3);	//RF Receiver
    PORTB |= (1 << PB3);	//Pull-up
    //LED
    DDRB |= (1 << DDB4)	;

    TCCR0A |= (1 << WGM01);		//CTC Mode
    TCCR0B |= (1 << CS00);		//No prescale
    TIMSK |= (1 << OCIE0A);		//Enable Compare Match A interrupt
    //1000 Hz = (F_CPU / 1000 / _SAMPLES_PER_BIT) - 1 -> 249
    OCR0A = (F_CPU / 1000.0 / _SAMPLES) - 1;
    OCR0A = 249;

    uint8_t data[120];

    sei();

    while (1)
    {
        rf_rx_start(data, sizeof(data), 4);
        rf_rx_wait();
    }
}