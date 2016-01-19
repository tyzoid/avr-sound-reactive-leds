/* This is AVR code for driving the RGB LED strips from Pololu.
	 It allows complete control over the color of an arbitrary number of LEDs.
	 This implementation disables interrupts while it does bit-banging with inline assembly.
 */

/* This line specifies the frequency your AVR is running at.
	 This code supports 20 MHz, 16 MHz and 8MHz */
#define F_CPU 8000000

// These lines specify what pin the LED strip is on.
// You will either need to attach the LED strip's data line to PC0 or change these
// lines to specify a different pin.
#define LED_STRIP_PORT PORTB
#define LED_STRIP_DDR  DDRB
#define LED_STRIP_PIN  4

#define ANALOG_INPUT_PIN 3

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define min(x,y) ((x<y)?x:y)
#define max(x,y) ((x>y)?x:y)

/**
 * The rgb_color struct represents the color for an 8-bit RGB LED.
 * Examples:
 *     Black:      (rgb_color){ 0, 0, 0 }
 *     Pure red:   (rgb_color){ 255, 0, 0 }
 *     Pure green: (rgb_color){ 0, 255, 0 }
 *     Pure blue:  (rgb_color){ 0, 0, 255 }
 *     White:      (rgb_color){ 255, 255, 255}
 */
typedef struct rgb_color
{
  unsigned char red, green, blue;
} rgb_color;

/**
 * led_strip_write sends a series of colors to the LED strip, updating the LEDs.
 * The colors parameter should point to an array of rgb_color structs that hold the colors to send.
 * The count parameter is the number of colors to send.
 *
 * This function takes about 1.1 ms to update 30 LEDs.
 * Interrupts must be disabled during that time, so any interrupt-based library
 * can be negatively affected by this function.
 *
 * Timing details at 20 MHz (the numbers slightly different at 16 MHz and 8MHz):
 *     0 pulse  = 400 ns
 *     1 pulse  = 850 ns
 *     "period" = 1300 ns
 */
void __attribute__((noinline)) led_strip_write(rgb_color * colors, unsigned int count)
{
	// Set the pin to be an output driving low.
	LED_STRIP_PORT &= ~(1<<LED_STRIP_PIN);
	LED_STRIP_DDR |= (1<<LED_STRIP_PIN);

	// Disable interrupts temporarily because we don't want our pulse timing to be messed up.
	cli();
	while(count--)
	{
		// Send a color to the LED strip.
		// The assembly below also increments the 'colors' pointer,
		// it will be pointing to the next color at the end of this loop.
		asm volatile(
				"ld __tmp_reg__, %a0+\n"
				"ld __tmp_reg__, %a0\n"
				"rcall send_led_strip_byte%=\n" // Send red component.
				"ld __tmp_reg__, -%a0\n"
				"rcall send_led_strip_byte%=\n" // Send green component.
				"ld __tmp_reg__, %a0+\n"
				"ld __tmp_reg__, %a0+\n"
				"ld __tmp_reg__, %a0+\n"
				"rcall send_led_strip_byte%=\n" // Send blue component.
				"rjmp led_strip_asm_end%=\n"    // Jump past the assembly subroutines.

				// send_led_strip_byte subroutine:  Sends a byte to the LED strip.
				"send_led_strip_byte%=:\n"
				"rcall send_led_strip_bit%=\n"  // Send most-significant bit (bit 7).
				"rcall send_led_strip_bit%=\n"
				"rcall send_led_strip_bit%=\n"
				"rcall send_led_strip_bit%=\n"
				"rcall send_led_strip_bit%=\n"
				"rcall send_led_strip_bit%=\n"
				"rcall send_led_strip_bit%=\n"
				"rcall send_led_strip_bit%=\n"  // Send least-significant bit (bit 0).
				"ret\n"

				// send_led_strip_bit subroutine:  Sends single bit to the LED strip by driving the data line
				// high for some time.  The amount of time the line is high depends on whether the bit is 0 or 1,
				// but this function always takes the same time (2 us).
				"send_led_strip_bit%=:\n"
#if F_CPU == 8000000
				"rol __tmp_reg__\n"             // Rotate left through carry.
#endif
				"sbi %2, %3\n"                           // Drive the line high.

#if F_CPU != 8000000
				"rol __tmp_reg__\n"                      // Rotate left through carry.
#endif

#if F_CPU == 16000000
				"nop\n" "nop\n"
#elif F_CPU == 20000000
				"nop\n" "nop\n" "nop\n" "nop\n"
#elif F_CPU != 8000000
#error "Unsupported F_CPU"
#endif

				"brcs .+2\n" "cbi %2, %3\n"              // If the bit to send is 0, drive the line low now.

#if F_CPU == 8000000
				"nop\n" "nop\n"
#elif F_CPU == 16000000
				"nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
#elif F_CPU == 20000000
				"nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
				"nop\n" "nop\n"
#endif

				"brcc .+2\n" "cbi %2, %3\n"              // If the bit to send is 1, drive the line low now.

				"ret\n"
				"led_strip_asm_end%=: "
				: "=b" (colors)
				: "0" (colors),         // %a0 points to the next color to display
					"I" (_SFR_IO_ADDR(LED_STRIP_PORT)),   // %2 is the port register (e.g. PORTC)
					"I" (LED_STRIP_PIN)     // %3 is the pin number (0-8)
	 );

		// Uncomment the line below to temporarily enable interrupts between each color.
		//sei(); asm volatile("nop\n"); cli();
	}
	sei();          // Re-enable interrupts now that we are done.
	_delay_us(50);  // Hold the line low for 15 microseconds to send the reset signal.
}

#define LED_BRIGHTNESS 1
#define LED_COUNT 160
#define MS_DELAY 20
#define MID 660
rgb_color colors[LED_COUNT];

int main()
{
	ADMUX  = 0x03;
	ADCSRA = 0x86;

	//uint32_t time = 0;
	uint16_t aread = 0;
	uint8_t j = 0;
	uint16_t i;

	//uint16_t avg;
	//uint16_t avgcount;

	_delay_ms(4000);

	while(1)
	{
		for(i = 0; i < LED_COUNT; i++)
		{
			for(j = 0; j < 3; j++)
			{
				ADCSRA |= 0x40;
				while (ADCSRA & 0x40);
				aread = ADCL | (ADCH << 8);

				/*uint32_t tmp = (uint32_t) avg * avgcount;
				tmp += aread;

				if (avgcount < 0x00ff) avgcount++;
				tmp /= avgcount;

				avg = (uint16_t) tmp;

				if (aread < avg) aread = avg - aread;
				else aread = aread - avg;

				aread = aread >> 1;
*/
				if (aread < MID) aread = MID - aread;
				else aread = aread - MID;

				aread = min(aread >> 1, 0x00ff);

				aread = (aread * aread) / 0x00ff;

				if (j == 0) colors[i].red = aread;
				else if (j == 1) colors[i].green = aread;
				else if (j == 2) colors[i].blue = aread;
			}
		}

		led_strip_write(colors, LED_COUNT);

		_delay_ms(MS_DELAY);
		//time += MS_SPEED;
	}
}
