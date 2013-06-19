#define YAW 0
#define PITCH 2
#define ROLL 1

long accel_offset = 16380;

/* ============================================
SPIdev device library code is placed under the MIT license
Copyright (c) 2012 Jeff Rowberg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
===============================================
*/

#include <SPI.h>
#include <Wire.h>

// SPIdev and MPU60X0 must be installed as libraries, or else the .cpp/.h files
// for both classes must be in the include path of your project
#include "I2Cdev.h"

#include "MPU60X0_6Axis_MotionApps20.h"

MPU60X0 mpu(false, 0x68);

/* =========================================================================
   NOTE: In addition to connection 3.3v, GND, SDA, and SCL, this sketch
   depends on the MPU-6050's INT pin being connected to the Arduino's
   external interrupt #0 pin. On the Arduino Uno and Mega 2560, this is
   digital I/O pin 2.
 * ========================================================================= */

/* =========================================================================
   NOTE: Arduino v1.0.1 with the Leonardo board generates a compile error
   when using Serial.write(buf, len). The Teapot output uses this method.
   The solution requires a modification to the Arduino USBAPI.h file, which
   is fortunately simple, but annoying. This will be fixed in the next IDE
   release. For more info, see these links:

   http://arduino.cc/forum/index.php/topic,109987.0.html
   http://code.google.com/p/arduino/issues/detail?id=958
 * ========================================================================= */



// uncomment "OUTPUT_READABLE_QUATERNION" if you want to see the actual
// quaternion components in a [w, x, y, z] format (not best for parsing
// on a remote host such as Processing or something though)
//#define OUTPUT_READABLE_QUATERNION

// uncomment "OUTPUT_READABLE_EULER" if you want to see Euler angles
// (in degrees) calculated from the quaternions coming from the FIFO.
// Note that Euler angles suffer from gimbal lock (for more info, see
// http://en.wikipedia.org/wiki/Gimbal_lock)
//#define OUTPUT_READABLE_EULER

// uncomment "OUTPUT_READABLE_YAWPITCHROLL" if you want to see the yaw/
// pitch/roll angles (in degrees) calculated from the quaternions coming
// from the FIFO. Note this also requires gravity vector calculations.
// Also note that yaw/pitch/roll angles suffer from gimbal lock (for
// more info, see: http://en.wikipedia.org/wiki/Gimbal_lock)
#define OUTPUT_READABLE_YAWPITCHROLL

// uncomment "OUTPUT_READABLE_REALACCEL" if you want to see acceleration
// components with gravity removed. This acceleration reference frame is
// not compensated for orientation, so +X is always +X according to the
// sensor, just without the effects of gravity. If you want acceleration
// compensated for orientation, us OUTPUT_READABLE_WORLDACCEL instead.
//#define OUTPUT_READABLE_REALACCEL

// uncomment "OUTPUT_READABLE_WORLDACCEL" if you want to see acceleration
// components with gravity removed and adjusted for the world frame of
// reference (yaw is relative to initial orientation, since no magnetometer
// is present in this case). Could be quite handy in some cases.
//#define OUTPUT_READABLE_WORLDACCEL

// uncomment "OUTPUT_TEAPOT" if you want output that matches the
// format used for the InvenSense teapot demo
//#define OUTPUT_TEAPOT


// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorInt16 aa;         // [x, y, z]            accel sensor measurements
VectorInt16 aaReal;     // [x, y, z]            gravity-free accel sensor measurements
VectorInt16 aaWorld;    // [x, y, z]            world-frame accel sensor measurements
VectorFloat gravity;    // [x, y, z]            gravity vector
float euler[3];         // [psi, theta, phi]    Euler angle container
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

// packet structure for InvenSense teapot demo
uint8_t teapotPacket[14] = { '$', 0x02, 0,0, 0,0, 0,0, 0,0, 0x00, 0x00, '\r', '\n' };

// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setupIMU() {

    // initialize serial communication
    // (115200 chosen because it is required for Teapot Demo output, but it's
    // really up to you depending on your project)
    Serial.begin(115200);
    Wire.begin();
    
    #if defined(__arm__)
      if(true) {
        #if defined(CORE_TEENSY) && F_BUS == 48000000
          I2C0_F = 0x1A;  // Teensy 3.0 at 48 or 96 MHz
          I2C0_FLT = 2;
        #elif defined(CORE_TEENSY) && F_BUS == 24000000
          I2C0_F = 0x45;  // Teensy 3.0 at 24 MHz
          I2C0_FLT = 1;
        #endif
    }
    #endif

    // initialize device
    Serial.println(F("Initializing MPU device..."));
    
    mpu.initialize();
    mpu.setRate(4); 						// Sample Rate (200Hz = 1kHz Gyro SR / 4+1)
    mpu.setDLPFMode(MPU60X0_DLPF_BW_20);			// Low Pass filter 20hz
    mpu.setFullScaleGyroRange(MPU60X0_GYRO_FS_250);		// 250? / s
    mpu.setFullScaleAccelRange(MPU60X0_ACCEL_FS_2);		// +-2g
	
    // verify connection
    Serial.println(F("Testing device connections..."));
    Serial.println(mpu.testConnection() ? F("MPU60X0 connection successful") : F("MPU60X0 connection failed"));

    // load and configure the DMP
    Serial.println(F("Initializing DMP..."));
    devStatus = mpu.dmpInitialize();
    
    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);

        // enable Arduino interrupt detection
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;
        mpu.setRate(4); 
        Serial.print("Sample Rate: ");
        Serial.println(mpu.getRate());

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
    Serial.print("Accel Range: ");
    Serial.println(mpu.getFullScaleAccelRange());
    
    for (int i = 0; i < 500; i++) {
      accel_offset += mpu.getAccelerationZ();
    }
    accel_offset = accel_offset / 500;
}



// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

unsigned long start = 0;
boolean mpuInterrupt;

void getAngles() {
    // if programming failed, don't try to do anything
    if (!dmpReady) return;

    // reset interrupt flag and get INT_STATUS byte
    mpuInterrupt = false;
    //start = micros();
    mpuIntStatus = mpu.getIntStatus();
    //Serial.print("Get Interrupt Time: ");
    //Serial.println(micros() - start);

    // get current FIFO count
    //fifoCount = mpu.getFIFOCount();

    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & MPU60X0_INTERRUPT_FIFO_OFLOW) || fifoCount == 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        Serial.println(F("FIFO overflow!"));

    // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus & MPU60X0_INTERRUPT_DMP_INT) {
        // wait for correct available data length, should be a VERY short wait
        if (fifoCount < packetSize) {  fifoCount = mpu.getFIFOCount();}
        else {
          // read a packet from FIFO
          //start = micros();
          mpu.getFIFOBytes(fifoBuffer, packetSize);
          //Serial.print("FIFO Read Time: ");
          //Serial.println(micros() - start);
          
          // track FIFO count here in case there is > 1 packet available
          // (this lets us immediately read more without waiting for an interrupt)
          fifoCount -= packetSize;
  
          mpu.dmpGetQuaternion(&q, fifoBuffer);
          mpu.dmpGetGravity(&gravity, &q);
          mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
      
          int accel_old = accel;
          accel = mpu.getAccelerationZ() - accel_offset;
      
          d_yaw = ypr[YAW] - yaw;
          d_pitch = ypr[PITCH] - pitch;
          d_roll = ypr[ROLL] - roll;
          d_accel = accel - accel_old;
      
          yaw = ypr[YAW] * 180 / M_PI;
          pitch = ypr[PITCH] * 180 / M_PI;
          roll = ypr[ROLL] * 180 / M_PI;
        }
    }
}
