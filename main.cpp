//***********************************************************************
//
//  Embedded Software
//  B31DG
//
//  Assignment 2
//
//  Author: Lorenzo Stilo
//  Date: Feb 2016
//
//************************************************************************
//
//  main.cpp
//  Version: 1.6 (Milestone 2)
//
//  Device: mbed LPC1768
//  WattBob I
//
//  Requirements: MCP23017.h, MCP23017.cpp, WattBob_TextLCD.h WattBob_TextLCD.cpp
//
//
//************************************************************************
//
//  This code is an example of Cyclic Executive as a real-time operating system
//  This code will execute 7 Task, as follow:
//      - Task 1 "Measure Frequency"    every 1s
//      - Task 2 "Read Digital In"      every 300ms
//      - Task 3 "Outout a Watchdog"    every 300ms
//       |-------*Pulse width 20 ms
//      - Task 4 "Read Two Analog In"   every 400ms
//      - Task 5 "Display on LCD"       every 2s
//      - Task 6 "Check Error State"    every 800ms
//       |-------*Display Error as LED
//       |-------*Check this Execution time pulse
//      - Task 7 "Send data on Serial"  every 5s    
//
//*************************************************************************

/* Standard Includes */
#include "mbed.h"

/* WattBob Includes */
#include "MCP23017.h"
#include "WattBob_TextLCD.h"

/* LCD Objects Declaration */
WattBob_TextLCD *lcd;
MCP23017 *par_port;

/* Serial Interface Declaration */
Serial serial(USBTX, USBRX);

/* Ticker Instance Declaration */
Ticker ticker;

/* I/O Pins Declaration */
DigitalIn FreqIn(p5);
DigitalIn Switch(p6);
DigitalIn switch_master(p8);

AnalogIn AnalogIn1(p15);
AnalogIn AnalogIn2(p16);

DigitalOut Watchdog(p7);
DigitalOut led1(LED1);
DigitalOut led3(LED3);
DigitalOut TimerPulse(p11);

/* Global Variable Declaration */
int frequency(0);
float analogue_in_1(0);
float analogue_in_2(0);
bool switch_1(0);
volatile long int slot(0);

/*  Mesure Frequency */
//  @param      pin     Input pin to read from
//  @return     int  Frequency in Hz
//
//  N.B. Uses Timer
//  N.B. Holds the system if no frequency is detected 
int MesureFreq(DigitalIn pin)
{
    Timer timer;
    timer.reset();
    volatile int period;
    
    if (pin == 0)
    {
        while (pin == 0);
        timer.start();
        while (pin == 1);
        timer.stop();
        
        period = timer.read_us()*2;
        return 1000000/period;
    }
    else if (pin == 1)
    {
        while (pin == 1);
        timer.start();
        while (pin == 0);
        timer.stop();
        
        period = timer.read_us()*2;
        return 1000000/period;
    }
}

/*  Generates Watchdog Pulse */
//  @param      pin     Output pin
void WatchDogPulse(DigitalOut pin)
{
    pin = 1;
    wait_ms(20);
    pin = 0;
}

/*  Read Analogue value */
//  @param      pin     Analog pin to read from
//  @return     float   Analog value 
float ReadAnalogue(AnalogIn pin)
{
    volatile int i;
    volatile float value(0);
    
    for (i=0; i<4; i++)
    value = value + pin.read();
    
    return (value/4)*10;
}

/*  Read Digital value */
//  @param      pin     Digital pin to read from
//  @return     bool    Digital value 
bool ReadSwitch(DigitalIn pin)
{
    return pin;
}
/*  Display Frequency on LCD */
//  @param      freq    Value to display
void DisplayFrequency(int freq)
{
    lcd->locate(0,0);
    lcd->printf("%i",freq);
}

/*  Display Analogue value on LCD */
//  @param      analogue    Value to display
//  @param      row         Row to display
void DisplayAnalogue(int analogue,int row)
{
    int volts = (analogue * 5)/10;
    int mvolts = (analogue * 5)%10;
    lcd->locate(row,8);
    lcd->printf("%d.%d",volts,mvolts);
}

/*  Display Digital value on LCD */
//  @param      digital  Value to display
void DisplayDigital(bool digital)
{
    lcd->locate(1,0);
    lcd->printf("%i",digital);
}

/*  Send Frequency over serial */
//  @param      freq    Value to send
void SendFrequency(int freq)
{
    serial.printf("%i,",freq);
}

/*  Send Digital value over serial */
//  @param      digital Value to send
void SendDigital(bool digital)
{
    serial.printf("%d,",digital);
}

