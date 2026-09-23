#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

class HoymilesDTUClient {
public:
  HoymilesDTUClient();

  void begin(const char* host, uint16_t port = 10081, uint32_t pollIntervalMs = 10000);
  void loop();

  bool isConnected() const { return _connected; }
  bool hasValidData() const { return _hasValidData; }
  float getActivePowerW() const { return _activePowerW; }
  float getTotalYieldKwh() const { return _totalYieldKwh; }
  uint32_t getLastSuccessTime() const { return _lastSuccessTime; }
  bool hasEncRand() const { return _hasEncRand; }
  int getLastDecryptRet() const { return _lastDecryptRet; }
  size_t getLastRxLen() const { return _lastRxLen; }
  uint16_t getLastRespLen() const { return _lastRespLen; }

private:
  const char* _host;
  uint16_t _port;
  uint32_t _pollIntervalMs;

  WiFiClient _client;
  bool _connected;
  bool _hasValidData;
  bool _isEncrypted;
  uint16_t _sequence;
  uint8_t _encRand[16];
  bool _hasEncRand;

  float _activePowerW;
  float _totalYieldKwh;
  uint32_t _lastPollTime;
  uint32_t _lastSuccessTime;
  uint32_t _lastHandshakeTime;

  int _lastDecryptRet;
  size_t _lastRxLen;
  uint16_t _lastRespLen;

  bool fetchAppInfo();
  bool fetchRealData();

  static uint16_t calculateCRC16(const uint8_t *buffer, size_t length);
  static void sha256Digest(const uint8_t *data, size_t len, uint8_t *out32);
  static void deriveKey(const uint8_t *encRand, uint8_t *outKey16);
  static void deriveNonce(const uint8_t *encRand, uint16_t tag, uint16_t seq, uint8_t *outNonce12);

  bool parseProtobufRealData(const uint8_t *data, size_t length, float &outPowerW, float &outYieldKwh);
};
