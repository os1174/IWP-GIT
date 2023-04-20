/*
 * File: Main.c
 * Author: jy1189 - and many more
 * 
 * Created on April 23, 2015, 11:05 AM
 */

//*****************************************************************************
#include "IWPUtilities.h"
#include "I2C.h"
#include "FONAUtilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <xc.h>
#include <string.h>

// ****************************************************************************
// *** PIC24F32KA302 Configuration Bit Settings *******************************
// ****************************************************************************
// FBS
#pragma config BWRP = OFF // Boot Segment Write Protect (Disabled)
#pragma config BSS = OFF // Boot segment Protect (No boot program flash segment)
// FGS
#pragma config GWRP = OFF // General Segment Write Protect (General segment may be written)
#pragma config GSS0 = OFF // General Segment Code Protect (No Protection)
// FOSCSEL
#pragma config FNOSC = FRC // Oscillator Select (Fast RC Oscillator (FRC))
#pragma config SOSCSRC = DIG // SOSC Source Type (Digital Mode for use with external source)
#pragma config LPRCSEL = HP // LPRC Oscillator Power and Accuracy (High Power, High Accuracy Mode)
#pragma config IESO = OFF // Internal External Switch Over bit (Internal External Switchover mode disabled (Two-speed Start-up disabled))
// FOSC
#pragma config POSCMOD = NONE // Primary Oscillator Configuration bits (Primary oscillator disabled)
#pragma config OSCIOFNC = OFF // CLKO Enable Configuration bit (CLKO output disabled)
#pragma config POSCFREQ = HS // Primary Oscillator Frequency Range Configuration bits (Primary oscillator/external
			                 // clock input frequency greater than 8MHz)
#pragma config SOSCSEL = SOSCHP // SOSC Power Selection Configuration bits (Secondary Oscillator configured for high-power operation)
#pragma config FCKSM = CSDCMD // Clock Switching and Monitor Selection (Both Clock Switching and Fail-safe Clock Monitor are disabled)

// FWDT
// Enable the WDT to work both when asleep and awake.  Set the time out to a nominal 131 seconds
#pragma config WDTPS = PS32768          // Watchdog Timer Postscale Select bits (1:32768)
#pragma config FWPSA = PR128            // WDT Prescaler bit (WDT prescaler ratio of 1:128)
#pragma config FWDTEN = ON              // Watchdog Timer Enable bits (WDT enabled in hardware)
// use this if you want to be able to turn WDT on and off 
// #pragma config FWDTEN = SWON            // Watchdog Timer Enable bits (WDT controlled with the SWDTEN bit setting)
#pragma config WINDIS = OFF             // Windowed Watchdog Timer Disable bit (Standard WDT selected(windowed WDT disabled))

// FPOR
#pragma config BOREN = BOR3 // Brown-out Reset Enable bits (Brown-out Reset enabled in hardware, SBOREN bit disabled)
#pragma config LVRCFG = OFF // (Low Voltage regulator is not available)
#pragma config PWRTEN = ON // Power-up Timer Enable bit (PWRT enabled)
#pragma config I2C1SEL = PRI // Alternate I2C1 Pin Mapping bit (Use Default SCL1/SDA1 Pins For I2C1)
#pragma config BORV = V20 // Brown-out Reset Voltage bits (Brown-out Reset set to lowest voltage (2.0V))
#pragma config MCLRE = ON // MCLR Pin Enable bit (RA5 input pin disabled,MCLR pin enabled)
// FICD
#pragma config ICS = PGx2 // ICD Pin Placement Select bits (EMUC/EMUD share PGC2/PGD2)
// FDS
#pragma config DSWDTPS = DSWDTPSF // Deep Sleep Watchdog Timer Postscale Select bits (1:2,147,483,648 (25.7 Days))
#pragma config DSWDTOSC = LPRC // DSWDT Reference Clock Select bit (DSWDT uses Low Power RC Oscillator (LPRC))
#pragma config DSBOREN = ON // Deep Sleep Zero-Power BOR Enable bit (Deep Sleep BOR enabled in Deep Sleep)
#pragma config DSWDTEN = ON // Deep Sleep Watchdog Timer Enable bit (DSWDT enabled)

