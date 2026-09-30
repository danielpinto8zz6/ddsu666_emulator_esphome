#include "ddsu666_master.h"
#include "chint_reader_config.h"
#include <cmath>
#include <cstring>

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

DDSU666Master::DDSU666Master()
  : _slaveId(DEFAULT_SLAVE_ID),
    _dataCb(nullptr),
    _spinlock(portMUX_INITIALIZER_UNLOCKED),
    _lastEnergyPoll(0),
    _lastRateCalcTime(0),
    _queriesAtLastCalc(0) {
  memset(&_telemetry, 0, sizeof(_telemetry));
  _telemetry.voltage = 230.0f;
  _telemetry.power_factor = 1.0f;
  _telemetry.frequency = 50.0f;

  memset(&_stats, 0, sizeof(_stats));
  _stats.slave_id = _slaveId;
}

uint16_t DDSU666Master::calculateCRC(const uint8_t *buffer, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    uint8_t index = (uint8_t)(crc ^ buffer[i]);
    crc = (crc >> 8) ^ pgm_read_word(&CRC16_TABLE[index]);
  }
  return crc;
}

float DDSU666Master::parseIEEE754(const uint8_t *p) {
  // Chint DDSU666 stores standard 32-bit Big-Endian IEEE-754 float
  uint32_t raw = ((uint32_t)p[0] << 24) |
                 ((uint32_t)p[1] << 16) |
                 ((uint32_t)p[2] << 8)  |
                 ((uint32_t)p[3]);
  float val;
  memcpy(&val, &raw, sizeof(float));
  return val;
}

void DDSU666Master::begin(uint8_t slaveId) {
  _slaveId = slaveId;
  _stats.slave_id = slaveId;

  pinMode(PIN_RS485_DE_RE, OUTPUT);
  digitalWrite(PIN_RS485_DE_RE, LOW); // Receive mode

#ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH); // OFF (active LOW)
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
  uart_driver_install(UART_NUM_1, 512, 512, 0, NULL, 0);

  // Set hardware RX inter-character timeout to 4 symbol times (~4.16ms @ 9600 baud, Modbus t3.5)
  uart_set_rx_timeout(UART_NUM_1, 4);

  _lastRateCalcTime = millis();
}

void DDSU666Master::setSlaveId(uint8_t slaveId) {
  portENTER_CRITICAL(&_spinlock);
  _slaveId = slaveId;
  _stats.slave_id = slaveId;
  portEXIT_CRITICAL(&_spinlock);
}

ChintTelemetry DDSU666Master::getTelemetry() {
  ChintTelemetry copy;
  portENTER_CRITICAL(&_spinlock);
  copy = _telemetry;
  portEXIT_CRITICAL(&_spinlock);
  return copy;
}

ModbusMasterStats DDSU666Master::getStats() {
  ModbusMasterStats copy;
  portENTER_CRITICAL(&_spinlock);
  copy = _stats;
  portEXIT_CRITICAL(&_spinlock);
  return copy;
}

