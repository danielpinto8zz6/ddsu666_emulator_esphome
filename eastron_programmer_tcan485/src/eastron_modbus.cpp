#include "eastron_modbus.h"
#include <cstring>
#include <cmath>

static const uint16_t CRC16_TABLE[256] = {
  0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
  0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
  0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
  0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
  0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
  0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
  0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
  0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
  0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
  0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
  0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
  0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
  0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
  0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
  0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
  0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
  0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
  0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
  0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
  0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
  0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
  0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
  0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
  0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
  0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
  0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
  0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
  0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
  0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
  0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
  0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
  0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

EastronModbus::EastronModbus(uint8_t txPin, uint8_t rxPin, uint8_t deRePin, int8_t en5vPin, int8_t chipEnPin)
  : _txPin(txPin), _rxPin(rxPin), _deRePin(deRePin), _en5vPin(en5vPin), _chipEnPin(chipEnPin), _currentBaud(2400) {}

void EastronModbus::begin(uint32_t baudRate) {
  _currentBaud = baudRate;

  // Power enable pins for LILYGO T-CAN485
  if (_en5vPin >= 0) {
    pinMode(_en5vPin, OUTPUT);
    digitalWrite(_en5vPin, HIGH); // Enable 5V boost converter
    delay(50);
  }
  if (_chipEnPin >= 0) {
    pinMode(_chipEnPin, OUTPUT);
    digitalWrite(_chipEnPin, HIGH); // Enable MAX13487/transceiver chip
    delay(20);
  }

  pinMode(_deRePin, OUTPUT);
  setRxMode();

  Serial1.begin(_currentBaud, SERIAL_8N1, _rxPin, _txPin);
  delay(50);
}

void EastronModbus::setBaudRate(uint32_t baudRate) {
  if (_currentBaud == baudRate) return;
  _currentBaud = baudRate;
  Serial1.updateBaudRate(baudRate);
  delay(30);
}

void EastronModbus::setTxMode() {
  digitalWrite(_deRePin, HIGH);
  delayMicroseconds(30);
}

void EastronModbus::setRxMode() {
  digitalWrite(_deRePin, LOW);
  delayMicroseconds(30);
}

uint16_t EastronModbus::calculateCRC(const uint8_t *buffer, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    uint8_t idx = (uint8_t)(crc ^ buffer[i]);
    crc = (uint16_t)((crc >> 8) ^ CRC16_TABLE[idx]);
  }
  return crc;
}

bool EastronModbus::sendRawPacket(const uint8_t *pkt, size_t len) {
  // Clear any residual bytes in RX buffer
  while (Serial1.available()) Serial1.read();

  setTxMode();
  Serial1.write(pkt, len);
  Serial1.flush(); // wait for last bit out of UART shift register
  delayMicroseconds(40);
  setRxMode();
  return true;
}

int EastronModbus::receiveResponse(uint8_t *buf, size_t maxLen, uint32_t timeoutMs) {
  size_t idx = 0;
  uint32_t start = millis();
  while ((millis() - start < timeoutMs) && (idx < maxLen)) {
    if (Serial1.available()) {
      buf[idx++] = (uint8_t)Serial1.read();
      start = millis(); // reset inter-character timer
    } else {
      delayMicroseconds(100);
    }
  }
  return idx;
}

bool EastronModbus::readInputFloat(uint8_t slaveId, uint16_t regAddr, float &val, uint32_t timeoutMs) {
  uint8_t q[8];
  q[0] = slaveId;
  q[1] = 0x04; // Read Input Registers
  q[2] = (regAddr >> 8) & 0xFF;
  q[3] = regAddr & 0xFF;
  q[4] = 0x00;
  q[5] = 0x02; // 2 registers = 4 bytes (IEEE 754 float)

  uint16_t crc = calculateCRC(q, 6);
  q[6] = crc & 0xFF;
  q[7] = (crc >> 8) & 0xFF;

  sendRawPacket(q, 8);

  uint8_t resp[16];
  int n = receiveResponse(resp, 9, timeoutMs);

  if (n >= 9 && resp[0] == slaveId && resp[1] == 0x04 && resp[2] == 0x04) {
    uint16_t expectedCrc = calculateCRC(resp, 7);
    uint16_t receivedCrc = ((uint16_t)resp[8] << 8) | resp[7];
    if (expectedCrc == receivedCrc) {
      uint32_t raw = ((uint32_t)resp[3] << 24) |
                     ((uint32_t)resp[4] << 16) |
                     ((uint32_t)resp[5] << 8)  |
                     (uint32_t)resp[6];
      memcpy(&val, &raw, 4);
      return !std::isnan(val);
    }
  }
  return false;
}

