#include "hoymiles_dtu.h"
#include "meter_data.h"
#include <WiFi.h>
#include <time.h>
#include "mbedtls/sha256.h"
#include "mbedtls/gcm.h"

// Modbus CRC16 lookup table (identical to DTU CRC16)
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

static uint32_t readVarint(const uint8_t *buf, size_t maxLen, size_t &offset) {
  uint32_t val = 0;
  uint8_t shift = 0;
  while (offset < maxLen) {
    uint8_t b = buf[offset++];
    val |= (uint32_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  return val;
}

static uint64_t readVarint64(const uint8_t *buf, size_t maxLen, size_t &offset) {
  uint64_t val = 0;
  uint8_t shift = 0;
  while (offset < maxLen) {
    uint8_t b = buf[offset++];
    val |= (uint64_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  return val;
}

HoymilesDTUClient::HoymilesDTUClient()
  : _host("10.0.0.164"),
    _port(10081),
    _pollIntervalMs(10000),
    _connected(false),
    _hasValidData(false),
    _isEncrypted(false),
    _sequence(1),
    _hasEncRand(false),
    _activePowerW(0.0f),
    _totalYieldKwh(0.0f),
    _lastPollTime(0),
    _lastSuccessTime(0),
    _lastHandshakeTime(0),
    _lastDecryptRet(0),
    _lastRxLen(0),
    _lastRespLen(0) {
  memset(_encRand, 0, sizeof(_encRand));
}

void HoymilesDTUClient::begin(const char* host, uint16_t port, uint32_t pollIntervalMs) {
  _host = host;
  _port = port;
  _pollIntervalMs = pollIntervalMs;
  _lastPollTime = 0;
  _lastSuccessTime = 0;
  _lastHandshakeTime = 0;
  _hasEncRand = false;
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.printf("[Hoymiles DTU] Client configured for %s:%u (interval: %u ms)\n",
                _host, _port, _pollIntervalMs);
}

uint16_t HoymilesDTUClient::calculateCRC16(const uint8_t *buffer, size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < length; i++) {
    uint8_t idx = (uint8_t)(crc ^ buffer[i]);
    crc = (uint16_t)((crc >> 8) ^ CRC16_TABLE[idx]);
  }
  return crc;
}

void HoymilesDTUClient::sha256Digest(const uint8_t *data, size_t len, uint8_t *out32) {
  mbedtls_sha256(data, len, out32, 0);
}

void HoymilesDTUClient::deriveKey(const uint8_t *encRand, uint8_t *outKey16) {
  uint8_t h1[32], h2[32], h3[32];
  sha256Digest(encRand, 16, h1);
  sha256Digest(h1, 32, h2);
  sha256Digest(h2, 32, h3);
  memcpy(outKey16, h3, 16);
}

void HoymilesDTUClient::deriveNonce(const uint8_t *encRand, uint16_t tag, uint16_t seq, uint8_t *outNonce12) {
  uint8_t buf[20];
  buf[0] = (uint8_t)(tag & 0xFF);
  buf[1] = (uint8_t)((tag >> 8) & 0xFF);
  buf[2] = (uint8_t)(seq & 0xFF);
  buf[3] = (uint8_t)((seq >> 8) & 0xFF);
  memcpy(buf + 4, encRand, 16);

  uint8_t h1[32], h2[32], h3[32];
  sha256Digest(buf, 20, h1);
  sha256Digest(h1, 32, h2);
  sha256Digest(h2, 32, h3);
  memcpy(outNonce12, h3 + 20, 12);
}

bool HoymilesDTUClient::fetchAppInfo() {
  if (!_client.connect(_host, _port, 3000)) {
    Serial.printf("[Hoymiles DTU] TCP connect to %s:%u failed for AppInfo\n", _host, _port);
    _connected = false;
    return false;
  }
  _connected = true;

  // Format request: CMD_APP_INFO_DATA_RES_DTO (0xa301)
  _sequence = (_sequence + 1) & 0xFFFF;
  uint16_t tag = 0xa301;

  char timeStr[20];
  time_t now = time(nullptr);
  if (now < 100000) now = 1790198400; // fallback if NTP not synced
  struct tm tm_info;
  localtime_r(&now, &tm_info);
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);

  uint8_t protoReq[64];
  size_t pLen = 0;
  protoReq[pLen++] = 0x0a; // field 1: time_ymd_hms
  protoReq[pLen++] = 19;
  memcpy(protoReq + pLen, timeStr, 19);
  pLen += 19;
  protoReq[pLen++] = 0x10; // field 2: offset = 28800
  protoReq[pLen++] = 0x80;
  protoReq[pLen++] = 0xe1;
  protoReq[pLen++] = 0x01;
  protoReq[pLen++] = 0x28; // field 5: time
  uint32_t tVal = (uint32_t)now;
  while (tVal >= 0x80) {
    protoReq[pLen++] = (uint8_t)((tVal & 0x7F) | 0x80);
    tVal >>= 7;
  }
  protoReq[pLen++] = (uint8_t)tVal;

  uint16_t crc = calculateCRC16(protoReq, pLen);
  uint16_t metaLen = (uint16_t)(pLen + 10);

  uint8_t packet[128];
  packet[0] = 'H';
  packet[1] = 'M';
  packet[2] = (uint8_t)((tag >> 8) & 0xFF);
  packet[3] = (uint8_t)(tag & 0xFF);
  packet[4] = (uint8_t)((_sequence >> 8) & 0xFF);
  packet[5] = (uint8_t)(_sequence & 0xFF);
  packet[6] = (uint8_t)((crc >> 8) & 0xFF);
  packet[7] = (uint8_t)(crc & 0xFF);
  packet[8] = (uint8_t)((metaLen >> 8) & 0xFF);
  packet[9] = (uint8_t)(metaLen & 0xFF);
  memcpy(packet + 10, protoReq, pLen);

  _client.write(packet, 10 + pLen);
  _client.flush();

  // Read response
  uint32_t startRead = millis();
  uint8_t rxBuf[512];
  size_t rxLen = 0;
  while (millis() - startRead < 4000) {
    while (_client.available() && rxLen < sizeof(rxBuf)) {
      rxBuf[rxLen++] = _client.read();
    }
    if (rxLen >= 10) {
      uint16_t expLen = ((uint16_t)rxBuf[8] << 8) | rxBuf[9];
      if (rxLen >= (size_t)expLen) break;
    }
    delay(10);
  }
  _client.stop();

  if (rxLen < 20 || rxBuf[0] != 'H' || rxBuf[1] != 'M') {
    Serial.printf("[Hoymiles DTU] Invalid or short AppInfo response (len=%u)\n", rxLen);
    return false;
  }

  // Parse APPDtuInfoMO inside field 8 (dtu_info)
  size_t i = 10;
  while (i < rxLen) {
    uint32_t key = readVarint(rxBuf, rxLen, i);
    uint32_t field = key >> 3;
    uint32_t wireType = key & 7;

    if (wireType == 0) {
      uint64_t v = readVarint64(rxBuf, rxLen, i);
      if (field == 2 && v > 1000000000ULL) {
        struct timeval tv = { (time_t)v, 0 };
        settimeofday(&tv, nullptr);
        Serial.printf("[Hoymiles DTU] Synchronized clock to DTU timestamp: %llu\n", v);
      }
    } else if (wireType == 2) {
      uint32_t len = readVarint(rxBuf, rxLen, i);
      if (field == 8) { // dtu_info
        size_t subEnd = i + len;
        if (subEnd > rxLen) subEnd = rxLen;
        while (i < subEnd) {
          uint32_t subKey = readVarint(rxBuf, subEnd, i);
          uint32_t subField = subKey >> 3;
          uint32_t subWire = subKey & 7;
          if (subWire == 0) {
            uint64_t v = readVarint64(rxBuf, subEnd, i);
            if (subField == 24) { // dfs
              _isEncrypted = ((v >> 25) & 1) != 0;
            }
          } else if (subWire == 2) {
            uint32_t slen = readVarint(rxBuf, subEnd, i);
            if (subField == 27 && slen == 16) { // enc_rand
              memcpy(_encRand, rxBuf + i, 16);
              _hasEncRand = true;
            }
            i += slen;
          } else {
            break;
          }
        }
      } else {
        i += len;
      }
    } else {
      break;
    }
  }

  if (_hasEncRand) {
    _lastHandshakeTime = millis();
    Serial.printf("[Hoymiles DTU] Handshake OK (encrypted: %s)\n", _isEncrypted ? "YES" : "NO");
    return true;
  }

  Serial.println("[Hoymiles DTU] enc_rand not found in AppInfo response");
  return false;
}

bool HoymilesDTUClient::fetchRealData() {
  if (!_hasEncRand) {
    if (!fetchAppInfo()) return false;
    delay(2000); // Allow DTU lwIP stack to close previous socket cleanly
  }

  if (!_client.connect(_host, _port, 3000)) {
    Serial.printf("[Hoymiles DTU] TCP connect to %s:%u failed for RealData\n", _host, _port);
    _connected = false;
    return false;
  }
  _connected = true;

  _sequence = (_sequence + 1) & 0xFFFF;
  uint16_t tag = 0xa311; // CMD_REAL_RES_DTO

  char timeStr[20];
  time_t now = time(nullptr);
  if (now < 100000) now = 1790198400;
  struct tm tm_info;
  localtime_r(&now, &tm_info);
  strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_info);

  uint8_t protoReq[64];
  size_t pLen = 0;
  protoReq[pLen++] = 0x0a; // field 1: time_ymd_hms
  protoReq[pLen++] = 19;
  memcpy(protoReq + pLen, timeStr, 19);
  pLen += 19;
  protoReq[pLen++] = 0x20; // field 4: offset = 28800
  protoReq[pLen++] = 0x80;
  protoReq[pLen++] = 0xe1;
  protoReq[pLen++] = 0x01;
  protoReq[pLen++] = 0x28; // field 5: time
  uint32_t tVal = (uint32_t)now;
  while (tVal >= 0x80) {
    protoReq[pLen++] = (uint8_t)((tVal & 0x7F) | 0x80);
    tVal >>= 7;
  }
  protoReq[pLen++] = (uint8_t)tVal;

  uint8_t cipherData[128];
  size_t cipherLen = pLen;
  uint8_t aesTag[16];

  if (_isEncrypted) {
    uint8_t key[16], nonce[12];
    deriveKey(_encRand, key);
    deriveNonce(_encRand, tag, _sequence, nonce);

    uint8_t aad[4];
    aad[0] = (uint8_t)(tag & 0xFF);
    aad[1] = (uint8_t)((tag >> 8) & 0xFF);
    aad[2] = (uint8_t)(_sequence & 0xFF);
    aad[3] = (uint8_t)((_sequence >> 8) & 0xFF);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, pLen, nonce, 12, aad, 4, protoReq, cipherData, 16, aesTag);
    mbedtls_gcm_free(&gcm);

    // append tag to ciphertext
    memcpy(cipherData + pLen, aesTag, 16);
    cipherLen = pLen + 16;
  } else {
    memcpy(cipherData, protoReq, pLen);
  }

  uint16_t crc = calculateCRC16(cipherData, _isEncrypted ? (cipherLen - 16) : cipherLen);
  uint16_t metaLen = (uint16_t)((_isEncrypted ? (cipherLen - 16) : cipherLen) + 10);

  uint8_t packet[256];
  packet[0] = 'H';
  packet[1] = 'M';
  packet[2] = (uint8_t)((tag >> 8) & 0xFF);
  packet[3] = (uint8_t)(tag & 0xFF);
  packet[4] = (uint8_t)((_sequence >> 8) & 0xFF);
  packet[5] = (uint8_t)(_sequence & 0xFF);
  packet[6] = (uint8_t)((crc >> 8) & 0xFF);
  packet[7] = (uint8_t)(crc & 0xFF);
  packet[8] = (uint8_t)((metaLen >> 8) & 0xFF);
  packet[9] = (uint8_t)(metaLen & 0xFF);
  memcpy(packet + 10, cipherData, cipherLen);

  _client.write(packet, 10 + cipherLen);
  _client.flush();

  // Read response
  uint32_t startRead = millis();
  uint8_t rxBuf[1024];
  size_t rxLen = 0;
  while (millis() - startRead < 4000) {
    while (_client.available() && rxLen < sizeof(rxBuf)) {
      rxBuf[rxLen++] = _client.read();
    }
    if (rxLen >= 10) {
      uint16_t readLen = ((uint16_t)rxBuf[8] << 8) | rxBuf[9];
      size_t expectedLen = _isEncrypted ? ((size_t)readLen + 16) : (size_t)readLen;
      if (rxLen >= expectedLen) break;
    }
    delay(10);
  }
  _client.stop();

  _lastRxLen = rxLen;
  if (rxLen < 10 || rxBuf[0] != 'H' || rxBuf[1] != 'M') {
    Serial.printf("[Hoymiles DTU] Short or invalid RealData response (len=%u)\n", rxLen);
    return false;
  }

  uint16_t respTag = ((uint16_t)rxBuf[2] << 8) | rxBuf[3];
  uint16_t respSeq = ((uint16_t)rxBuf[4] << 8) | rxBuf[5];
  uint16_t respLen = ((uint16_t)rxBuf[8] << 8) | rxBuf[9];
  _lastRespLen = respLen;

  uint8_t plaintext[1024];
  size_t plainLen = 0;

  if (_isEncrypted) {
    size_t expectedLen = (size_t)respLen + 16;
    if (respLen < 10 || rxLen < expectedLen) {
      Serial.printf("[Hoymiles DTU] Response length error: respLen=%u, rxLen=%u, expected=%u\n", respLen, rxLen, expectedLen);
      return false;
    }
    size_t ctLen = respLen - 10;
    const uint8_t *ct = rxBuf + 10;
    const uint8_t *tagPtr = rxBuf + respLen;

    uint8_t key[16], nonce[12];
    deriveKey(_encRand, key);
    deriveNonce(_encRand, respTag, respSeq, nonce);

    uint8_t aad[4];
    aad[0] = (uint8_t)(respTag & 0xFF);
    aad[1] = (uint8_t)((respTag >> 8) & 0xFF);
    aad[2] = (uint8_t)(respSeq & 0xFF);
    aad[3] = (uint8_t)((respSeq >> 8) & 0xFF);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 128);
    int ret = mbedtls_gcm_auth_decrypt(&gcm, ctLen, nonce, 12, aad, 4, tagPtr, 16, ct, plaintext);
    mbedtls_gcm_free(&gcm);
    _lastDecryptRet = ret;

    if (ret != 0) {
      Serial.printf("[Hoymiles DTU] Decryption failed! (ret=%d). Forcing re-handshake.\n", ret);
      _hasEncRand = false;
      return false;
    }
    plainLen = ctLen;
  } else {
    plainLen = (respLen >= 10) ? (respLen - 10) : 0;
    memcpy(plaintext, rxBuf + 10, plainLen);
  }

  // Parse extracted plaintext
  float pW = 0.0f;
  float yKwh = 0.0f;
  if (parseProtobufRealData(plaintext, plainLen, pW, yKwh)) {
    _activePowerW = pW;
    if (yKwh > 0.0f) {
      _totalYieldKwh = yKwh;
    }
    _hasValidData = true;
    _lastSuccessTime = millis();
    MeterState::updatePvPower(_activePowerW);
    if (_totalYieldKwh > 0.0f) {
      MeterState::updatePvYield(_totalYieldKwh);
    }
    Serial.printf("[Hoymiles DTU] PV Data: %.1f W | Yield: %.3f kWh\n", _activePowerW, _totalYieldKwh);
    return true;
  }

  return false;
}