int WaitForPrime(float); // Measures the meters water needed to be lifted in order to prime the pump
float Volume(void);      // Measures the volume (L) of water pumped 
float LeakRate(float);  // Measure the leak rate in L/hr
int TechAtPumpActivities(int);
//void TechAtPumpActivities(void); // Check for text messages and light diagnostic LEDs
void HourlyActivities(void);  //Things we do once each hour
void SaveBatteryPower(void);  //Sleep and wake up routine to save battery power


// ****************************************************************************
// *** Main Function **********************************************************
// ****************************************************************************

// ****************************************************************************
// *** Main Function **********************************************************
// ****************************************************************************
void main(void)
{
    
    int handleMovement = 0; // Either 1 or no 0 if the handle moving upward
    float angleAtRest = 0; // Used to store the handle angle when it is not moving
    
	initialization();
    int nextTechAtPumpCheck = 0; //Only check for text messages every 20 seconds.
                                 // for the first 20sec after Initialization, the
                                 // system will not look for messages
	
    
    //                    DEBUG
    // print_debug_messages controls the debug reporting
    //   0 = send message only at noon to upside Wireless
    //   1 = print debug messages to Tx but send only noon message to Upside (MainphoneNumber[])
    //  ? do we do this one still ? 2 = print debug messages, send hour message after power up and every hour
    //       to debug phone number.  Still sends noon message to Upside
        
    //   Note: selecting 1 or 2 will change some system timing since it takes 
    //         time to form and send a serial message
    print_debug_messages = 1;       //DEBUG CHANGED TO A 1 SO THAT WE RECIEVE MESSAGES
    int temp_debug_flag = print_debug_messages;
    
    EEProm_Read_Float(EEpromCodeRevisionNumber,&codeRevisionNumber); //Get the current code revision number from EEPROM
    
    print_debug_messages = 1;                                        //// We always want to print this out
    sendDebugMessage("   \n JUST CAME OUT OF INITIALIZATION ",-0.1);  //Debug
    sendDebugMessage("The hour is = ", BcdToDec(getHourI2C()));  //Debug
    sendDebugMessage("The minute is = ", BcdToDec(getMinuteI2C()));  //Debug
    sendDebugMessage("The battery is at ",batteryLevel()); //Debug
    sendDebugMessage("The hourly diagnostic reports are at ",diagnostic); //Debug
    sendDebugMessage("The revision number of this code is ",codeRevisionNumber); //Debug
    TimeSinceLastHourCheck = 0;
    
    print_debug_messages = temp_debug_flag;                          // Go back to setting chosen by user
    nextTechAtPumpCheck = secondVTCC + 20;  // We want to give ourselves 20sec before
  
        if (nextTechAtPumpCheck >=60){      // we start looking for text messages
            nextTechAtPumpCheck = nextTechAtPumpCheck - 60; // if Tech at Pump, this is 
        }                             // for manufacturing test 

    while (1)
	{
        //MAIN LOOP; repeats indefinitely
		////////////////////////////////////////////////////////////
		// Idle Handle Monitor Loop
		// 2 axis of the accelerometer are constantly polled for
		// motion in the upward direction by comparing angles
		////////////////////////////////////////////////////////////
        angleAtRest = getHandleAngle();                            // Get the angle of the pump handle to measure against.  
                                                                     // This is our reference when looking for sufficient movement to say the handle is actually moving.  
                                                                     // the "moving" threshold is defined by handleMovementThreshold in IWPUtilities
		handleMovement = 0;                                          // Set the handle movement to 0 (handle is not moving)
		while (handleMovement == 0){
            pumping_Led = 0;   //Turn off pumping led - red
            if(techNotAtPump == 0){ // There is a tech at the pump so we want to 
                                // give feedback on LEDs and check text messages
                nextTechAtPumpCheck = TechAtPumpActivities(nextTechAtPumpCheck);
            }
            else{if(FONAisON){turnOffSIM();}} //Usually only power Cellphone board on the hour
            ClearWatchDogTimer();     // We stay in this loop if no one is pumping so we need to clear the WDT 
                     
           // See if the external RTCC is keeping track of time or if we need to rely on 
           // our less accurate internal timer VTCC
            if(TimeSinceLastHourCheck == 1){ // Check every minute, updated in VTCC interrupt routine
                VerifyProperTimeSource();   
                TimeSinceLastHourCheck = 0; //this gets updated in VTCC interrupt routine
            }  
            // Do hourly tasks
            if(hour != prevHour){
               HourlyActivities();
            }
            
            // should we be asleep to save power?
           //if(((BatteryLevelArray[0]-BatteryLevelArray[2])>BatteryDyingThreshold)||(BatteryLevelArray[2]<=3.2)){
              // keep sleeping until the battery charges up to where it was 2 hrs before we went to sleep
             //  SaveBatteryPower();  //Sleep and wake up routine to save battery power
           //}
           // OK, go ahead and look for handle movement again
           handleMovement = HasTheHandleStartedMoving(angleAtRest);
            
           delayMs(upstrokeInterval);                            // Delay for a short time
		}
        //////////////////////////////////
        // The handle has started pumping
        //////////////////////////////////
        angleCurrent = angleAtRest;  // Priming movement includes the movement 
                                     // needed to declare that someone has started pumping
        pumping_Led = 1;   //Turn on pumping led - red
        int primed = WaitForPrime(angleCurrent);  // Measure effort to Prime Pump
        if(primed){
            float volumeDispensed = Volume();   // Measure the Volume Pumped
            float leakMeasured = LeakRate(volumeDispensed); // Measure the Leak Rate (L/hr)
            sendDebugMessage("Pumped (L): ",volumeDispensed); //Debug
            sendDebugMessage("Leak Rate (L/hr) = ",leakMeasured); //Debug
        }
        angleAtRest = getHandleAngle(); //gets the filtered current angle
	} // End of main loop
} // End of main program
/*********************************************************************
 * Function: int WaitForPrime(void)
 * Input: angle in degrees of the handle when it is assumed to be at rest before
 *        the user began to pump.  This is made available via the global variable
 *        angleCurrent.  Prior to calling this function, angleCurrent should be
 *        set to angleAtRest.  
 *        This does not need to be at one of the stops.  A person could be 
 *        holding it anywhere but not moving it up and down.
 * Output: 0 if the WPS sensor is broken or the user stopped trying to prime
 *           the pump as evidenced by stopping to move the handle for 10sec.
 *         1 if the pump has primed as evidenced by water being detected
 * Overview:  Measures total lifting water angle that is required before water
 *            is detected.  
 *            Uses total lifting angle and time to determine the meters water
 *            needed to be lifted to prime the pump.
 *            If this is the largest amount seen since midnight, saves the value
 *            as longestPrime             
 * TestDate: NOT TESTED
 ********************************************************************/
