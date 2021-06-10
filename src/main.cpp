#include <Arduino.h>
#include "Constants.h"
#include "SavedData.h"
#include <SPI.h>
#include <RF24.h>
#include <AccelStepper.h>

//#define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x)  Serial.println (x)
#include "printf.h"
#else
#define DEBUG_PRINT(x)
#endif

struct ReturnDataType
{
  uint8_t roomId;
  uint8_t blindId;
  int8_t currentTargetPercent;
};

//Special values written in targetPercent for various commands
static constexpr int8_t TARGET_DISCOVER = -1; //does nothing, just send the latest ack packet
static constexpr int8_t TARGET_SET_START = -2;
static constexpr int8_t TARGET_SET_END = -3;
static constexpr int8_t TARGET_MOVE_BACK = -4;
static constexpr int8_t TARGET_MOVE_FWD = -5;

struct SentDataType
{
  int8_t targetPercent;
  uint8_t speed;
};

SentDataType sentData;
ReturnDataType returnedData;

// RF24 object for NRF24 communication
RF24 radio(PIN_NRF_CE, PIN_NRF_CS);

AccelStepper motor(AccelStepper::DRIVER, PIN_MOTOR_STEP, PIN_MOTOR_DIR);

unsigned long lastRadioCheck = 0;
unsigned long lastCalibrationCommand = 0;

SavedData savedData;

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
#endif
  DEBUG_PRINT(F("Starting..."));

  //Load what we think was the last state
  //Note that state is only saved in some cases to avoid trashing the EEPROM
  savedData.loadFromEEPROM();

	pinMode(PIN_MOTOR_STEP, OUTPUT);
	pinMode(PIN_MOTOR_DIR, OUTPUT);
	pinMode(PIN_MOTOR_SLP, OUTPUT);
	pinMode(PIN_MOTOR_MS3, OUTPUT);
	pinMode(PIN_MOTOR_MS2, OUTPUT);
	pinMode(PIN_MOTOR_MS1, OUTPUT);
	pinMode(PIN_MOTOR_EN, OUTPUT);

  digitalWrite(PIN_MOTOR_MS1, HIGH);
  digitalWrite(PIN_MOTOR_MS2, LOW);
  digitalWrite(PIN_MOTOR_MS3, LOW);

  digitalWrite(PIN_MOTOR_EN, LOW);
  digitalWrite(PIN_MOTOR_SLP, HIGH);

  motor.setMaxSpeed(STEPPER_MAX_SPEED);
	motor.setAcceleration(STEPPER_ACCELERATION);
  motor.setPinsInverted(STEPPER_REVERSE);

  returnedData.roomId = ROOM_NUMBER;
  returnedData.blindId = BLIND_NUMBER;
  returnedData.currentTargetPercent = savedData.currentTargetPercent;

  motor.setCurrentPosition((savedData.totalLengthSteps*savedData.currentTargetPercent)/100);

  // Start radio communication
  radio.begin();
  radio.setChannel(NRF_CHANNEL);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.openReadingPipe(1, NRF_PIPE);
  radio.startListening();
  //Ack payloads are handled automatically by the radio chip when a payload is received.
  //Users should generally write an ack payload as soon as startListening() is called, so one is available when a regular payload is received.  
  radio.writeAckPayload(1, &returnedData, sizeof(returnedData));

#ifdef DEBUG
  printf_begin();
  radio.printDetails();
#endif
}

