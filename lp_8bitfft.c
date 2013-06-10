/******************************************************************************
CREDITS:

The core fft code is a "fix_fft.c" which have been floating around on the web
for many years. The particular version that I eventually adopted has the following
footprints;

  Written by:  Tom Roberts  11/8/89
  Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
  Enhanced:  Dimitrios P. Bouras  14 Jun 2006 dbouras@ieee.org
  Modified for 8bit values David Keller  10.10.2010


Description:

This audio spectrum analyzer is a project for the TI Launchpad (Value Line) w/
CircuitCo's Educational BoosterPack. It is microphone based and require minimal
external components. Efforts were made to maximize the use of device / features from
the Educational BoosterPack.

ADC10, TimerA interrupt LPM wakeup, TimerA PWM like output, button use, integer arithmatic
are used and demonstrated.

Features:
 
	. 8 bit integer FFT
	. 32 samples at 250Hz separation
	. shows 16 amplitudes of 250Hz, 500Hz, 750Hz,....... 5.75Khz, 6.75Khz, 7.75Khz non-linear
	. partial logarithm map to show amplitudes, limited as resolution has been reduced for 8 bit FFT
	. LM358 two stage mic pre-amp at 100x 100x gain (can be improve vastly w/ better op-amps)
	. utilize Educational BoosterPack; mic for input, potentiometer for pre-amp biasing
	. draws power from launchpad
	. square signal generator from TA0.1 toggling, good for initial testing
	. TA0.1 ouput to P1.6 (io) or P2.6 (buzzer)
	. P1.3 button used to cycle thru 1. no ouput, 2. P1.6 signal, 3. P2.6 buzzer
	* in mode 2 and 3, both band and amplitude scales are linear
	* in mode 3, signals are distorted after passing buzzer and condensor mic, especially in low frequency


          TI LaunchPad + Educational BoosterPack
         ---------------
     /|\|            XIN|-
      | |               |
      --|RST        XOUT|-
        |               |
        |           P1.4|<-- ADC4
        |               |
        |           P1.6|--> TA0.1



   LM358 Dual Op-Amp, GBW @0.7Mhz, each stage @x100 gain, bandwidth is 7Khz

                     +------------------------------+
                    _|_                             |
                    ___ 10uF                        |
                   + |   ---------------            |
                     +-1|----.       Vcc|8          |
                     |. |    ^      .---|7--+-------|-----o (A)
                100k| | |   / \     ^   |   |.      |.
                    |_| |  /__+\   / \  |  | |100k | |1k
       0.1u          |  |   | |   /__+\ |  |_|     |_|
 (B) o--||--[ 1k  ]--+-2|---+ |    | |  |   |       |
 (C) o-------------+---3|-----+    +-|--|6--+-------+
                   |   4|Gnd         +--|5----+
                   |     ---------------      |
                   |                          |
                   +--------------------------+

  (A) to P1.4 EduBoost Mic jumper middle pin
  (B) to Condenser Mic, EduBoost Mic Jumper top pin 
  (C) to Potentiometer, EduBooster Potentiometer Jumper top pin
  (*) connect Gnd + Vcc to Launchpad
 

 Chris Chung June 2013
 . init release

 code provided as is, no warranty

 you cannot use code for commercial purpose w/o my permission
 nice if you give credit, mention my site if you adopt much of my code


******************************************************************************/

#include <msp430.h>
#include <stdint.h>
#include <stdlib.h>

#define EBLCD_CLK	BIT5	// P1
#define EBLCD_DATA	BIT7	// P1
#define EBLCD_RS	BIT3	// P2

//______________________________________________________________________
void eblcd_write(uint8_t d, uint8_t cmd) {
	if (cmd) P2OUT &= ~EBLCD_RS;
	else P2OUT |= EBLCD_RS;

	cmd = 8;
	while (cmd--) {
		if (d&0x80) P1OUT |= EBLCD_DATA;
		else P1OUT &= ~EBLCD_DATA;
		d <<= 1;
		P1OUT &= ~EBLCD_CLK;
		P1OUT |= EBLCD_CLK;
		P1OUT &= ~EBLCD_CLK;
	}//while
	__delay_cycles(50);
}

