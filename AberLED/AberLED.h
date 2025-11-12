/**
 * @file AberLED.h
 * @author Jim Finnis (jcf1@aber.ac.uk)
 * @version 3.0
 * @date 5 September 2023
 * @copyright 2014-2023 Jim Finnis, Aberstwyth University.
 * @brief The declaration for the AberLED class.
 * Many implementation details are inside the AberLED implementation file
 * AberLED.cpp. Note that there must be only one of these objects, but there's
 * no code to enforce this. Weird things will happen if you have
 * more than one.
 * 
 */

#ifndef __ABERLED_H
#define __ABERLED_H

#include "Arduino.h"

/// the "off" colour for pixels, used in set()
#define BLACK 0
/// the green colour for pixels, used in set()
#define GREEN 1
/// the red colour for pixels, used in set()
#define RED 2
/// the yellow colour for pixels, used in set()
#define YELLOW 3

///////////////// board revision codes

// beige board
#define REV00 0
// black board
#define REV01 1



/// the number for button S1, the "up" button
extern int UP;
/// the number for button S2, the "down" button
extern int DOWN;
/// the number for button S3, the "right" button
extern int LEFT;
/// the number for button S4, the "left" button
extern int RIGHT;
/// the number for button S5, the "action" or "fire" button
extern int FIRE;

enum AberLEDFlags {
    /// Use the old-style bicolor LED display, not the TFT screen
    AF_LEDDISPLAY = 1,
    /// Use a TFT screen display (this is the default)
    AF_TFTDISPLAY = 2,
    /// Do not set up the interrupt. The screen will not be refreshed
    /// automatically. You will need to do by calling refresh() often.
    AF_NOINTERRUPT = 4
};

/// The class for the AberLED shield. One object of this class, called
/// **AberLED**, is automatically created (similar to how Serial works).
/// Yes, all this time you've been writing C++, not C. You should put
/// `AberLED.` before every function, so `AberLED.begin()` instead
/// of just `begin()`. 
///
/// \warning
/// If you are using an bicolor LED display, you should call
/// `AberLED.begin(AF_LEDDISPLAY)`!
///
/// 
/// Calling begin() will initialise the display, setting all the pins to
/// the appropriate mode and setting up the interrupt handler.
/// Thereafter, every nth of a second the Arduino will automatically
/// send the front buffer to the matrix.
/// Calling getBuffer() will give a pointer to the back buffer, which
/// can be be written to. Alternatively, pixels can be written by calling
/// set() with x,y coordinates and a colour (GREEN, RED, YELLOW or BLACK).
/// Once writing is complete, calling swap() will
/// exchange the front and back buffers, so that the Arduino will
/// now write the new front buffer to the matrix every interrupt tick.
///
/// The format of the buffers is 8 16-bit integers: unsigned shorts.
/// Each integer represents a single row. Each pixel - individual LED -
/// is represented by 2 adjacent bits with the most significant bit
/// being red and the least significant bit being green.


class AberLEDClass {
public:
    /// Initialises all pin modes, clears the buffers,
    /// starts the interrupt and begins to output data to the LED.
    /// \param flags the flags to use - AF_LEDDISPLAY for a bicolor LED display, AF_TFTDISPLAY for a TFT display, 
    /// AF_NOINTERRUPT to use disable interrupts. If you specify AF_NOINTERRUPT, the display will not be refreshed
    /// automatically and you will have to call refresh() frequently.
    /// \param colourMap a pointer to an array of 9 8-bit integers - each triple defines how to convert the 2-bit colour
    /// into RGB. The first 3 bytes are for BLACK, the next 3 for GREEN, the next 3 for RED and the last 3 for YELLOW.
    /// The default is the "standard" colours but this could be useful for people with colour vision deficiencies.
    
    void begin(AberLEDFlags flags=AF_TFTDISPLAY, uint8_t *colourMap=NULL);
    
    /// Call this code to finish drawing operations.
    /// It swaps the back and front buffer, so that
    /// the newly written back buffer becomes the front buffer and is
    /// displayed.

    void swap();
    
    /// This sets the given pixel in the back buffer to the given value.
    /// The pixel values are 0 (off), 1 (green), 2 (red) or 3 (yellow),
    /// but the macros BLACK, GREEN, RED and YELLOW should be used.
    /// \param x the x coordinate of the pixel to write (0-7)
    /// \param y the y coordinate of the pixel to write (0-7)
    /// \param col the colour to use: BLACK, GREEN, RED or YELLOW.
    
    void set(int x, int y, unsigned char col);
    
    /// Set all pixels in the back buffer to black
    
    void clear();
    
    /// Return nonzero if a given switch is pressed - switches are numbered
    /// 1 to 5, which is against my Golden Rule of computers numbering
    /// things from zero, but that's how they're labelled on the physical
    /// board.
    /// It's better to use the UP, DOWN, LEFT, RIGHT and FIRE constants
    /// instead of numbers.
    /// \param c the button code (UP, DOWN, LEFT, RIGHT or FIRE)
   
    
    int getButton(unsigned char c);
    
    /// Return nonzero if the button has been pressed since the
    /// last swap().
    /// It's better to use the UP, DOWN, LEFT, RIGHT and FIRE constants
    /// instead of numbers.
    /// \param c the button code (UP, DOWN, LEFT, RIGHT or FIRE)
    
    int getButtonDown(unsigned char c);

    /// Call this to write to the back buffer directly.
    /// It returns a pointer to the buffer: a set of 8 16-bit ints,
    /// each pair of bits representing a pixel.

    uint16_t *getBuffer();
    
    /// Return the number of interrupt ticks which occurred in the
    /// last swap()-swap() cycle
    int getTicks();
    
    /// clear the string which is written to the text area on a TFT display.
    void clearText();

    /// Append a string to the text area on a TFT display. Does nothing
    /// if the display is not a TFT display. This just stores the data in
    /// an internal variable, the actual writing is done when swap() is called.
    /// If the resulting string would be too long, nothing is done.
    void addToText(const char *s);

    /// Write a number to the text area - see the string version for details.
    void addToText(int n);
    
    /// Use only when interrupts are disabled - copies the front
    /// buffer to the display
    static void refresh();
    
    /// Call this to set the revision, after AberLED.begin() (which
    /// will set it to REV01 by default). <b>Only relevant to LED
    /// matrix boards, and only the really old beige ones</b>. You
    /// shouldn't ever need to use this.
    /// \param rev revision number - REV00 or REV01.
    static void setRevision(int rev);

    /// return the version string
    static const char *version();
};

/// this is the single instance of the LED class - for documentation see AberLEDClass.

extern AberLEDClass AberLED;


#endif /* __ABERLED_H */
