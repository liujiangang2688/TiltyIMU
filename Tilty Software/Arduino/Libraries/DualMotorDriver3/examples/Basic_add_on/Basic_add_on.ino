/*
!!!!!!BEWARE!!!!!!
The FastPWM used by the motor driver changes the timer used by delay(), delayMicroseconds(), millis() and micros().
To correct for this change, multiply any delays you may want by 64 or divide any timers by 64.

For example:
To delay 10ms, use delay(640);
To read a timer, actual_timer_value = millis() / 64;

Or for a more permanent change, you can modify a line in the wiring.c function in the Arduino program files
hardware\arduino\cores\arduino\wiring.c
Change: #define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(64 * 256))
to
#define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(256))

However this is a global change and will affect all Arduino code, not just this program.
*/

#include "DualMotorDriver3.h"
#include "Wire.h"
#include "EEPROM.h"
#include "Encoder.h"

// Included to ensure no compatibility errors
#include "RCadapter.h"
#include "SatelliteRX.h"
#include "Servo.h"
//===========================================

MotorDriver motors;

void setup() {
	//motors.init();
	//Wire.begin(0x03);
	
	//pinMode(10, OUTPUT);
	
	Wire.onReceive(receiveEvent);
	Wire.onRequest(requestEvent);
}


void loop() {
	// Purely testing code, everything else is handled by interrupt routines
}


void receiveEvent(int bytes) {
	//digitalWrite(10, !digitalRead(10));
	motors.getData(bytes);
}

void requestEvent() {
	motors.sendData();
}

ISR(TIMER2_COMPA_vect)
{
	// Motor 1 updates will go here
	motors.updateMotor(&motors.motor1);
}

ISR(TIMER2_COMPB_vect)
{
	// Motor 2 updates will go here
}