//______________________________________________________________________
void eblcd_setcg(uint8_t which, uint8_t *dp) {
	uint8_t i;
	which <<= 3;
	which |= 0x40;
	for (i=0; i<8; i++) {
		eblcd_write(which+i, 1);
		eblcd_write(*dp++, 0);
	}//for
}

//______________________________________________________________________
const char hex_map[] = "0123456789abcdef";
void eblcd_hex(uint8_t d) {
	eblcd_write(hex_map[d>>4], 0);
	eblcd_write(hex_map[d&0x0f], 0);
}


//______________________________________________________________________
volatile uint16_t play_at = 0;
volatile uint8_t ticks=0;

int16_t fix_fft(int8_t fr[], int8_t fi[], int16_t m, int16_t inverse);
//______________________________________________________________________
void main(void) {
	WDTCTL = WDTPW + WDTHOLD;		// Stop WDT
	BCSCTL1 = CALBC1_16MHZ;			// 16MHz clock
	DCOCTL = CALDCO_16MHZ;

	P1SEL = P2SEL = 0;
	P1DIR = P2DIR = 0;
	P1OUT = P2OUT = 0;

	//______________ lcd port use
	P1DIR |= BIT0|EBLCD_CLK|EBLCD_DATA;
	P2DIR |= EBLCD_RS;

	//______________ adc setting, use via microphone jumper on educational boost
	ADC10CTL0 = SREF_1 + ADC10SHT_2 + REFON + ADC10ON + ADC10IE;
	ADC10CTL1 = INCH_4;                       // input A4
	ADC10AE0 |= BIT4;                         // P1.4 ADC microphone

	uint8_t gen_tone=0;						// default, not tone generation

	P1OUT |= BIT3;			// tactile button pull-up
	P1REN |= BIT3;
	//______________ setup test tone signal via TA0.1
	TA0CCR0 = TA0CCR1 = 0;

	TA0CTL = TASSEL_2 + MC_2 + TAIE;	// smclk, continous mode
	TA0CCTL1 = OUTMOD_4 + CCIE;			// we want pin-toggle, 2 times slower
	TA0CCR1 = play_at;
	P1DIR |= BIT6;		// prepare both T0.1
	P2DIR |= BIT6;
	_BIS_SR(GIE); 						// now

	const uint8_t lcd_init[] = { 0x30, 0x30, 0x39, 0x14, 0x56, 0x6d, 0x70, 0x0c, 0x06, 0x01, 0x00, };
	const char hello0[] = "     MSP430     ";
	const char hello1[] = "  POWER PLAYERS ";

    eblcd_write(0x30, 1);
	__delay_cycles(500000);

	const uint8_t *cmd_ptr = lcd_init;
	while (*cmd_ptr) eblcd_write(*cmd_ptr++, 1);		// lcd init sequence
	__delay_cycles(50000);

	const uint8_t cgfonts[8][8] = {		// vertical bar fonts for graphing
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, },
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, },
		{ 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, },
		{ 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, },
		{ 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, },
		{ 0x00, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, },
		{ 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, },
	};

	uint8_t i=0;
	eblcd_write(0x38, 1);
	for (i=0;i<8;i++) eblcd_setcg(i, (uint8_t*) cgfonts[i]);	// load bar fonts
	eblcd_write(0x39, 1);
    eblcd_write(0x02, 1);
    eblcd_write(0x80, 1);

	cmd_ptr = (uint8_t*) hello0;
	while (*cmd_ptr) eblcd_write(*cmd_ptr++, 0);		// hello display
	eblcd_write(0x80|0x40, 1);
	cmd_ptr = (uint8_t*) hello1;
	while (*cmd_ptr) eblcd_write(*cmd_ptr++, 0);

	__delay_cycles(5000000);

