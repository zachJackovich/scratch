#include <msp430fr6989.h>

#define true 1
#define false 0

// uC GPIO Port assignment
#define UC_PORT     P2OUT
#define UC_PORT_DIR P2DIR

// Peripheral pin assignments
#define LCD_RS          BIT0        // RS at P2.0
#define LCD_EN          BIT1        // EN at P2.1
#define US_ECHO         BIT2        // Echo at P2.2
#define US_TRIG         BIT3        // Trigger at P2.3
#define LCD_DATA        BIT4 | BIT5 | BIT6 | BIT7
#define LCD_D0_OFFSET   4   // D0 at BIT4, so it is 4
#define US_MASK         US_TRIG | US_ECHO
#define LCD_MASK        LCD_RS | LCD_EN | LCD_DATA

#define redLED BIT0         // Red LED at P1.0
#define greenLED BIT7       // Green LED at P9.7

#define BUT1 BIT1           // Button S1 at Port 3.1
#define BUT2 BIT2           // Button S2 at Port 3.2
#define BUT3 BIT3           // Button S3 at Port 3.3


unsigned int up_counter = 0;
unsigned int distance_cm = 0;
unsigned int distance_in = 0;
int but = 0;
int play = 0;
int unit = 0;

void config_ACLK_to_32KHz_crystal();
void timer_init();
void lcd_reset();
void lcd_cmd(char cmd);
void lcd_data(unsigned char dat);
void lcd_init();
void display_line(char *line);
void display_distance(char *line, int len);
char* itoa(int num, char* str, int base);

int main()
{
    char distance_string[4];

    WDTCTL = WDTPW + WDTHOLD;       // Stop Watch Dog Timer
    PM5CTL0 &= ~LOCKLPM5;           // Enable the GPIO pins
    UC_PORT_DIR = LCD_MASK;         // Output direction for LCD connections
    UC_PORT_DIR |= US_TRIG;         // Output direction for trigger to sensor
    UC_PORT_DIR &= ~US_ECHO;        // Input direction for echo from sensor
    UC_PORT &= ~US_TRIG;            // keep trigger at low
    P2SEL0 &= ~US_ECHO;                // set US_ECHO as trigger for Timer from Port-2
    P2SEL1 |= US_ECHO;

    P1DIR |= redLED; // Direct pin as output
    P9DIR |= greenLED; // Direct pin as output
    P1OUT &= ~redLED; // Turn LED Off
    P9OUT &= ~greenLED; // Turn LED On

    // Configuring buttons with interrupt
   P3DIR &= ~(BUT1|BUT2|BUT3); // 0: input
   P3REN |= (BUT1|BUT2|BUT3); // 1: enable built-in resistors
   P3OUT |= (BUT1|BUT2|BUT3); // 1: built-in resistor is pulled up to Vcc
   P3IE |= (BUT1|BUT2|BUT3); // 1: enable interrupts
   P3IES |= (BUT1|BUT2|BUT3); // 1: interrupt on falling edge
   P3IFG &= ~(BUT1|BUT2|BUT3); // 0: clear the interrupt flags

   TA0CCR0 = 11554-1; // @ 16 KHz --> 3 seconds

    // Initialize LCD
    lcd_init();

    // Initialize timer for Ultrasonice distance sensing
    timer_init();

    lcd_cmd(0x80); // select 1st line (0x80 + addr) - here addr = 0x00
    display_line(" Distance Meter");
    lcd_cmd(0xce); // select 2nd line (0x80 + addr) - here addr = 0x4e
    display_line("cm");

    while (1)
    {
        if ((play) != 0)
        {
            // measuring the distance
            UC_PORT ^= US_TRIG;                 // assert
            __delay_cycles(10);                 // 10us wide
            UC_PORT ^= US_TRIG;                 // deassert
            __delay_cycles(500000);             // 0.5sec measurement cycle

        }
        if (unit == 0)
        {
            lcd_cmd(0xce); // select 2nd line (0x80 + addr) - here addr = 0x4e
            display_line("cm");
        }
        else
        {
            lcd_cmd(0xce); // select 2nd line (0x80 + addr) - here addr = 0x4e
            display_line("in");
        }

        // displaying the current distance
        lcd_cmd(0xca); // select 2nd line (0x80 + addr) - here addr = 0x4b
        display_distance( itoa((unit ? distance_in: distance_cm), distance_string, 10), 3);
    }


}

