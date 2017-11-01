////////////////////////////////////////////////////////////////////////////////
// ECE 2534:        Lab 3
//
// File name:       main
// Description:     This is a Snake Game using a Joystick and Oled Display
// Resources:       timer2 is configured to interrupt every 1.0ms, and
//                  ADC is configured to automatically restart and interrupt
//                  after each conversion (about 160us).
// How to use:      AN2 is on header JA, pin 1. AN2 is on header JA, pin 2. Must connect
//                  VCC to L/R+, AN2 to L/R, and AN7 to U/D+
//
//ADDED FEATURES:   3 different game difficulties(speeds)
// Written by:      Karthik Ramesh
// Last modified:   6 November 2015
#define USE_OLED_DRAW_GLYPH

#ifdef USE_OLED_DRAW_GLYPH
void OledDrawGlyph(char ch);
#endif

#define _PLIB_DISABLE_LEGACY
#include <plib.h>
#include "PmodOLED.h"
#include "OledChar.h"
#include "OledGrph.h"
#include "delay.h"
#include "stdbool.h"

// Configure the microcontroller
#pragma config FPLLMUL  = MUL_20        // PLL Multiplier
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider
#pragma config FPLLODIV = DIV_1         // PLL Output Divider
#pragma config FPBDIV   = DIV_8         // Peripheral Clock divisor
#pragma config FWDTEN   = OFF           // Watchdog Timer
#pragma config WDTPS    = PS1           // Watchdog Timer Postscale
#pragma config FCKSM    = CSDCMD        // Clock Switching & Fail Safe Clock Monitor
#pragma config OSCIOFNC = OFF           // CLKO Enable
#pragma config POSCMOD  = HS            // Primary Oscillator
#pragma config IESO     = OFF           // Internal/External Switch-over
#pragma config FSOSCEN  = OFF           // Secondary Oscillator Enable
#pragma config FNOSC    = PRIPLL        // Oscillator Selection
#pragma config CP       = OFF           // Code Protect
#pragma config BWP      = OFF           // Boot Flash Write Protect
#pragma config PWP      = OFF           // Program Flash Write Protect
#pragma config ICESEL   = ICS_PGx1      // ICE/ICD Comm Channel Select
#pragma config DEBUG    = OFF           // Debugger Disabled for Starter Kit

// Intrumentation for the logic analyzer (or oscilliscope)
#define MASK_DBG1  0x1;
#define MASK_DBG2  0x2;
#define MASK_DBG3  0x4;
#define DBG_ON(a)  LATESET = a
#define DBG_OFF(a) LATECLR = a
#define DBG_INIT() TRISECLR = 0x7

// Definitions for the ADC averaging. How many samples (should be a power
// of 2, and the log2 of this number to be able to shift right instead of
// divide to get the average.
#define NUM_ADC_SAMPLES 32
#define LOG2_NUM_ADC_SAMPLES 5

#define CHECK_RET_VALUE(a) { \
  if (a == 0) { \
    LATGSET = 0xF << 12; \
    return(EXIT_FAILURE) ; \
  } \
}

// Global variable to count number of times in timer2 ISR
unsigned char Button1History = 0x00; //State of Btns 1
unsigned char Button2History = 0x00; //State of Btns 2
unsigned char Button3History = 0x00;
bool BTN1_on; //If Btn 1 on
bool BTN2_on; //If Btn 2 on
bool BTN3_on; //If Btn 3 on
bool BTN1flag = false; //Default Btn1 off
bool BTN2flag = false; //Default Btn2 off
bool BTN3flag = false;//Default Btn3 off
bool empty = true;//checks to see if apple is gone
bool gamespeed = false;//used to control speeds of game
bool gamelost = false;//lose or win
unsigned int timer2_ms_value = 0;
int mins = 0;
int secs = 0;
int count1 = 0;
int count2 = 0;
int count3 = 0;
int goalnumber = 1;
int score = 0;
int randx = 0;
int randy = 0;
int count = 0;
BYTE apple[8] = { 0x78, 0xCC, 0x85, 0x87, 0x87, 0x84, 0xCC, 0x78};//Apple
char apple_char = 0x00;//User Defined Apple char
BYTE blank[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};//Blank
char blank_char = 0x01;//User Defined Blank Char
BYTE head[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };//Snake Head
char head_char = 0x02;//User Defined Snake Head Char
BYTE body[8] = { 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA };//Snake Body
char body_char = 0x03;//User Defined Snake Body Char
unsigned int seconds;//display runtime in seconds
int leftright;//0-1023 value for left/right
int updown;//0-1023 value for up/down
int modulous = 1000;//default speed
char oledstring[17];
unsigned int timer2_ms_value_save;
unsigned int last_oled_update = 0;
unsigned int ms_since_last_oled_update;
char playfield[10][4];//Playing Area 2d Array
bool randbool = false;//Whether Random apple is present
bool noapple = true;//If apple is there
typedef struct _position{
    int x;
    int y;
    char z;
} position;//Holds head and body segments

