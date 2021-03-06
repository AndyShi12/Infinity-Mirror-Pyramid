/*
************************************************************************
 ECE 362 - Mini-Project C Source File - Fall 2014                    
***********************************************************************
                                  
 Team ID: Team 6

 Project Name: Infinity Mirror Pyramid (Audio Visualizer)

 Team Members:

   - Team/Doc Leader: Mark Oscai     Signature: 
   
   - Software Leader: Andy Shi      Signature: 

   - Interface Leader: Jeff Heath     Signature:  

   - Peripheral Leader: Thaddeus Morken    Signature: 


 Academic Honesty Statement:  In signing above, we hereby certify that we 
 are the individuals who created this HC(S)12 source file and that we have
 not copied the work of any other student (past or present) while completing 
 it. We understand that if we fail to honor this agreement, we will receive 
 a grade of ZERO and be subject to possible disciplinary action.

***********************************************************************

 The objective of this Mini-Project is to .... 
 
 Create a 4 mode LED strip controller:
  mode 1: Solid RGB
  mode 2: Spectrum
  mode 3: Fade
  mode 4: Audio input


***********************************************************************

 List of project-specific success criteria (functionality that will be
 demonstrated):

 1. Solid RGB, potentiometer input (ATD), power output to LED strip (PWM)

 2. Spectrum, timer interrupt - preset colors  (TIM/RTI)

 3. Fade, same as RGB mode with an extra potentiometer for frequency adjust
                                                                            
 4. Audio mode, ATD input for audio signal, PWM output to LED strip

 5. SPI for LCD display (SPI)

***********************************************************************

  Date code started: 11/20/2014

  Update history (add an entry every time a significant change is made):

  See Github, https://github.com/AndyShi12/ECE362

***********************************************************************
*/

//*********************************************************************** Declaration

#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include <mc9s12c32.h>

/* All functions after main should be initialized here */
void outchar(char x); // for debugging use only
char inchar(void);  // for bonus option (terminal input for setting clock)
void rdisp(void);   // RPM display
void bco(char x);   // SCI buffered character output
void shiftout(char);  // LCD drivers (written previously)
void lcdwait(void);
void send_byte(char);
void send_i(char);
void chgline(void);
void print_c(char);
void pmsglcd(char[]);

/* Variable declarations */
char leftpb = 0;  // left pushbutton flag
char rghtpb = 0;  // right pushbutton flag
char prevpb = 0;  // previous pushbutton state
char runstp = 0;  // motor run/stop flag
char onesec   = 0;  // one second flag
char tenths = 0;  // tenth of a second flag
char tin  = 0;  // SCI transmit display buffer IN pointer
char tout = 0;  // SCI transmit display buffer OUT pointer

char * message;
int pulscnt = 0;  // pulse count (read from PA every second)
int color = 1;

int prevRight=0, prevLeft=0;
int red = 0, blue = 0, green = 0, white = 0;
int isRising = 1, skip = 0, freq = 1;
int colormode = 1;
int brightness=1;

unsigned int fade = 0;
unsigned int in0;
unsigned int in1;
unsigned int in2;
unsigned int in3;
unsigned int in4;
unsigned int in5;
unsigned int in6;
unsigned int in7;
                                   

/* Special ASCII characters */
#define TSIZE 81  // transmit buffer size (80 characters)
char tbuf[TSIZE]; // SCI transmit display buffer

#define CR 0x0D   // ASCII return 
#define LF 0x0A   // ASCII new line 

/* LCD COMMUNICATION BIT MASKS (note - different than previous labs) */
#define RS 0x10   // RS pin mask (PTT[4])
#define RW 0x20   // R/W pin mask (PTT[5])
#define LCDCLK 0x40 // LCD EN/CLK pin mask (PTT[6])

