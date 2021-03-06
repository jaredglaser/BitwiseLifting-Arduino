#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Mahony.h>
#include <Madgwick.h>
#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
#include "Adafruit_BluefruitLE_UART.h"
#if SOFTWARE_SERIAL_AVAILABLE
  #include <SoftwareSerial.h>
#endif

#include "BluefruitConfig.h"

// Note: This sketch is a WORK IN PROGRESS

/*
 * Required:
 * Library for Bluetooth: https://github.com/adafruit/Adafruit_BluefruitLE_nRF51/archive/master.zip
 * ^Place in Libraries folder for Arduino IDE
 * Page for Bluetooth: https://www.adafruit.com/product/2633
 * 
 * Libraries for IMU
 * Adafruit_FXOS8700
 * Adafruit_FXAS21002C
 * Adafruit Unified Sensor (Adafruit_Sensor)
 * Adafruit_AHRS
 * Page for IMU:https://www.adafruit.com/product/3463
 * 
 * Pin info for Bluetooth:
#define BLUEFRUIT_SPI_CS               10
#define BLUEFRUIT_SPI_IRQ              2
#define BLUEFRUIT_SPI_RST              -1    // Optional but recommended, set to -1 if unused
#define BLUEFRUIT_SPI_SCK              13
#define BLUEFRUIT_SPI_MISO             12
#define BLUEFRUIT_SPI_MOSI             11
 
 */



/*
 * -----------------------
 * IMU
 * -----------------------
 */
#define ST_LSM303DLHC_L3GD20        (0)
#define ST_LSM9DS1                  (1)
#define NXP_FXOS8700_FXAS21002      (2)

// Define your target sensor(s) here based on the list above!
// #define AHRS_VARIANT    ST_LSM303DLHC_L3GD20
#define AHRS_VARIANT   NXP_FXOS8700_FXAS21002

#if AHRS_VARIANT == ST_LSM303DLHC_L3GD20
#include <Adafruit_L3GD20_U.h>
#include <Adafruit_LSM303_U.h>
#elif AHRS_VARIANT == ST_LSM9DS1
// ToDo!
#elif AHRS_VARIANT == NXP_FXOS8700_FXAS21002
#include <Adafruit_FXAS21002C.h>
#include <Adafruit_FXOS8700.h>
#else
#error "AHRS_VARIANT undefined! Please select a target sensor combination!"
#endif


// Create sensor instances.
#if AHRS_VARIANT == ST_LSM303DLHC_L3GD20
Adafruit_L3GD20_Unified       gyro(20);
Adafruit_LSM303_Accel_Unified accel(30301);
Adafruit_LSM303_Mag_Unified   mag(30302);
#elif AHRS_VARIANT == ST_LSM9DS1
// ToDo!
#elif AHRS_VARIANT == NXP_FXOS8700_FXAS21002
Adafruit_FXAS21002C gyro = Adafruit_FXAS21002C(0x0021002C);
Adafruit_FXOS8700 accelmag = Adafruit_FXOS8700(0x8700A, 0x8700B);
#endif

// Mag calibration values are calculated via ahrs_calibration.
// These values must be determined for each baord/environment.
// See the image in this sketch folder for the values used
// below.

// Offsets applied to raw x/y/z mag values
float mag_offsets[3]            = { 5.61F, -38.84F, 26.91F };

// Soft iron error compensation matrix
float mag_softiron_matrix[3][3] = { {  0.984,  -0.030,  -0.024 },
                                    {  -0.030,  0.979, -0.005 },
                                    {  -0.024, -0.005,  1.039 } };

float mag_field_strength        = 46.13F;

// Offsets applied to compensate for gyro zero-drift error for x/y/z
float gyro_zero_offsets[3]      = { 0.0F, 0.0F, 0.0F };

// Mahony is lighter weight as a filter and should be used
// on slower systems
Mahony filter;
//Madgwick filter;

/*
 * -----------------------
 * BLUETOOTH
 * -----------------------
 */
 
 #define FACTORYRESET_ENABLE      1
// Create the bluefruit object, either software serial...uncomment these lines

/* ...or hardware serial, which does not need the RTS/CTS pins. Uncomment this line */
// Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/* ...software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST */
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
//                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
//                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

// String to send in the throughput test
#define TEST_STRING     "01234567899876543210"

// Number of total data sent ( 1024 times the test string)
#define TOTAL_BYTES     (1024 * strlen(TEST_STRING))

// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}