bool DDSU666Master::queryRegisters(uint16_t startAddress, uint16_t count, uint8_t *responseBuffer, size_t &responseLen, float &latencyMs) {
  // Clear any residual garbage in the RX FIFO
  uart_flush_input(UART_NUM_1);

  uint8_t request[8];
  request[0] = _slaveId;
  request[1] = 0x03; // Function Code 0x03: Read Holding Registers
  request[2] = (uint8_t)((startAddress >> 8) & 0xFF);
  request[3] = (uint8_t)(startAddress & 0xFF);
  request[4] = (uint8_t)((count >> 8) & 0xFF);
  request[5] = (uint8_t)(count & 0xFF);

  uint16_t crc = calculateCRC(request, 6);
  request[6] = (uint8_t)(crc & 0xFF);
  request[7] = (uint8_t)((crc >> 8) & 0xFF);

  // Switch transceiver to TRANSMIT mode
  digitalWrite(PIN_RS485_DE_RE, HIGH);
  delayMicroseconds(50); // Driver enable setup time

  uint32_t startMicros = micros();
  uart_write_bytes(UART_NUM_1, (const char *)request, 8);
  uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(20)); // Ensure last stop bit has left the hardware shift register

  // Switch transceiver back to RECEIVE mode
  delayMicroseconds(50);
  digitalWrite(PIN_RS485_DE_RE, LOW);

  // Expected response size: 1 (ID) + 1 (FC) + 1 (ByteCount) + count*2 (Data) + 2 (CRC) = 5 + count*2
  size_t expectedLen = 5 + (count * 2);
  size_t bytesRead = 0;
  uint32_t startWait = millis();

  while ((millis() - startWait) < MODBUS_TIMEOUT_MS) {
    size_t available = 0;
    uart_get_buffered_data_len(UART_NUM_1, &available);
    if (available > 0) {
      int len = uart_read_bytes(UART_NUM_1, responseBuffer + bytesRead, available, pdMS_TO_TICKS(5));
      if (len > 0) {
        bytesRead += len;
        if (bytesRead >= expectedLen) {
          break;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  uint32_t endMicros = micros();
  latencyMs = (float)(endMicros - startMicros) / 1000.0f;
  responseLen = bytesRead;

  if (bytesRead < expectedLen) {
    return false; // Timeout
  }

  // Validate Slave ID & Function Code
  if (responseBuffer[0] != _slaveId || responseBuffer[1] != 0x03) {
    return false;
  }

  // Validate CRC16
  uint16_t expectedCrc = calculateCRC(responseBuffer, expectedLen - 2);
  uint16_t receivedCrc = (uint16_t)responseBuffer[expectedLen - 2] | ((uint16_t)responseBuffer[expectedLen - 1] << 8);

  return (expectedCrc == receivedCrc);
}

void DDSU666Master::loop() {
  uint8_t buffer[64];
  size_t respLen = 0;
  float latency = 0.0f;

  portENTER_CRITICAL(&_spinlock);
  _stats.total_queries++;
  portEXIT_CRITICAL(&_spinlock);

  // 1. Ultra-Fast Real-Time Telemetry Query:
  // Reads 16 registers (32 bytes) starting at 0x2000
  // 0x2000: Voltage (V)
  // 0x2002: Current (A)
  // 0x2004: Active Power (kW)
  // 0x2006: Reactive Power (kVAR)
  // 0x2008: Apparent Power (kVA)
  // 0x200A: Power Factor
  // 0x200C: Reserved/Phase Angle
  // 0x200E: Frequency (Hz)
  bool success = queryRegisters(0x2000, 16, buffer, respLen, latency);

  if (success && respLen >= 37) {
    const uint8_t *data = &buffer[3]; // Skip SlaveID, FC, ByteCount

    float v   = parseIEEE754(&data[0]);
    float c   = parseIEEE754(&data[4]);
    float p_kw = parseIEEE754(&data[8]);
    float q   = parseIEEE754(&data[12]);
    float s   = parseIEEE754(&data[16]);
    float pf  = parseIEEE754(&data[20]);
    float f   = parseIEEE754(&data[28]); // 0x200E is offset 14 registers = 28 bytes

    // Chint active power is in kW -> convert to Watts
    float watts = p_kw * 1000.0f;

    portENTER_CRITICAL(&_spinlock);
    if (!std::isnan(v) && v > 150.0f && v < 300.0f) _telemetry.voltage = v;
    if (!std::isnan(c) && c >= 0.0f && c < 200.0f) _telemetry.current = c;
    if (!std::isnan(watts) && watts > -30000.0f && watts < 30000.0f) _telemetry.active_power = watts;
    if (!std::isnan(q)) _telemetry.reactive_power = q * 1000.0f;
    if (!std::isnan(s)) _telemetry.apparent_power = s * 1000.0f;
    if (!std::isnan(pf) && pf >= -1.0f && pf <= 1.0f) _telemetry.power_factor = pf;
    if (!std::isnan(f) && f > 40.0f && f < 65.0f) _telemetry.frequency = f;

    _telemetry.last_update = millis();
    _stats.successful_queries++;
    _stats.response_latency_ms = latency;
    _stats.meter_online = true;
    portEXIT_CRITICAL(&_spinlock);

#ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, LOW); // Flash LED on valid response
#endif

    // Emit to callback
    if (_dataCb) {
      ChintTelemetry snap = getTelemetry();
      _dataCb(snap);
    }

#ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, HIGH);
#endif
  } else {
    portENTER_CRITICAL(&_spinlock);
    if (respLen == 0) {
      _stats.timeout_errors++;
    } else {
      _stats.crc_errors++;
    }
    // If consecutive timeouts exceed 2 seconds, mark meter offline
    if (millis() - _telemetry.last_update > 2000) {
      _stats.meter_online = false;
      // If we haven't seen a response in 5 seconds, try toggling slave ID between 1 and 2 to auto-detect
      static uint32_t lastToggle = 0;
      if (millis() - lastToggle > 5000) {
        lastToggle = millis();
        uint8_t nextId = (_slaveId == 2) ? 1 : 2;
        _slaveId = nextId;
        _stats.slave_id = nextId;
      }
    }
    portEXIT_CRITICAL(&_spinlock);
  }

  // 2. Periodic Energy Accumulator Query (0x4000, 12 registers)
  uint32_t now = millis();
  if (now - _lastEnergyPoll >= ENERGY_POLL_INTERVAL_MS) {
    _lastEnergyPoll = now;
    delay(MODBUS_INTER_FRAME_DELAY_MS);

    size_t energyLen = 0;
    float energyLatency = 0.0f;
    // 0x4000: Total Import Energy (kWh), 0x400A: Total Export Energy (kWh)
    if (queryRegisters(0x4000, 12, buffer, energyLen, energyLatency) && energyLen >= 29) {
      const uint8_t *data = &buffer[3];
      float imp_kwh = parseIEEE754(&data[0]);  // 0x4000
      float exp_kwh = parseIEEE754(&data[20]); // 0x400A (offset 10 registers = 20 bytes)

      portENTER_CRITICAL(&_spinlock);
      if (!std::isnan(imp_kwh) && imp_kwh >= 0.0f) _telemetry.import_kwh = imp_kwh;
      if (!std::isnan(exp_kwh) && exp_kwh >= 0.0f) _telemetry.export_kwh = exp_kwh;
      portEXIT_CRITICAL(&_spinlock);
    }
  }

  // Calculate rolling polling rate in Hz (every 1 second)
  if (now - _lastRateCalcTime >= 1000) {
    uint32_t elapsed = now - _lastRateCalcTime;
    uint32_t curQueries = _stats.successful_queries;
    portENTER_CRITICAL(&_spinlock);
    _stats.query_rate_hz = (float)(curQueries - _queriesAtLastCalc) * 1000.0f / (float)elapsed;
    portEXIT_CRITICAL(&_spinlock);
    _queriesAtLastCalc = curQueries;
    _lastRateCalcTime = now;
  }

  // Inter-frame turnaround silence
  delay(MODBUS_INTER_FRAME_DELAY_MS);
}