int WaitForPrime(float primeAngleCurrent){
    //float angleCurrent = 0; // Stores the current angle of the pump handle
    float angleDelta = 0; // Stores the difference between the current and previous angles
	float upStrokePrime = 0; // Stores the cumulative lifting water handle movement in degrees
    float upStrokePrimeMeters = 0; // Stores the upstroke in meters
    int stopped_pumping_index = 0;
    int primed = 0;  // Assume that the pump is not primed
    int quit = 0;    // Assume the user pumps until the pump primes
    
    sendDebugMessage("\n\n We are in the Priming Loop ", -0.1);  //Debug
    anglePrevious = primeAngleCurrent;
    water_Led = 0;  // Assume no Water at first 
    waterPresenceSensorOnOffPin = 1; //turn on the water presence sensor
    delayMs(5); //make sure the 555 has had time to turn on
    if(readWaterSensor() == 2){ // The WPS is broken
        waterPresenceSensorOnOffPin = 0; //turn OFF the water presence sensor
    }
    else{
        // Record the time that priming effort begins
        // needed in the calculation of measured volume required to prime
        int primeMinutes = minuteVTCC; // keeps track of loop minutes
        float primeSeconds = secondVTCC; // keeps track of loop seconds
        float primeSubSeconds = TMR2;
        // Turn on the HMS sample timer
        TMR4 = 0;
        T4CONbits.TON = 1; // Starts 16-bit Timer4 (inc every 64usec)
        while(readWaterSensor() != 0){ // Wait until there is water
            ClearWatchDogTimer();     // It is unlikely that we will be priming for 
                                      // 130sec without a stop, but we might
            
            primeAngleCurrent = getHandleAngle(); //gets the filtered current angle
			angleDelta = primeAngleCurrent - anglePrevious;  //determines the amount of handle movement from last reading
			anglePrevious = primeAngleCurrent;               //Prepares anglePrevious for the next loop
            // Has the handle stopped moving?
			if((angleDelta > (-1*angleThresholdSmall)) && (angleDelta < angleThresholdSmall)){   //Determines if the handle is at rest
                stopped_pumping_index++; // we want to stop if the user stops pumping
                pumping_Led = 0;   //Turn off pumping led - red
                if((stopped_pumping_index) > max_pause_while_priming){  // They quit trying for at least 10 seconds
                    sendDebugMessage("        Stopped trying to prime   ", upStrokePrime);  //Debug
                    quit = 1;
                    waterPresenceSensorOnOffPin = 0; //turn OFF the water presence sensor
                    break;
                }
			} else{
                stopped_pumping_index=0;   // they are still trying
                pumping_Led = 1;   //Turn on pumping led - red
			}
            // Are they lifting water so we should add this delta to our sum?            
			if(angleDelta < 0) {  //Determines direction of handle movement
                upStrokePrime += (-1) * angleDelta;                 //If the valve is moving downward, the movement is added to an
										                            //accumulation var
			}
            while (TMR4 < hmsSampleThreshold); //fixes the sampling rate at about 102Hz
            TMR4 = 0; //reset the timer before reading WPS            
        }
        if(!quit){ // Water was detected, we are not here because they quit
            primed = 1;     // The pump has primed
            water_Led = 1;  // Turn on the Water LED   
        }
        T4CONbits.TON = 0; // Turn off HMS sample timer
        // Calculate the number of seconds that it took to prime the pump
        primeSeconds = secondVTCC - primeSeconds; // get the number of seconds pumping from VTCC (in increments of 4 seconds)
        primeSeconds += (TMR2 - primeSubSeconds) / 15625.0; // get the remainder of seconds (less than the 4 second increment)
        primeMinutes = minuteVTCC - primeMinutes; // get the number of minutes pumping from VTCC
        if (primeMinutes < 0) {
            primeMinutes += 60; // in case the hour incremented and you get negative minutes, make them positive
        }
        primeSeconds += primeMinutes * 60; // add minutes to the seconds
        if(quit){
            primeSeconds = primeSeconds - 10;  //subtract time that handle was stopped when they quit
        }
        upStrokePrimeMeters = CalculateVolume(upStrokePrime,primeSeconds); // First get volume lifted
        upStrokePrimeMeters = (upStrokePrimeMeters / 1000) / 0.000899; // convert to meters based on volume of rising main
        
		if (upStrokePrimeMeters > longestPrime){                      // Updates the longestPrime  
			longestPrime = upStrokePrimeMeters;
            EEProm_Write_Float(EELongestPrimeCurrent,&longestPrime);                      // Save to EEProm
            sendDebugMessage("Saved New Max Prime Distance ", upStrokePrimeMeters);  //Debug
		}
        sendDebugMessage("Prime Distance ", upStrokePrimeMeters);  //Debug
    }
    angleCurrent = primeAngleCurrent;  // Starting point for the volume function
    
  return primed; 
}
/*********************************************************************
 * Function: float Volume(void)
 * Input: The last angle of the handle in the WaitForPrime function is held in the
 *        variable angleCurrent.  This is the starting point for the Volume
 *        event.
 * Output: The volume of water dispensed in liters.  
 * Overview:  It is assumed that the pump has been primed before this function
 *            is called.  The total lifting water angle of the handle is 
 *            accumulated.  (stops when the handle is motionless for 1sec.)
 *            Uses total lifting angle and time to determine the Liters of water
 *            which has been dispensed and adds this to the proper volume bin
 *            depending upon the current 2hr time window of the pumping             
 * TestDate: NOT TESTED
 ********************************************************************/