/* LCD INSTRUCTION CHARACTERS */
#define LCDON 0x0F  // LCD initialization command
#define LCDCLR 0x01 // LCD clear display command
#define TWOLINE 0x38  // LCD 2-line enable command
#define CURMOV 0xFE // LCD cursor move instruction
#define LINE1 = 0x80  // LCD line 1 cursor position
#define LINE2 = 0xC0  // LCD line 2 cursor position
          
//*********************************************************************** /Declaration

//*********************************************************************** Initialization

void  initializations(void) {

/* Set the PLL speed (bus clock = 24 MHz) */
  CLKSEL = CLKSEL & 0x80; // disengage PLL from system
  PLLCTL = PLLCTL | 0x40; // turn on PLL
  SYNR = 0x02;            // set PLL multiplier
  REFDV = 0;              // set PLL divider
  while (!(CRGFLG & 0x08)){  }
  CLKSEL = CLKSEL | 0x80; // engage PLL

/* Disable watchdog timer (COPCTL register) */
  COPCTL = 0x40   ; // COP off; RTI and COP stopped in BDM-mode

/* Initialize asynchronous serial port (SCI) for 9600 baud, interrupts off initially */
  SCIBDH =  0x00; //set baud rate to 9600
  SCIBDL =  0x9C; //24,000,000 / 16 / 156 = 9600 (approx)  
  SCICR1 =  0x00; //$9C = 156
  SCICR2 =  0x0C; //initialize SCI for program-driven operation
  DDRB   =  0x10; //set PB4 for output mode
  PORTB  =  0x10; //assert DTR pin on COM port
            
/* 
   Initialize TIM Ch 7 (TC7) for periodic interrupts every 10.0 ms  
    - Enable timer subsystem                         
    - Set channel 7 for output compare
    - Set appropriate pre-scale factor and enable counter reset after OC7
    - Set up channel 7 to generate 10 ms interrupt rate
    - Initially disable TIM Ch 7 interrupts                                 
*/                                  
  TSCR1 = 0x80;
  TSCR2 = 0x0C;
  TIOS = 0x80;
  TIE = 0x00;
  TC7 = 15000;
  TIE_C7I = 0;

/*
 Initialize the PWM unit to produce an input signal on AN0-AN4
*/                                
  PWME = 0x0F;
  PWMPOL = 0x0F;
// Set PWMPER and PWMDTY to each input/output
  PWMPER0 = 0xFF; 
  PWMDTY0 = 0x00;
  PWMPER3 = 0xFF;
  PWMDTY3 = 0x00;
  PWMPER2 = 0xFF;
  PWMDTY2 = 0x00;
  PWMPER1 = 0xFF;
  PWMDTY1 = 0x00;
  
  PWMCLK_PCLK0 = 1;
  PWMCLK_PCLK1 = 1;
  PWMCLK_PCLK2 = 1;
  PWMCLK_PCLK3 = 1;
  
  PWMPRCLK = 0x33;
  PWMSCLA = 0x3B;
  PWMSCLB = 0x3B;  

  MODRR_MODRR0 = 1; 
  MODRR_MODRR1 = 1; 
  MODRR_MODRR2 = 1; 
  MODRR_MODRR3 = 1; 

/* 
 Initialize the ATD to sample a D.C. input voltage (range: 0 to 5V)
 on Channel 0 (connected to a 10K-ohm potentiometer). The ATD should
 be operated in a program-driven (i.e., non-interrupt driven), normal
 flag clear mode using nominal sample time/clock prescaler values,
 8-bit, unsigned, non-FIFO mode.
                         
 Note: Vrh (the ATD reference high voltage) is connected to 5 VDC and
       Vrl (the reference low voltage) is connected to GND on the 
       9S12C32 kit.  An input of 0v will produce output code $00,
       while an input of 5.00 volts will produce output code $FF
*/                                  
  ATDCTL2 = 0x80;
  ATDCTL3 = 0x38; //00111000    
  ATDCTL4 = 0x85;
                                
/*
  Initialize the RTI for an interrupt rate
*/
  CRGINT = 0x80;
  RTICTL = 0x1F;  
  
/*
  Initialize SPI for baud rate of 6 Mbs, MSB first
  (note that R/S, R/W', and LCD clk are on different PTT pins)
*/
  SPICR1 = SPICR1 | 0x58;
  SPIBR_SPPR0 = 0x01;
  SPIBR_SPR0 = 0x00;

/* Initialize digital I/O port pins */
  DDRT=0xff;    //DDRT = 0x7F;
  DDRM = 0xFF;
  DDRAD = 0;
  ATDDIEN = 0xC0;
  
/* 
   Initialize the LCD
     - pull LCDCLK high (idle)
     - pull R/W' low (write state)
     - turn on LCD (LCDON instruction)
     - enable two-line mode (TWOLINE instruction)
     - clear LCD (LCDCLR instruction)
     - wait for 2ms so that the LCD can wake up     
*/ 
     PTT_PTT6 = 1;  //clock idle as high 
     PTT_PTT5 = 0;  //LCD write mode
     send_i(LCDON);
     send_i(TWOLINE);
     send_i(LCDCLR);
     lcdwait();                                        
        
}
//******************************************************************; ***** /Initialization
                                                                 
