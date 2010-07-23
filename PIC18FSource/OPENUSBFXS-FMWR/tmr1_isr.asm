; FileName:	tmr1_isr.asm
; Dependencies:	HardwareProfile.inc (generated from HardwareProfile.h)
; Processor:	PIC18
; Hardware:	The code is intended to be used on the Open USB FXS board
; Assembler:	MPASM
; Copyright:	(C) Angelos Varvitsiotis 2009
; License:	GPLv3

#include p18f2550.inc
#include HardwareProfile.inc
#include "pcmpacket.h"

; Various debugging-related defines follow

; If defined, causes the serial number of OUT packets to be
; mirrored into IN packets
#define DEBUG_USB_SERIAL

; If defined, causes the 16-bit value of TMR3 to be included
; into IN packets; this is very useful when debugging timing
; and sequencing issues.
; NOTE: !!!! in order that the board returns DTMF and hook
; status with PCM data, this needs to be undefined!!
;;;;;;#define DEBUG_USB_TMR3

; if defined, causes the IN packets to use the same buffers
; as OUT packets, essentially mirroring back all data received
;;;;;;#define DEBUG_PCM_MIRROR

; DEBUG_USB_NO_OUT, if defined, causes isochronous OUT
; packets sent by the host to be ignored; it may be useful
; under some circumstances
;;;;;;#define DEBUG_USB_NO_OUT

; Defining DEBUG_CYCLES causes TMR3 to be set at the 32nd
; ISR cycle, so that it has the value 0 when the ISR is next
; entered; then, a block of code should write the value of TMR3
; into the debug variable "passout"; its use is in debugging
; timing issues. NOTE: the block that copies the value of TMR3
; into passout screws up the timing from that point on, so
; don't ever leave this in if not debugging
;;;;;;#define DEBUG_CYCLES

; TEST_PCM_TIMING, if defined, produces a pulsing sequence of
; 1's and 0's as the DRX output of the PIC; it is useful for
; verifying the PCM highway timing using an oscilloscope
;;;;;;#define TEST_PCM_TIMING

; Defining DEBUG_LOST_OUT causes a field within IN_PCMData1 to be
; incremented once for every lost OUT packet
#define DEBUG_LOST_OUT


; Labels for external addresses

	; NOTE: the labels below are valid only in FULL ping-pong mode! 

	; specific fields of BD for EP2 IN, even ping-pong phase
EP2IEAH	EQU	0x42b			; bank-part of buffer address field
EP2IEAL	EQU	0x42a			; offset-part of buffer address field
EP2IECN	EQU	0x429			; byte count field
EP2IEST	EQU	0x428			; status flags field
	; specific fields of BD for EP2 IN, odd ping-pong phase
EP2IOAH	EQU	0x42f			; bank-part of buffer address field
EP2IOAL	EQU	0x42e			; offset-part of buffer address field
EP2IOCN	EQU	0x42d			; byte count field
EP2IOST	EQU	0x42c			; status flags field
	; specific fields of BD for EP2 OUT, even ping-pong phase
EP2OEAH	EQU	0x423
EP2OEAL	EQU	0x422
EP2OECN	EQU	0x421
EP2OEST	EQU	0x420
	; specific fields of BD for EP2 OUT, odd ping-pong phase
EP2OOAH	EQU	0x427
EP2OOAL	EQU	0x426
EP2OOCN	EQU	0x425
EP2OOST	EQU	0x424

; Our exports
	; code entry points
	GLOBAL		tmr1_isr_init
	GLOBAL		tmr1_isr

	; data
	GLOBAL		OUTPCMData0	; currently, for debugging
	GLOBAL		OUTPCMData1	; currently, for debugging
	GLOBAL		IN_PCMData0	; currently, for debugging
	GLOBAL		IN_PCMData1	; currently, for debugging

	GLOBAL		passout		; a debug return value
	GLOBAL		pasHout		; a debug return value (for WORDs)
	GLOBAL		pauseIO		; pause USB I/O
	GLOBAL		hkdtmf		; hook and DTMF status
	GLOBAL		seensof		; flag, true if we have seen a SOF

	; these are exposed ONLY for debugging, they should not
	; be touched from the outside world, or bad things will
	; happen...
	GLOBAL		cnt4
	GLOBAL		cnt8



