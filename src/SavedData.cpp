#include <SavedData.h>
#include <EEPROM.h>

void SavedData::resetToDefault()
{
  this->totalLengthSteps = 8*2048;
  this->currentTargetPercent = 0;
  this->saveToEEPROM();
}

void SavedData::loadFromEEPROM()
{
  // Load settings from EEPROM to this
  EEPROM.get(0, *this);

  if (this->dataVersion != SAVED_DATA_VERSION_CHECK)
    this->resetToDefault();
}

void SavedData::saveToEEPROM()
{
  this->dataVersion = SAVED_DATA_VERSION_CHECK;
  EEPROM.put(0, *this);
}