//*********************************************************************** Main
void main(void) {
  DisableInterrupts
  initializations();                      
  EnableInterrupts;

  TIE_C7I = 1;
  colormode = 1;
  PWMDTY3 = 255;
  rdisp();
  for(;;) {

//*********************************************************************** Solid mode
  if(colormode == 1)
  {
    ATDCTL5 = 0x10;      
      while((128&ATDSTAT0)==0) 
      {} 
    // Sample input
    in0 = ATDDR4H;     
    in1 = ATDDR1H;
    in2 = ATDDR2H;
    in3 = ATDDR3H;
    // Assign output
    PWMDTY0 = in0;
    PWMDTY1 = in1;
    PWMDTY2 = in2;
    PWMDTY3 = in3;
    // change to mode 2
    if(rghtpb) {
    rdisp();
    outchar('1');
      rghtpb = 0;
      PWMDTY1 = 0;
      PWMDTY2 = 0;
      PWMDTY3 = 255;
      colormode = 2;
    } 
  }
  
//*********************************************************************** Random mode
    // Runs on timer interrupts, change to mode 3
    if(colormode==2) {
    if(rghtpb) {
    rdisp();
      outchar('2');
      rghtpb = 0;
      colormode = 3;       
      }      
    }
//*********************************************************************** Fade mode
 if(colormode==3) {
   ATDCTL5 = 0x10;      
   while((128&ATDSTAT0)==0) 
    {} 
  // sample inputs
    in0 = ATDDR4H;
    in1 = ATDDR1H;
    in2 = ATDDR2H;
    in3 = ATDDR3H;
  // assign outputs, +1 to prevent turning off LEDs
    PWMDTY0 = (in0*fade)/255+1;
    PWMDTY1 = (in1*fade)/255+1;
    PWMDTY2 = (in2*fade)/255+1;
    PWMDTY3 = (in3*fade)/255+1;
  // change modes
  if(rghtpb) {
  rdisp();
  outchar('3');
  rghtpb=0;
  colormode=4;
   }    
 }  
 
 //************************************************************************ Audio
 if(colormode == 4) 
  {
   ATDCTL5 = 0x10; 
   while((128&ATDSTAT0)==0) 
    {} 
    // sample audio inputs
    in1 = ATDDR3H; //R bass
    in2 = ATDDR1H; //G mid
    in3 = ATDDR2H; //B treble
    in4 = ATDDR4H; // white 
    in5 = ATDDR5H;
    // assign outputs     
    PWMDTY3 = in3*in5/255;
    PWMDTY2 = in2*in5/255;
    PWMDTY1 = in1*in5/255;  
    PWMDTY0 = in4;
  // change modes
  if(rghtpb) {
  rdisp();
  outchar('4');
  rghtpb=0;
  colormode=1;
  }   
 }
// assign color
  white = PWMDTY0; 
  red = PWMDTY1;
  green = PWMDTY2;
  blue = PWMDTY3; 
  } 
}