int snakelength = 2;//starts with 2 body pieces



    enum snake {//states of left/right FSM
        welcome, goal, speed, startgame, game, end
    };//snake state machine
    enum snake snakestate = welcome; //starts at neutral
     enum States {//states of debounce FSM
        UP, DOWN_TRANSITION, DOWN
    };
    enum States systemState = UP; //starts at not pressed
    

void welc();//welcome screen
void gl();//goal screen
void spd();//speed screen
void gamescreen();//score and goal
void applerand();//make random apple
void drawfield();//print out 2d array
void initplayfield();//clear out 2d array
void dir();//seals in direction based on Joystick
void moving();//moving and growing function
void initsnake();//start snake as 1 body, 1 head
void loser();//outputs losing info
void winner();//outputs winning info
char direction;//seals in the direction
void __ISR(_ADC_VECTOR, IPL7SRS) _ADCHandler(void) {

    DBG_ON(MASK_DBG2);

    if (ReadActiveBufferADC10() == 0) {//from supporting docs, when Buffer is reading 0, use first 2 bits
        leftright = ReadADC10(0);//set value of left or right
        updown = ReadADC10(1);//set value of up or down
    } else if (ReadActiveBufferADC10() == 1) {//when Buffer read is 1, use bitts 8 and 9
        leftright = ReadADC10(8);
        updown = ReadADC10(9);
    }

    DBG_OFF(MASK_DBG2);
    INTClearFlag(INT_AD1);
}

// The interrupt handler for timer2
// IPL4 medium interrupt priority
// SOFT|AUTO|SRS refers to the shadow register use

void __ISR(_TIMER_2_VECTOR, IPL4AUTO) _Timer2Handler(void) {


    DBG_ON(MASK_DBG1);
    timer2_ms_value++; // Increment the millisecond counter.
   

    if ((timer2_ms_value % 1000) == 0)//Determines how often seconds is updated. mod of 1000 increments seconds each second
    {
        seconds++;
    }
    if ((timer2_ms_value % modulous) == 0)
    {
        gamespeed = true;//based on user selected speed, this flag will trigger
    }

        BTN1_on = (bool) (PORTG & 0x40);
        BTN2_on = (bool) (PORTG & 0x80);
        BTN3_on = (bool) (PORTA & 0x01);

        Button1History <<= 1;
        if (BTN1_on) {
            Button1History |= 0x01;
        }
        Button2History <<= 1;
        if (BTN2_on) {
            Button2History |= 0x01;
        }
        Button3History <<=1;
        if(BTN3_on) {
            Button3History |= 0x01;
        }


    DBG_OFF(MASK_DBG1);
    INTClearFlag(INT_T2); // Acknowledge the interrupt source by clearing its flag.
}

#define AD_MUX_CONFIG ADC_CH0_POS_SAMPLEA_AN2 | ADC_CH0_NEG_SAMPLEA_NVREF | ADC_CH0_POS_SAMPLEB_AN3 | ADC_CH0_NEG_SAMPLEB_NVREF//enable second sample for U/D
#define AD_CONFIG1 ADC_FORMAT_INTG | ADC_CLK_AUTO | ADC_AUTO_SAMPLING_ON
#define AD_CONFIG2 ADC_VREF_AVDD_AVSS | ADC_SCAN_OFF | \
                  ADC_SAMPLES_PER_INT_2 | \
                  ADC_BUF_16 | ADC_ALT_INPUT_ON//allow up/down to be alternate input
