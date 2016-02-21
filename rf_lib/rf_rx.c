/*
 * rf_rx.c
 *
 * Created: 17.02.2016 23:04:57
 * Author : Christian Wagner
 */

#define LED_ON (PORTB |= (1 << PORTB4))
#define LED_SWITCH (PORTB ^= (1 << PORTB4))
#define LED_OFF (PORTB &= ~(1 << PORTB4))

#define F_CPU 1000000UL

#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>
#include <string.h>

#include "rf_rx.h"
#include "rf_man.h"

enum RX_State
{
    RX_PRE,
    RX_SYNC,
    RX_DATA_LEN,
    RX_DATA,
};


static bool volatile _receive = false;
static uint8_t _samples_min = 3;
static uint8_t _samples_max = 5;
static uint8_t* _buffer;
static uint8_t _buf_size;

//Everything after preamble is Manchester encoded except the EOT bits
//3 byte PREAMBLE | 2 byte len (big endian) | len byte message | EOT bits '01'
//PREAMBLE: 22 '1010...' bit followed by bits '01'  (0xAAAAA9)
//16 bit len, Manchester encoded
//n byte data, Manchester encoded
//end bit '01'
void rf_rx_irq()
{
    static uint8_t rx_count;
    static uint8_t rx_last;
    static uint8_t rx_sync_count;
    static enum RX_State rx_state = RX_PRE;
    static uint8_t rx_bits;
    static uint8_t rx_buf_idx;
    static uint8_t rx_len;

    if(!_receive)
        return;

    uint8_t rx_sample = PINB & (1 << PINB3);

    rx_count++;
    if(rx_sample == rx_last)
    {
        return;
    }
    //Transition occurred

    //Too short/long
    if(rx_count < _samples_min || rx_count > _samples_max*2)
    {
        rx_sync_count = 0;
        rx_state = RX_PRE;
    }
    else
    {
        //Valid signal
        bool rx_double_bit = rx_count >= _samples_min*2 && rx_count <= _samples_max*2;
        switch (rx_state)
        {
            case RX_PRE:
                if(rx_sample != 0)		//Transition 0 -> 1
                {
                    rx_state = RX_SYNC;
                    rx_sync_count = 0;
                }
                break;

            case RX_SYNC:
                rx_sync_count++;
                if(rx_double_bit)
                {
                    //If a double bit occurred too early or is '11'
                    if(rx_sync_count < 10 || rx_last != 0)
                    {
                        rx_state = RX_PRE;
                    }
                    else if(rx_sync_count >= 10)		//rx_last == 0, received '...001'
                    {
                        rx_state = RX_DATA_LEN;
                        _buffer[0] = _buffer[1] = rx_bits = 0;
                    }
                }
                break;

            case RX_DATA_LEN:
                rx_buf_idx = rx_bits >> 3;
                if(rx_double_bit)
                {
                    _buffer[rx_buf_idx] <<= 1;
                    if(rx_last)
                        _buffer[rx_buf_idx] |= 1;
                    rx_bits++;
                }

                if(rx_bits == 16)
                {
                    rx_state = RX_DATA;
                    rx_bits = 1;
                    rx_buf_idx = 0;
                    rx_len = rf_man_dec(_buffer);
                    _buffer[2] <<= 1;
                    if(rx_sample)
                        _buffer[2] |= 1;

//                     if(_rx_buf_size < rx_len)
//                         rx_len = _rx_buf_size;
                }
                else
                {
                    _buffer[rx_buf_idx] <<= 1;
                    if(rx_sample)
                        _buffer[rx_buf_idx] |= 1;
                    rx_bits++;
                    if(rx_bits == 16)
                    {
                        rx_state = RX_DATA;
                        rx_bits = 0;
                        rx_buf_idx = 0;
                        rx_len = rf_man_dec(_buffer);
//                         if(_rx_buf_size < rx_len)
//                             rx_len = _rx_buf_size;
                    }
                }
                break;

            case RX_DATA:

                if(rx_len == 5)
                    LED_OFF;
                if(rx_len == 10)
                    LED_ON;

                if(rx_double_bit)
                {
                    rx_bits++;
                    _buffer[rx_buf_idx] <<= 1;
                    if(rx_last)
                        _buffer[rx_buf_idx] |= 1;

                    if(rx_bits == 8)
                    {
                        rx_buf_idx++;
                        rx_bits = 0;
                        if(rx_buf_idx == rx_len)
                        {
                            rx_state = RX_PRE;
                            _receive = false;
                            break;
                        }
                    }
                }

                rx_bits++;
                _buffer[rx_buf_idx] <<= 1;
                if(rx_sample)
                    _buffer[rx_buf_idx] |= 1;
                if(rx_bits == 8)
                {
                    rx_buf_idx++;
                    rx_bits = 0;
                    if(rx_buf_idx == rx_len)
                    {
                        rx_state = RX_PRE;
                        _receive = false;
                    }
                }
                break;
        }
    }

    //Transition occurred, reset counter and store last sample
    rx_count = 0;
    rx_last = rx_sample;
}



void rf_rx_start(uint8_t* buffer, uint8_t size, uint8_t samples)
{
    _samples_min = samples - 1;		//TODO
    _samples_max = samples + 1;
    _buffer = buffer;
    _buf_size = size;
    _receive = true;
    memset(buffer, 0, size);
}

bool rf_rx_done()
{
    return !_receive;
}

void rf_rx_wait()
{
    while(_receive);
}