float Volume(void){
    float angleDelta = 0; // Stores the difference between the current and previous angles
    float upStrokeExtract = 0; // Stores the cumulative lifting water handle movement in degrees
    float volumeEvent; //Liters of water dispensed
    int stopped_pumping_index = 0; // Used to time how long the handle is stopped
    int dispensing = 1;  // Assume we are still dispensing water 
                         // (water is there and we are moving the handle)
    //sendDebugMessage("\n We are in the Volume Loop ", -0.1);  //Debug
    anglePrevious = angleCurrent; // This is where priming left off
    // Record the time that dispensing water begins
    // needed in the calculation of measured volume required to prime
    int pumpMinutes = minuteVTCC; // keeps track of loop minutes
    float pumpSeconds = secondVTCC; // keeps track of loop seconds
    float pumpSubSeconds = TMR2;
    //////////  New variables used to count # of Strokes
    angleCurrent = getHandleAngle();
    float MaxUp = angleCurrent;    //Top of the latest pumping stroke
    float MaxDown = angleCurrent;  // Bottom of the latest pumping stroke
    int StrokeOver= 0;  //Binary indicating that the latest pumping stroke has ended
    int StrokeStarting = 1; //Binary indicating that a new pumping stroke has begun
    float MinStrokeRange = 10;  //degrees, roughly moving handle through 10"
    int NumStrokes = 0;     // The number of pumping strokes dispensing water
    
    ///////////
    sendDebugMessage("    we are entering the volume loop   ", -0.1);
    // Turn on the HMS sample timer
    TMR4 = 0;
    T4CONbits.TON = 1; // Starts 16-bit Timer4 (inc every 64usec)
    while(dispensing){  //Water is present and handle is moving
        ClearWatchDogTimer();     // It is unlikely that we will be pumping for 
                                  // 130sec without a stop, but we might
        if(readWaterSensor() != 0){ //Water not detected
            dispensing = 0; // no longer dispensing water
            water_Led = 0;  // Turn OFF the Water LED
        } 
        else{
            angleCurrent = getHandleAngle(); //gets the filtered current angle
            angleDelta = angleCurrent - anglePrevious;  //determines the amount of handle movement from last reading
            anglePrevious = angleCurrent;               //Prepares anglePrevious for the next loop
            
            
            
            /////////////////////////////////////
            // Debug - Development sendDebugMessage("", angleCurrent);     //Shows stream of angles in radians
            /////////////////////////////////////
            
            
            
            // Has the handle stopped moving?
            if((angleDelta > (-1*angleThresholdSmall)) && (angleDelta < angleThresholdSmall)){   //Determines if the handle is at rest
                stopped_pumping_index++; // we want to stop if the user stops pumping
                pumping_Led = 0;   //Turn OFF pumping led - red
                if((stopped_pumping_index) > max_pause_while_pumping){  // They quit trying for at least 1 second
                    
                    dispensing = 0; // no longer moving the handle
                    sendDebugMessage("        Stopped pumping   ", -1);  //Debug
                    if(StrokeStarting == 1){
                        NumStrokes++; //They stopped on a pumping stroke
                    }
                }
            } else{
                stopped_pumping_index=0;   // they are still trying
                pumping_Led = 1;   //Turn ON pumping led - red
            }
            // Are they lifting water so we should add this delta to our sum?            
            if(angleDelta < 0) {  //Determines direction of handle movement
                upStrokeExtract += (-1) * angleDelta;   //If the valve is moving downward, the movement is added to an
									                //accumulation var
                if(angleCurrent < MaxUp-MinStrokeRange ){ // A new stroke may be starting
                    if(StrokeStarting==0){//Yes this is the start of a new stroke
                        StrokeStarting = 1;
                        StrokeOver = 0;
                        MaxDown = angleCurrent;
                    }
                }
                if(angleCurrent < MaxDown){
                    MaxDown = angleCurrent; // Still lifting water
                }
            }
            else{// The handle is not lifting water
                if(angleCurrent > MaxDown+MinStrokeRange){ //Pumping stroke may be finished
                    if(StrokeOver == 0){ //Yes the pumping stroke is complete
                        NumStrokes++;
                        StrokeOver = 1;
                        StrokeStarting = 0;
                        MaxUp = angleCurrent;
                    }
                    if(angleCurrent > MaxUp){
                        MaxUp = angleCurrent;
                    }
                }
            }
        }
        while (TMR4 < hmsSampleThreshold); //fixes the sampling rate at about 102Hz
        TMR4 = 0; //reset the timer before reading WPS            
    }
    // Pumping has stopped
    T4CONbits.TON = 0; // Turn off HMS sample timer
    // Calculate the number of seconds that water was being dispensed
    pumpSeconds = secondVTCC - pumpSeconds; // get the number of seconds pumping from VTCC (in increments of 4 seconds)
    pumpSeconds += (TMR2 - pumpSubSeconds) / 15625.0; // get the remainder of seconds (less than the 4 second increment)
    pumpMinutes = minuteVTCC - pumpMinutes; // get the number of minutes pumping from VTCC
    if (pumpMinutes < 0) {
        pumpMinutes += 60; // in case the hour incremented and you get negative minutes, make them positive
    }
    pumpSeconds += pumpMinutes * 60; // add minutes to the seconds
    volumeEvent = CalculateVolume(upStrokeExtract,pumpSeconds); // Find volume lifted
    // We will assume that the leak rate is slow enough that it does not effect
    // the volume pumped calculation
    sendDebugMessage("Number of pumping strokes = ",NumStrokes); //Debug
    sendDebugMessage("handle movement in degrees ", upStrokeExtract);  //Debug
    sendDebugMessage("handle moving for (sec) ", pumpSeconds);  //Debug
    sendDebugMessage("Volume Event = ", volumeEvent);  //Debug
    sendDebugMessage("  for time slot ", hour);  //Debug
    // Add to the volume bin
    // organize flow into 2 hours bins
    // The hour was read at the start of the handle movement routine       
	switch (hour / 2)
    {
        case 0:
			volume02 = volume02 + volumeEvent;
  			break;
		case 1:
			volume24 = volume24 + volumeEvent;
			break;
		case 2:
			volume46 = volume46 + volumeEvent;
			break;
		case 3:
			volume68 = volume68 + volumeEvent;
			break;
		case 4:
			volume810 = volume810 + volumeEvent;
			break;
		case 5:
			volume1012 = volume1012 + volumeEvent;
			break;
		case 6:
			volume1214 = volume1214 + volumeEvent;
			break;
		case 7:
			volume1416 = volume1416 + volumeEvent;
			break;
		case 8:
			volume1618 = volume1618 + volumeEvent;
			break;
		case 9:
			volume1820 = volume1820 + volumeEvent;
			break;
		case 10:
			volume2022 = volume2022 + volumeEvent;
			break;
		case 11:
			volume2224 = volume2224 + volumeEvent;
			break;
		}
    return volumeEvent;
}
/*********************************************************************
 * Function: float LeakRate(float volumeEvent)
 * Input: the number of liters reported as having been pumped by the Volume Function
 * Output: latest valid leak rate.  Also, if the latest leak rate  is larger 
 *         than the largest since midnight, the maximum leak rate is updated
 * Overview:  If there is water detected and the user does not begin pumping
 *            again, and at least 2 ltr of water were dispensed during pumping,
 *            the time it takes for water to drain from the pump tank down below
 *            the Water Presence Sensor is measured and used to determine the 
 *            leak rate of the pump in ltr/sec.  The maximum is saved as ltr/hr.             
 * TestDate: NOT TESTED
 ********************************************************************/
