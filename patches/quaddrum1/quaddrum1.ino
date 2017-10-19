#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

// sound_data & sound_length
#include "sample.h"

#define LED_PIN     13
#define SPEAKER_PIN 11  // This is fixed on the Grains to 11 

#define KNOB_1  (2)   // coupled with IN_1, sample start (percentage of whole sample)
#define KNOB_2  (1)   // coupled with IN_2, loop size    (percentage of whole sample)
#define KNOB_3  (0)   // pitch
#define INPUT_3 (3)   // gate trigger

volatile uint16_t sample;
int grid[4] = {0, 4000, 8000, 12000};
volatile uint16_t loop_pos;
volatile uint16_t loop_start;
volatile uint16_t loop_length;
volatile uint16_t index_bounds;
volatile uint16_t loop_overflow;

volatile boolean gate;
volatile boolean gate_prev;

byte lastSample;

void startPlayback()
{
    pinMode(SPEAKER_PIN, OUTPUT);

    // Set up Timer 2 to do pulse width modulation on the speaker pin.
    // Use internal clock (datasheet p.160)
    ASSR &= ~(_BV(EXCLK) | _BV(AS2));

    // Set fast PWM mode  (p.157)
    TCCR2A |= _BV(WGM21) | _BV(WGM20);
    TCCR2B &= ~_BV(WGM22);

    // Do non-inverting PWM on pin OC2A (p.155)
    // On the Arduino this is pin 11.
    TCCR2A = (TCCR2A | _BV(COM2A1)) & ~_BV(COM2A0);
    TCCR2A &= ~(_BV(COM2B1) | _BV(COM2B0));
    // No prescaler (p.158)
    TCCR2B = (TCCR2B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

    // Set initial pulse width to the first sample.
    OCR2A = pgm_read_byte(&sound_data[0]);

    // Set up Timer 1 to send a sample every interrupt.
    cli();

    // Set CTC mode (Clear Timer on Compare Match) (p.133)
    // Have to set OCR1A *after*, otherwise it gets reset to 0!
    TCCR1B = (TCCR1B & ~_BV(WGM13)) | _BV(WGM12);
    TCCR1A = TCCR1A & ~(_BV(WGM11) | _BV(WGM10));

    // No prescaler (p.134)
    TCCR1B = (TCCR1B & ~(_BV(CS12) | _BV(CS11))) | _BV(CS10);

    // Set the compare register (OCR1A).
    // OCR1A is a 16-bit register, so we have to do this with
    // interrupts disabled to be safe.
    OCR1A = F_CPU / SAMPLE_RATE; // 16e6 / 8000 = 2000

    // Enable interrupt when TCNT1 == OCR1A (p.136)
    TIMSK1 |= _BV(OCIE1A);

    lastSample = pgm_read_byte(&sound_data[sound_length - 1]);
    sample = 0;
    sei();
}

void stopPlayback()
{
    TIMSK1 &= ~_BV(OCIE1A); // Disable playback per-sample interrupt.
    TCCR1B &= ~_BV(CS10);   // Disable the per-sample timer completely.
    TCCR2B &= ~_BV(CS10);   // Disable the PWM timer.
    digitalWrite(SPEAKER_PIN, LOW);
}

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);
    randomSeed(analogRead(0));
        //debug
    Serial.begin(9600);

    startPlayback();

    loop_pos = 0;
    loop_start  = 0; 
    loop_length = sound_length;
    gate = false;
    gate_prev = false;
}

// This is called at 8000 Hz to load the next sample.
ISR(TIMER1_COMPA_vect)
{
    if(sample >= index_bounds)
    {
        sample = loop_start;
    }
    else if((sample < loop_start) &&
            (sample >= loop_overflow))
    {
        sample = loop_start;
    }
    else if((gate == true) &&
            (gate_prev == false))
    {
        sample = loop_start;
    }
    else
    {
        OCR2A = pgm_read_byte(&sound_data[sample % sound_length]);
    }
    gate_prev = gate;
    sample++;
}

void loop()
{
    int loop_pos = analogRead(KNOB_1);
    
    if(loop_pos <= 100)
    {
      loop_start = 0;     
    }
    else if(loop_pos > 100 && loop_pos <= 293)
    {
      loop_start = 4000;
    }
    else if(loop_pos > 293 && loop_pos <= 508)
    {
      loop_start = 8000;
    }
    else
    {
      loop_start = 12000;
    }


    //loop_start = analogRead(KNOB_1) / 1024.0 * sound_length;
    loop_length = (analogRead(KNOB_2) + 1) / 1024.0 * sound_length;
    OCR1A = (512.0 / (analogRead(KNOB_3) + 1)) * (F_CPU / SAMPLE_RATE);
    Serial.println(loop_start);

    gate = analogRead(3) >> 9;  // 10 bits in. gate < 512 == off, gate >= 512 == on

//  can be up to 2x sound length. the more you know.
    index_bounds = loop_start + loop_length;
//  this will set the overflow length. take the loop overflow into account when checking the loop boundaries
    if(index_bounds > sound_length)
    {
        loop_overflow = index_bounds - sound_length;
    }
    else
    {
        loop_overflow = 0;
    }
}