#define AD_CONFIG3 ADC_SAMPLE_TIME_8 | ADC_CONV_CLK_20Tcy

#define AD_CONFIGPORT ENABLE_AN2_ANA | ENABLE_AN3_ANA//enable both inputs

#define AD_CONFIGSCAN SKIP_SCAN_ALL

void initADC(void) {

    // Configure and enable the ADC HW
    SetChanADC10(AD_MUX_CONFIG);
    OpenADC10(AD_CONFIG1, AD_CONFIG2, AD_CONFIG3, AD_CONFIGPORT, AD_CONFIGSCAN);
    EnableADC10();

    // Set up, clear, and enable ADC interrupts
    INTSetVectorPriority(INT_ADC_VECTOR, INT_PRIORITY_LEVEL_7);
    INTClearFlag(INT_AD1);
    INTEnable(INT_AD1, INT_ENABLED);
}

// Initialize timer2 and set up the interrupts

void initTimer2() {
    // Configure Timer 2 to request a real-time interrupt once per millisecond.
    // The period of Timer 2 is (16 * 625)/(10 MHz) = 1 s.
    OpenTimer2(T2_ON | T2_IDLE_CON | T2_SOURCE_INT | T2_PS_1_16 | T2_GATE_OFF, 624);
    INTSetVectorPriority(INT_TIMER_2_VECTOR, INT_PRIORITY_LEVEL_4);
    INTClearFlag(INT_T2);
    INTEnable(INT_T2, INT_ENABLED);
}

void initGPIO(void)//GPIO initializer
{
    TRISASET = 0x01;
    TRISGSET = 0x40; // For BTN1: configure PortG bit for input
    TRISGSET = 0x80;
    TRISGCLR = 0xF000; // For all led's: configure PortG pin for output
    ODCGCLR = 0xF000; // For all led's: configure as normal output
     // Initialize GPIO for LEDs
    LATGCLR = 0xf000; // Initialize LEDs to 0000

    //initialize yellow wire as a powered output for up/down
    TRISBCLR = 0x40;
    ODCBCLR = 0x40;
    LATBSET = 0x40;
}

void initOLED(void)//OLED initializer
{
    DelayInit();
    OledInit();

}
position snake_position[10];
unsigned int snake_length = 3;