float LeakRate(float volumeEvent){
    sendDebugMessage("\n We are in the Leak Rate Loop ", -0.1);  //Debug
    int validLeakRate = 1; //Assume we can measure leak rate
    long leakDurationCounter = 100; // The user stopped pumping for 1 second before DEBUG made leakDurationCounter a long variable to ensure space for max time
                                   // we get here (100Hz leak sample rate)
    float angleAtRest = angleCurrent; // This is where pumping left off
    
    if(volumeEvent < 2){// can't measure a valid leak rate (maybe just a splash)
        validLeakRate = 0;
    }
    else{
        // Turn on the HMS sample timer
        TMR4 = 0;
        T4CONbits.TON = 1; // Starts 16-bit Timer4 (inc every 64usec)
        while(validLeakRate){
            ClearWatchDogTimer(); // We need to wait between 130 and 600 seconds for our desired leak rates, 
                                  // so we do not want the WatchDogTimer expiring
            if(readWaterSensor() != 0){ //Water not detected
                water_Led = 0;  // Turn OFF the Water LED 
                sendDebugMessage("\n There is no water. ", -0.1);
                if(leakDurationCounter == 100){
                    validLeakRate = 0; // Water was gone when we got here, something is not right
                }
                break;                
            }
            if(HasTheHandleStartedMoving(angleAtRest)){ // if they start pumping, stop calculating leak
                pumping_Led = 1;   //Turn ON pumping led - red
                validLeakRate = 0; // Can't measure leak rate have to monitor volume
            }
            if((leakDurationCounter * 9.98) >= leakRateTimeOut){ //9.98ms per sample
                break;
            }
            while (TMR4 < 156); //fixes the sampling rate at about 100Hz
            TMR4 = 0; //reset the timer before reading WPS
            leakDurationCounter++;            
        }
    }
     // Now we need to calculate leak rate and save it if we have a valid basis to calculate
    T4CONbits.TON = 0; // Stops 16-bit Timer4 (inc every 64usec)
    waterPresenceSensorOnOffPin = 0; //turn OFF the water presence sensor
    if(validLeakRate){
        //Calculate a new leak rate
        leakRateCurrent = leakSensorVolume * 3600 / ((leakDurationCounter * 9.98) / 1000.0); // L/H

        /* Possible lines for extremes of leak rate testing (117 seconds to 20 minutes)
        
        long leakDurationHours = ((leakDurationCounter * 9.98) / 1000.0);    //DEBUG leak duration in hours, making calculations easier since we are only concerned about L/H now

        if((leakDurationCounter) > 120000)     
        {
            leakRateCurrent = 0;
            sendDebugMessage("Leak Rate is less than 0.389 L/h");
            sendDebugMessage("Elapsed time is 20 minutes");
                    
            leakRateLong = leakRateCurrent;  //Liters/hour
            EEProm_Write_Float(EELeakRateLongCurrent,&leakRateLong);                   // Save to EEProm DEBUG going through it says that EELeakRateLongCurrent is 0x0000
            sendDebugMessage("Saved new longest leak rate to EEProm ", leakRateLong);  
        }
        if((leakDurationCounter) < 1170)
        {
            leakRateCurrent = 4;
            sendDebugMessage("Leak Rate is more than 4 L/h");
            sendDebugMessage("Elapsed time is less than 2 minutes");
            sendDebugMessage("Pump is not effective");
            sendDebugMessage("Inform Jared Groff")
        }
        else{
        */
        
        sendDebugMessage("Leak Rate (L/hour) = ", leakRateCurrent);  
        sendDebugMessage("  - Leak Rate Long = ", leakRateLong);  
        sendDebugMessage("Elapsed Time (sec): ", (leakDurationCounter * 9.98 / 1000)); 
        if ((leakRateCurrent) > leakRateLong)
		{
			leakRateLong = leakRateCurrent;  //Liters/hour
            EEProm_Write_Float(EELeakRateLongCurrent,&leakRateLong);                   // Save to EEProm
            sendDebugMessage("Saved new longest leak rate to EEProm ", leakRateLong);
		}
        //}     Bracket needed for possible else statement for the extreme cases
    }
    return leakRateCurrent;
}
/*********************************************************************
 * Function: void TechAtPumpActivities(void)
 * Input: None.
 * Output: None
 * Overview:  We know that there is a tech at the pump if the Tech_At_Pump
 *            switch is putting a LOW on the PIC pin.  In this case we enable
 *            the WPS and check for water (to update the waterLED) each time
 *            around the handle_not_moving loop.  We also check for text messages
 *            if it has been at least 20sec since we last checked.
 *            We don't spend as long waiting for a 
 *            message as we do when checking each hour and the Cellphone board
 *            is left ON so that we don't waste time turning it on and off.
 * TestDate: NOT TESTED
 ********************************************************************/