;	Globals, all placed in the "usb4" bank. It is important to have all
;	of these in the same bank, because in the ISR we only do a single
;	BANKSEL, assuming that all data lie in the same bank, in order to
;	save us time. Thus, we ask the assembler to tell the linker to
;	place our data at the beginning of GPR1.

	; Note: for the specific project, we have only 3 endpoints, 0, 1,
	; and 2. The BD for these occupies 4 (bytes/entry) x (#EPs) x 2
	; (in/out) x 2 (even/odd ping-pong) bytes, which amounts to 48
	; bytes; two more 32-byte USB packet buffers are allocated right
	; after that, which amounts up to 112 bytes. So theoretically,
	; the space from 0x480 is free for us to use as space for common
	; variables (which however we need to be on the same bank as
	; the USB packet data for speed reasons, see above).
ourvars	UDATA		0x480

	; next globals implement two (for ping-pong buffering mode) read
	; and another two write audio buffers (NOTE: don't add anything
	; before these variables, so as to make sure they are 16-byte
	; aligned).

; the next variables should lie on a 64-byte aligned area
OUTPCMData0 RES		16		; even ping-pong buffer for OUT data
OUTPCMData1 RES		16		; odd  ping-pong buffer for OUT data
IN_PCMData0 RES		16		; even ping-pong buffer for IN  data
IN_PCMData1 RES		16		; odd  ping-pong buffer for IN  data

ppindex	RES		1		; index (incl. ping-pong offset); this
					; ranges from 8..15 for even ping-pong
					; buffer state and from 24..31 for odd
					; state

outodev	RES		1		; points to appropriate BD for even/odd
					; OUT

pauseIO	RES		1		; flags whether we should do USB I/O


	; next two bytes are bit buffers for the current bytes of PCM data
	; that are currently being read in and written out
byteDRX	RES		1		; stores 1 DRX byte (PCM data PIC->3210)
byteDTX	RES		1		; stores 1 DTX byte (PCM data 3210->PIC)




	; next come two timers that are maintained by the ISR in order to do
	; different things at each invocation
cnt8	RES		1		; counts 8 isr invocations
cnt4	RES		1		; counts 4x8 invocations

	; debugging return value
passout	RES		1		; variable to pass out debugging info
pasHout	RES		1		; variable to pass out debugging info

	; Hook and DTMF state
hkdtmf	RES		1		; set in user.c, holds hook/DTMF status

	; Flags seen SOF at least once
seensof	RES		1

	; temporary storage for FSR1 and FSR2
sfsr1l	RES		1		; FSR1L temp save space
sfsr1h	RES		1		; FSR1H temp save space
sfsr2l	RES		1		; FSR2L temp save space
sfsr2h	RES		1		; FSR2H temp save space


; TMR1 service interrupt routine

;	I have modified the bootloader code to jump to 0x820 for the
;	high-priority interrupt; thus, the PIC takes 3 instruction
;	cycles (I use C to denote an instruction cycle) to process
;	the interrupt and another 2 C to jump here, which sums up to
;	5 C.

timer1isr CODE		0x820		

tmr1_isr

;	DOCNOTE:
;	I use the @ symbol to denote the cycles already elapsed since
;	TMR1 fireup when we enter this routine; this is equal to 5 (see
;	previous note).


						; T:@, where @ == 5, see above
;	Our first job is to raise PCLK (note that PCLK is raised at time @==5)

	BSF		pcm_3210_pclk, ACCESS	; C:1 raise PCLK

;	Clear the interrupt flag (otherwise the call to the ISR loops forever!)
	BCF		PIR1, TMR1IF, ACCESS	; C:1 clear interrupt flag
	
						; T:@+2

;	Our next step is to increment our counters and choose accordingly
;	what to do next


	BANKSEL		cnt8			; C:1 use appropriate bank
	; Note: this is the only BANKSEL instruction used throughout the
	; code. All data are assumed to lie on the same bank. See note on
	; this at the UDATA section.

	; first increment cnt8 modulo 8 and keep the status handy

	INCF		cnt8, F, BANKED		; C:1 increment 8-step counter
	MOVLW		7			; C:1 prepare % 8 operation
	ANDWF		cnt8, F, BANKED		; C:1 AND it and save it

						; T:@+6	
	; if cnt8 has not just looped over, jump to "notfirstof8"
	BNZ		notfirstof8		; C:1/2 

;	Our cnt8 has looped to 0; we need to increment our cnt4 and check
;	if we are starting a new 32-step sequence

						; T:@+7

	INCF		cnt4, F, BANKED		; C:1 increment 4-step counter
	MOVLW		3			; C:1 modulo 4
	ANDWF		cnt4, F, BANKED		; C:1 save it

						; T:@+10

	; if cnt4 has not looped over, jump to "firstof8not32"
	BNZ		firstof8not32		; C:1/2


	; (following label is just for clarity, there's not any jump into it)
firstof32					; T:@+11

;	At this point, we are starting a new 32-step sequence; the tasks that
;	we have to do are (i) assert the DRX signal (our output) to the 3210,
;	(ii) re-arm the TMR1 to fire at @+46-5 (this is our first round, so it
;	should take 23+23=46 cycles), (iii) initialize the input PCM byte to
;	zero before collecting any PCM input, (iv) collect our PCM input from
;	the DTX signal and (v) lower PCLK at time @+5+23.

;	NOTE: this round, just like all other cases where cnt8==0, requires
;	a PCLK-high half-cycle of 23TCy in duration (PCLK lowered at @+23)



	; task (0): lower FSYNC

	BCF		pcm_3210_fsync, ACCESS	; C:1

						; T:@+12
	; task (i): prepare the DRX signal (our output -- must
	; have been asserted before the falling edge of PCLK)


	RLCF		byteDRX, F, BANKED	; C:1 rotate left through carry
	BNC		fo32_clr_DRX		; C:1/2 check if carry is set
      #ifdef TEST_PCM_TIMING
	; if TEST_PCM_TIMING is defined, we ignore the actual data in byteDRX
	; (data sent by the host) and just toggle the DRX output
	BTG		pcm_3210_drx, ACCESS	; (C:1) just toggle (for tests)
      #else
	BSF		pcm_3210_drx, ACCESS	; C:1 it is set, so set DRX to 1
      #endif
	BRA		fo32_DRX_ok		; C:2 done

fo32_clr_DRX					; T:@+15

      #ifdef TEST_PCM_TIMING			; same here
	BTG		pcm_3210_drx, ACCESS	; (C:1) just toggle (for tests)
      #else
	BCF		pcm_3210_drx, ACCESS	; C:1 carry not set, clear DRX
      #endif
	NOP					; C:1 make both paths' delay eq.
	

fo32_DRX_ok					; T:@+17 via both paths

	; task (ii): re-arm TMR1 to fire again at @+46-5
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		236			; C:1 next interrupt at @+46-5
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+20

	; task (iii): initialize the DTX PCM byte to zero
	CLRF		byteDTX, BANKED		; C:1 clear byteDTX

						; T:@+21

	; task (iv): collect our PCM input from the DTX signal
	BTFSC		pcm_3210_dtx, ACCESS	; C:1/2 skip next if DTX is 0
	BSF		byteDTX, 0, BANKED	; C:1 if 1, set bit 0 of byteDTX

						; T:@+23 via both paths

	; task (v): wait until T:@+23, then lower PCLK
						; T:@+23

	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+24

	; finished with all of our tasks, time to return
	RETFIE		FAST			; C:2



;	We get here if this is not the first cnt8 round (cnt8<>0). We need to
;	check if cnt4==0, in which case we 'll do PCM I/O and the other
;	cases.
;	FIXDOC: <task description>
notfirstof8					; T:@+8

	DECF		cnt4, W, BANKED		; C:1 W:=cnt4-1; if cnt4<>0, the
	BNN		notfirst8of32		; C:1/2 "neg" bit is clear, jump

	; (following label is just for clarity, there is not any jump into it)
first8of32					; T:@+10

;	We come here if cnt8<>0 and cnt4==0, so we are dealing with the
;	first 8 cycles of our 32-cycle train. During these cycles, we must
;	do PCM I/O.

	; task (i): prepare the DRX signal (our output -- must
	; be ready before the falling edge of PCLK)
	RLCF		byteDRX, F, BANKED	; C:1 rotate left through carry
	BNC		f8o32_clr_DRX		; C:1/2 check if carry is set
      #ifdef TEST_PCM_TIMING
	BTG		pcm_3210_drx, ACCESS	; (C:1) just toggle (for tests)
      #else
	BSF		pcm_3210_drx, ACCESS	; C:1 it is set, so set DRX to 1
      #endif
	BRA		f8of32_DRX_ok		; C:2 done

f8o32_clr_DRX					; T:@+13

      #ifdef TEST_PCM_TIMING
	BTG		pcm_3210_drx, ACCESS	; (C:1) just toggle (for tests)
      #else
	BCF		pcm_3210_drx, ACCESS	; C:1 carry not set, clear DRX
      #endif

	NOP					; C:1 make both paths' delay eq.

f8of32_DRX_ok					; T:@+15 via both paths

	; task (ii): re-arm TMR1 to fire again at @+47-5
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		233			; C:1 next interrupt at @+47-5
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+18

	; task (iii): collect our PCM input from the DTX signal
	RLNCF		byteDTX, F, BANKED	; C:1 rotate 1 bit current part 
	BTFSC		pcm_3210_dtx, ACCESS	; C:1 test DTX, skip if 0
	BSF		byteDTX, 0, BANKED	; C:1 if it was 1, set our bit 0

						; T:@+21

	; task (iv): wait until time @+24 (this is not the first round, so
	; we wait 24 TCy before lowering PCLK), then lower PCLK
	NOP					; C:1 wait until T+24
	NOP					; C:1
	NOP					; C:1
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

	; done
	RETFIE		FAST			; C:2


;	At this point, other than the last of the 32 cycles, all we have
;	to do is lower PCLK at the right time (@+24). For the last cycle
;	(cnt4==3 and cnt8==7) we need to pulse FSYNC up and down
notfirst8of32					; T:@+11

	; 246 + 3 + 7 yields zero, so this is the fastest test I could think of
	; (it's ok only because cnt4 cannot be > 3 and cnt8 cannot be > 7)
	MOVLW		246			; C:1
	ADDWF		cnt4, W, BANKED		; C:1 W := cnt4 - 10
	ADDWF		cnt8, W, BANKED		; C:1
	BNZ		notlastof32		; C:1/2

	; (following label is just for clarity, there is not any jump into it)
lastof32					; T:@+15
      #ifdef DEBUG_CYCLES

	#if 0
	; NOTE: this block adds extra cycles to the ISR, so it's good just
	; once, just before the point that needs to be measured. It's if'ed
	; out here, so it won't be included even if DEBUG_CYCLES is defined;
	; if testing, you must copy-paste it to the appropriate point in the
	; ISR code; if it is for 'lastcycle' (here), it should be if'ed back
	; in (for testing purposes only)
      	MOVF		TMR3L, W, ACCESS
	MOVWF		passout, BANKED
      	MOVF		TMR3H, W, ACCESS
	MOVWF		pasHout, BANKED
	#endif

						; T:@+15

	NOP					; C:1

						; T:@+16

	; task (i): re-arm TMR1 to fire at @+47-5
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		234			; C:1 next interrupt at @+47-5
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+19

	; task (ii): while waiting until time @+24 in order to lower PCLK,
	; synchronize TMR3 with TMR1, so that TMR3 will be zero at time @
	; on next (cnt4==cnt8==0)
      	SETF		TMR3H, ACCESS		; C:1
      	MOVLW		231			; C:1
	MOVWF		TMR3L, ACCESS		; C:1
	BSF		pcm_3210_fsync, ACCESS	; C:1
	;NOP					; C:1
	NOP					; C:1

						; T:@+24

	; task (iii): lower PCLK
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

        ; done
	RETFIE		FAST			; C:2

      #else ; DEBUG_CYCLES
      						; T:@+15

	INCF		ppindex, W, BANKED	; C:1 ppindex about to overflow?
	BTFSC		WREG, 3, ACCESS		; C:1/2
	BRA		lastnosync		; C:2

      						; T:@+18

	; (following label is just for clarity, there is not any jump into it)
syncSOF
	; task (i): check if we have seen SOF at least once so far
	BTFSC		seensof, 0, BANKED	; C:1/2 if SOF seen before,
	BRA		dosyncSOF		; C:2  branch forward

						; T:@+20

	; task (ii): never seen SOF before, check if it is asserted now,
	; and if so, set seensof for next round
	BTFSC		UIR, SOFIF, ACCESS	; C:1/2 
	SETF		seensof, BANKED		; C:1

						; T:@+22 via both paths

	; task (iii): wait until @+24, then lower PCLK
	BSF		pcm_3210_fsync, ACCESS	; C:1
	;NOP					; C:1
	NOP

						; T:@+24

	BCF		pcm_3210_pclk, ACCESS	; C:1

	; task (iv): re-arm TMR1 to fire again at @+47-5

						; T:@+25

	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		243			; C:1 next interrupt at @+47-5
	MOVWF		TMR1L, ACCESS;		; C:1


	; done
	RETFIE		FAST			; C:2


dosyncSOF					; T:@+21

	; task (i): wait until @+24, then lower PCLK
	NOP					; C:1
	BSF		pcm_3210_fsync, ACCESS	; C:1
	NOP					; C:1
	;NOP					; C:1

						; T:@+24

	BCF		pcm_3210_pclk, ACCESS	; C:1


						; T:@+25

	; task (ii): synchronize with SOF
waitSOF
SOFloop	BTFSS		UIR, SOFIF, ACCESS	; C:1/2 break loop if SOF is set
	BRA		SOFloop			; C:2 otherwise loop waiting
	BCF		UIR, SOFIF, ACCESS	; C:1 clear SOFIF for next time

	
	; after the loop, T can be anything, but it cannot be less than @+28;
	; initially, we can safely assume that SOF will be have been set at
	; some time before now, so @+28 will rather be the rule the first time
	; we go through this code; with this assumption, we shall 'steal'
	; 0.5 microseconds from the ISR's time, by pretending that T is now
	; equal to @+34 and re-arming TMR1 accordingly; this will result
	; in the ISR executing at a somewhat faster pace than SOF; thus,
	; the point in time at which SOF is set during the execution of
	; the ISR will drift slowly and occur later and later, until
	; eventually 'waitSOF' will be reached slightly before SOF is set;
	; this will result in the code looping until SOF appears,
	; so effectively we will have synchronized with SOF!

						; T:@+34 (fake)
	; task (v): re-arm TMR1 and return
	SETF		TMR1H, ACCESS		; C:1
	MOVLW		252			; C:1
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+37 (fake)
	; done
	RETFIE		FAST			; C:2



	; we jump here in 7 out of 8 invocations, when we do not need to
	; do SOF synchronization, so we just re-arm TMR1 and return
lastnosync					; T:@+19
	;
	; task (i): re-arm TMR1 to fire at @+47-5
	SETF		TMR1H, ACCESS		; C:1
	MOVLW		237			; C:1
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+22
	; task (ii): wait until @+24, then lower PCLK
	BSF		pcm_3210_fsync, ACCESS	; C:1
	;NOP					; C:1
	NOP					; C:1

						; T:@+24

	BCF		pcm_3210_pclk, ACCESS	; C:1

	; done
	RETFIE		FAST			; C:2
      #endif ; DEBUG_CYCLES




notlastof32					; T:@+16
; 	In this case (27 out of 32 invocations fall here), all that's left
;	for us to do is re-arm, wait, lower PCLK and return.
	; task (ii): re-arm TMR1 to fire at @+47-5
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		234			; C:1 next interrupt at @+47-5
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+19

	NOP					; C:1
	NOP					; C:1
	NOP					; C:1
	NOP					; C:1
	NOP					; C:1

						; T:@+24

	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+25
	; done
	RETFIE		FAST			; C:2


firstof8not32					; T:@+12

;	At this point, we are running the first cnt8 (cnt8 == 0) round of a
;	cnt4 round other than the first one (cnt4 != 0). We have to do
;	different things depending on the value of cnt4, so we test for
;	the value of cnt4 first.

	DECF		cnt4, W, BANKED		; C:1 W := cnt4 - 1
	BNZ		notsecondof4		; C:1/2 if (cnt4 > 1), jump on


;	We are running the first cnt8 (cnt8 == 0) round of the second cnt4
;	(cnt4 == 1) round. PCM I/O had been going on while (cnt4 == 0), so
;	now it has just finished and it's time to move the relevant bytes
;	to and from the respective USB PCM I/O buffers. Notice that we
;	finish this cycle without returning to user space, because we take
;	quite long and we finish right on time for a jump back to the start
;	of the ISR. Our tasks for this section are: (i) save FSRs to temp
;	storage (FSRs are very powerful registers for optimizing all the
;	tasks that we need to do), (ii) lower PCLK (has to happen now for
;	timing reasons), (iii) initialize FSRs 1 and 2 and REGW with
;	appropriate values, (iv) pass data from USB buffer to ISR bit buffer,
;	(v) pass data from ISR bit buffer to USB buffer, (vi) restore FSRs

;	NOTE: this round, just like all other rounds with cnt8==0, requires
;	a PCLK-high half-cycle of 23TCy in duration (PCLK lowered at @+23)


	; (following label is just for clarity, there is not any jump into it)
secondof4					; T:@+14

	; task (i): save FSR1 and FSR2 to temp storage
	MOVFF		FSR1L, sfsr1l		; C:2 save FSR1L in ISR space
	MOVFF		FSR1H, sfsr1h		; C:2 save FSR1H in ISR space
	MOVFF		FSR2L, sfsr2l		; C:2 save FSR2L in ISR space
	MOVFF		FSR2H, sfsr2h		; C:2 save FSR2H in ISR space

						; T:@+22

	NOP					; C:1
	; task (iii) [must occur at @+23]: lower PCLK
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+24

	; task (ii): initialize FSR1 and FSR2 for passing data between
	; the USB buffers and our local bytes
	LFSR		FSR1, OUTPCMData0	; C:2 FSR1 := &OUTPCMData0[0]
	LFSR		FSR2, byteDRX		; C:2 FSR2 := &byteDRX
	MOVF		ppindex, W, BANKED	; C:1 add the index incl. p-p

						; T:@+29

	; task (iv): move one byte of data from the OUT USB data buffer into
	; the byteDRX bit buffer

	MOVFF		PLUSW1, POSTINC2	; C:2 move byte indexed by
						;     FSR1+W (OUTPCMData0 +
						;     ppindex) into the byte
						;     indexed by FSR2 (byteDRX),
						;     then post-increment FSR2,
						;     which now points to
						;     byteDTX

						; T:@+31


	; task (v): move one byte of data from the byteDTX bit buffer into the
	; IN_ USB data buffer
	LFSR		FSR1, IN_PCMData0	; C:2
	MOVFF		INDF2, PLUSW1		; C:2 move byte indexed by
						;     FSR2 (byteDTX) into the
						;     byte indexed by FSR1+W
						;     (IN_PCMData0+ppindex)

						; T:@+35

	; task (vi): restore FSR registers from temporary storage
	MOVFF		sfsr1l, FSR1L		; C:2 restore FSR1L
	MOVFF		sfsr1h, FSR1H		; C:2 restore FSR1H
	MOVFF		sfsr2l, FSR2L		; C:2 restore FSR2L
	MOVFF		sfsr2h, FSR2H		; C:2 restore FSR2H

						; T:@+43

	NOP					; C:1

						; T:@+44
	; by means of the following BRA, we get back to the start of our
	; ISR just in time (@+46)
	BRA		tmr1_isr		; C:2






notsecondof4					; T:@+15

;	At this point, we are running the first cnt8 (cnt8 == 0) round of a
;	cnt4 round other than the first two (cnt4 != 0, cnt4 != 1). We have
;	to do different things depending on the value of cnt4, so we test for
;	the value of cnt4 first.

	DECF		WREG, W, ACCESS		; C:1 W := cnt4 - 2
	BNZ		notthirdof4		; C:1/2

	; (following label is just for clarity, there's not any jump into it)
thirdof4					; T:@+17

;	We are at  the first cnt8 (cnt8 == 0) round of the third cnt4
;	(cnt4 == 2) round. During the previous (cnt4 == 1) round, and
;	we did I/O between the USB- and the bit-buffer space.
;	Now it's time to increment our	USB buffer index ppindex, and
;	check if an overflow occured; if so, we are to initiate USB I/O.
;	Our tasks here are to: (i) increas ppindex modulo 32, (ii) check
;	if we overflowed our 8-byte PCM audio buffer space; if not, just
;	re-arm TMR1 and return; if yes, (iii) distinguish if we are to
;	do an even or an odd-phase ping-pong transmission, (iv) setup
;	the respective BD for transmission, (v) lower PCLK at T:@+23,
;	and (vi) either re-arm TMR1 or wait and loop over

; FIXDOC: rewrite tasks, PCLK has moved up (now is task (iii))

;	NOTE: this round, just like all other rounds with cnt8==0, requires
;	a PCLK-high half-cycle of 23TCy in duration (PCLK lowered at @+23)

	; task (i): increase ppindex modulo 32
	INCF		ppindex, F, BANKED	; C:1
	MOVLW		0x1f			; C:1
	ANDWF		ppindex, F, BANKED	; C:1

						; T:@+20

	; task (ii): check whether we have overflowed: a valid ppindex
	; must be between 8 and 15 or between 24 and 31, so it must
	; always have bit 3 set; if not, we have just overflowed.
	BTFSC		ppindex, 3, BANKED	; C:1/2
	BRA		to4noovfl		; C:2

						; T:@+22
	NOP					; C:1
	; task (iii): lower PCLK, right at @+23
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK
						
						; T:@+24

;	So now we have overflowed, and we must transmit either
;	the packet IN_PCMData0 or IN_PCMData1, using the BD at
;	address either 0x428 or 0x42c, depending on whether we
;	are in an even or an odd ping-pong phase, respectively;
;	bit 4 of ppindex tells us in which ping-pong phase we
;	are: if bit 4 is set, this means that we have just
;	finished with the packet IN_PCMData0; if it is clear,
;	this means we have just finished with the packet
;	IN_PCMData1

	; task (iv): distinguish between an even and an odd ping-pong phase
	BTFSS		ppindex, 4, BANKED	; C:1/2
	BRA		usbIN_odd		; C:2

	; (following label is just for clarity, there's not any jump into it)
usbIN_evn					; T:@+26

	; if we are (re) synchronizing with the SOF frame, don't do the
	; actual transmit;
	; will have to remove stuff here; patched with a NOP to keep timing
	;BTFSC		inisync, 0, BANKED	; C:1/2 do initial sync?
	NOP
	BRA		nosyncevn		; C:2 skip initial sync if not


nosyncevn					; T:@+29
	; clear the SOF flag - REMOVED
	NOP
	;BCF		UIR, SOFIF, ACCESS	; C:1 clear SOF interrupt flag

						; T:@+30

	BTFSC		pauseIO, 0, BANKED	; C:1/2
	BRA		atpls33			; C:2

						; T:@+32

	INCF		IN_EVN_SERIAL,BANKED	; C:1
	NOP					; C:1

						; T:@+34


	; task (iv) case a: transmit even buffer using even BD

      #ifdef DEBUG_PCM_MIRROR
      	; mirror back previously received OUTPCMData1
	MOVLW		HIGH OUTPCMData1	; C:1 bank part of OUTPCMData1
	MOVWF		EP2IEAH, BANKED		; C:1 store it in BD
	MOVLW		LOW  OUTPCMData1	; C:1 offset part of OUTPCMData1
	MOVWF		EP2IEAL, BANKED		; C:1 store it in BD
      #else
	MOVLW		HIGH IN_PCMData0	; C:1 bank part of IN_PCMData0
	MOVWF		EP2IEAH, BANKED		; C:1 store it in BD
	MOVLW		LOW  IN_PCMData0	; C:1 offset part of IN_PCMData0
	MOVWF		EP2IEAL, BANKED		; C:1 store it in BD
      #endif
	MOVLW		0x10			; C:1 packet length, 16
	MOVWF		EP2IECN, BANKED		; C:1 store it in BD
	MOVLW		0x40			; C:1 just keep the DTS bit
	ANDWF		EP2IEST, BANKED		; C:1 
	MOVLW		0x88			; C:1 set UOWN and DTSEN bits
	IORWF		EP2IEST, BANKED		; C:1 that's it, packet queued!

						; T:@+44


	; right on time to loop back!
	BRA		tmr1_isr		; C:2

	; various NOP-filled delays until we loop back, used as jump labels
	; (note that these apply only for 1st-of-8 cycles, where the period
	; of the cycle is 23+23=46 TCy)
atpls31						; T:@+31
	NOP					; C:1
atpls32						; T:@+32
	NOP					; C:1
atpls33						; T:@+33
	NOP					; C:1
atpls34						; T:@+34
	NOP					; C:1
atpls35						; T:@+35
	NOP					; C:1
atpls36						; T:@+36
	NOP					; C:1
atpls37						; T:@+37
	NOP					; C:1
atpls38						; T:@+38
	NOP					; C:1
atpls39						; T:@+39
	NOP					; C:1
atpls40						; T:@+40
	NOP					; C:1
atpls41						; T:@+41
	NOP					; C:1
atpls42						; T:@+42
	NOP					; C:1
atpls43						; T:@+43
	NOP					; C:1
						; T:@+44
	BRA		tmr1_isr		; C:2


usbIN_odd					; T:@+27
	; clear the SOF flag - REMOVED
	NOP

						; T:@+28
	BTFSC		pauseIO, 0, BANKED	; C:1/2
	BRA		atpls31			; C:2

						; T:@+30

      #ifdef DEBUG_USB_TMR3
	; pass current TMR3 value in odd packets; this is a very useful
	; tool to help make sure that timing is 100% OK;
	; 
        MOVFF		TMR3L, IN_ODD_TMR3LV	; C:2
        MOVFF		TMR3H, IN_ODD_TMR3HV	; C:2
      #else
        ; increment current odd-packet serial # and pass DTMF/hook status
	INCF		IN_ODD_SERIAL, BANKED	; C:1
	MOVFF		hkdtmf, IN_ODD_HKDTMF	; C:2
	NOP					; C:1
      #endif

						; T:@+34

	; task (iv) case b: transmit odd buffer using odd BD
      #ifdef DEBUG_PCM_MIRROR
      	; mirror back previously received OUTPCMData0
	MOVLW		HIGH OUTPCMData0	; C:1 bank part of IN_PCMData1
	MOVWF		EP2IOAH, BANKED		; C:1 store it in BD
	MOVLW		LOW  OUTPCMData0	; C:1 offset part of IN_PCMData1
	MOVWF		EP2IOAL, BANKED		; C:1 store it in BD
      #else
	MOVLW		HIGH IN_PCMData1	; C:1 bank part of IN_PCMData1
	MOVWF		EP2IOAH, BANKED		; C:1 store it in BD
	MOVLW		LOW  IN_PCMData1	; C:1 offset part of IN_PCMData1
	MOVWF		EP2IOAL, BANKED		; C:1 store it in BD
      #endif
	MOVLW		0x10			; C:1 packet length, 16
	MOVWF		EP2IOCN, BANKED		; C:1 store it in BD
	MOVLW		0x40			; C:1 just keep the DTS bit
	ANDWF		EP2IOST, BANKED		; C:1 
	MOVLW		0x88			; C:1 set UOWN and DTSEN bits
	IORWF		EP2IOST, BANKED		; C:1 that's it, packet queued!

						; T:@+44

	BRA tmr1_isr				; C:2


to4noovfl					; T:@+23
;	We come here if ppindex indicates no overflow, so all we need to do
;	is to lower PCLK, re-arm TMR1 and return

	; task (ii): lower PCLK at time @+23
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+24

	; task (iii): re-arm TMR1 to fire again at @+46-5
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		243			; C:1 next interrupt at @+46-5
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+27
	; we are done
	RETFIE		FAST			; C:2


notthirdof4					; T:@+18


;	At this point, we are running the first cnt8 (cnt8 == 0) round of
;	the last cnt4 round (cnt4 == 3).<addtasks>

;	NOTE: this round, just like all other (cnt8 == 0) rounds requires
;	a PCLK-high half-cycle of 23TCy in duration (PCLK lowered at @+23)


	; (following label is just for clarity, there's not any jump into it)
fourthof4					; T:@+18

      	; task (i): check (again) whether we have overflowed our 8-byte
	; audio buffer by our last incrementing ppindex (in thirdof4)
	BTFSC		ppindex, 3, BANKED	; C:1/2
	BRA		fo4noovfl		; C:2

; FIXME: FIXDOC HERE
;	Here again, we must re-initiate a read for either OUTPCMData0
;	or OUTPCMData1, using the BD at address either 0x424 or 0x428
;	respectively, depending on the ping-pong phase. If bit 4 of
;	ppindex is set, this means that we have just finished sending
;	OUTPCMData0 to the 3210 and we may arm a USB receive for data
;	to fill that buffer; if	bit 4 of ppindex is clear, this means
;	that we have just finished sending OUTPCMData1 to the 3210
;	and we may arm a USB receive for data to fill that.

						; T:@+20

	BTFSC		outodev, 0, BANKED	; C:1/2
	BRA		usbOUTodd		; C:2

	; (following label is just for clarity, there's not any jump into it)
usbOUTevn					; T:@+22

	; task (iii): fix overflow condition by setting bit 3 of ppindex
	BSF		ppindex, 3, BANKED	; C:1

						; T:@+23

	; task (iv): lower PCLK, right at @+23
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+24

	; task (v) case a: check even BD's UOWN flag to see if I/O is ready
	BTFSC		EP2OEST, 7, BANKED	; C:1/2 check UOWN bit (bit is
						;	zero if I/O is finished)
	BRA		usbOUTNoData27		; C:2

						; T:@+26
	
#ifdef DEBUG_USB_NO_OUT
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		245			; C:1 next interrupt at @+46-5
	MOVWF		TMR1L, ACCESS		; C:1

	RETFIE		FAST			; C:2
#else

	; task (vi) case a: arm a receive to prepare receiving the even BD

	MOVLW		HIGH OUTPCMData0	; C:1 bank part of OUTPCMData0
	MOVWF		EP2OEAH, BANKED		; C:1 store it in BD
	MOVLW		LOW  OUTPCMData0	; C:1 offset part of OUTPCMData0
	BTFSC		ppindex, 4, BANKED	; C:1 if ppindex->OUTPCMData0,
	BSF		WREG, 4, ACCESS		; C:1 change it to OUTPCMData1 
	MOVWF		EP2OEAL, BANKED		; C:1 store it in BD
	MOVLW		0x10			; C:1 packet length, 16
	MOVWF		EP2OECN, BANKED		; C:1 store it in BD
	MOVLW		0x40			; C:1 just keep the DTS bit
	ANDWF		EP2OEST, BANKED		; C:1 
	MOVLW		0x88			; C:1 set UOWN and DTSEN bits
	IORWF		EP2OEST, BANKED		; C:1 that's it, receive armed!

						; T:@+38

	; task (vii): toggle outodev (next time do an odd OUT)

	BTG		outodev, 0, BANKED	; C:1

						; T:@+39

	; it's too late to return, so wait until it's time and then restart
      #ifdef DEBUG_USB_SERIAL
	; copy serial from last OUT into next IN packet
      	MOVFF		OUTPCMData1+3,IN_ODD_MOUTSN
						; C:2
      #else
	NOP					; C:1
	NOP					; C:1
      #endif
	NOP					; C:1
	NOP					; C:1
	NOP					; C:1

						; T:@+44

	BRA tmr1_isr				; C:2 ...just at @+46

#endif

usbOUTodd					; T:@+23

	; task (iii): lower PCLK, right at @+23
	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+24

	; task (iv): fix overflow condition by setting bit 3 of ppindex
	BSF		ppindex, 3, BANKED	; C:1

						; T:@+25
	; task (v) case b: check odd BD's UOWN flag to see if I/O is ready
	BTFSC		EP2OOST, 7, BANKED	; C:1/2 check UOWN bit (bit is
						;	zero if I/O is finished)
	BRA		usbOUTNoData28		; C:2

						; T:@+27

#ifdef DEBUG_USB_NO_OUT
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		246			; C:1 next interrupt at @+46-5
	MOVWF		TMR1L, ACCESS		; C:1

	RETFIE		FAST			; C:2
#else

	; task (vi) case b: arm a receive to prepare receiving the odd BD
	MOVLW		HIGH OUTPCMData0	; C:1 bank part of OUTPCMData0
	MOVWF		EP2OOAH, BANKED		; C:1 store it in BD
	MOVLW		LOW  OUTPCMData0	; C:1 offset part of OUTPCMData0
	BTFSC		ppindex, 4, BANKED	; C:1 if ppindex->OUTPCMData0,
	BSF		WREG, 4, ACCESS		; C:1 change it to OUTPCMData1 
	MOVWF		EP2OOAL, BANKED		; C:1 store it in BD
	MOVLW		0x10			; C:1 packet length, 16
	MOVWF		EP2OOCN, BANKED		; C:1 store it in BD
	MOVLW		0x40			; C:1 just keep the DTS bit
	ANDWF		EP2OOST, BANKED		; C:1 
	MOVLW		0x88			; C:1 set UOWN and DTSEN bits
	IORWF		EP2OOST, BANKED		; C:1 that's it, receive armed!

						; T:@+39

	; task (vii): toggle outodev (next time do an even OUT)

	BTG		outodev, 0, BANKED	; C:1

						; T:@+40
	
	; it's too late to return, so wait until it's time and then restart
      #ifdef DEBUG_USB_SERIAL
	; copy serial from last OUT into next IN packet
      	MOVFF		OUTPCMData0+3,IN_EVN_MOUTSN
						; C:2
      #else
	NOP					; C:1
	NOP					; C:1
      #endif
	NOP					; C:1
	NOP					; C:1

						; T:@+44

	BRA tmr1_isr				; C:2 ...just at @+46
#endif



fo4noovfl					; T:@+21

	; task (ii): wait, then lower PCLK
	NOP					; C:1
	NOP					; C:1

						; T:@+23

	BCF		pcm_3210_pclk, ACCESS	; C:1 lower PCLK

						; T:@+24

	; task (iii): re-arm TMR1 to fire again at @+46-5
	SETF		TMR1H, ACCESS		; C:1 set TMR1H to 0xFF
	MOVLW		243			; C:1 next interrupt at @+46-5
	MOVWF		TMR1L, ACCESS		; C:1

						; T:@+27
	; done
	RETFIE		FAST			; C:2





#if 1

usbOUTNoData27					; T:@+27

	NOP					; C:1

usbOUTNoData28					; T:@+28

	BTFSC		ppindex, 4, BANKED	; C:1/2
	BRA		usbOUTNoData1		; C:2

usbOUTNoData0					; T:@+30

	SETF		OUTPCMData0+0x8, BANKED	; C:1
	SETF		OUTPCMData0+0x9, BANKED	; C:1
	SETF		OUTPCMData0+0xA, BANKED	; C:1
	SETF		OUTPCMData0+0xB, BANKED	; C:1
	SETF		OUTPCMData0+0xC, BANKED	; C:1
	SETF		OUTPCMData0+0xD, BANKED	; C:1
	SETF		OUTPCMData0+0xE, BANKED	; C:1
	SETF		OUTPCMData0+0xF, BANKED	; C:1

						; T:@+38
	
      #ifdef DEBUG_LOST_OUT
      	INCF		IN_ODD_LOSSES, BANKED	; C:1
	BRA		atpls41			; C:2
      #else
	BRA		atpls40			; C:2
      #endif

usbOUTNoData1					; T:@+31
	SETF		OUTPCMData1+0x8, BANKED	; C:1
	SETF		OUTPCMData1+0x9, BANKED	; C:1
	SETF		OUTPCMData1+0xA, BANKED	; C:1
	SETF		OUTPCMData1+0xB, BANKED	; C:1
	SETF		OUTPCMData1+0xC, BANKED	; C:1
	SETF		OUTPCMData1+0xD, BANKED	; C:1
	SETF		OUTPCMData1+0xE, BANKED	; C:1
	SETF		OUTPCMData1+0xF, BANKED	; C:1

						; T:@+39
	
      #ifdef DEBUG_LOST_OUT
      	INCF		IN_ODD_LOSSES, BANKED	; C:1
	BRA		atpls42			; C:2
      #else
	BRA		atpls41			; C:2
      #endif


#else
usbOUTevnNoData					; T:@+27

	MOVLW		0xFF			; C:1
	MOVWF		OUTPCMData0+0x8, BANKED	; C:1
	MOVWF		OUTPCMData0+0x9, BANKED	; C:1
	MOVWF		OUTPCMData0+0xA, BANKED	; C:1
	MOVWF		OUTPCMData0+0xB, BANKED	; C:1
	MOVWF		OUTPCMData0+0xC, BANKED	; C:1
	MOVWF		OUTPCMData0+0xD, BANKED	; C:1
	MOVWF		OUTPCMData0+0xE, BANKED	; C:1
	MOVWF		OUTPCMData0+0xF, BANKED	; C:1

						; T:@+36
      #ifdef DEBUG_LOST_OUT
      	INCF		IN_ODD_LOSSES, BANKED	; C:1
	BRA		atpls39			; C:2
      #else
	BRA		atpls38			; C:2
      #endif


usbOUToddNoData					; T:@+28

	MOVLW		0xFF			; C:1
	MOVWF		OUTPCMData1+0x8, BANKED	; C:1
	MOVWF		OUTPCMData1+0x9, BANKED	; C:1
	MOVWF		OUTPCMData1+0xA, BANKED	; C:1
	MOVWF		OUTPCMData1+0xB, BANKED	; C:1
	MOVWF		OUTPCMData1+0xC, BANKED	; C:1
	MOVWF		OUTPCMData1+0xD, BANKED	; C:1
	MOVWF		OUTPCMData1+0xE, BANKED	; C:1
	MOVWF		OUTPCMData1+0xF, BANKED	; C:1

						; T:@+37
      #ifdef DEBUG_LOST_OUT
       	INCF		IN_ODD_LOSSES, BANKED	; C:1
	BRA		atpls40			; C:2
      #else
	BRA		atpls39			; C:2
      #endif
#endif


;	Following code section is relocatable
tmr1isrinit CODE


; The TMR1 ISR initialization function
tmr1_isr_init

	; disable TMR1 interrupts while we are working
	BCF		PIE1, TMR1IE, ACCESS

	; (re-?) initialize all ISR data and outputs
	BANKSEL		cnt8
	MOVLW		7
	MOVWF		cnt8, BANKED		; set cnt8 to 7 and cnt4 to 3
	MOVLW		3			; so that the first time around
	MOVWF		cnt4, BANKED		; we start a new 32-cycle train

	MOVLW		8			; set ppindex to 8
	MOVWF		ppindex, BANKED

	CLRF		outodev, BANKED		; set outodev to zero

	CLRF		passout, BANKED		; set passout to 0

	CLRF		pauseIO, BANKED		; disable USB I/O (FIXME/NOTYET)

	CLRF		seensof, BANKED		; have not seen SOF yet

	; make sure PCLK and FSYNC are set to zero
	BCF		pcm_3210_pclk, ACCESS	; lower PCLK
	BCF		pcm_3210_fsync, ACCESS	; lower FSYNC

	; enable SOFIE
	BSF		UIE, SOFIE, ACCESS	;

	; re-enable TMR1 interrupts
	BSF		PIE1, TMR1IE, ACCESS	;

	; done
	RETURN


	END
