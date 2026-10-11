#pragma once

#include <ArduinoJson.h>
#include "telemetry_types.h"

class ITelemetrySink;

/**
 * @brief Common interface for all telemetry source providers (Chint WS, Shelly WS, OpenDTU MQTT, etc.)
 */
class ITelemetrySource {
public:
  virtual ~ITelemetrySource() = default;

  /**
   * @brief Unique machine identifier for this source (e.g., "CHINT_WS", "SHELLY_WS", "OPENDTU_MQTT").
   */
  virtual const char* getId() const = 0;

  /**
   * @brief Human-readable name for diagnostics.
   */
  virtual const char* getName() const = 0;

  /**
   * @brief Initialize the source and register the destination sink.
   */
  virtual void begin(ITelemetrySink *sink) = 0;

  /**
   * @brief Periodic polling or processing loop called by the telemetry task.
   */
  virtual void loop() = 0;

  /**
   * @brief Whether the transport layer (e.g. WebSocket, MQTT connection) is currently connected.
   */
  virtual bool isConnected() const = 0;

  /**
   * @brief Millis timestamp of the last successfully received message.
   */
  virtual uint32_t getLastMessageTime() const = 0;

  /**
   * @brief Age of data in seconds (-1 if never received).
   */
  virtual float getAgeSeconds() const {
    uint32_t last = getLastMessageTime();
    return (last != 0) ? (float)(millis() - last) / 1000.0f : -1.0f;
  }

  /**
   * @brief Check if source has provided data recently within the given timeout window.
   */
  virtual bool isRecent(uint32_t timeoutMs = 3000) const {
    uint32_t last = getLastMessageTime();
    return (last != 0) && (millis() - last <= timeoutMs);
  }

  /**
   * @brief Whether this source produces grid measurements.
   */
  virtual bool isGridSource() const { return false; }

  /**
   * @brief Whether this source produces PV solar generation measurements.
   */
  virtual bool isPvSource() const { return false; }

  /**
   * @brief Check if source has provided PV solar data recently within the given timeout window.
   */
  virtual bool isPvRecent(uint32_t timeoutMs = 5000) const {
    return isRecent(timeoutMs);
  }

  /**
   * @brief Millis timestamp of the last successfully received PV message.
   */
  virtual uint32_t getLastPvMessageTime() const {
    return getLastMessageTime();
  }

  /**
   * @brief Key under which this source is serialized in the /health JSON document (e.g., "chint", "shelly").
   */
  virtual const char* getHealthKey() const = 0;

  /**
   * @brief Polymorphic diagnostics serialization for /health API.
   */
  virtual void populateHealth(JsonObject &obj) const = 0;
};

/**
 * @brief Ingestion sink interface. Receives measurements from any source technology.
 */
class ITelemetrySink {
public:
  virtual ~ITelemetrySink() = default;

  /**
   * @brief Ingest a normalized grid measurement from any source.
   * @param sourceId Identifier of the reporting source.
   * @param meas Normalized grid measurements.
   */
  virtual void reportGridMeasurement(const char *sourceId, const GridMeasurement &meas) = 0;

  /**
   * @brief Ingest a normalized PV solar generation measurement.
   * @param sourceId Identifier of the reporting source.
   * @param meas Normalized PV measurements.
   */
  virtual void reportPvMeasurement(const char *sourceId, const PvMeasurement &meas) = 0;
};