#define log2FFT   5
#define FFT_SIZE  (1<<log2FFT)
#define Nx        (2 * FFT_SIZE)
#define log2N     (log2FFT + 1)
#define BAND_FREQ_KHZ	8


	const  uint8_t pick[16] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 18, 20, 23, 27, 31, };
	int8_t data[Nx], im[Nx];
	uint8_t plot[Nx/2] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, };
	uint8_t cnt=0, freq=0;
	while (1) {
		if (gen_tone) {
			if (!(++cnt&0x3f)) {
				cnt = 0;
				freq++;
				if (freq > 16) freq = 1;
				//____________ now play at 250Hz increments
				play_at = (16000/freq*2)-1;
				P1OUT ^= BIT0;
				__delay_cycles(100000);
				P1OUT ^= BIT0;
			}//if
		}//if

		TA0CCR0 = TA0R;
		TA0CCTL0 |= CCIE;
		for (i=0;i<Nx;i++) {

			// time delay between adc samples
			// this will become the band frequency after time - frequency conversion
			
			TA0CCR0 += (16000/(BAND_FREQ_KHZ*2))-1;	// begin counting for next period
			ADC10CTL0 |= ENC + ADC10SC;			// sampling and conversion start
			while (ADC10CTL1 & ADC10BUSY);		// stay and wait for it

			data[i] = (ADC10MEM>>2) - 128;		// signal leveling?
			//data[i] = ADC10MEM - 512;		// signal leveling?
			im[i] = 0;

			_BIS_SR(LPM0_bits + GIE);			// wake me up when timeup

		}//for
		TA0CCTL0 &= ~CCIE;

		fix_fft(data, im, log2N, 0);	// thank you, Tom Roberts(89),Malcolm Slaney(94),...

		//_______ logarithm scale mapping
		const uint16_t lvls[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 16, 22, 32, 45, 63, 89,  };

		for (i=0;i<FFT_SIZE;i++) {
			data[i] = sqrt(data[i]*data[i] + im[i]*im[i]);
			//
			if (gen_tone) {
				data[i] >>= 3;
			}//if
			else {
				uint8_t c=16;
				while (--c) {
					if (data[i] >= lvls[c]) {
						data[i] = c;
						break;
					}//if
				}//while
				if (!c) data[i] = 0;
			}//else
			if (data[i] > 16) data[i] = 15;
			if (data[i] < 0) data[i] = 0;
			if (data[i] > plot[i]) {
				plot[i] = data[i];
			}//if
			else {
				if (plot[i]) plot[i]--;
			}//else
			// debug use
			//eblcd_hex(data[i]>>8);
			//eblcd_hex(data[i]&0xff);
		}//for
		eblcd_write(0x02, 1);


		for (i=0;i<16;i++) {
			uint8_t idx = pick[i];
			if (gen_tone) idx = i;
			eblcd_write(0x80+i, 1);
			eblcd_write(plot[idx]>=8 ? plot[idx]-8 : ' ', 0);
			eblcd_write((0x80|0x40)+i, 1);
			eblcd_write(plot[idx]>7 ? 7 : plot[idx], 0);
		}//for

		if (gen_tone) {
			eblcd_write(0x02, 1);
			if (freq > 7)
				eblcd_write(0x80, 1);
			else
				eblcd_write(0x80|0x0e, 1);
			eblcd_write((freq/10)+'0', 0);
			eblcd_write((freq%10)+'0', 0);
		}//if
		if (!(P1IN&BIT3)) {
			while (!(P1IN&BIT3)) asm("nop");
			play_at = 0;
			P1SEL &= ~BIT6;
			P2SEL &= ~BIT6;
			gen_tone++;
			switch (gen_tone) {
				case 1:
					P1SEL |= BIT6;	// pin toggle on
					break;
				case 2:
					P2SEL |= BIT6;	// buzzer on
					break;
				default:
					gen_tone = 0;
					break;
			}//switch
		}//while
		//__delay_cycles(100000);		// personal taste
	}//while

} 
 
// ADC10 interrupt service routine
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void) {
	__bic_SR_register_on_exit(CPUOFF);
}

// Timer A0 interrupt service routine
//
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer0_A0 (void) {
	//P1OUT ^= BIT0;
	__bic_SR_register_on_exit(CPUOFF);
}
//
//
//________________________________________________________________________________
//interrupt(TIMERA1_VECTOR) Timer_A1(void) {
#pragma vector=TIMER0_A1_VECTOR
__interrupt void Timer0_A1 (void) {
	switch(TAIV) {
		case  2: 
			CCR1 += play_at;
			break;
		case 10: 
			if (ticks) ticks--;
			break;
	}//switch
}
//
