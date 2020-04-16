/*
  Adapted by Kevin Mulligan from, "ArduMeter (Arduino incident light meter for cameras)"  by Alan Wang originally using BH1750/BH1750FVI digital light intensity sensor:
*/

#include <math.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include "EncoderStepCounter.h"

#define ROTARY_ENC_PIN1 2
#define ROTARY_ENC_INT1 digitalPinToInterrupt(ROTARY_ENC_PIN1)
#define ROTARY_ENC_PIN2 3
#define ROTARY_ENC_INT2 digitalPinToInterrupt(ROTARY_ENC_PIN2)

EncoderStepCounter rotary_encoder(ROTARY_ENC_PIN1, ROTARY_ENC_PIN2, HALF_STEP);

Adafruit_TSL2561_Unified TSL = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);


int currentMode = 0; //keep track of current mode
int caseStatementLength[3] = {8, 12, 10}; //the length of a case statement is determined by the mode
const int ISOConfigMode = 0; //set mode to change ISO
const int shutterPriorityMode = 1; //set mode to shutter priority
const int aperturePriorityMode = 2; //set mode to aperture priority

String StrMode [3] = { "ISO CONFIGURATION", "SHUTTER PRIORITY", "APERTURE PRIORITY" }; //Name for current mode
int knobApertureStatus = 1; //status of aperture select knob
int prevKnobApertureStatus; //previous status of aperture select knob
int knobISOStatus; //status of ISO select knob
int prevKnobISOStatus; //previous status of ISO select knob
int knobShutterStatus; //status of Shutter select knob
int prevKnobShutterStatus; //previous status of Shutter select knob

long lux; //illuminance
float EV; //exposure value
int ISO; //film or digital sensor sensitivity    - always a constant in calculations, i.e. the ISO needn't change to acommadate the other values
float aperture = 1; //diameter of lens opening       - can be set by the user or calculation
float shutterSpeed = 1; //time that the film or sensor is exposed to light - can be set by the user or calculation



const int incidentCalibration = 340; //incident light calibration const, see https://en.wikipedia.org/wiki/Light_meter#Calibration_constants

void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // TSL.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  TSL.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  //TSL.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */

  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  TSL.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // TSL.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // TSL.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

}

// Call tick on every change interrupt
void rotary_interrupt() {
  rotary_encoder.tick();
}

signed long rotary_position = 1;  //is the current rotary setting
signed long rotary_position_ISO = 3;       //keeps track of ISO setting, start on ISO 100
signed long rotary_position_shutter = 9;   //keeps track of shutter setting
signed long rotary_position_aperture = 3;  //keeps track of aperture setting, start on F 2.0

void setup() {
  //initialize
  pinMode(10, INPUT_PULLUP);
  Serial.begin(9600);
  // Initialize encoder
  rotary_encoder.begin();
  // Initialize interrupts

  attachInterrupt(ROTARY_ENC_INT1, rotary_interrupt, CHANGE);
  attachInterrupt(ROTARY_ENC_INT2, rotary_interrupt, CHANGE);

  Wire.begin();
  configureSensor();
  if (!TSL.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }

  calculateExposureSetting(true);
}

