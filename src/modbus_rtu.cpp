#include "modbus_rtu.h"

// DDSU666 System Parameters (0x0001 to 0x000C)
static const uint16_t DDSU666_SYS_REGS[12] = {
  504, 0, 166, 5, 5, 3, 10, 1, 5, 0, 166, 3
};

// Fast Precomputed Modbus CRC16 Lookup Table (Polynomial 0xA001)
static const uint16_t CRC16_TABLE[256] PROGMEM = {
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

static uint16_t getFloatRegisterWord(float val, bool highWord) {
  uint32_t raw;
  memcpy(&raw, &val, sizeof(float));
  return highWord ? (uint16_t)((raw >> 16) & 0xFFFF) : (uint16_t)(raw & 0xFFFF);
}

ModbusRTUServer::ModbusRTUServer()
  : _uartQueue(nullptr),
    _enabled(true),
    _rxLen(0),
    _queryCount(0),
    _crcErrorCount(0),
    _lastQueryTime(0),
    _lastRateCalcTime(0),
    _queriesAtLastCalc(0),
    _queryRateHz(0.0f) {}

void ModbusRTUServer::begin() {
  pinMode(PIN_RS485_DE_RE, OUTPUT);
  digitalWrite(PIN_RS485_DE_RE, LOW); // Receive mode

#if HAS_RS485_POWER_PINS
  pinMode(PIN_RS485_5V_EN, OUTPUT);
  pinMode(PIN_RS485_CHIP_EN, OUTPUT);
  digitalWrite(PIN_RS485_5V_EN, HIGH);
  digitalWrite(PIN_RS485_CHIP_EN, HIGH);
#endif

  uart_config_t uart_config = {};
  uart_config.baud_rate = RS485_BAUD_RATE;
  uart_config.data_bits = UART_DATA_8_BITS;
  uart_config.parity    = UART_PARITY_DISABLE;
  uart_config.stop_bits = UART_STOP_BITS_1;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
  uart_config.rx_flow_ctrl_thresh = 122;

  uart_param_config(UART_NUM_1, &uart_config);
  uart_set_pin(UART_NUM_1, PIN_RS485_TX, PIN_RS485_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  uart_driver_install(UART_NUM_1, 512, 512, 20, &_uartQueue, 0);
  // Set hardware RX timeout to 4 symbol times (~4.16ms @ 9600 baud, matching Modbus t3.5)
  uart_set_rx_timeout(UART_NUM_1, 4);

  _rxLen = 0;
}

void ModbusRTUServer::setEnabled(bool enabled) {
  _enabled = enabled;
#if HAS_RS485_POWER_PINS
  if (enabled) {
    digitalWrite(PIN_RS485_5V_EN, HIGH);
    digitalWrite(PIN_RS485_CHIP_EN, HIGH);
  } else {
    digitalWrite(PIN_RS485_5V_EN, LOW);
    digitalWrite(PIN_RS485_CHIP_EN, LOW);
  }
#endif
}

uint16_t ModbusRTUServer::calculateCRC(const uint8_t *buffer, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    uint8_t idx = (uint8_t)(crc ^ buffer[i]);
    crc = (uint16_t)((crc >> 8) ^ CRC16_TABLE[idx]);
  }
  return crc;
}

void ModbusRTUServer::handle() {
  if (!_enabled || _uartQueue == nullptr) {
    vTaskDelay(pdMS_TO_TICKS(100));
    return;
  }

  uart_event_t event;
  // Block until a hardware UART event arrives (FreeRTOS 0% CPU sleep)
  if (xQueueReceive(_uartQueue, &event, pdMS_TO_TICKS(1000))) {
    switch (event.type) {
      case UART_DATA: {
        size_t buffered_size = 0;
        uart_get_buffered_data_len(UART_NUM_1, &buffered_size);
        if (buffered_size > 0) {
          if (_rxLen + buffered_size <= sizeof(_rxBuffer)) {
            int bytesRead = uart_read_bytes(UART_NUM_1, _rxBuffer + _rxLen, buffered_size, 0);
            if (bytesRead > 0) {
              _rxLen += bytesRead;
            }
          } else {
            // Buffer overflow, reset
            _rxLen = 0;
            uart_flush_input(UART_NUM_1);
          }
        }

        // Modbus RTU frame complete: valid query is 8 bytes
        if (_rxLen >= 8) {
          uint16_t rxCRC = ((uint16_t)_rxBuffer[_rxLen - 1] << 8) | _rxBuffer[_rxLen - 2];
          uint16_t calcCRC = calculateCRC(_rxBuffer, _rxLen - 2);

          if (rxCRC == calcCRC) {
            processFrame(_rxBuffer, _rxLen);
          } else {
            _crcErrorCount++;
          }
          _rxLen = 0;
        } else if (event.timeout_flag) {
          // Incomplete fragment received on timeout
          _rxLen = 0;
        }
        break;
      }

      case UART_FIFO_OVF:
      case UART_BUFFER_FULL:
        uart_flush_input(UART_NUM_1);
        xQueueReset(_uartQueue);
        _rxLen = 0;
        break;

      default:
        break;
    }
  }
}

bool ModbusRTUServer::getRegisterValue(uint8_t slaveId, uint16_t regAddress, const MeterTelemetry &telemetry, uint16_t &value) const {
  // 1. System Registers (0x0001 - 0x000C)
  if (regAddress >= 0x0001 && regAddress <= 0x000C) {
    value = DDSU666_SYS_REGS[regAddress - 1];
    return true;
  }

  // 2. Real-Time Telemetry Registers (0x2000 - 0x200F, 32-bit floats ABCD)
  if (regAddress >= 0x2000 && regAddress <= 0x200F) {
    bool isHighWord = ((regAddress % 2) == 0);
    uint16_t baseReg = regAddress & ~1;

    switch (baseReg) {
      case 0x2000: value = getFloatRegisterWord(telemetry.voltage, isHighWord); return true;
      case 0x2002: value = getFloatRegisterWord(telemetry.current, isHighWord); return true;
      case 0x2004: value = getFloatRegisterWord(telemetry.active_power, isHighWord); return true;
      case 0x2006: value = getFloatRegisterWord(telemetry.reactive_power, isHighWord); return true;
      case 0x2008: value = getFloatRegisterWord(telemetry.apparent_power, isHighWord); return true;
      case 0x200A: value = getFloatRegisterWord(telemetry.power_factor, isHighWord); return true;
      case 0x200C: value = getFloatRegisterWord(0.0f, isHighWord); return true;
      case 0x200E: value = getFloatRegisterWord(telemetry.frequency, isHighWord); return true;
      default: return false;
    }
  }

  // 3. Energy Accumulator Registers (0x4000 - 0x400B, 32-bit floats ABCD)
  if (regAddress >= 0x4000 && regAddress <= 0x400B) {
    bool isHighWord = ((regAddress % 2) == 0);
    uint16_t baseReg = regAddress & ~1;

    switch (baseReg) {
      case 0x4000: value = getFloatRegisterWord(telemetry.import_kwh, isHighWord); return true;
      case 0x4002: value = getFloatRegisterWord(0.0f, isHighWord); return true;
      case 0x4004: value = getFloatRegisterWord(0.0f, isHighWord); return true;
      case 0x4006: value = getFloatRegisterWord(0.0f, isHighWord); return true;
      case 0x4008: value = getFloatRegisterWord(0.0f, isHighWord); return true;
      case 0x400A: value = getFloatRegisterWord(telemetry.export_kwh, isHighWord); return true;
      default: return false;
    }
  }

  return false;
}

void ModbusRTUServer::processFrame(const uint8_t *frame, size_t length) {
  if (!_enabled) {
    return;
  }

  uint8_t slaveId = frame[0];
  if (slaveId != MODBUS_SLAVE_GRID && slaveId != MODBUS_SLAVE_PV) {
    return;
  }

  uint8_t functionCode = frame[1];
  if (functionCode != 0x03 && functionCode != 0x04) {
    sendException(slaveId, functionCode, 0x01);
    return;
  }

  uint16_t startAddress = ((uint16_t)frame[2] << 8) | frame[3];
  uint16_t registerCount = ((uint16_t)frame[4] << 8) | frame[5];

  if (registerCount < 1 || registerCount > 64) {
    sendException(slaveId, functionCode, 0x03);
    return;
  }

  // Snapshot meter telemetry once for the entire frame to minimize spinlock contention
  const MeterTelemetry telemetry = (slaveId == MODBUS_SLAVE_GRID)
                                 ? MeterState::getGrid()
                                 : MeterState::getPv();

  uint8_t byteCount = (uint8_t)(registerCount * 2);
  uint8_t response[256];
  response[0] = slaveId;
  response[1] = functionCode;
  response[2] = byteCount;

  size_t respIdx = 3;
  for (uint16_t i = 0; i < registerCount; i++) {
    uint16_t regVal = 0;
    if (!getRegisterValue(slaveId, startAddress + i, telemetry, regVal)) {
      sendException(slaveId, functionCode, 0x02);
      return;
    }
    response[respIdx++] = (uint8_t)((regVal >> 8) & 0xFF);
    response[respIdx++] = (uint8_t)(regVal & 0xFF);
  }

  uint16_t crc = calculateCRC(response, respIdx);
  response[respIdx++] = (uint8_t)(crc & 0xFF);
  response[respIdx++] = (uint8_t)((crc >> 8) & 0xFF);

  _queryCount++;
  _lastQueryTime = millis();
  sendResponse(response, respIdx);
}

float ModbusRTUServer::getQueryRateHz() {
  uint32_t now = millis();
  if (_lastQueryTime == 0 || (now - _lastQueryTime > 2500)) {
    _queryRateHz = 0.0f;
    return 0.0f;
  }
  if (now - _lastRateCalcTime >= 1000) {
    uint32_t elapsedMs = now - _lastRateCalcTime;
    if (elapsedMs > 0) {
      uint32_t curQueries = _queryCount;
      _queryRateHz = (float)(curQueries - _queriesAtLastCalc) * 1000.0f / (float)elapsedMs;
      _queriesAtLastCalc = curQueries;
      _lastRateCalcTime = now;
    }
  }
  return _queryRateHz;
}

void ModbusRTUServer::sendResponse(const uint8_t *response, size_t length) {
  digitalWrite(PIN_RS485_DE_RE, HIGH);
  delayMicroseconds(20);
  uart_write_bytes(UART_NUM_1, (const char *)response, length);
  uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(100));
  delayMicroseconds(30);
  digitalWrite(PIN_RS485_DE_RE, LOW);
}

void ModbusRTUServer::sendException(uint8_t slaveId, uint8_t functionCode, uint8_t exceptionCode) {
  uint8_t response[5];
  response[0] = slaveId;
  response[1] = functionCode | 0x80;
  response[2] = exceptionCode;

  uint16_t crc = calculateCRC(response, 3);
  response[3] = (uint8_t)(crc & 0xFF);
  response[4] = (uint8_t)((crc >> 8) & 0xFF);

  sendResponse(response, sizeof(response));
}
