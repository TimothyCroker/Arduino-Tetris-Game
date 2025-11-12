/**
 * @file AberLED.cpp
 * @brief Implementation of the bicolor LED library.
 * @author Jim Finnis (jcf1@aber.ac.uk)
 * @version 3.3
 * @date 6 November 2023
 * @copyright Aberstwyth University
 *
 * This file implements the AberLED class. Stylistically it's somewhat horrific,
 * using global variables all over the place. This is because (a) it keeps
 * implementation details (private variables) out of the include file, and (b)
 * the class is always used as a singleton.
 */



/*
 * These two libraries are for the ST7735 TFT display. You'll also need the BusIO and Seesaw
 * libraries installed.
 */

#include "TFT_ST7735.h"
#include <SPI.h>

#include "AberLED.h"

const char *AberLEDClass::version(){
    // which line of the LED we'll be drawn on
    //      00000000000000000000011111111
    return "v3.3 ETERNAL EVENING 16-11-23";
}


// pin mappings - the actual refresh code uses direct
// port manipulation, so YOU MUST CHANGE THAT TOO IF YOU
// CHANGE THESE!

// pins for the row shift register
#define RDATA 2
#define RLATCH 3
#define RCLOCK 4

// pins for the column shift registers
#define CDATA 5
#define CCLOCK 6
#define CLATCH 7

int UP, DOWN, LEFT, RIGHT, FIRE;

// these are the two buffers.
static uint16_t bufferA[8];
static uint16_t bufferB[8];

// these are pointers to the back and front buffers
static uint16_t *backBuffer;
static uint16_t *frontBuffer;

// button variables
static byte buttonStates[] = {0, 0, 0, 0, 0};          // set in swap()
static byte debouncedButtonStates[] = {0, 0, 0, 0, 0}; // set in interrupt
static byte buttonDebounceCounters[] = {0, 0, 0, 0, 0};
static byte buttonWentDown[] = {0, 0, 0, 0, 0};
static byte buttonWentDownInLastLoop[] = {0, 0, 0, 0, 0};
static int boardRev = -1;

int ticks = 0;
bool interruptRunning = false;
volatile int interruptTicks = 0;
bool isTFT;

#define MAXTEXTLEN 32
// we only render text when it has changed, so we keep the previously rendered text
static char prevTxtBuffer[MAXTEXTLEN];
static char txtBuffer[MAXTEXTLEN];

AberLEDClass AberLED;

/*
 * This is stuff for the TFT - it should be OK to declare and even construct the TFT,
 * provided you don't initialise it.
 */



TFT_ST7735 tft = TFT_ST7735();


void AberLEDClass::setRevision(int rev)
{
    boardRev = rev;
    if (rev == REV01)
    {
        UP = 1;
        DOWN = 2;
        LEFT = 4;
        RIGHT = 3;
        FIRE = 5;
    }
    else
    {
        UP = 2;
        DOWN = 4;
        LEFT = 1;
        RIGHT = 3;
        FIRE = 5;
    }
}

int AberLEDClass::getTicks()
{
    return ticks;
}

// these are faster routines for doing the shift register writes

static void fastShiftOutRows(byte n)
{
    PORTD &= ~(1 << 2); // data off
    for (int i = 7; i >= 0; i--)
    {
        PORTD &= ~(1 << 4); // clock off
        if (n & (1 << i))
            PORTD |= 1 << 2; // data on if true
        else
            PORTD &= ~(1 << 2); // data off if true
        PORTD |= 1 << 4;        // clock on

        PORTD &= ~(1 << 2); // data off (to prevent bleed through)
    }
    PORTD &= ~(1 << 4); // clock off
}

static void fastShiftOutCols(uint16_t n)
{
    PORTD &= ~(1 << 5); // data off
    for (int i = 15; i >= 0; --i)
    {
        PORTD &= ~(1 << 6); // clock off
        if (n & (1 << i))
            PORTD |= 1 << 5; // data on if true
        else
            PORTD &= ~(1 << 5); // data off if true
        PORTD |= 1 << 6;        // clock on
        PORTD &= ~(1 << 5);     // data off (to prevent bleed through)
    }
    PORTD &= ~(1 << 6); // clock off
}

static void setupInterrupt();


// this is the colour map for the TFT display. It's an array of 16 bits per colour in a 565 format
// (5 bits red, 6 bits green, 5 bits blue). It is initialised with the colours black, green, red
// and yellow, but you can change it if you want to by calling AberLED.begin() with a colour map.

static uint16_t cols[] = {ST7735_BLACK, ST7735_GREEN, ST7735_RED, ST7735_YELLOW};