int TechAtPumpActivities(int nextTextMsgCheck){
    waterPresenceSensorOnOffPin = 1; //turn on the water presence sensor
    delayMs(5); // Let WPS freq stabilize.
    int wpsStatus = readWaterSensor();
    waterPresenceSensorOnOffPin = 0; //turn off the water presence sensor
    if(wpsStatus == 0) { // there is water
        water_Led = 1;
    } else if(wpsStatus == 1){ //there is not water
        water_Led = 0;
    } else {            // WPS is disconnected, flash the water LED
        water_Led = !water_Led;
        delayMs(250);
    }
    
    if((secondVTCC >= nextTextMsgCheck)&&(secondVTCC < nextTextMsgCheck+2)){// 20 seconds have gone by since we last checked for text messages
        CheckIncommingTextMessages(); //See if there are any text messages and take
                                  //any called for action
                                  // the SIM is powered ON at this point 
        nextTextMsgCheck = secondVTCC + 20;
        if (nextTextMsgCheck >=60){
            nextTextMsgCheck = nextTextMsgCheck - 60;
        }
    }
    return nextTextMsgCheck;
    
}
/*********************************************************************
 * Function: void HourlyActivities(void)
 * Input: None.
 * Output: None
 * Overview:  We call this function when a new hour begins.
 *            * If it is a new even hour, save the accumulated pumping volume to EEPROM
 *            * Measure the battery voltage and update our 3hour array which tracks 
 *              if its voltage is dropping quickly indicating that it is out of charge
 *            * Read and respond to any text messages which have been sent
 *            * If it is noon, create the daily text message to send
 *            * try to send the daily text message this will be attempted every 
 *              hour for up to 5 days until it is sent
 *            * send daily text messages from previous days which have not yet 
 *              been sent
 *            * send the hourly diagnostic message if a text message enabled this
 *              feature.  If this can't be sent due to the network being inactive
 *              it is lost rather that attempted later like the daily message 
 * TestDate: NOT TESTED
 ********************************************************************/ 