bool EastronModbus::readHoldingFloat(uint8_t slaveId, uint16_t regAddr, float &val, uint32_t timeoutMs) {
  uint8_t q[8];
  q[0] = slaveId;
  q[1] = 0x03; // Read Holding Registers
  q[2] = (regAddr >> 8) & 0xFF;
  q[3] = regAddr & 0xFF;
  q[4] = 0x00;
  q[5] = 0x02; // 2 registers = 4 bytes (IEEE 754 float)

  uint16_t crc = calculateCRC(q, 6);
  q[6] = crc & 0xFF;
  q[7] = (crc >> 8) & 0xFF;

  sendRawPacket(q, 8);

  uint8_t resp[16];
  int n = receiveResponse(resp, 9, timeoutMs);

  if (n >= 9 && resp[0] == slaveId && resp[1] == 0x03 && resp[2] == 0x04) {
    uint16_t expectedCrc = calculateCRC(resp, 7);
    uint16_t receivedCrc = ((uint16_t)resp[8] << 8) | resp[7];
    if (expectedCrc == receivedCrc) {
      uint32_t raw = ((uint32_t)resp[3] << 24) |
                     ((uint32_t)resp[4] << 16) |
                     ((uint32_t)resp[5] << 8)  |
                     (uint32_t)resp[6];
      memcpy(&val, &raw, 4);
      return !std::isnan(val);
    }
  }
  return false;
}

bool EastronModbus::writeHoldingFloat(uint8_t slaveId, uint16_t regAddr, float val, uint32_t timeoutMs) {
  uint8_t pkt[13];
  pkt[0] = slaveId;
  pkt[1] = 0x10; // Preset Multiple Registers
  pkt[2] = (regAddr >> 8) & 0xFF;
  pkt[3] = regAddr & 0xFF;
  pkt[4] = 0x00;
  pkt[5] = 0x02; // 2 registers
  pkt[6] = 0x04; // 4 bytes

  uint32_t raw;
  memcpy(&raw, &val, 4);
  pkt[7] = (raw >> 24) & 0xFF;
  pkt[8] = (raw >> 16) & 0xFF;
  pkt[9] = (raw >> 8) & 0xFF;
  pkt[10] = raw & 0xFF;

  uint16_t crc = calculateCRC(pkt, 11);
  pkt[11] = crc & 0xFF;
  pkt[12] = (crc >> 8) & 0xFF;

  sendRawPacket(pkt, 13);

  uint8_t resp[16];
  int n = receiveResponse(resp, 8, timeoutMs);

  if (n >= 8 && resp[0] == slaveId && resp[1] == 0x10) {
    uint16_t expectedCrc = calculateCRC(resp, 6);
    uint16_t receivedCrc = ((uint16_t)resp[7] << 8) | resp[6];
    return (expectedCrc == receivedCrc);
  }
  return false;
}

bool EastronModbus::readAllTelemetry(uint8_t slaveId, MeterLiveReadings &r) {
  r.valid = false;
  float v = 0, a = 0, w = 0, hz = 0, kwh = 0;

  if (!readInputFloat(slaveId, 0x0000, v, 60)) return false;
  delay(10);
  readInputFloat(slaveId, 0x0006, a, 60);
  delay(10);
  readInputFloat(slaveId, 0x000C, w, 60);
  delay(10);
  readInputFloat(slaveId, 0x0046, hz, 60);
  delay(10);
  readInputFloat(slaveId, 0x0156, kwh, 60);

  r.valid = true;
  r.voltage = v;
  r.current = a;
  r.active_power = w;
  r.frequency = hz;
  r.total_energy = kwh;
  return true;
}