// Configures ACLK to 32 KHz crystal
void config_ACLK_to_32KHz_crystal()
{
    // By default, ACLK runs on LFMODCLK at 5MHz/128 = 39 KHz
    // Reroute pins to LFXIN/LFXOUT functionality
    PJSEL1 &= ~BIT4;
    PJSEL0 |= BIT4;

    // Wait until the oscillator fault flags remain cleared
    CSCTL0 = CSKEY;             // Unlock CS registers
    do
    {
        CSCTL5 &= ~LFXTOFFG;        // Local fault flag
        SFRIFG1 &= ~OFIFG;          // Global fault flag
    }
    while((CSCTL5 & LFXTOFFG) != 0);

    CSCTL0_H = 0;               // Lock CS registers
    return;
}

// ISR of Channel 0 (A0 vector)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void T0A0_ISR()
{
    if (but == 1)
    {
        TA0CCTL0 &= ~CCIE;
        P3IFG &= ~BUT1;
        P3IE |= BUT1;
    }
    else if (but == 2)
    {
        TA0CCTL0 &= ~CCIE;
        P3IFG &= ~BUT2;
        P3IE |= BUT2;
    }
    else if (but == 3)
    {
        TA0CCTL0 &= ~CCIE;
        P3IFG &= ~BUT3;
        P3IE |= BUT3;
    }
    //but = 0;

    // Hardware clears Channel 0 flag (CCIFG in TA0CCTL0)
}

//*******************************
#pragma vector = PORT3_VECTOR // Write the vector name
__interrupt void Port3_ISR()
{
    // Button 1 is the pause/play button
    // Button 2 is the cm/inches button
    // Button 3 is for LPM4 toggle
    // Detect button 1 (BUT1 in P3IFG is 1)
    if ( (P3IFG & BUT1) != 0) {

        P3IE &= ~BUT1;
        but = 1;

        if (play == 0)
            play = 1;
        else
            play = 0;

        // Toggle the red LED
        P1OUT ^= redLED;

        TA0CCTL0 &= ~CCIFG;
        TA0CCTL0 |= CCIE;
        // Configure timer (ACLK) (divide by 2) (cont mode)
        TA0CTL = TASSEL_1 | ID_0 | MC_2 | TACLR;

        // Clear BUT1 in P1IFG
        P3IFG &= ~BUT1;
    }
    // Detect button 2 (BUT2 in P1IFG is 1)
    if ( (P3IFG & BUT2) != 0 )
    {
        P3IE &= ~BUT2;
        but = 2;

        if (unit == 0)
            unit = 1;
        else
            unit = 0;

        // Toggle the green LED
        P9OUT ^= greenLED;

        TA0CCTL0 &= ~CCIFG;
        TA0CCTL0 |= CCIE;
        // Configure timer (ACLK) (divide by 2) (cont mode)
        TA0CTL = TASSEL_1 | ID_0 | MC_2 | TACLR;

        // Clear BUT2 in P1IFG
        P3IFG &= ~BUT2;
    }
    if ( (P3IFG & BUT3) != 0 )
        {
            P3IE &= ~BUT3;
            but = 3;

            // Toggle the green LED
            P9OUT ^= greenLED;

            TA0CCTL0 &= ~CCIFG;
            TA0CCTL0 |= CCIE;
            // Configure timer (ACLK) (divide by 2) (cont mode)
            TA0CTL = TASSEL_1 | ID_0 | MC_2 | TACLR;

            // Clear BUT2 in P1IFG
            P3IFG &= ~BUT3;
        }
}


