// DUSTDUINO v1.0
// Released 18 October 2013
//
// This software is released as-is, without warranty,
// under a Creative Commons Attribution-ShareAlike
// 3.0 Unported license. For more information about
// this license, visit:
// http://creativecommons.org/licenses/by-sa/3.0/
//
// Written by Matthew Schroyer, except where specified.
// For more information on building a DustDuino, visit:
// http://www.mentalmunition.com/2013/10/measure-air-pollution-with-dustduino-of.html

/*888888888888888888888888888888888888888888888888888888888888888888888888888888888888*/

// WiFlyHQ Library written by Harlequin-Tech.
// Available at: https://github.com/harlequin-tech/WiFlyHQ
#include <WiFlyHQ.h>
#include <avr/wdt.h>

unsigned long starttime;

unsigned long triggerOnP1;
unsigned long triggerOffP1;
unsigned long pulseLengthP1;
unsigned long durationP1;
boolean valP1 = HIGH;
boolean triggerP1 = false;

unsigned long triggerOnP2;
unsigned long triggerOffP2;
unsigned long pulseLengthP2;
unsigned long durationP2;
boolean valP2 = HIGH;
boolean triggerP2 = false;
int i = 0;
float ratioP1 = 0;
float ratioP2 = 0;
unsigned long sampletime_ms = 30000;
float countP1;
float countP2;

WiFly wifly;

void terminal();

#define ARDUINO_RX_PIN  2
#define ARDUINO_TX_PIN  3

#define APIKEY         "LDpN1jHYhW5izbmPLDe2cIyy674zGXdILQxb3fHKP9KPhNEE" // your cosm api key
#define FEEDID         2066768642 // your feed ID
#define USERAGENT      "GoDustMites" // user agent is the project name
const char server[] = "192.168.1.100";

void setup(){
  Serial.begin(9600);
  wifly.begin(&Serial, NULL);
  pinMode(8, INPUT);
  wdt_enable(WDTO_8S);
  starttime = millis();
}

void loop(){
  
  // 1V, >= 1 micrometer
  // (mold, bacteria, ?) 
  // smoke
  valP1 = digitalRead(8);

  // 2.5V, >= 2.5 micrometer  
  // dust/pollen
  // disabled for now
  valP2 = digitalRead(9);
  
  if(valP1 == LOW && triggerP1 == false){
    triggerP1 = true;
    triggerOnP1 = micros();
  }
  
  if (valP1 == HIGH && triggerP1 == true){
      triggerOffP1 = micros();
      pulseLengthP1 = triggerOffP1 - triggerOnP1;
      durationP1 = durationP1 + pulseLengthP1;
      triggerP1 = false;
  }
  
  if(valP2 == LOW && triggerP2 == false){
    triggerP2 = true;
    triggerOnP2 = micros();
  }
  
  if (valP2 == HIGH && triggerP2 == true){
      triggerOffP2 = micros();
      pulseLengthP2 = triggerOffP2 - triggerOnP2;
      durationP2 = durationP2 + pulseLengthP2;
      triggerP2 = false;
  }
  
    wdt_reset();

    // Function creates particle count and mass concentration
    // from PPD-42 low pulse occupancy (LPO).
    if ((millis() - starttime) > sampletime_ms) {
      
      // Generates PM10 and PM2.5 count from LPO.
      // Derived from code created by Chris Nafis
      // http://www.howmuchsnow.com/arduino/airquality/grovedust/
      
      ratioP1 = durationP1/(sampletime_ms*10.0);
      countP1 = 1.1 * pow(ratioP1,3) - 3.8 * pow(ratioP1,2) + 520 * ratioP1 + 0.62;
      
      float PM1mCount = countP1;

      ratioP2 = durationP2/(sampletime_ms*10.0);
      countP2 = 1.1 * pow(ratioP2,3) - 3.8 * pow(ratioP2,2) + 520 * ratioP2 + 0.62;

      float PM10count = countP2;      
      float PM25count = countP1 - countP2;

      
      // Assues density, shape, and size of dust
      // to estimate mass concentration from particle
      // count. This method was described in a 2009
      // paper by Uva, M., Falcone, R., McClellan, A.,
      // and Ostapowicz, E.
      // http://wireless.ece.drexel.edu/research/sd_air_quality.pdf
      double pi = 3.14159;

      // individual particle ave density
      double density = 1.65*pow(10,12);
      

      double K = 3531.5;
      
      // do 1micrometer particles
      // ave radius of particle micrometers
      double r1m = 0.44*pow(10,-6);

      // volume of the r1m particle
      double vol1m = (4/3) * pi * pow(r1m,3);

      // mass of the r10 particle
      double mass1m = density * vol1m;

      float conc1m = (PM1mCount) * K * mass1m;

      
      // begins PM10 mass concentration algorithm

      // ave radius of particle micrometers
      double r10 = 2.6*pow(10,-6);

      // volume of the r10 particle
      double vol10 = (4/3) * pi * pow(r10,3);

      // mass of the r10 particle
      double mass10 = density * vol10;


      float concLarge = (PM10count) * K * mass10;
      
      // next, PM2.5 mass concentration algorithm
      double r25 = 0.44*pow(10,-6);
      double vol25 = (4/3)*pi*pow(r25,3);
      double mass25 = density*vol25;
      float concSmall = (PM25count)*K*mass25;
      
      sendData(concLarge, concSmall, PM10count, PM25count, conc1m, PM1mCount, i);
      
      i++;
      
      durationP1 = 0;
      durationP2 = 0;
      starttime = millis();
      wdt_reset();
    }
  }
  
