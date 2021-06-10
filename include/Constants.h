#ifndef _CONSTANTS_h
#define _CONSTANTS_h
#include "arduino.h"

// ======= Configuration =======
const static uint8_t ROOM_NUMBER = 2;
const static uint8_t BLIND_NUMBER = 2; //window number

const static float STEPPER_MAX_SPEED = 1000; //steps per second
const static float STEPPER_ACCELERATION = 1000; //steps per second per second
const static bool STEPPER_REVERSE = true;

const static float STEPPER_MOVE_DOWN_MULTIPLIER = 1.8; //when moving down, move at higher speed

// ======= NRF =======
const static uint8_t NRF_PIPE[5] = {BLIND_NUMBER, ROOM_NUMBER, 0x0e, 0xf1, 0xbf};
const static uint8_t NRF_CHANNEL = 103;

// ======= Pin defination =======
const uint8_t PIN_NRF_CE = 9;
const uint8_t PIN_NRF_CS = 10;
const uint8_t PIN_MOTOR_DIR = 2;
const uint8_t PIN_MOTOR_STEP = 3;
const uint8_t PIN_MOTOR_SLP = 4;
const uint8_t PIN_MOTOR_MS3 = 5;
const uint8_t PIN_MOTOR_MS2 = 6;
const uint8_t PIN_MOTOR_MS1 = 7;
const uint8_t PIN_MOTOR_EN = 8;

#endif
