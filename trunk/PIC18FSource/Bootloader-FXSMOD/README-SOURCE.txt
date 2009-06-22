NOTE: the bootloader is based on the PICDEM FS bootloader code
by Microchip with a few changes. Since that firmware is provided
under copyright by Microchip, I thought it was a bad idea to
publish its source here with my changes, claim a copyright on
it and placing the result under GNU GPL. After all, this is not
exactly the reason why Microchip offer their source code: their
aim is to help developers modify the source to adapt it to any
other needs, such as this board here. It feels like publishing
the whole source under GPL is not just the right way to treat
that code (and the intents of the company behind publishing it).

Thus, I am just publishing here the HEX file which is the only
thing required to get a developer up-and-running with my board.
In case anyone is interested in having a bootloader in source,
a summary of my changes from the Microchip version is as follows:

- in io_cfg.h, the LEDs are on different pins:

    #define mInitAllLEDs()        LATA &= 0xCF; TRISA &= 0xCF;
    #define mLED_1                LATAbits.LATA5
    #define mLED_2                LATAbits.LATA4

- in io_cfg.h, the user switch is on a different pin:

    #define mInitAllSwitches()  TRISCbits.TRISC2=64;
    #define mInitSwitch2()      TRISCbits.TRISC2=64;
    #define sw2                 PORTCbits.RC6

- in system/usb/class/boot/boot.h (or wherever this has moved),
  the high-priority interrupt vector has been moved, to save me
  extra jumps (check the blog on this, and compare the address
  to that in "18f2550.lkr" in the sibling OPENUSBFXS-FMWR
  directory):

    // avarvit: I needed to move this to 0x820 in order to have enough space and
    // avoid a second GOTO which eats valuable cycles
    //    #define RM_HIGH_INTERRUPT_VECTOR    0x000808
	#define RM_HIGH_INTERRUPT_VECTOR    0x000820

So, if you need to have the bootloader source, you may get the
PICDEM bootloader source, make the above changes, recompile,
and you are all set!

Note also that, in order to be able to use the PICDEM FS tool
as a flash programmer (over USB), I had to leave the USB
descriptor data of the bootloader unchanged, as the ones on
Microchip's board. As a result, a third "vendor"'s (myself)
board (Open USB FXS) has the same USB vendor and product ids
as another vendor's product, which is definitely not right.
As soon as this project gets a more final state, either the
ids will have to be changed to something identifying me as
the vendor and the board as a product of mine (and a flash
programmer will have to be written for it), or the PICDEM-
compatible bootloader will have to be removed altogether.

-A.
