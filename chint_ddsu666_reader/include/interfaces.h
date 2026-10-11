#pragma once

#include "chint_telemetry.h"

/**
 * @brief Interface for Modbus meter reader engines (DIP / ISP).
 */
class IChintMeterReader {
public:
  virtual ~IChintMeterReader() = default;

  virtual void begin(uint8_t slaveId = 1) = 0;
  virtual void loop() = 0;

  virtual ChintTelemetry getTelemetry() = 0;
  virtual ModbusMasterStats getStats() = 0;

  virtual void setSlaveId(uint8_t slaveId) = 0;
  virtual uint8_t getSlaveId() const = 0;


  typedef void (*OnDataCallback)(const ChintTelemetry &data);
  virtual void onData(OnDataCallback cb) = 0;
};

#include <vector>
#include <Arduino.h>

/**
 * @brief Interface for telemetry stream broadcasting & publishing (DIP / ISP).
 */
class ITelemetryPublisher {
public:
  virtual ~ITelemetryPublisher() = default;

  virtual void begin() = 0;
  virtual void loop() = 0;

  virtual void broadcastTelemetry(const ChintTelemetry &data, const ModbusMasterStats &stats) = 0;

  virtual bool hasClients() const = 0;
  virtual uint8_t getClientCount() const = 0;
  virtual uint32_t getPacketsBroadcast() const = 0;
  virtual std::vector<String> getClientIPs() const { return {}; }
};
