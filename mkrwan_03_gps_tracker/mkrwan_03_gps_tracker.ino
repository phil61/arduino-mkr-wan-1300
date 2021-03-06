/*
 * mkrwan_03_gps_tracker.ino - GPS Tracker example code.
 * Copyright (c) 2018 Gonzalo Casas
 * MIT License
 *
 * Credits:
 * GPS packet format and TTN Mapper integration by:
 *    Copyright (c) 2016 JP Meijers 
 *    Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0
 *    https://github.com/jpmeijers/RN2483-Arduino-Library
 */
#include <TinyGPS++.h>
#include <MKRWAN.h>
#include "arduino_secrets.h" 

#define debugSerial Serial
#define gpsSerial Serial1
#define loraSerial Serial2

// LoRaWAN
// Select your region (AS923, AU915, EU868, KR920, IN865, US915, US915_HYBRID)
_lora_band region = AU915;
LoRaModem modem(loraSerial);

// GPS
TinyGPSPlus gps;

#define PMTK_SET_NMEA_UPDATE_05HZ  "$PMTK220,2000*1C"
#define PMTK_SET_NMEA_UPDATE_1HZ  "$PMTK220,1000*1F"
#define PMTK_SET_NMEA_OUTPUT_RMCGGA "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28"

// TTN Mapper stuff
unsigned long last_update = 0;
uint8_t txBuffer[9];
uint32_t latitudeBinary, longitudeBinary;
uint16_t altitudeGps;
uint8_t hdopGps;

// Flashy stuff
#define VERY_FAST 50
#define FAST 200
#define SLOW 500
#define FOREVER -1

void flash(int times, unsigned int speed) {
  while (times == -1 || times--) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(speed);
    digitalWrite(LED_BUILTIN, LOW);
    delay(speed);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  debugSerial.begin(115200);
  gpsSerial.begin(4800);  //py - changed baud rate from 9600 to 4800 to suit default for EM-411 GPS module

  flash(10, VERY_FAST);

  // Wait a maximum of 5s for Serial Monitor serial
  while (!debugSerial && millis() < 5000);
  debugSerial.println(F("Starting up..."));
  flash(2, SLOW);
  
  // Start LoRa modem
  if (!modem.begin(region)) {
    debugSerial.println(F("Failed to start module"));
    flash(FOREVER, VERY_FAST);
  };

  debugSerial.print(F("Device EUI: "));
  debugSerial.println(modem.deviceEUI());
  flash(2, SLOW);

  int connected = modem.joinOTAA(appEui, appKey);
  
  while (!connected) {
    debugSerial.println(F("Unable to log into TTN LoRaWAN network - Retrying..."));
    modem.restart();
    flash(80, VERY_FAST);
    modem.begin(region);
    connected = modem.joinOTAA(appEui, appKey);
  }
  debugSerial.println(F("Successfully joined the network!"));

  debugSerial.println(F("Enabling ADR and setting low spreading factor"));
  modem.setADR(true);
  modem.dataRate(5);

//Optional section to initialise GPS.  Not required for EM-411
//  debugSerial.println(F("GPS serial init"));
//  gpsSerial.println(F(PMTK_SET_NMEA_OUTPUT_RMCGGA));
//  gpsSerial.println(F(PMTK_SET_NMEA_UPDATE_1HZ));   // 1 Hz update rate - py - disable for EM-411 as already updating at 1 Hz
//  debugSerial.println(F("GPS serial ready"));
//  flash(2, SLOW);
}

void loop() {
  while (gpsSerial.available() > 0) {
    if (gps.encode(gpsSerial.read())) {
      
      displayGpsInfo(); // For debugging
      sendCoords();

      // Delay between updates
      delay(2000);
    }
  }

  if (millis() > 5000 && gps.charsProcessed() < 10) {
    debugSerial.println(F("No GPS detected: check wiring."));
    flash(FOREVER, VERY_FAST);
  }
}

void sendCoords() {
  // py - modified following conditional to include test for hdop <= 2, time and date are valid to prevent sending bad co-ordinates  
  if ((gps.location.age() < 1000) && ((millis() - last_update) >= 1000) && (gps.hdop.value() <= 200)
      && gps.time.isValid() && gps.location.isValid()){
    digitalWrite(LED_BUILTIN, HIGH);

    buildPacket();

    modem.beginPacket();
    modem.write(txBuffer, sizeof(txBuffer));
    int err = modem.endPacket(false);
  
    if (err > 0) {
      debugSerial.println("Coordinates sent!");
      flash(3, FAST);
    } else {
      debugSerial.println("Error");
      flash(10, VERY_FAST);
    }

    last_update = millis();

    delay(3000);
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void buildPacket() {
  latitudeBinary = ((gps.location.lat() + 90) / 180.0) * 16777215;
  longitudeBinary = ((gps.location.lng() + 180) / 360.0) * 16777215;

  txBuffer[0] = ( latitudeBinary >> 16 ) & 0xFF;
  txBuffer[1] = ( latitudeBinary >> 8 ) & 0xFF;
  txBuffer[2] = latitudeBinary & 0xFF;

  txBuffer[3] = ( longitudeBinary >> 16 ) & 0xFF;
  txBuffer[4] = ( longitudeBinary >> 8 ) & 0xFF;
  txBuffer[5] = longitudeBinary & 0xFF;

  altitudeGps = gps.altitude.meters();
  txBuffer[6] = ( altitudeGps >> 8 ) & 0xFF;
  txBuffer[7] = altitudeGps & 0xFF;

  hdopGps = gps.hdop.value()/10.0;
  txBuffer[8] = hdopGps & 0xFF;
}

void displayGpsInfo()
{
  debugSerial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    debugSerial.print(gps.location.lat(), 6);
    debugSerial.print(F(","));
    debugSerial.print(gps.location.lng(), 6);
  }
  else
  {
    debugSerial.print(F("INVALID"));
  }

  debugSerial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    debugSerial.print(gps.date.day());
    debugSerial.print(F("/"));
    debugSerial.print(gps.date.month());
    debugSerial.print(F("/"));
    debugSerial.print(gps.date.year());
  }
  else
  {
    debugSerial.print(F("INVALID"));
  }

  debugSerial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) debugSerial.print(F("0"));
    debugSerial.print(gps.time.hour());
    debugSerial.print(F(":"));
    if (gps.time.minute() < 10) debugSerial.print(F("0"));
    debugSerial.print(gps.time.minute());
    debugSerial.print(F(":"));
    if (gps.time.second() < 10) debugSerial.print(F("0"));
    debugSerial.print(gps.time.second());
    debugSerial.print(F("."));
    if (gps.time.centisecond() < 10) debugSerial.print(F("0"));
    debugSerial.print(gps.time.centisecond());
  }
  else
  {
    debugSerial.print(F("INVALID"));
  }

  debugSerial.print(F("  ALT: "));
  debugSerial.print(gps.altitude.meters());

  debugSerial.print(F("  SAT: "));
  debugSerial.print(gps.satellites.value());

  debugSerial.print(F("  HDOP: "));
  debugSerial.print(gps.hdop.value()/100.0);

  debugSerial.println();
}