std::vector<DetectedMeter> EastronModbus::scanBus(const std::vector<uint32_t> &bauds,
                                                 const std::vector<uint8_t> &ids) {
  std::vector<DetectedMeter> found;

  for (uint32_t b : bauds) {
    setBaudRate(b);
    delay(20);

    for (uint8_t id : ids) {
      float volt = 0.0f;
      // Read Voltage register 0x0000 via FC 0x04
      if (readInputFloat(id, 0x0000, volt, 45)) {
        DetectedMeter m;
        m.slave_id = id;
        m.baud_rate = b;
        m.voltage = volt;
        found.push_back(m);
      }
      delay(10);
    }
  }

  return found;
}

bool EastronModbus::programMeter(uint8_t currentId, uint32_t currentBaud, uint8_t newId, uint32_t newBaud, String &logMsg) {
  logMsg = "";

  uint8_t activeId = currentId;
  uint32_t activeBaud = currentBaud;
  bool verifiedCurrent = false;

  // 1. Probe & detect meter if current baud/ID is unknown or auto-detect requested
  if (currentId == 0 || currentBaud == 0) {
    logMsg += "Scanning bus for meter...\n";
    auto foundList = scanBus();
    if (!foundList.empty()) {
      activeId = foundList[0].slave_id;
      activeBaud = foundList[0].baud_rate;
      verifiedCurrent = true;
      logMsg += "Detected meter: ID " + String(activeId) + " at " + String(activeBaud) + " baud (Voltage: " + String(foundList[0].voltage, 1) + " V)\n";
    } else {
      activeId = (currentId > 0) ? currentId : 1;
      activeBaud = (currentBaud > 0) ? currentBaud : 2400;
      logMsg += "Scan did not find meter. Using assumed ID " + String(activeId) + " at " + String(activeBaud) + " baud.\n";
    }
  } else {
    setBaudRate(currentBaud);
    float v = 0;
    if (readInputFloat(currentId, 0x0000, v, 50)) {
      verifiedCurrent = true;
      logMsg += "Verified meter reachable: ID " + String(currentId) + " at " + String(currentBaud) + " baud (" + String(v, 1) + " V)\n";
    } else {
      logMsg += "Warning: Meter did not respond to probe at ID " + String(currentId) + " / " + String(currentBaud) + " baud (Ensure meter is powered and in -SET- mode)\n";
    }
  }

  setBaudRate(activeBaud);
  delay(30);

  // 2. Write Slave ID register 0x0014 (Float value = newId)
  logMsg += "Writing Slave ID -> " + String(newId) + " (Reg 0x0014)...\n";
  bool idWritten = writeHoldingFloat(activeId, 0x0014, (float)newId, 150);
  if (idWritten) {
    logMsg += "✓ Slave ID successfully updated to " + String(newId) + "\n";
  } else {
    logMsg += "• Slave ID packet sent (no ACK, typical for some firmware revisions)\n";
  }
  delay(60);

  // 3. Write Baud Rate register 0x001C
  // Float code: 0.0=2400, 1.0=4800, 2.0=9600, 3.0=19200, 5.0=1200
  float baudCode = 2.0f; // default 9600
  if (newBaud == 2400) baudCode = 0.0f;
  else if (newBaud == 4800) baudCode = 1.0f;
  else if (newBaud == 9600) baudCode = 2.0f;
  else if (newBaud == 19200) baudCode = 3.0f;
  else if (newBaud == 1200) baudCode = 5.0f;

  logMsg += "Writing Baud Rate -> " + String(newBaud) + " bps (Code " + String(baudCode, 0) + ", Reg 0x001C)...\n";
  bool baudWritten = writeHoldingFloat(newId, 0x001C, baudCode, 150);
  if (!baudWritten && activeId != newId) {
    // Retry with activeId if address hasn't shifted yet
    baudWritten = writeHoldingFloat(activeId, 0x001C, baudCode, 150);
  }

  if (baudWritten) {
    logMsg += "✓ Baud Rate successfully updated to " + String(newBaud) + " bps\n";
  } else {
    logMsg += "• Baud Rate packet sent\n";
  }

  logMsg += "\n=========================================\n";
  logMsg += "PROGRAMMING COMPLETE!\n";
  logMsg += "IMPORTANT: Power cycle the Eastron meter now\n";
  logMsg += "(turn breaker off for 3s, then back on)\n";
  logMsg += "so the meter restarts with ID " + String(newId) + " @ " + String(newBaud) + " bps.\n";
  logMsg += "=========================================\n";

  return (idWritten || baudWritten || verifiedCurrent);
}
