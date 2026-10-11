#pragma once

#include <Arduino.h>
#include <vector>

struct MeterLiveReadings {
  bool valid = false;
  float voltage = 0.0f;       // 0x0000 (V)
  float current = 0.0f;       // 0x0006 (A)
  float active_power = 0.0f;  // 0x000C (W)
  float frequency = 0.0f;     // 0x0046 (Hz)
  float total_energy = 0.0f;  // 0x0156 (kWh)
};

struct DetectedMeter {
  uint8_t slave_id;
  uint32_t baud_rate;
  float voltage;
};

class EastronModbus {
public:
  EastronModbus(uint8_t txPin, uint8_t rxPin, uint8_t deRePin, int8_t en5vPin = -1, int8_t chipEnPin = -1);

  void begin(uint32_t baudRate = 2400);
  void setBaudRate(uint32_t baudRate);

  static uint16_t calculateCRC(const uint8_t *buffer, size_t length);

  // Modbus read & write primitives
  bool readInputFloat(uint8_t slaveId, uint16_t regAddr, float &val, uint32_t timeoutMs = 60);
  bool readHoldingFloat(uint8_t slaveId, uint16_t regAddr, float &val, uint32_t timeoutMs = 60);
  bool writeHoldingFloat(uint8_t slaveId, uint16_t regAddr, float val, uint32_t timeoutMs = 120);

  // High-level operations
  bool readAllTelemetry(uint8_t slaveId, MeterLiveReadings &readings);
  std::vector<DetectedMeter> scanBus(const std::vector<uint32_t> &bauds = {2400, 9600, 4800, 1200},
                                    const std::vector<uint8_t> &ids = {1, 2, 3, 4, 5, 6, 7, 8});

  bool programMeter(uint8_t currentId, uint32_t currentBaud, uint8_t newId, uint32_t newBaud, String &logMsg);

private:
  uint8_t _txPin;
  uint8_t _rxPin;
  uint8_t _deRePin;
  int8_t  _en5vPin;
  int8_t  _chipEnPin;
  uint32_t _currentBaud;

  void setTxMode();
  void setRxMode();
  bool sendRawPacket(const uint8_t *pkt, size_t len);
  int  receiveResponse(uint8_t *buf, size_t maxLen, uint32_t timeoutMs);
};