void AberLEDClass::begin(AberLEDFlags flags, uint8_t *colourMap){
    setRevision(REV01);
    isTFT = (flags & AF_TFTDISPLAY) != 0;

    // set all the shift register pins to output
    if (isTFT) {
        // Use this initializer if you're using a 1.44" TFT
        tft.init();
        tft.setRotation(2);
        tft.fillScreen(TFT_BLACK);

        tft.setCursor(4, 4);
        tft.setTextColor(TFT_WHITE);
        tft.setTextWrap(true);
        tft.print("Arduino LED\n\n");
        tft.setTextColor(TFT_GREEN);
        tft.print(version());
        delay(2000);

        tft.fillScreen(TFT_BLACK);

        if(colourMap){
            for(int i=0;i<4;i++){
                uint16_t r = (*colourMap++ >> 3) & 0x1f;    // chop down frop 8 bits to 5
                r <<= 11;                                   // put into the right place
                uint16_t g = (*colourMap++ >> 2) & 0x3f;    // chop down frop 8 bits to 6
                g <<= 5;                                    // put into the right place
                uint16_t b = (*colourMap++ >> 3) & 0x1f;    // chop down frop 8 bits to 5
                cols[i] = r | g | b;
            }
        }


    } else {
        // this initialiser is used when using a bicolor LED display
        pinMode(CLATCH, OUTPUT);
        pinMode(CDATA, OUTPUT);
        pinMode(CCLOCK, OUTPUT);
        pinMode(RLATCH, OUTPUT);
        pinMode(RDATA, OUTPUT);
        pinMode(RCLOCK, OUTPUT);

        // clear the SRs

        digitalWrite(RLATCH, LOW);
        fastShiftOutRows(0);
        digitalWrite(RLATCH, HIGH);

        digitalWrite(CLATCH, LOW);
        fastShiftOutCols(0);
        digitalWrite(CLATCH, HIGH);
    }

    // set up the switch inputs
    pinMode(A0, INPUT_PULLUP);
    pinMode(A1, INPUT_PULLUP);
    pinMode(A2, INPUT_PULLUP);
    pinMode(A3, INPUT_PULLUP);
    pinMode(9, INPUT_PULLUP);
    // and the default LED
    pinMode(13, OUTPUT);

    // set up the initial buffers and clear them

    backBuffer = bufferA;
    frontBuffer = bufferB;

    memset(backBuffer, 0, 16);
    memset(frontBuffer, 0, 16);

    txtBuffer[0] = 0;
    prevTxtBuffer[0] = 0;


    if (!(flags & AF_NOINTERRUPT))
        setupInterrupt();
}

// render the text to the screen, if it is not the same text as was previously rendered.
static void renderText(){
    if(strcmp(txtBuffer, prevTxtBuffer)){
        Serial.println(txtBuffer);
        strcpy(prevTxtBuffer, txtBuffer);
        tft.fillRect(0, 150, 128, 16, TFT_BLACK);
        // we display the text in the front buffer
        if(txtBuffer[0]){
            tft.setTextColor(TFT_WHITE);
            tft.setCursor(4, 150);
            tft.print(txtBuffer);
        }
    }
}


// The user calls this code when they have finished writing to
// the back buffer. It swaps the back and front buffer, so that
// the newly written buffer becomes the front buffer and is
// displayed. This is done by swapping the pointers, not the data.
// Interrupts are disabled to avoid the "tearing" effect produced
// by the buffers being swapped during redraw.

void AberLEDClass::swap()
{
    uint16_t *t;

    cli();
    for (int i = 0; i < 5; i++)
    {
        buttonWentDownInLastLoop[i] = buttonWentDown[i];
        buttonWentDown[i] = 0;
        buttonStates[i] = debouncedButtonStates[i];
    }

    ticks = interruptTicks;
    interruptTicks = 0;

    t = frontBuffer;
    frontBuffer = backBuffer;
    backBuffer = t;

    // render text to the screen if it has changed. Will do nothing if not using a TFT.
    if(isTFT)
        renderText();

    sei();
    if (interruptRunning)
    {
        while (interruptTicks < 2)
        {
        }
    }
}

void AberLEDClass::clearText(){
    if(isTFT){
        cli(); // disable interrupts
        // clear the text buffer
        txtBuffer[0] = 0;
        sei(); // re-enable interrupts
    }
}
 
void AberLEDClass::addToText(const char *s){
    if(isTFT){
        cli();

        // is the length of this string, plus the length of the 
        // current string, plus the null terminator, less than the
        // maximum length? If so, we can concatenate the strings.
        if(strlen(txtBuffer) + strlen(s) + 1 < MAXTEXTLEN){
            strcat(txtBuffer, s);
        }
        sei();
    }
}

void AberLEDClass::addToText(int n){
    // write the number to a temporary string, taking care not to overflow the buffer.
    char tmp[16];
    snprintf(tmp, 16, "%d", n);
    addToText(tmp);
}

int AberLEDClass::getButton(unsigned char c)
{
    return buttonStates[c - 1];
}

int AberLEDClass::getButtonDown(unsigned char c)
{
    return buttonWentDownInLastLoop[c - 1];
}

// The user calls this before writing to the back buffer, to
// get its pointer to write to.

uint16_t *AberLEDClass::getBuffer()
{
    return backBuffer;
}

// set a pixel in the back buffer to a given colour

void AberLEDClass::set(int x, int y, unsigned char col)
{
    if (x < 8 && y < 8 && x >= 0 && y >= 0)
    {                                 // check we're in range
        uint16_t *p = backBuffer + y; // get row pointer
        x *= 2;                       // double x to get the column bit index
        // clear the bits first
        *p &= ~(3 << x);
        // then set the new colour
        *p |= col << x;
    }
}