void HourlyActivities(void){
    if(hour/2 != active_volume_bin){
        SaveVolumeToEEProm();
    }
    //Update the Battery level Array used in sleep decision
    BatteryLevelArray[0]=BatteryLevelArray[1];
    BatteryLevelArray[1]=BatteryLevelArray[2];
    BatteryLevelArray[2]=batteryLevel();
    // Read text messages sent to the system
    
    CheckIncommingTextMessages();  // Reads and responds to any messages sent to the system
                                   // The SIM is ON at this point              
    if(hour == 12){  // If it is noon, save a daily report
        CreateAndSaveDailyReport();
    }           
    // Attempt to Send daily report and if enabled, diagnostic reports
    SendSavedDailyReports();   
    SendHourlyDiagnosticReport();
    turnOffSIM();
    prevHour = hour; // update so we know this is not the start of a new hour 
}
/*********************************************************************
 * Function: void SaveBatteryPower(void)
 * Input: None.
 * Output: None
 * Overview:  We call this function when the current battery voltage is either
 *            less than 3.2V or has dropped more than 0.05V in the last two hours.
 *            a drop that quickly indicates that the battery is about empty.
 *            * To save power, turn off the Cellphone board.
 *            * For the diagnostic report, set the bit indicating that the system
 *              went to sleep at some time during the day
 *            * Using the Watchdog timer to wake up, put PIC to sleep for 131 second
 *            * When we wake up, make the 131 sec correction to the VTCC
 *            * If is has been 24min since the battery voltage was checked, check
 *              it so we can wake up again once it has charged to where it was
 *              2hrs before entering this function.  If it is not charged back up
 *              go back to sleep for another 131sec and repeat this process  
 * TestDate: NOT TESTED
 ********************************************************************/
void SaveBatteryPower(void){
    while(BatteryLevelArray[2]<BatteryLevelArray[0]){
        // The WDT settings will let the PIC sleep for about 131 seconds.
        if((!threeG && !fourG && statusPin) || (threeG && !statusPin) || (fourG && !statusPin)) { // if the Fona is on, shut it off
            turnOffSIM();             
        }
        if (sleepHrStatus != 1){ // Record the fact that we went to sleep for diagnostic reporting purposes
            sleepHrStatus = 1;
            EEProm_Write_Float(DiagSystemWentToSleep,&sleepHrStatus);        
        }                
        Sleep(); 
        // OK, we just woke up  
        secondVTCC = secondVTCC + 131;
        updateVTCC(); //Try to keep VTCC as accurate as possible
        TimeSinceLastBatteryCheck++; // only check the battery every 11th time you wake up (approx 24min)
        // check the battery every 24 min
        if(TimeSinceLastBatteryCheck >= 11){
            BatteryLevelArray[2]=batteryLevel();
            TimeSinceLastBatteryCheck = 0;
        }
    }    
} 