void sendData(int PM10Conc, int PM25Conc, int PM10count, int PM25count, int PM1mConc, int PM1mCount, int start) {
    int ts = wifly.time();
    
    wifly.open(server, 5984);
    
    wifly.println("POST /dust/ HTTP/1.1");
    //wifly.print(ts);
    //wifly.println(" HTTP/1.1");
    
    // 70 is the baseline with json format + 1 for new line
    wifly.print("Content-Length: ");
    int thisLength = 71 + getLength(start) + getLength(PM10Conc) + getLength(PM25Conc) + getLength(PM10count) + getLength(PM25count) + getLength(PM1mConc) + getLength(PM1mCount);
    wifly.println(thisLength);
    
    wifly.println("Content-Type: application/json");
    wifly.println("Connection: close");
    wifly.println();

    wifly.print("{");

    wifly.print("\"ts\":");
    wifly.print(start);
    wifly.print(",");
    
    wifly.print("\"PM10\":");
    wifly.print(PM10Conc);
    wifly.print(",");
    
    wifly.print("\"PM25\":");
    wifly.print(PM25Conc);
    wifly.print(",");
    
    
    wifly.print("\"PM10count\":");
    wifly.print(PM10count);
    wifly.print(",");
    
    wifly.print("\"PM25count\":");
    wifly.print(PM25count);
    wifly.print(",");
    
    wifly.print("\"PM1m\":");
    wifly.print(PM1mConc);
    wifly.print(",");
    
    wifly.print("\"PM1mCount\":");
    wifly.print(PM1mCount);
    
    wifly.println("}");
    
    wifly.close();
}
  
  // This function also is derived from
  // the Patchube client sketch. Returns
  // number of digits, which Xively needs
  // to post data correctly.
  int getLength(int someValue) {
  int digits = 1;
  int dividend = someValue /10;
  while (dividend > 0) {
    dividend = dividend /10;
    digits++;
  }
  return digits;
}

// This function connects the WiFly to serial.
// Developed by Harlequin-Tech for the WiFlyHQ library.
// https://github.com/harlequin-tech/WiFlyHQ

void terminal()
{
    while (1) {
	if (wifly.available() > 0) {
	    Serial.write(wifly.read());
	}


	if (Serial.available() > 0) {
	    wifly.write(Serial.read());
	}
    }
}