void setup()
{
  Serial.begin(115200);

  // Wait for the Serial Monitor to open (comment out to run without Serial Monitor)
  // while(!Serial);

  Serial.println(F("Adafruit AHRS Fusion Example")); Serial.println("");

  // Initialize the sensors.
  if(!gyro.begin())
  {
    /* There was a problem detecting the gyro ... check your connections */
    Serial.println("Ooops, no gyro detected ... Check your wiring!");
    while(1);
  }

#if AHRS_VARIANT == NXP_FXOS8700_FXAS21002
  if(!accelmag.begin(ACCEL_RANGE_4G))
  {
    Serial.println("Ooops, no FXOS8700 detected ... Check your wiring!");
    while(1);
  }
#else
  if (!accel.begin())
  {
    /* There was a problem detecting the accel ... check your connections */
    Serial.println("Ooops, no accel detected ... Check your wiring!");
    while (1);
  }

  if (!mag.begin())
  {
    /* There was a problem detecting the mag ... check your connections */
    Serial.println("Ooops, no mag detected ... Check your wiring!");
    while (1);
  }
#endif

  // Filter expects 70 samples per second
  // Based on a Bluefruit M0 Feather ... rate should be adjuted for other MCUs
  filter.begin(10);

  //-----------------------
  //Connect to Bluetooth
  //-----------------------

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ){
      error(F("Couldn't factory reset"));
    }
  }

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  /* Switch to DATA mode to have a better throughput */
  Serial.println("Switch to DATA mode to have a better throughput ...");

  /* Wait for a connection before starting the test */
  Serial.println("Waiting for a BLE connection to continue ...");
  ble.setMode(BLUEFRUIT_MODE_DATA);

  ble.verbose(false);  // debug info is a little annoying after this point!

  // Wait for connection to finish
  while (! ble.isConnected()) {
      delay(5000);
  }

  // Wait for the connection to complete
  delay(1000);

  Serial.println(F("CONNECTED!"));
  Serial.println(F("**********"));
}

void loop(void)
{
  sensors_event_t gyro_event;
  sensors_event_t accel_event;
  sensors_event_t mag_event;

  // Get new data samples
  gyro.getEvent(&gyro_event);
#if AHRS_VARIANT == NXP_FXOS8700_FXAS21002
  accelmag.getEvent(&accel_event, &mag_event);
#else
  accel.getEvent(&accel_event);
  mag.getEvent(&mag_event);
#endif

  // Apply mag offset compensation (base values in uTesla)
  float x = mag_event.magnetic.x - mag_offsets[0];
  float y = mag_event.magnetic.y - mag_offsets[1];
  float z = mag_event.magnetic.z - mag_offsets[2];

  // Apply mag soft iron error compensation
  float mx = x * mag_softiron_matrix[0][0] + y * mag_softiron_matrix[0][1] + z * mag_softiron_matrix[0][2];
  float my = x * mag_softiron_matrix[1][0] + y * mag_softiron_matrix[1][1] + z * mag_softiron_matrix[1][2];
  float mz = x * mag_softiron_matrix[2][0] + y * mag_softiron_matrix[2][1] + z * mag_softiron_matrix[2][2];

  // Apply gyro zero-rate error compensation
  float gx = gyro_event.gyro.x + gyro_zero_offsets[0];
  float gy = gyro_event.gyro.y + gyro_zero_offsets[1];
  float gz = gyro_event.gyro.z + gyro_zero_offsets[2];

  // The filter library expects gyro data in degrees/s, but adafruit sensor
  // uses rad/s so we need to convert them first (or adapt the filter lib
  // where they are being converted)
  gx *= 57.2958F;
  gy *= 57.2958F;
  gz *= 57.2958F;

  // Update the filter
  filter.update(gx, gy, gz,
                accel_event.acceleration.x, accel_event.acceleration.y, accel_event.acceleration.z,
                mx, my, mz);

  // Print the orientation filter output
  // Note: To avoid gimbal lock you should read quaternions not Euler
  // angles, but Euler angles are used here since they are easier to
  // understand looking at the raw values. See the ble fusion sketch for
  // and example of working with quaternion data.
  float roll = filter.getRoll();
  float pitch = filter.getPitch();
  float heading = filter.getYaw();
  String completeString; 
  String mill = String(millis());
 
  String subaccel;
  subaccel = String(accel_event.acceleration.y ,4);
  String rollround;
  rollround = String(roll);
  String toSend;
  String comma = ",";
  String newline = "\n";
  toSend = (millis() + comma + rollround.substring(0,4) + comma + subaccel.substring(0,6) + newline);

  int numZeros = 19-toSend.length();
  for(int i = 0; i<numZeros; i++){
    toSend = "0" + toSend;
  }

  
  //Serial.println(toSend);
  char charBuf[toSend.length()];
  toSend.toCharArray(charBuf, toSend.length());
 /* String bluetoothbuilder = "abcdefghijklmnopqrstuvwxyz";
//  we now need to break it up into 20 byte segments
  char charBuf[mill.length()];
  mill.toCharArray(charBuf, mill.length());
   */
      if(ble.isConnected()){ //make sure the user is still connected\

        
        ble.writeBLEUart(charBuf);
        //Serial.print(charBuf);
        //Serial.println();
        
      }
      else{
        Serial.println("ERROR");
      }
    
 
  
  
  delay(200);
}
