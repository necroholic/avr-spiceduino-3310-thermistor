/**************************************************************************
* AVR temp sensor - display temp themometer on nokia LCD
*
* copyright 2008 Michael Spiceland (mikeNOSPAM@fuzzymonkey.org)
**************************************************************************/
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "3310_routines.h"

/* defines */
#define FCPU		1000000 // CPU freq
#define F_CPU FCPU
#define BAUD_RATE	9600 // desired baud rate
#define UBRR_DATA	(FCPU/(BAUD_RATE)-1)/16 // sets baud rate
#define ALL_INPUT	0x00 // 0000 0000
#define ALL_OUTPUT	0xFF // 1111 1111

/* globals */
double temperature;

inline void beep(void)
{
	PORTD |= (1 << 7); // start beep
	_delay_loop_2(10000);
	PORTD &= ~(1 << 7); // clear beep
}

inline void send_serial(uint8_t byte)
{
	UDR = byte;
	while (!(UCSRA & (1 << TXC)));
	UCSRA |= (1 << TXC);
}

/**************************************************************************
Function: init_serial()
Purpose: initialilze serial port by enabling it and setting baudrate
Input: none
Returns: none
**************************************************************************/
void init_serial(void)
{
	UCSRA |= _BV(U2X);
	UCSRB = _BV(RXEN) | _BV(TXEN);
	UBRRL = UBRR_DATA * 2;
}

/***************************************************************************
* double2string
* convert a double to a string and place it in a pre-allocated space
***************************************************************************/
inline void double2string (double actualTemp, uint8_t* string)
{
	int temp;

	/* prep the string */
	string[2] = '.';
	string[4] = '\0';

	temp = (int16_t)(actualTemp * 10.0); // to include decimal point for display
	if (0.5 <= (actualTemp * 10.0 - temp))
	{
		temp = temp + 1;
	}
	if (0 > temp)
	{
	  temp *= -1;
	}

	string[3] = ((uint8_t)(temp % 10)) | 0x30;
	temp = temp / 10;

	string[1] = ((uint8_t)(temp % 10)) | 0x30;
	temp = temp / 10;

	string[0] = ((uint8_t)(temp % 10)) | 0x30;
	temp = temp / 10;
}

/*************************************************************************
* getTempF - return the temp in Farenheight from 
*            Vishay thermistor NTCLE100E3103JB0
* v10bit - 10 bit value read from the A/D
* pdRes - value (in ohms) of the resistor that is in series with thermistor
*************************************************************************/
double getTempF(double v10bit, double pdRes)
{
	if (1024 == v10bit)
	{
		return -1;
	}

	/* with therm on the top */
	double thermResistance = (pdRes * (1.0 - v10bit / 1024.0))
			/ (v10bit / 1024.0);
	double thermRefResistance = 10000.0;

	/* Steinhart and Hart constants for Vishay thermistor NTCLE100E3103JB0 */
	double a = 3.354016 * pow(10, -3);
	double b = 2.56985 * pow(10, -4);
	double c = 2.620131 * pow(10, -6);
	double d = 6.383091 * pow(10, -8);

	double celcius = 1.0 / (
			a + b * log(thermResistance / thermRefResistance)
			+ c * pow(log(thermResistance / thermRefResistance), 2)
			+ d * pow(log(thermResistance / thermRefResistance), 3)
		) - 272.15;

	double farenheit = 9.0 / 5.0 * celcius + 32.0;

	return farenheit;
}

SIGNAL(SIG_UART_RECV)
{
	uint8_t data;
	data = UDR;
}

SIGNAL(SIG_OVERFLOW0)
{
	static uint8_t count = 0;
	uint8_t tmp_string[] = "xx.x";
	uint8_t i;

	count++;

	if (count % 4)
	{
		return;
	}

	/* send our latest reading out the serial port */
	double2string(temperature, tmp_string);
	i = 0;
	while (tmp_string[i])
	{
		send_serial(tmp_string[i]);
		i++;
	}
	send_serial('\r');
	send_serial('\n');
}

/* Empty on purpose for now */
SIGNAL(SIG_OVERFLOW2)
{
}

int main(void)
{
	uint8_t tmp_string[] = "xx.x";
	uint8_t i, j;

	/* initialize port data directions */
	DDRB = ALL_OUTPUT;
	PORTB = 0xFF;
	DDRD |= (1 << 7); // buzzer
	DDRC &= ~(0x01); // temp sensor on AN2

	/* initialize the ADC */
	ADCSRA |= _BV(ADEN); // for now we don't do this in the ISR | _BV(ADIE);
	ADCSRA |= _BV(ADPS2) | _BV(ADPS1); // clk is / 64

	/* 8-bit timer for waking up */
	TCCR2 |= _BV(CS22) | _BV(CS20) | _BV(CS21); // CLK / 1025 == every 3.8s
	TCNT2 = 0; // reset the timer
	TIMSK |= _BV(TOIE2); // enable timer/counter1 overflow interrupt

	/* set up */
	spi_init();
	init_serial();

	/* get LCD ready */
	LCD_init();
	_delay_loop_2(65535);
	LCD_clear();
	LCD_drawSplash();
	beep();

	sei();
	while(1)
	{
		cli();
		ADCSRA |= _BV(ADEN); // turn on ADC
		ADCSRA |= (1 << ADSC); // start ADC conversion
		while (ADCSRA & (1 << ADSC)); // wait for the result to be available
		double current_temp = (uint16_t)(ADCL | (ADCH << 8));
		temperature = getTempF(current_temp, 10100);

		double2string(temperature, tmp_string);

		LCD_drawSplashNoUpdate();
		LCD_gotoXY(0, 1);
		LCD_writeString_megaFont(tmp_string);

		for (i = 0; i < 40; i++)
		{
			if (((temperature - 60) * 2) > i) // only 60 through 80 degrees
			{
				for (j = 0; j < 5; j++)
				{
					LCD_setPixelNoUpdate(63 + j,48 - i);
				}
				/* handle the bulb at the bottom */
				if ((7 > i) && (2 < i))
				{
					for (j = 0; j < 11; j++)
					{
						LCD_setPixelNoUpdate(60 + j,48 - i);
					}
				}
				else if ((9 > i) && (2 < i))
				{
					for (j = 0; j < 9; j++)
					{
						LCD_setPixelNoUpdate(61 + j,48 - i);
					}
				}
				else if (10 > i)
				{
					for (j = 0; j < 7; j++)
						LCD_setPixelNoUpdate(62 + j,48 - i);
				}
			}
			else
			{
				for (j = 0; j < 5; j++)
				{
					LCD_clearPixelNoUpdate(63 + j,48 - i);
				}
			}
		}
		LCD_update();

		/* save power */
		ADCSRA &= ~_BV(ADEN); // turn off ADC
		set_sleep_mode(SLEEP_MODE_IDLE);
		TCNT2 = 0; // reset the timer
		sei();
		sleep_mode();
	}
}