/* Timer B0 Capture Interrupt routine
 P2.2 (echo) causes this routine to be called */
#pragma vector=TIMER0_B1_VECTOR
__interrupt void T0B4_ISR(void)
{
  //  TB0CTL &= ~TAIE;
    if ((TB0CCTL4 & CCI) != 0)            // Raising edge
    {
        up_counter = TB0CCR4;      // Copy counter to variable
    }
    else                        // Falling edge
    {
        // Formula: Distance in cm = (Time in uSec)/58
        distance_cm = (TB0CCR4 - up_counter)/58;
        distance_in = (TB0CCR4 - up_counter)/148;
    }

    TB0CCTL4 &= ~CCIFG;
    TB0CTL &= ~TAIFG;           // Clear interrupt flag - handled
}


void timer_init()
{
    /* Timer B0 configure to read echo signal:
    Timer B Capture/Compare Control 4 =>
    capture mode: both edges +
    capture sychronize +
    capture input select 1 => P2.4 (CCI1B) +
    capture mode +
    capture compare interrupt enable */
    TB0CCTL4 = CM_3 | SCS | CCIS_1 | CAP | CCIE;

    /* Timer B Control configuration =>
    Timer B clock source select: 1 - SMClock +
    Timer B mode control: 2 - Continous up +
    Timer B clock input divider 0 - No divider */
    TB0CTL = TBSSEL_2 | MC_2 | ID_0 | TBCLR;

    // Global Interrupt Enable
    _enable_interrupts();
}

void lcd_reset()
{

    UC_PORT = 0x00;
    __delay_cycles(20000);

    UC_PORT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
    UC_PORT &= ~LCD_EN;
    __delay_cycles(10000);

    UC_PORT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
    UC_PORT &= ~LCD_EN;
    __delay_cycles(1000);

    UC_PORT = (0x03 << LCD_D0_OFFSET) | LCD_EN;
    UC_PORT &= ~LCD_EN;
    __delay_cycles(1000);

    UC_PORT = (0x02 << LCD_D0_OFFSET) | LCD_EN;
    UC_PORT &= ~LCD_EN;
    __delay_cycles(1000);

}

void lcd_cmd(char cmd)
{
    // Send upper nibble
    UC_PORT = ((((cmd >> 4) & 0x0F) << LCD_D0_OFFSET) | LCD_EN);
    UC_PORT &= ~LCD_EN;

    // Send lower nibble
    UC_PORT = (((cmd & 0x0F) << LCD_D0_OFFSET) | LCD_EN);
    UC_PORT &= ~LCD_EN;

    __delay_cycles(4000);
}

void lcd_data(unsigned char dat)
{

    // Send upper nibble
    UC_PORT = ((((dat >> 4) & 0x0F) << LCD_D0_OFFSET) | LCD_EN | LCD_RS);
    UC_PORT &= ~(LCD_EN | LCD_RS);

    // Send lower nibble
    UC_PORT = (((dat & 0x0F) << LCD_D0_OFFSET) | LCD_EN | LCD_RS);
    UC_PORT &= ~(LCD_EN | LCD_RS);

    __delay_cycles(4000); // a small delay may result in missing char display
}

void lcd_init()
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

void swap(char *xp, char *yp)
{
    char temp = *xp;
    *xp = *yp;
    *yp = temp;
}

/* A utility function to reverse a string  */
void reverse(char str[], int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        swap((str+start), (str+end));
        start++;
        end--;
    }
}

// Implementation of itoa()
char* itoa(int num, char* str, int base)
{
    int i = 0;
    int isNegative = false;

    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled only with
    // base 10. Otherwise numbers are considered unsigned.
    if (num < 0 && base == 10)
    {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
        num = num/base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    reverse(str, i);

    return str;
}
