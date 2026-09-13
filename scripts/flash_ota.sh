#!/usr/bin/env bash
# ==============================================================================
# One-Click Over-The-Air (OTA) Firmware Flasher for DDSU666 Emulator Ecosystem
# Supports:
#   - Lilygo T-CAN485 (ESP32)
#   - Waveshare ESP32-S3 RS485/CAN
#   - ESP32-C3 SuperMini / Lolin C3 Mini (Transmitter)
# ==============================================================================

set -euo pipefail

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

usage() {
    echo -e "${CYAN}Usage:${NC} $0 <target> <device_ip> [--build]"
    echo ""
    echo -e "${CYAN}Targets:${NC}"
    echo "  lilygo      LILYGO T-CAN485 (ESP32)"
    echo "  waveshare   Waveshare ESP32-S3 RS485"
    echo "  supermini   ESP32-C3 SuperMini Transmitter"
    echo ""
    echo -e "${CYAN}Options:${NC}"
    echo "  --build     Force re-compile with PlatformIO before uploading"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo "  $0 lilygo 10.0.0.110"
    echo "  $0 supermini 10.0.0.125 --build"
    echo "  $0 waveshare 10.0.0.110"
    exit 1
}

if [[ $# -lt 2 ]]; then
    usage
fi

TARGET="$1"
DEVICE_IP="$2"
DO_BUILD=false

if [[ "${3:-}" == "--build" ]]; then
    DO_BUILD=true
fi

# Resolve environment name and binary path
case "$TARGET" in
    lilygo|tcan485)
        ENV_NAME="lilygo-t-can485"
        BIN_PATH=".pio/build/lilygo-t-can485/firmware.bin"
        ;;
    waveshare|s3)
        ENV_NAME="waveshare-esp32-s3"
        BIN_PATH=".pio/build/waveshare-esp32-s3/firmware.bin"
        ;;
    supermini|c3|lolin)
        ENV_NAME="esp32-c3-supermini"
        BIN_PATH=".pio/build/esp32-c3-supermini/firmware.bin"
        ;;
    *)
        echo -e "${RED}Error: Unknown target '$TARGET'${NC}"
        usage
        ;;
esac

# Locate PlatformIO executable
PIO_BIN="pio"
if [[ -f ".venv/bin/pio" ]]; then
    PIO_BIN=".venv/bin/pio"
elif [[ -f "venv/bin/pio" ]]; then
    PIO_BIN="venv/bin/pio"
fi

# Build if requested or if binary is missing
if [[ "$DO_BUILD" == true ]] || [[ ! -f "$BIN_PATH" ]]; then
    echo -e "${CYAN}==> Building firmware for environment '${ENV_NAME}' using ${PIO_BIN}...${NC}"
    "$PIO_BIN" run -e "$ENV_NAME"
fi

if [[ ! -f "$BIN_PATH" ]]; then
    echo -e "${RED}Error: Firmware binary not found at ${BIN_PATH}!${NC}"
    exit 1
fi

# Resolve OTA Password (from secrets.h, environment variable, or fallback "admin")
OTA_PASS="admin"
if [[ -f "include/secrets.h" ]]; then
    EXTRACTED_PASS=$(grep -E '^\s*#define\s+OTA_PASSWORD\s+' include/secrets.h | head -n1 | sed -E 's/.*"([^"]+)".*/\1/' || true)
    if [[ -n "$EXTRACTED_PASS" ]]; then
        OTA_PASS="$EXTRACTED_PASS"
    fi
fi
OTA_PASS="${OTA_PASSWORD:-$OTA_PASS}"

BIN_SIZE=$(wc -c < "$BIN_PATH")
echo -e "${CYAN}==> Target:${NC}    ${ENV_NAME}"
echo -e "${CYAN}==> Device IP:${NC} ${DEVICE_IP}"
echo -e "${CYAN}==> Binary:${NC}    ${BIN_PATH} (${BIN_SIZE} bytes)"
echo -e "${CYAN}==> Uploading OTA firmware to http://${DEVICE_IP}/update (auth: admin)...${NC}"

# Perform OTA upload with curl (Expect: header is mandatory to prevent 100-continue stalls)
HTTP_CODE=$(curl -s -o /tmp/ota_response.txt -w "%{http_code}" \
    -u "admin:${OTA_PASS}" \
    -F "file=@${BIN_PATH}" \
    -H "Expect:" \
    -H "X-OTA-Password: ${OTA_PASS}" \
    --connect-timeout 8 \
    --max-time 60 \
    "http://${DEVICE_IP}/update?password=${OTA_PASS}")

RESPONSE=$(cat /tmp/ota_response.txt 2>/dev/null || echo "")

if [[ "$HTTP_CODE" == "200" ]] && [[ "$RESPONSE" == *"OK"* ]]; then
    echo -e "${GREEN}✔ OTA Flash Successful!${NC} Device is rebooting into the new firmware."
    echo -e "${YELLOW}ℹ Waiting 5 seconds for reboot...${NC}"
    sleep 5
    if curl -s --connect-timeout 3 "http://${DEVICE_IP}/health" >/dev/null 2>&1; then
        echo -e "${GREEN}✔ Device is back online!${NC} (http://${DEVICE_IP}/health)"
    else
        echo -e "${YELLOW}ℹ Device is rebooting. Check http://${DEVICE_IP}/ in a few seconds.${NC}"
    fi
else
    echo -e "${RED}✘ OTA Flash Failed! (HTTP Status: ${HTTP_CODE})${NC}"
    echo -e "Server response: ${RESPONSE}"
    exit 1
fi