int main() {
    //init the snake
    

    DDPCONbits.JTAGEN = 0;
    int retValue = 0;
    initGPIO();
    // Initialize Timer1 and OLED for display
    initOLED();
    // Set up our user-defined characters for the OLED
    retValue = OledDefUserChar(blank_char, blank);
    CHECK_RET_VALUE(retValue);
    retValue = OledDefUserChar(apple_char, apple);
    CHECK_RET_VALUE(retValue);
    retValue = OledDefUserChar(head_char, head);
    CHECK_RET_VALUE(retValue);
     retValue = OledDefUserChar(body_char, body);
    CHECK_RET_VALUE(retValue);
    // Initialize GPIO for debugging
    DBG_INIT();
    // Initial Timer2 and ADC
    initTimer2();
    initADC();

    // Configure the system for vectored interrupts
    INTConfigureSystem(INT_SYSTEM_CONFIG_MULT_VECTOR);

    // Enable the interrupt controller
    INTEnableInterrupts();

    // Send a welcome message to the OLED display
    OledClear();
    while (1) {//main loop

        switch (systemState) {//debounce FSM from class notes, modified
            case UP:

                if (Button1History == 0xFF) {//if btn1 pressed
                    systemState = DOWN_TRANSITION; //if pressed, go to down
                    BTN1flag = true; //1 was pressed
                } else if (Button2History == 0xFF) {//if btn2 pressed
                    systemState = DOWN_TRANSITION;
                    BTN2flag = true; //2 was pressed
                }
                else if (Button3History == 0xFF) {
                    systemState = DOWN_TRANSITION;
                    BTN3flag = true;//3 was pressed
                }
                break;
            case DOWN_TRANSITION:
                if (BTN1flag)//if 1 was pressed
                {

                    
                        BTN1flag = false; //1 not pressed

                } else if (BTN2flag) {
                    
                        BTN2flag = false; //btn2 not pressed
                        
                    }
                else if (BTN3flag) {
                    
                    BTN3flag = false;//3 not pressed
                }
                systemState = DOWN;
                break;
            case DOWN:

                if (Button1History == 0x00 && Button2History == 0x00 && Button3History == 0x00) {//if none are pressed
                    systemState = UP; //nothing is pressed

                }
                break;
            default:
                LATG = 0xF << 12;
                while (1) {
                }
        }

        switch (snakestate){//snake game

            case welcome:
               welc();//welcome screen
               
            break;

            case goal:
               gl();//goal screen
            break;

            case speed:
                spd();
            break;

            case startgame:
                initplayfield();
                break;

            case game:
              if (BTN1flag == true)
             {   
                    OledClear();
                    snakestate = welcome;
                    OledUpdate();
                    seconds = 0;
                    break;
              }
             

              OledSetCharUpdate(0);
              dir();
              gamescreen(); 
              applerand();
              moving();
              drawfield();
              OledSetCharUpdate(1);
              OledUpdate();
              break;

            case end:
                OledClearBuffer();
                goalnumber = 1;
                if (BTN1flag == true)
             {
        snakestate = welcome;
            OledClear();
                OledUpdate();
                
                seconds = 0;
                gamelost = false;
        break;
            }

                if (gamelost == false)
                {
                    winner();
                }
                else if (gamelost == true)
                {
                    loser();
                }
                break;
        }  
    }
    return (EXIT_SUCCESS);
}
//Description: goal selection screen
//pre-conditions: none
//i/o: none
//post conditions: none
void gl(){
    OledSetCharUpdate(0);
    OledClearBuffer();
 OledSetCursor(0, 0);
             OledSetCursor(0, 0);
            sprintf(oledstring, "Set Goal: %3d", goalnumber);//print goal
            OledPutString(oledstring);
            OledSetCursor(0, 1);
            OledPutString("BTN1: Goal++");
            OledSetCursor(0, 2);
            OledPutString("BTN2: Goal--");
            OledSetCursor(0, 3);
            OledPutString("BTN3: Accept");
            OledSetCharUpdate(1);
            OledUpdate();

            if (BTN1flag == true)//increase goal
            {
                if (goalnumber == 9)
                {
                    goalnumber = 9;
                }
                else {
                goalnumber++;
}

            }
            else if (BTN2flag == true)//decrease goal
            {
                if (goalnumber == 1)
                {
                    goalnumber = 1;
                }

                else{
                goalnumber--;
                }
            }
            else if (BTN3flag == true)//submit
            {
                BTN3flag = false;
                OledClear();
                snakestate = speed;
            }
}

