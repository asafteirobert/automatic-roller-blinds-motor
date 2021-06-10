#ifndef _SAVEDDATA_h
#define _SAVEDDATA_h
#include "arduino.h"

const uint16_t SAVED_DATA_VERSION_CHECK = 57350; //change to rewrite data

class SavedData
{
public:
  void resetToDefault();
  void loadFromEEPROM();
  void saveToEEPROM();
  uint16_t dataVersion = 0;
  long totalLengthSteps = 8*2048;
  int8_t currentTargetPercent = 0;
};
#endif