// sets the entire back buffer to zero

void AberLEDClass::clear()
{
    memset(backBuffer, 0, 16);
}


// called periodically by the interrupt, this writes the front
// buffer data out to the screen. The front buffer is the one
// which is not being drawn to.

static int refrow = 0;
inline void refreshNextRow()
{
    if(isTFT) {
        // using a TFT, so we draw rectangles instead of using the shift registers (which
        // don't exist on the TFT boards)

        uint16_t v = *(frontBuffer + refrow); // get row pointer
//        uint16_t vold = *(backBuffer + refrow); // get row pointer

        for(int x=0;x<8;x++){
            uint16_t q = (v >> (x*2)) & 3;
            tft.fillRect(16*x+2, 16*refrow+2, 12, 12, cols[q]);
        }
    } else {
        // this code is used for the older LED boards, and use the shift registers -
        // we directly manipulate the port registers for speed.

        if (!refrow)
            PORTD |= 1 << 2; // turn on the row data line to get the first bit set

        // set latches low
        PORTD &= ~((1 << 3) | (1 << 7));

        // tick the row clock to move the next bit in (high on
        // the first row, low after that)
        PORTD |= (1 << 4);
        PORTD &= ~(1 << 4);

        // and turn off the row data line
        PORTD &= ~(1 << 2);

        // now the appropriate row is set high, set the column
        // bits low for the pixels we want.

        fastShiftOutCols(~(frontBuffer[refrow]));
        // and latch the registers

        PORTD |= ((1 << 3) | (1 << 7));
    }

    refrow = (refrow + 1) % 8;
}

// refresh the entire display BY HAND. This IS NOT CALLED BY THE INTERRUPT!!!!
void AberLEDClass::refresh()
{
    refrow = 0;
    refreshNextRow();
    refreshNextRow();
    refreshNextRow();
    refreshNextRow();
    refreshNextRow();
    refreshNextRow();
    refreshNextRow();
    refreshNextRow();

    if(!isTFT){
        // hold the last line for a little while
        for (int volatile i = 0; i < 30; i++)
        {
            __asm__ __volatile__("nop\n\t");
        }

        //    // latch off values into the columns, to avoid last row bright.
        PORTD &= ~((1 << 3) | (1 << 7));
        fastShiftOutCols(0xffff);
        PORTD |= ((1 << 3) | (1 << 7));
    }
}

// this is the interrupt service routine for the timer interrupt

ISR(TIMER1_COMPA_vect)
{
    interruptTicks++;

    // draw the next row
    refreshNextRow();

    static byte trueButtonStates[] = {0, 0, 0, 0, 0};
    static byte button = 0;
    // and process the next button

    byte bstate;
    if (boardRev == REV00)
    {
        switch (button)
        {
        case 0:
            bstate = PINC & 2;
            break;
        case 1:
            bstate = PINC & 1;
            break;
        case 2:
            bstate = PINC & 4;
            break;
        case 3:
            bstate = PINC & 8;
            break;
        case 4:
            bstate = PINB & 2;
            break;
        }
    }
    else
    {
        switch (button)
        {
        case 0:
            bstate = PINC & 1;
            break;
        case 1:
            bstate = PINC & 8;
            break;
        case 2:
            bstate = PINC & 4;
            break;
        case 3:
            bstate = PINC & 2;
            break;
        case 4:
            bstate = PINB & 2;
            break;
        }
    }

    bstate = bstate ? 0 : 1; // make booleanesque and invert

    if (bstate != trueButtonStates[button])
    {
        buttonDebounceCounters[button] = 0;
        if (bstate)
            buttonWentDown[button] = 1;
    }
    else if (++buttonDebounceCounters[button] == 4)
    {
        buttonDebounceCounters[button] = 0;
        debouncedButtonStates[button] = bstate;
    }
    trueButtonStates[button] = bstate;

    button = (button + 1) % 5;
}

// Set up a 1kHz interrupt handler - the code for the interrupt
// is in the TIMER1_COMPA_vect() function.

static void setupInterrupt()
{
    cli(); // disable all interrupts

    // some very hardware-specific code, setting registers
    // inside the ATMega328p processor.

    // set timer1 interrupt at 500Hz or slower for the TFT
    TCCR1A = 0; // set entire TCCR1A register to 0
    TCCR1B = 0; // same for TCCR1B
    TCNT1 = 0;  // initialize counter value to 0
    if(isTFT) {
        // 200 Hz
        OCR1A = 1249;
        TCCR1B |= (1 << WGM12);
        TCCR1B |= (1 << CS11) | (1 << CS10);
    } else {
        // set compare match register for correct frequency
        // (16*10^6) / (500*8) - 1 gives 3999 (for 500Hz)
        OCR1A = 3999;
        // turn on CTC mode
        TCCR1B |= (1 << WGM12);
        // Set prescaler to 8
        TCCR1B |= (1 << CS11);
    }
    // enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);
    interruptRunning = true;
    sei(); // re-enable interrupts
}