//Description: controls moving, growing, winning/losing conditions
//pre-conditions: none
//i/o: none
//post conditions: none
void moving(){
    gamelost = false;//initialize not lose
   if ((snake_position[0].x == randx) && (snake_position[0].y == randy))//if apple is eaten
{
       gamelost = false;
    noapple = true;
    empty = true;
    randbool = false;
    score++;
    snakelength++;
    

   if (score == goalnumber)//if eaten and goal reached, win
    {
        gamelost = false;//win
        snakestate = end;
        return;
    }
}
   
   else{
    if (gamespeed == true)//handles speed of movement
    {
        int k = 0;
        for (k = 1; k < snakelength - 1; k ++)//hit body of snake
        {
            if (snake_position[0].x == snake_position[k].x)
            {
               if (snake_position[0].y == snake_position[k].y)
               {
                OledClear();
                gamelost = true;
                snakestate = end;
                return;
               }
            }
        }

              if (direction == 'u')//up movement
             {
                  if (snake_position[0].y == 0)//hit top boundary, end game
                  {
                     
                      snakestate = end;
                      gamelost = true;
                      return;
                  }
                  else
                      
                  {
                 int i = 0;

                for(i = snakelength; i > 0; i--)//shift body segments to old position of next body segment
                {

                    snake_position[i].x = snake_position[i - 1].x;
                    snake_position[i].y = snake_position[i - 1].y;
                }


                snake_position[0].y--;//move head up
                playfield[snake_position[0].x][snake_position[0].y] =  snake_position[0].z;//put head in array
                for(i = 1; i <snakelength; i++)
                {
                 playfield[snake_position[i].x][snake_position[i].y] = body_char;//put body segments into array

                }

             
             playfield[snake_position[snakelength].x][snake_position[snakelength].y] = ' ';//clear last spot
                  }
             }
              else if (direction == 'l') {//same method as before, just decrement x

                  if (snake_position[0].x == 0)
                  {
                      
                      snakestate = end;
                      gamelost = true;
                      return;
                  }
                  else{
                  
                int i = 0;

                for(i = snakelength; i > 0; i--)
                {

                    snake_position[i].x = snake_position[i - 1].x;
                    snake_position[i].y = snake_position[i - 1].y;
                }


                snake_position[0].x--;
                playfield[snake_position[0].x][snake_position[0].y] =  snake_position[0].z;
                for(i = 1; i <snakelength; i++)
                {
                 playfield[snake_position[i].x][snake_position[i].y] = body_char;//snake_position[i].z;

                }

             
             playfield[snake_position[snakelength].x][snake_position[snakelength].y] = ' ';
            
            }
              }
              else if (direction == 'd') {//same as previous methods, just increment y
                  
                  if (snake_position[0].y == 3)
                  {
                      
                      snakestate = end;
                      gamelost = true;
                      return;
                  }
                  else{
               int i = 0;

                for(i = snakelength; i > 0; i--)
                {

                    snake_position[i].x = snake_position[i - 1].x;
                    snake_position[i].y = snake_position[i - 1].y;
                }


                snake_position[0].y++;
                playfield[snake_position[0].x][snake_position[0].y] =  snake_position[0].z;
                for(i = 1; i <snakelength; i++)
                {
                 playfield[snake_position[i].x][snake_position[i].y] = body_char;

                }
            
             playfield[snake_position[snakelength].x][snake_position[snakelength].y] = ' ';
                  }}

              else if (direction == 'r')//same method as above, just increment x
          {

               if (snake_position[0].x == 9)
                  {
                     
                      snakestate = end;
                      gamelost = true;
                      return;
                  }
               else{
               int i = 0;

                for(i = snakelength; i > 0; i--)
                {

                    snake_position[i].x = snake_position[i - 1].x;
                    snake_position[i].y = snake_position[i - 1].y;
                }
              
                       
                snake_position[0].x++;
                playfield[snake_position[0].x][snake_position[0].y] =  snake_position[0].z;
                for(i = 1; i <snakelength; i++)
                {
                 playfield[snake_position[i].x][snake_position[i].y] = body_char;//snake_position[i].z;

                }

             playfield[snake_position[snakelength].x][snake_position[snakelength].y] = ' ';
                    
              }}

    }}
              gamespeed = false;//for timer
                }
//Description: deciphers joystick controls
//pre-conditions: none
//i/o: none
//post conditions: none
void dir()
{
    if (updown > 485 && leftright < 510 && leftright > 480 && direction != 'd')//if joystick up
    {
        direction = 'u';
    }
    if (leftright < 480  && updown > 475 && updown < 485 && direction !='r')//if joystick left
    {
        direction = 'l';
    }

    if (updown < 475 && leftright < 510 && leftright > 480 && direction !='u')//if joystick down
    {
        direction = 'd';
    }
    if (leftright > 510  && updown > 475 && updown < 485 && direction != 'l')//if joystick right
    {
        direction = 'r';
    }
}



//Description: welcome screen
//pre-conditions: none
//i/o: none
//post conditions: none
void welc(){//welcome screen
               count = 0;
               snakelength = 2;
               score = 0;
    OledSetCharUpdate(0);
            OledSetCursor(0, 0);
            OledPutString("Snake Game");
            OledSetCursor(0, 1);
            OledPutString("Karthik Ramesh");
            OledSetCursor(0, 2);
            OledPutString("November 6 2015");
            OledSetCharUpdate(1);
            OledUpdate();
            if (seconds == 5)
            {
                 OledClear();
                 OledUpdate();
                snakestate = goal;
            }
}
//Description: allows user to change game speed
//pre-conditions: none
//i/o: none
//post conditions: none
void spd(){
 OledSetCharUpdate(0);
                OledClearBuffer();
                OledSetCursor(0, 0);
            OledPutString("Difficulty");
                sprintf(oledstring, "%4d", count);
                OledSetCursor(11, 0);
                OledPutString(oledstring);
                OledSetCursor(0, 1);
                OledPutString("BTN1: Speed++");
                 OledSetCursor(0, 2);
            OledPutString("BTN2: Speed--");
            OledSetCharUpdate(1);
            OledUpdate();
                if(BTN1flag == true){
                    if (count == 2)
                    {
                        count = 2;
                    }
                    else
                    {
                        count++;
                    }
                }
                else if (BTN2flag == true)
                {
                if (count == 0)
                    {
                        count = 0;
                    }
                    else
                    {
                    count--;
                    }

                }

            if (BTN3flag == true){
                if (count == 0){
                    modulous = 600;//slow
                }
                 if (count == 2)
                {
                    modulous = 100;//fast
                }
                if (count == 1)
                {
                    modulous = 300;//medium
                }
               snakestate = startgame;
               OledClear();
               return;

            }

}

