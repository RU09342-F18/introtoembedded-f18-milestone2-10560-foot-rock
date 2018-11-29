#include <msp430.h>
#include <stdint.h>

int DES_TEMP = 24; // degrees C

int TEMP_RANGE = 76; // 15-90 degrees C
// array of ADC values for temperatures of degrees C
uint16_t ADC_VALUES[76] = {0x064, 0x069, 0x06E, 0x073, 0x078,
                                   0x07E, 0x083, 0x089, 0x08F, 0x095,
                                   0x09C, 0x0A2, 0x0A9, 0x0B0, 0x0B7,
                                   0x0BF, 0x0C6, 0x0CE, 0x0D6, 0x0DF,
                                   0x0E7, 0x0F0, 0x0F9, 0x102, 0x10C,
                                   0x116, 0x120, 0x12A, 0x134, 0x13F,
                                   0x14A, 0x155, 0x161, 0x16C, 0x178,
                                   0x184, 0x191, 0x19D, 0x1AA, 0x1B7,
                                   0x1C4, 0x1D2, 0x1E0, 0x1ED, 0x1FB,
                                   0x20A, 0x218, 0x227, 0x236, 0x245,
                                   0x254, 0x263, 0x273, 0x282, 0x292,
                                   0x2A2, 0x2B2, 0x2C2, 0x2D2, 0x2E2,
                                   0x2F3, 0x303, 0x314, 0x324, 0x335,
                                   0x346, 0x357, 0x367, 0x378, 0x389,
                                   0x39A, 0x3AB, 0x3BC, 0x3CD, 0x3DD,
                                   0x3EE};

int last_temp = 0;
int temp;
uint16_t data;

void AdcSetup(void)
{
    P1DIR |= BIT0;      // Set P1.0 to output direction
    ADC10CTL0 = SREF_1 + ADC10SHT_2 + REFON + ADC10ON + ADC10IE;
    ADC10CTL1 = INCH_0; // input A1
    ADC10AE0 |= BIT0;   // P1.1 input
}

void PWMSetup(void)
{
    TA1CCR0 = 0xFF; // reset register
    TA1CCR1 = 0x00; // fan pwm

    TA1CCTL1 = OUTMOD_7; // set/reset mode

    TA1CTL = MC_1 | TASSEL_2 | ID_1; // up | SMCLK | 2^1 division
}

void PWMFanSetup(void)
{
    P2DIR |= BIT2; // P2.2 output
    P2SEL |= BIT2; // P2.2 = CCR1
}

void UARTSetup(void)
{
    P1SEL  |=  BIT1 + BIT2;  // P1.1 UCA0RXD input
    P1SEL2 |=  BIT1 + BIT2;  // P1.2 UCA0TXD output
    P1DIR  &=  ~BIT1;
    P1DIR  |=  BIT2;
    UCA0CTL1 |= UCSWRST;  // reset state machine
    UCA0CTL1 |= UCSSEL_2; // SMCLK
    UCA0BR0 = 6; // 9600 baud
    UCA0BR1 = 0;
    UCA0MCTL |= UCBRS_0|UCBRF_13|UCOS16; // modulation UCBRSx=0, UCBRFx=0
    UCA0CTL1 &= ~UCSWRST; // initialize state machine
}

void initSetup(void)
{
    PWMSetup();
    PWMFanSetup();
    AdcSetup();
    UARTSetup();
}

uint16_t getTemp(void)
{
    int index = 0;
    while (index < TEMP_RANGE && ADC_VALUES[index] < data)
    {
        ++index;
    }
    if(index == TEMP_RANGE)
    {
        return index + 15 - 1; // index 0 starts at 15 degrees C
    }
    else if(index == 0)
    {
        return 15;
    }
    else if((data - ADC_VALUES[index - 1]) < (ADC_VALUES[index] - data))
    {
        return index + 15 - 1;
    }
    else
    {
        return index + 15;
    }
}

void setPWM(void)
{
    temp = getTemp();
    int delta = (temp - DES_TEMP);
    if (delta == 1 && delta == -1)
    {
        if((last_temp - DES_TEMP)>0)
        {
            TA1CCR1 = 0xBF; // 3/4 on
        }
        else
        {
            TA1CCR1 = 0x5F; // 3/8 on
        }
    }
    else if(delta < 0)
    {
        TA1CCR1 = 0x00; // off
    }
    else if (delta > 0)
    {
        TA1CCR1 = 0xFF; // on
    }
    else
    {
        TA1CCR1 = 0x9F; // 5/8 on
    }
    if(DES_TEMP != temp)
    {
        last_temp = temp;
    }
}

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog
    initSetup();
    ADC10CTL0 |= ENC + ADC10SC;         // Sampling and conversion start
    __bis_SR_register(LPM0_bits + GIE); // low power mode + enable global interrupt
}

// ADC
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
{
    data = ADC10MEM; // data is bits 9-0
    if (data < 0x3ff)   // ADC10MEM = A1 < 1.5V // 0x29D = 1.0V
    {
        P1OUT &= ~0x01;     // Clear P1.0 LED off
    }
    else
    {
        P1OUT |= 0x01;      // Set P1.0 LED on
    }
    setPWM();
    ADC10CTL0 |= ENC + ADC10SC;         // Sampling and conversion start
    UCA0TXBUF = temp;
}