void loop() {
  Serial.println("---------------------------------------------------------------------------------------");
  if (!digitalRead(10)) {
    Serial.println("Clicked");
    currentMode = currentMode + 1;

    if (currentMode > 2) {
      currentMode = 0;
    }
    if (currentMode < 0) {
      currentMode = 2;
    }

    //    switch (currentMode) {
    //      case ISOConfigMode: StrMode = "ISO CONFIGURATION"; break; //update the iso position if in ISO mode
    //      case shutterPriorityMode: StrMode = "SHUTTER PRIORITY"; break;//update the shutter position if in shutter mode
    //      case aperturePriorityMode: StrMode = "APERTURE PRIORITY"; break;//update the aperture position if in aperture mode
    //      default: ;
    //    }

    Serial.println("Current mode: " + StrMode[currentMode]);
  }

  signed char rotary_pos = rotary_encoder.getPosition();

  switch (currentMode) {
    case ISOConfigMode: rotary_position = rotary_position_ISO; break; //set rotary position to current iso postion
    case shutterPriorityMode: rotary_position = rotary_position_shutter; break;//set rotary position to current shutter postion
    case aperturePriorityMode: rotary_position = rotary_position_aperture; break;//set rotary position to current aperture postion
    default: ;
  }

  if (rotary_pos != 0) {
    rotary_position += rotary_pos;
    if (rotary_position < 1) {
      rotary_position = 1;
    } else if (rotary_position > caseStatementLength[currentMode]) {
      rotary_position = caseStatementLength[currentMode];
    }

    switch (currentMode) {
      case ISOConfigMode: rotary_position_ISO = rotary_position; break; //update the iso position if in ISO mode
      case shutterPriorityMode: rotary_position_shutter = rotary_position; break;//update the shutter position if in shutter mode
      case aperturePriorityMode: rotary_position_aperture = rotary_position; break;//update the aperture position if in aperture mode
      default: ;
    }

    rotary_encoder.reset();
  }


  /* Get a new sensor event */
  sensors_event_t event;
  TSL.getEvent(&event);
  if (event.light)
  {
    lux = event.light;
  }
  else
  {
    /* If event.light = 0 lux the sensor is probably saturated
       and no reliable data could be generated! */
    Serial.println("Sensor overload");
  }

  knobISOStatus = rotary_position_ISO;
  knobApertureStatus = rotary_position_aperture;
  knobShutterStatus = rotary_position_shutter;

  //knobApertureStatus = map(rotary_position_aperture, 0, 30, 1, 10);
  calculateExposureSetting(true);

  //record potentiometer previous status
  prevKnobApertureStatus = knobApertureStatus;
  prevKnobISOStatus = knobISOStatus;
  prevKnobShutterStatus = knobShutterStatus;
  delay(500);
}