//Description: shows goal and score
//pre-conditions: none
//i/o: none
//post conditions: none
void gamescreen(){
OledSetCursor(11, 0);
            OledPutString("Score");
            OledSetCursor(11, 1);
            sprintf(oledstring, "%4d", score);
            OledPutString(oledstring);
            OledSetCursor(11, 2);
            OledPutString("Goal");
            OledSetCursor(11, 3);
            sprintf(oledstring, "%4d", goalnumber);
            OledPutString(oledstring);
}
//Description: places apple in random open spot
//pre-conditions: none
//i/o: none
//post conditions: none
void applerand(){
    if (noapple == true)
    {
    while (empty == true)
    {
    randx = rand()%11;
    randy = rand()%4;
    if (randbool == false && playfield[randx][randy] == ' ')//make sure position is empty, else make new random numbers
    {
    playfield[randx][randy] = apple_char;

    randbool = true;
    empty = false;
    
    }
    }
    noapple = false;
    }
}

//Description: clears 2d array
//pre-conditions: none
//i/o: none
//post conditions: none
void initplayfield(){//space fills field
    int i = 0;
    int j = 0;
    for (i = 0; i < 10; i++)
    {
        for(j = 0; j <4; j++)
        {
            playfield[i][j] = ' ';
        }}
                noapple = true;
                randbool = false;
                initsnake();
                    empty = true;
                    gamelost = false;
                    direction = 'r';

                snakestate = game;
}

//Description: outputs 2'd array
//pre-conditions: none
//i/o: none
//post conditions: none
void drawfield(){//draws array
    int i = 0;
    int j = 0;
    for (i = 0; i< 10; i++)
    {
        for (j = 0; j<4; j++)
        {
            OledSetCursor(i, j);
            OledPutChar(playfield[i][j]);
        }
    }
}

//Description: snake is initialized with one head, one body, rest are empty
//pre-conditions: none
//i/o: none
//post conditions: none
void initsnake(){

snake_position[0].x = 1;
snake_position[0].y = 2;
snake_position[0].z = head_char;
snake_position[1].x = 0;
snake_position[1].y = 2;
snake_position[1].z = body_char;

int i = 0;
for(i = 2; i <10; i++)
{
snake_position[i].z = ' ';

}
for (i=0; i< 2; i++ )
{
    playfield[snake_position[i].x][snake_position[i].y] = snake_position[i].z;
}
}

//Description: This outputs text for losing
//pre-conditions: none
//i/o: none
//post conditions: none
void loser(){//if lose

                OledSetCharUpdate(0);
                OledSetCursor(0,0);
                OledPutString("You Lost!");
                OledSetCursor(0,1);
                sprintf(oledstring, "Your score: %3d", score);
                OledPutString(oledstring);
                OledSetCursor(0,2);
                OledPutString("BTN1: Return");
                OledSetCharUpdate(1);
                OledUpdate();

}
//Description: This outputs text for winning
//pre-conditions: none
//i/o: none
//post conditions: none
void winner(){//if win
                OledSetCharUpdate(0);
                OledSetCursor(0,0);
                OledPutString("You Win!");
                OledSetCursor(0,1);
                sprintf(oledstring, "Your score: %3d", score);
                OledPutString(oledstring);
                OledSetCursor(0,2);
                OledPutString("BTN1: Return");
                OledSetCharUpdate(1);
                OledUpdate();
}