bool HoymilesDTUClient::parseProtobufRealData(const uint8_t *data, size_t length, float &outPowerW, float &outYieldKwh) {
  size_t i = 0;
  uint32_t totalPowerTenths = 0;
  uint64_t totalEnergyWh = 0;

  while (i < length) {
    uint32_t key = readVarint(data, length, i);
    uint32_t field = key >> 3;
    uint32_t wireType = key & 7;

    if (wireType == 0) {
      uint64_t v = readVarint64(data, length, i);
      if (field == 12) { // dtu_power
        totalPowerTenths = (uint32_t)v;
      }
    } else if (wireType == 2) {
      uint32_t len = readVarint(data, length, i);
      size_t subEnd = i + len;
      if (subEnd > length) subEnd = length;

      if (field == 9) { // sgs_data
        while (i < subEnd) {
          uint32_t sKey = readVarint(data, subEnd, i);
          uint32_t sField = sKey >> 3;
          uint32_t sWire = sKey & 7;
          if (sWire == 0) {
            uint64_t v = readVarint64(data, subEnd, i);
            if (sField == 5 && totalPowerTenths == 0) { // active_power
              totalPowerTenths += (uint32_t)v;
            }
          } else if (sWire == 2) {
            uint32_t sl = readVarint(data, subEnd, i);
            i += sl;
          } else {
            break;
          }
        }
      } else if (field == 11) { // pv_data
        while (i < subEnd) {
          uint32_t pKey = readVarint(data, subEnd, i);
          uint32_t pField = pKey >> 3;
          uint32_t pWire = pKey & 7;
          if (pWire == 0) {
            uint64_t v = readVarint64(data, subEnd, i);
            if (pField == 6) { // energy_total
              totalEnergyWh += v;
            }
          } else if (pWire == 2) {
            uint32_t sl = readVarint(data, subEnd, i);
            i += sl;
          } else {
            break;
          }
        }
      } else {
        i += len;
      }
    } else {
      break;
    }
  }

  outPowerW = totalPowerTenths * 0.1f;
  outYieldKwh = (float)totalEnergyWh / 1000.0f;
  return true;
}

void HoymilesDTUClient::loop() {
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (now - _lastPollTime < _pollIntervalMs) return;
  _lastPollTime = now;

  // Refresh handshake key every 60 minutes
  if (_hasEncRand && (now - _lastHandshakeTime > 3600000)) {
    _hasEncRand = false;
  }

  fetchRealData();
}