/*  Send Analogue value over serial */
//  @param      analogue    Value to send
//  @param      reading     Analog to display (1 or 2)
void SendAnalogue(int analogue, int reading)
{
    int volts = (analogue * 5)/10;
    int mvolts = (analogue * 5)%10;
    serial.printf("%d.%d", volts, mvolts);
    if (reading == 1)
    serial.printf(",");
    if (reading == 2)
    serial.printf("\r\n");
}

/*  Display Error Code 3 */
void ErrorCode3()
{
    volatile int i;
    for(i=0; i<5; i++)
    {
        led3 = 1;
        wait_ms(1);
        led3 = 0;
        wait_ms(1);
    }
}

/*  Display Error Code 0 */
void ErrorCode0()
{
    volatile int i;
    for(i=0; i<5; i++)
    {
        led1 = 1;
        wait_ms(1);
        led1 = 0;
        wait_ms(1);
    }
}

/*  Check if the system is in one error state */
//  @param  switch_1                Digital value to check
//  @param  average_analogue_in_1   Analogue value to check
//  @param  average_analogue_in_2   Analogue value to check
void ErrorCheck(bool switch_1, double average_analogue_in_1, double average_analogue_in_2)
{
    if (switch_1 && average_analogue_in_1 > average_analogue_in_2)
    ErrorCode3();
    else
    ErrorCode0();
}

/*  Calls the procedure to Shut the system down */
void Shutdown()
{
    lcd->cls();
    par_port->write_bit(0,BL_BIT); 
    serial.printf("Closing\r\n");
    ticker. detach();
}

/*  LCD Screen Initialization */
void LCDInit()
{
    // Initialise 16-bit I/O chip
    par_port = new MCP23017(p9, p10, 0x40); 
    
    // Initialise 2x26 char display
    lcd = new WattBob_TextLCD(par_port); 
    
    // Turn LCD backlight ON
    par_port->write_bit(1,BL_BIT); 
    
    // Clear display
    lcd->cls();
    
    // Display Initial Layout
    lcd->locate(0,0);
    lcd->printf("000 Hz");
    lcd->locate(0,8);
    lcd->printf("0.0 V");
    lcd->locate(1,8);
    lcd->printf("0.0 V");
    lcd->locate(1,0);
    lcd->printf("0 bool");
    
}
/*  Serial Initialization */
void SerialInit()
{
    // Set Baud Rate
    serial.baud(9600);
    
    // Print heading for CSV
    serial.printf("frequency, digital, analogue_value_1, analogue_value_2\r\n");     
}

/*  Cyclic Executive handler */
void Cyclic()
{   
    // Execute Task 1 every 50 slots (50*20mS = 1S)
    if(slot % 50 == 0)
    {
        frequency = MesureFreq(FreqIn);
    }    
    // Execute Task 2 every 15 slots (15*20mS = 300mS)
    else if(slot % 15 == 1)
    {       
        switch_1 = ReadSwitch(Switch);      
    }
    // Execute Task 3 every 15 slots (15*20mS = 300mS)
    else if(slot % 15 == 2)
    {
        WatchDogPulse(Watchdog);    
    }
    // Execute Task 4 every 20 slots (20*20mS = 400mS)
    else if(slot % 20 == 1)
    {
        analogue_in_1 = ReadAnalogue(AnalogIn1);
        analogue_in_2 = ReadAnalogue(AnalogIn2); 
    }
    // Execute Task 5 every 100 slots (100*20mS = 2S)
    else if(slot % 100 == 2)
    {   
        DisplayFrequency(frequency);
        DisplayAnalogue(analogue_in_1,0);
        DisplayAnalogue(analogue_in_2,1);
        DisplayDigital(switch_1);
        
        // Uses 4 slots
        slot = slot + 3;
         
    }
    // Execute Task 6 every 40 slots (40*20mS = 800mS)
    else if(slot % 40 == 8)
    {
        //Check Execution Time Pulse for Task 6
        TimerPulse = 1;
        ErrorCheck(switch_1, analogue_in_1, analogue_in_2);
        TimerPulse = 0;
    }
    // Execute Task 7 every 250 slots (250*20mS = 5S)
    else if(slot % 250 == 9)
    {
        SendFrequency(frequency);
        SendDigital(switch_1);
        SendAnalogue(analogue_in_1,1);
        SendAnalogue(analogue_in_2,2); 
    }
    // Execute master switch test in every unused slots
    else if(switch_master==1)
    {  
        Shutdown(); 
    }
    // Increment the slot counter
    slot++;   
}

int main()
{
    // Initialize LCD
    LCDInit();
    
    // Initialize Serial
    SerialInit();
    
    // Call the Cyclic Executive function every 20mS
    ticker.attach_us(&Cyclic, 20000);
    
    // Loop Forever
    while (1);
}