/********************************************************************/
/* Project: Ultrasonice based Distance Meter with LCD Display       */
/* Microcontroller: MSP430FR6989 on MSP-EXP430FR6989 Launchpad      */
/* Ultrasonic Ranging Module: HC-SR04                               */
/* 16x2 LCD Display: 1602K27-00                                     */
/********************************************************************/
/********************************************************************/
/* uC and Ultrasonic sensor Connections
    P1.4 - Trigger
    P1.5 - Echo     */
/********************************************************************/
/* uC and LCD Connections
    TP1 - Vdd (+5v)
    TP3 - Vss (Gnd)
    P2.2 - EN
    P2.3 - RS
    P2.4 - D4
    P2.5 - D5
    P2.6 - D6
    P2.7 - D7
    Gnd  - RW
    Gnd  - V0 - Connect to Gnd through a 1K Resistor
            - this value determines contrast -
            - i.e. without resistor all dots always visible, whereas
            - higher resistor means dots not at all displayed.
    Gnd  - K (LED-)
    Vcc  - A (LED+) +5V - For Backlight
    Clock: 1MHz                                                     */
/********************************************************************/

#include <msp430fr6989.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// uC GPIO Port assignment

// Peripheral pin assignments
#define US_TRIG         BIT4
#define US_ECHO         BIT5
#define LCD_EN          BIT2
#define LCD_RS          BIT3
#define LCD_DATA        BIT4 | BIT5 | BIT6 | BIT7
#define LCD_D0_OFFSET   4   // D0 at BIT4, so it is 4
#define US_MASK         US_TRIG | US_ECHO
#define LCD_MASK        LCD_EN | LCD_RS | LCD_DATA

unsigned int up_counter = 0;
unsigned int distance_cm = 0;



/* Timer A0 Capture Interrupt routine
 P1.1 (echo) causes this routine to be called */
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TimerA0(void)
{
    if (TA0CCTL0 & CCI)            // Raising edge
    {
        up_counter = TA0CCR0;      // Copy counter to variable
    }
    else                        // Falling edge
    {
        // Formula: Distance in cm = (Time in uSec)/58
        distance_cm = (TA0CCR0 - up_counter)/58;
    }
    TA0CCTL0 &= ~CCIFG;
    TA0CTL &= ~TAIFG;           // Clear interrupt flag - handled
}

void timer_init()
{
    /* Timer A0 configure to read echo signal:
    Timer A Capture/Compare Control 0 =>
    capture mode: 1 - both edges +
    capture sychronize +
    capture input select 0 => P1.5 +
    capture mode +
    capture compare interrupt enable */
    TA0CCTL0 |= CM_3 + SCS + CCIS_0 + CAP + CCIE;

    /* Timer A Control configuration =>
    Timer A clock source select: 1 - SMClock +
    Timer A mode control: 2 - Continous up +
    Timer A clock input divider 0 - No divider */
    TA0CTL |= TASSEL_2 + MC_2 + ID_0;

    // Global Interrupt Enable
    _BIS_SR(GIE);

}

void lcd_reset()
{
    P2OUT = 0x00;
    __delay_cycles(20000);

    P2OUT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
    P2OUT &= ~LCD_EN;
    __delay_cycles(10000);

    P2OUT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
    P2OUT &= ~LCD_EN;
    __delay_cycles(1000);

    P2OUT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
    P2OUT &= ~LCD_EN;
    __delay_cycles(1000);

    P2OUT = (0x02 << LCD_D0_OFFSET) | LCD_EN;
    P2OUT &= ~LCD_EN;
    __delay_cycles(1000);

}

void lcd_cmd (char cmd)
{
    // Send upper nibble
    P2OUT = (((cmd >> 4) & 0x0F) << LCD_D0_OFFSET) | LCD_EN;
    P2OUT &= ~LCD_EN;

    // Send lower nibble
    P2OUT = ((cmd & 0x0F) << LCD_D0_OFFSET) | LCD_EN;
    P2OUT &= ~LCD_EN;

    __delay_cycles(4000);
}

void lcd_data (unsigned char dat)
{
    // Send upper nibble
    P2OUT = ((((dat >> 4) & 0x0F) << LCD_D0_OFFSET) | LCD_EN | LCD_RS);
    P2OUT &= ~LCD_EN;

    // Send lower nibble
    P2OUT = (((dat & 0x0F) << LCD_D0_OFFSET) | LCD_EN | LCD_RS);
    P2OUT &= ~LCD_EN;

    __delay_cycles(4000); // a small delay may result in missing char display
}

void lcd_init ()
{
    lcd_reset();         // Call LCD reset

    lcd_cmd(0x28);       // 4-bit mode - 2 line - 5x7 font.
    lcd_cmd(0x0C);       // Display no cursor - no blink.
    lcd_cmd(0x06);       // Automatic Increment - No Display shift.
    lcd_cmd(0x80);       // Address DDRAM with 0 offset 80h.
    lcd_cmd(0x01);       // Clear screen

}

void display_line(char *line)
{
    while (*line)
        lcd_data(*line++);
}

void display_distance(char *line, int len)
{
    while (len--)
        if (*line)
            lcd_data(*line++);
        else
            lcd_data(' ');
}
int main()
{
    char distance_string[10];
    WDTCTL = WDTPW | WDTHOLD;       // Stop Watch Dog Timer
    PM5CTL0 &=  ~LOCKLPM5;
    P2DIR = LCD_MASK;         // Output direction for LCD connections
    P1DIR |= US_TRIG;         // Output direction for trigger to sensor
    P1DIR &= ~US_ECHO;        // Input direction for echo from sensor
    P1OUT &= ~US_TRIG;            // keep trigger at low
    P1SEL1 |= US_ECHO;                // set US_ECHO as trigger for Timer from Port-1
    P1SEL0 |= US_ECHO;

    // Initialize LCD
    lcd_init();

    // Initialize timer for Ultrasonice distance sensing
    timer_init();

    lcd_cmd(0x80); // select 1st line (0x80 + addr) - here addr = 0x00
    display_line(" Distance");
    lcd_cmd(0xce); // select 2nd line (0x80 + addr) - here addr = 0x4e
    display_line("cm");

    while (1)
    {
        // measuring the distance
        P1OUT |= US_TRIG;                 // assert
        __delay_cycles(10);                 // 10us wide
        P1OUT &= ~US_TRIG;                 // deassert
        //__delay_cycles(60000);            // 60ms measurement cycle
        __delay_cycles(500000);             // 0.5sec measurement cycle

        // displaying the current distance
        snprintf(distance_string, 10, "%d", distance_cm);
        lcd_cmd(0xcb); // select 2nd line (0x80 + addr) - here addr = 0x4b
        display_distance(distance_string, 3);
    }

}
