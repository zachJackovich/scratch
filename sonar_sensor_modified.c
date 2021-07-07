#include <msp430fr6989.h>

#define true 1
#define false 0

// Peripheral pin assignments
#define US_ECHO    BIT2        // Echo at P2.2
#define US_TRIG    BIT3        // Trigger at P2.3
#define US_MASK    US_TRIG | US_ECHO

#define redLED     BIT0        // Red LED at P1.0
#define greenLED   BIT7        // Green LED at P9.7

unsigned int up_counter = 0;
unsigned int distance_cm = 0;

//Declaring functions
void timer_init();
char* itoa(int num, char* str, int base);

int main()
{
    /*** Starting main.c ***/
    printf("Beginning main.c... \n");
    
    char distance_string[4];

    WDTCTL   = WDTPW + WDTHOLD;       // Stop Watch Dog Timer
    PM5CTL0 &= ~LOCKLPM5;           // Enable the GPIO pins
    
    // Configure Sonar Sensor Signals
    printf("Configuring Sonar Sensor...\n");
    P2DIR |= US_TRIG;         // Set Trigger pin as Output
    P2DIR &= ~US_ECHO;        // Set Echo pin as Input

    P2OUT &= ~US_TRIG;              // Set trigger low
    P2SEL0 &= ~US_ECHO;                // set US_ECHO as trigger for Timer from Port-2
    P2SEL1 |= US_ECHO;

    // Configure LEDs
    printf("Configuring LEDs... \n");
    P1DIR |= redLED; // Direct pin as output
    P9DIR |= greenLED; // Direct pin as output
    P1OUT &= ~redLED; // Turn LED Off
    P9OUT &= ~greenLED; // Turn LED Off

    printf("Set TA0CCR0 to 3s using 16 KHz clk...\n");
    TA0CCR0 = 11554-1; // @ 16 KHz --> 3 seconds                ************ Make sure this number is correct

    // Initialize timer for Ultrasonic distance sensing
    printf("Running timer_init()...\n");
    timer_init();

    while (1)
    {

        // measuring the distance
        P2OUT ^= US_TRIG;                 // Assert Trigger
        __delay_cycles(10);               // 10us wide
        P2OUT ^= US_TRIG;                 // De-assert Trigger
        __delay_cycles(500000);           // 0.5sec measurement cycle

        // displaying the current distance
        printf("Distance is: %f cm...\n", itoa(distance_cm, distance_string, 10);

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