void calculateExposureSetting(bool measureNewExposure) {
  Serial.print("knobISOStatus:  ");
  Serial.println(knobISOStatus);
  Serial.print("knobApertureStatus:  ");
  Serial.println(knobApertureStatus);
  Serial.print("knobShutterStatus:  ");
  Serial.println(knobShutterStatus);


  //select ISO to the rotary encoder's value whilst the camera was in ISO mode.
  //in our case, ISO is never dependant on other variables and is always set by the user
  switch (knobISOStatus) {
    case 1: ISO = 25; break;
    case 2: ISO = 50; break;
    case 3: ISO = 100; break;
    case 4: ISO = 200; break;
    case 5: ISO = 400; break;
    case 6: ISO = 800; break;
    case 7: ISO = 1600; break;
    case 8: ISO = 3200;
  }


  //select aperture to the rotary encoder's value whilst the camera was in aperture mode.
  if (currentMode == aperturePriorityMode) {  //only update the value to the knob postition IF it's in aperture priority mode, otherwise it's dependant on the other variables
    switch (knobApertureStatus) {
      case 1: aperture = 1; break;
      case 2: aperture = 1.4; break;
      case 3: aperture = 2; break;
      case 4: aperture = 2.8; break;
      case 5: aperture = 4; break;
      case 6: aperture = 5.6; break;
      case 7: aperture = 8; break;
      case 8: aperture = 11; break;
      case 9: aperture = 16; break;
      case 10: aperture = 22; break;
      default: aperture = 1;
    }
  }

  //select shutter to the rotary encoder's value whilst the camera was in shutter mode.
  if (currentMode == shutterPriorityMode) {  //only update the value to the knob postition IF it's in shutter priority mode, otherwise it's dependant on the other variables
    switch (knobShutterStatus) {
      //            case 1: shutterSpeed = 0.0005; break; // 1/2000
      //            case 2: shutterSpeed = 0.001; break;    // 1/1000
      //            case 3: shutterSpeed = 0.002; break;      // 1/500
      //            case 4: shutterSpeed = 0.004; break;    // 1/250
      //            case 5: shutterSpeed = 0.008; break;      // 1/125
      //            case 6: shutterSpeed = 0.0166666666666667; break;    // 1/60
      //            case 7: shutterSpeed = 0.0333333333333333; break;      // 1/30
      //            case 8: shutterSpeed = 0.0666666666666667; break;      // 1/15
      //            case 9: shutterSpeed = 0.125; break;     // 1/8
      //            case 10: shutterSpeed = 0.25; break;    // 1/4
      //            case 11: shutterSpeed = 0.5; break;    // 1/2
      //            case 12: shutterSpeed = 1; break;    // 1
      //            default: shutterSpeed = 1;
      case 1: shutterSpeed = 2000; break; // 1/2000
      case 2: shutterSpeed = 1000; break;    // 1/1000
      case 3: shutterSpeed = 500; break;      // 1/500
      case 4: shutterSpeed = 250; break;    // 1/250
      case 5: shutterSpeed = 125; break;      // 1/125
      case 6: shutterSpeed = 60; break;    // 1/60
      case 7: shutterSpeed = 30; break;      // 1/30
      case 8: shutterSpeed = 15; break;      // 1/15
      case 9: shutterSpeed = 8; break;     // 1/8
      case 10: shutterSpeed = 4; break;    // 1/4
      case 11: shutterSpeed = 2; break;    // 1/2
      case 12: shutterSpeed = 1; break;    // 1
      default: shutterSpeed = 1;
    }
  }


  if (measureNewExposure) {
    //measure light level (illuminance) and get a new lux value
    /* Get a new sensor event */
    sensors_event_t event;
    TSL.getEvent(&event);

    /* Display the results (light is measured in lux) */
    if (event.light)
    {
      lux = event.light;
    }
    else
    {
      /* If event.light = 0 lux the sensor is probably saturated
         and no reliable data could be generated! */
      Serial.println("Sensor overload");
    }

    Serial.print("Measured illuminance = ");
    Serial.print(lux);
    Serial.println(" lux");
  }
  float tmp;

  //calculate EV
  EV = (log10(lux * ISO / incidentCalibration) / log10(2));
  if (isfinite(EV)) {
    //calculate shutter speed if EV is not NaN or infinity
    if (currentMode == aperturePriorityMode) {
      shutterSpeed = pow(2, EV) / pow(aperture, 2);
      Serial.println(shutterSpeed);


      if (shutterSpeed < 1.5) {
        tmp = 1;
      } else if (shutterSpeed < (2 + (2 - 1) / 2)) {
        tmp = 2;
      } else if (shutterSpeed < (4 + (8 - 4) / 2)) {
        tmp = 4;
      } else if (shutterSpeed < (8 + (15 - 8) / 2)) {
        tmp = 8;
      } else if (shutterSpeed < (15 + (30 - 15) / 2)) {
        tmp = 15;
      } else if (shutterSpeed < (30 + (60 - 30) / 2)) {
        tmp = 30;
      } else if (shutterSpeed < (60 + (125 - 60) / 2)) {
        tmp = 60;
      } else if (shutterSpeed < (125 + (250 - 125) / 2)) {
        tmp = 125;
      } else if (shutterSpeed < (250 + (500 - 250) / 2)) {
        tmp = 250;
      } else if (shutterSpeed < (500 + (1000 - 500) / 2)) {
        tmp = 500;
      } else if (shutterSpeed < (1000 + (2000 - 1000) / 2)) {
        tmp = 1000;
      } else {
        tmp = 2000;
      }
      shutterSpeed = tmp;




    } else if (currentMode == shutterPriorityMode) {
      aperture =  sqrt(pow(2, EV) / shutterSpeed);
      if (aperture < 1) {
        tmp = 1;
      } else if (aperture < (1.4 + (2 - 1.4) / 2)) {
        tmp = 1.4;
      } else if (aperture < (2 + (2.8 - 2) / 2)) {
        tmp = 2;
      } else if (aperture < (2.8 + (4 - 2.8) / 2)) {
        tmp = 2.8;
      } else if (aperture < (4 + (5.6 - 4) / 2)) {
        tmp = 4;
      } else if (aperture < (5.6 + (8 - 5.6) / 2)) {
        tmp = 5.6;
      } else if (aperture < (8 + (11 - 8) / 2)) {
        tmp = 8;
      } else if (aperture < (11 + (16 - 11) / 2)) {
        tmp = 11;
      } else if (aperture < (16 + (22 - 16) / 2)) {
        tmp = 16;
      } else {
        tmp = 22;
      }
      aperture = tmp;
    }

    //output results to serial port
    Serial.print("Numberic mode  ");
    Serial.println(currentMode);
    Serial.print("Mode           ");
    Serial.println(StrMode[currentMode]);

    Serial.print("Exposure settings: ISO = ");
    Serial.print(ISO);
    Serial.print(", EV = ");
    Serial.print(EV);
    Serial.print(", aperture = f/");
    Serial.print(aperture, 1);
    Serial.print(", ");
    Serial.print("shutter speed = ");
    if (shutterSpeed >= 1) {
      Serial.print("1/");
      Serial.print(shutterSpeed);
    } else {
      Serial.print((1 / shutterSpeed));
    }
    Serial.println("s");
    Serial.println("");
  }
}
