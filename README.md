lp_8bitfft
==========

TI Launchpad 8 bit FFT Spectrum Analyzer

Description:

This audio spectrum analyzer is a project for the TI Launchpad (Value Line) w/
CircuitCo's Educational BoosterPack. It is microphone based and require minimal
external components. Efforts were made to maximize the use of device / features from
the Educational BoosterPack.

ADC10, TimerA interrupt LPM wakeup, TimerA PWM like output, button use, integer arithmatic
are used and demonstrated.

Features:
 
	. 8 bit integer FFT
	. 32 samples at 500Hz separation
	. shows 16 amplitudes of 250Hz, 500Hz, 750Hz,....... 5.75Khz, 6.75Khz, 7.75Khz non-linear
	. partial logarithm map to show amplitudes, limited as resolution has been reduced for 8 bit FFT
	. LM358 two stage mic pre-amp at 100x 100x gain (can be improve vastly w/ better op-amps)
	. utilize Educational BoosterPack; mic for input, potentiometer for pre-amp biasing
	. draws power from launchpad
	. square signal generator from TA0.1 toggling, good for initial testing


          TI LaunchPad + Educational BoosterPack
         ---------------
     /|\|            XIN|-
      | |               |
      --|RST        XOUT|-
        |               |
        |           P1.4|<-- ADC4
        |               |
        |           P1.6|--> TA0.1


	. LM358 Dual Op-Amp, GBW @0.7Mhz, each stage @x100 gain, bandwidth is 7Khz

                     +------------------------------+
				    _|-                             |
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
  (+) connect Gnd + Vcc to Launchpad
 

 Chris Chung June 2013
 . init release

 code provided as is, no warranty

 you cannot use code for commercial purpose w/o my permission
 nice if you give credit, mention my site if you adopt much of my code