/*
***********************************************************************                       
 RTI interrupt service routine: RTI_ISR

 Initialized for 8.192 ms interrupt rate

  Samples state of pushbuttons (PAD7 = left, PAD6 = right)

  If change in state from "high" to "low" detected, set pushbutton flag
     leftpb (for PAD7 H -> L), rghtpb (for PAD6 H -> L)
     Recall that pushbuttons are momentary contact closures to ground 
************************************************************************
*/
interrupt 7 void RTI_ISR(void)
{
  CRGFLG = CRGFLG | 0x80; 

  // assign to ATD7 pushbutton
  if(PORTAD0_PTAD7 == 0) {
   rghtpb =1 && prevRight;
  }
  prevRight = PTAD_PTAD7;
return;
}

/*
***********************************************************************                       
  TIM interrupt service routine

  Initialized for 10.0 ms interrupt rate

  Uses variable "tencnt" to track if one-tenth second has accumulated
     and sets "tenths" flag 
                         
  Uses variable "onecnt" to track if one second has accumulated and
     sets "onesec" flag                         
;***********************************************************************
*/
interrupt 15 void TIM_ISR(void)
{
  // clear TIM CH 7 interrupt flag 
  TFLG1 = TFLG1 | 0x80; 

//************************************ Fade Mode
if(colormode==3) {
    freq = ATDDR6H;
    TC7 = freq * 50 + 3000;
    
    // adjust frequency from ATD6 input
    if( (isRising==1) && (fade<255) ) {
      fade++;
    }  
    if( (isRising==1) && (fade==255)) {
      isRising = 0;
    }
    if((isRising==0)&&(fade>0)) {
       fade--;
    }
    if((isRising==0)&&(fade==0)){
      isRising=1;
    }
}   
//************************************************** color spectrum
if(colormode == 2)
{  
ATDCTL5 = 0x10;      
   while((128&ATDSTAT0)==0) 
    {}                 
PWMDTY0 = ATDDR4H;    

// Loop through preset color settings  
if(PWMDTY3 == 255 && (PWMDTY2 == PWMDTY1 == 0))
{
    lcdwait();
}

if (color == 1)
{
  PWMDTY2++;
    lcdwait();      
  if (PWMDTY2 == 127)
    color++;
}

if (color == 2)
  {
    PWMDTY2++;
    lcdwait();
    if (PWMDTY2 == 255)
      color++;
  }
  
if (color == 3)
  {
    PWMDTY3--;
    lcdwait();
    if (PWMDTY3 == 0)
      color++;
  }
  
if (color == 4)
  {
    PWMDTY2--;
    PWMDTY1++;
    lcdwait();
    if (PWMDTY2 == 0)
      color++;
  }
  
if (color == 5)
  {
    PWMDTY3++;
    PWMDTY1--;
    lcdwait();
    if (PWMDTY3 == 127)    
      color++;
  }
  
if (color == 6)
  {
    PWMDTY1++;
    lcdwait(); 
    if (PWMDTY3 < 143)
        PWMDTY3++;
    if (PWMDTY1 == 255)
      color++;
  }
  
if (color == 7)
  { 
    if (PWMDTY3 < 255)
        PWMDTY3++;
    PWMDTY1--;
    if (PWMDTY1 == 0)
        color = 1; 
    lcdwait();
  } 
}
}