void loop()
{
  unsigned long currentMillis = millis();
  bool motorIsRunning = true;
	if (!motor.run())
  {
		motor.disableOutputs();
    digitalWrite(PIN_MOTOR_SLP, LOW);
    motorIsRunning = false;
  }

  //don't check the radio very often when the motor is running
  //check constantly when doing calibration
  if ((!motorIsRunning && (currentMillis - lastRadioCheck > 100)) ||
      (currentMillis - lastRadioCheck > 1000) ||
      (currentMillis - lastCalibrationCommand < 1000))
  {
    lastRadioCheck = currentMillis;
    // If transmission is available
    uint8_t pipeNum;
    if (radio.available(&pipeNum))
    {
      uint8_t len = radio.getDynamicPayloadSize();
      if (pipeNum == 1 && len == sizeof(sentData))
      {
        radio.read(&sentData, sizeof(sentData));
        if (sentData.targetPercent >= 0)
        {
          savedData.currentTargetPercent = sentData.targetPercent;
          digitalWrite(PIN_MOTOR_SLP, HIGH);
          motor.enableOutputs();
          long newPosition = (savedData.totalLengthSteps*sentData.targetPercent)/100;
          motor.setMaxSpeed(STEPPER_MAX_SPEED*sentData.speed/100*(motor.currentPosition() < newPosition?STEPPER_MOVE_DOWN_MULTIPLIER:1));
          motor.moveTo(newPosition);
          savedData.saveToEEPROM();
        }
        else if (sentData.targetPercent == TARGET_MOVE_BACK)
        {
          digitalWrite(PIN_MOTOR_SLP, HIGH);
          motor.enableOutputs();
          motor.setMaxSpeed(STEPPER_MAX_SPEED*sentData.speed/100);
          //TARGET_MOVE_* is sent every 100ms, so move enough for 100ms plus the distance it takes to decelerate
          long stepsToTake = (long)((motor.maxSpeed() * motor.maxSpeed()) / (2.0 * STEPPER_ACCELERATION)) + 1 + motor.maxSpeed()/10;
          motor.move(-stepsToTake);
          savedData.currentTargetPercent = (motor.targetPosition()*100)/savedData.totalLengthSteps;
          lastCalibrationCommand = currentMillis;
        }
        else if (sentData.targetPercent == TARGET_MOVE_FWD)
        {
          digitalWrite(PIN_MOTOR_SLP, HIGH);
          motor.enableOutputs();
          motor.setMaxSpeed(STEPPER_MAX_SPEED*sentData.speed/100);
          //TARGET_MOVE_* is sent every 100ms, so move enough for 100ms plus the distance it takes to decelerate
          long stepsToTake = (long)((motor.maxSpeed() * motor.maxSpeed()) / (2.0 * STEPPER_ACCELERATION)) + 1 + motor.maxSpeed()/10;
          motor.move(stepsToTake);
          savedData.currentTargetPercent = (motor.targetPosition()*100)/savedData.totalLengthSteps;
          lastCalibrationCommand = currentMillis;
        }
        else if (sentData.targetPercent == TARGET_SET_START)
        {
          digitalWrite(PIN_MOTOR_SLP, HIGH);
          motor.enableOutputs();
          motor.moveTo(motor.currentPosition()); //in case we are moving, decelerate, stop and move back
          motor.runToPosition(); //block until we are stopped
          savedData.totalLengthSteps -= motor.currentPosition(); //keep the end position
          motor.setCurrentPosition(0);
          savedData.currentTargetPercent = 0;
          savedData.saveToEEPROM();
          lastCalibrationCommand = currentMillis;
        }
        else if (sentData.targetPercent == TARGET_SET_END)
        {
          digitalWrite(PIN_MOTOR_SLP, HIGH);
          motor.enableOutputs();
          savedData.totalLengthSteps = motor.currentPosition();
          motor.moveTo(motor.currentPosition()); //in case we are moving, decelerate, stop and move back
          //no need to block, motor will continue it's movement in main loop
          savedData.currentTargetPercent = 100;
          savedData.saveToEEPROM();
          lastCalibrationCommand = currentMillis;
        }
        returnedData.currentTargetPercent = savedData.currentTargetPercent;
        DEBUG_PRINT("Received " + String(sentData.targetPercent));
      }
      else
      {
        radio.flush_rx();
        delay(2);
        DEBUG_PRINT("Discarded bad packet <<<");
      }
      //always set ack payload for next packet, even if we got bad packet now
      radio.flush_tx(); //make sure we always have one ack payload
      radio.writeAckPayload(1, &returnedData, sizeof(returnedData));
    }
  }
}