/*
***********************************************************************                              
 RPM display routine - rdisp
                         
 This routine starts by reading (and clearing) the 16-bit PA register.
 It then calculates an estimate of the RPM based on the number of
 pulses accumulated from the 64-hole chopper over a one second integration
 period and divides this value by 28 to estimate the gear head output
 shaft speed. Next, it converts this binary value to a 3-digit binary coded
 decimal (BCD) representation and displays the converted value on the
 terminal as "RPM = NNN" (updated in place).  Finally this RPM value, along
 with a bar graph showing ther percent-of-max of the current RPM value
 are shifted out to the LCD using pmsglcd.

***********************************************************************
*/
void rdisp()
{
   send_i(0x01); // clear LCD

   // color mode to LCD output
   if(colormode == 1) 
   {
     pmsglcd("Mode: Solid");         
   }
   if(colormode == 2) 
   {
    pmsglcd("Mode: Fade");   
   }
  if(colormode == 3) 
  {
    pmsglcd("Mode: Spectrum");
  }
  if(colormode == 4) 
  {
    pmsglcd("Mode: Music");
  }
}

/*
***********************************************************************
  shiftout: Transmits the character x to external shift 
            register using the SPI.  It should shift MSB first.  
             
            MISO = PM[4]
            SCK  = PM[5]
***********************************************************************
*/
 
void shiftout(char x)
{   
  // read the SPTEF bit, continue if bit is 1
  // write data to SPI data register
  // wait for 30 cycles for SPI data to shift out 

  // read the SPTEF bit, continue if bit is 1
  while(SPISR_SPTEF != 1) 
  {}
  // write data to SPI data register
  SPIDR = x;
  // wait for 30 cycles for SPI data to shift out
  lcdwait();
}

/*
***********************************************************************
  lcdwait: Delay for approx 2 ms
***********************************************************************
*/

void lcdwait()
{
  asm{
    delay:
      pshx
      psha
      pshc
      ldaa #2
      
    loopo:  
      ldx #7995
      
     loopi:
       dbne x,loopi
       
       dbne a,loopo
       
       pulc
       pula
       pulx
  }
}

/*
*********************************************************************** 
  send_byte: writes character x to the LCD
***********************************************************************
*/

void send_byte(char x)
{
 // shift out character
 shiftout(x);
 
 // pulse LCD clock line low->high->low
 PTT_PTT6 = 1; 
 PTT_PTT6 = 0;
 PTT_PTT6 = 1;

 lcdwait();
}

/*
***********************************************************************
  send_i: Sends instruction byte x to LCD  
***********************************************************************
*/
void send_i(char x)
{
  // set the register select line low (instruction data)
  PTT_PTT4 = 0;
  send_byte(x);
}

/*
***********************************************************************
  chgline: Move LCD cursor to position x
  NOTE: Cursor positions are encoded in the LINE1/LINE2 variables
***********************************************************************
*/
void chgline(char x)
{
  send_i(CURMOV);
  send_byte(x);   
}

/*
***********************************************************************
  print_c: Print (single) character x on LCD            
***********************************************************************
*/
void print_c(char x)
{
     PTT_PTT4 = 1;
     send_byte(x); 
}

/*
***********************************************************************
  pmsglcd: print character string str[] on LCD
***********************************************************************
*/
void pmsglcd(char str[])
{
 int index = 0;
 int toprint;
 
   while(str[index] != 0) 
   {
     toprint = str[index];
     print_c(toprint);
     index++;
   }
}

/*
***********************************************************************
 Character I/O Library Routines for 9S12C32 
***********************************************************************
 Name:         inchar
 Description:  inputs ASCII character from SCI serial port and returns it
 Example:      char ch1 = inchar();
***********************************************************************
*/
char inchar(void) {
  /* receives character from the terminal channel */
    while (!(SCISR1 & 0x20)); /* wait for input */
    return SCIDRL;
}

/*
***********************************************************************
 Name:         outchar    (use only for DEBUGGING purposes)
 Description:  outputs ASCII character x to SCI serial port
 Example:      outchar('x');
***********************************************************************
*/
void outchar(char x) {
  /* sends a character to the terminal channel */
    while (!(SCISR1 & 0x80));  /* wait for output buffer empty */
    SCIDRL = x;
}

