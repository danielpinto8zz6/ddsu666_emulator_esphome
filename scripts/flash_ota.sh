#!/usr/bin/env bash
# ==============================================================================
# One-Click Over-The-Air (OTA) Firmware Flasher for DDSU666 Emulator Ecosystem
# Supports:
#   - Seeed Studio XIAO ESP32-S3 (Modbus Emulator - Target Board)
#   - LILYGO T-CAN485 ESP32 (Chint DDSU666 Master Transmitter - Target Board)
#   - Seeed Studio XIAO ESP32-S3 (Chint DDSU666 Master alternative)
#   - LILYGO T-CAN485 ESP32 (Modbus Emulator alternative)
#   - Waveshare ESP32-S3 RS485/CAN (Modbus Emulator alternative)
#   - LILYGO T-CAN485 (Dedicated Eastron SDM120 Programmer utility)
# ==============================================================================

set -euo pipefail

# Ensure script runs from the repository root regardless of where it is invoked
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

# Colors
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

usage() {
    echo -e "${CYAN}Usage:${NC} $0 <target> [device_ip_or_host] [--build]"
    echo ""
    echo -e "${CYAN}Primary Ecosystem Devices:${NC}"
    echo "  emulator          Seeed Studio XIAO ESP32-S3 (Modbus Emulator, default: ddsu666.local)"
    echo "  transmitter       LILYGO T-CAN485 ESP32 (Chint DDSU666 Master, default: chint-reader.local)"
    echo ""
    echo -e "${CYAN}All Available Targets:${NC}"
    echo "  emulator | xiao           XIAO ESP32-S3 Modbus Emulator (xiao-esp32-s3)"
    echo "  transmitter | chint-reader LILYGO T-CAN485 Chint Master (chint-reader-lilygo-tcan485)"
    echo "  chint-reader-xiao         XIAO ESP32-S3 Chint Master (chint-reader-xiao-s3)"
    echo "  emulator-lilygo | lilygo  LILYGO T-CAN485 Emulator (lilygo-t-can485)"
    echo "  emulator-waveshare        Waveshare ESP32-S3 Emulator (waveshare-esp32-s3)"
    echo "  eastron-programmer        LILYGO T-CAN485 Eastron Programmer (eastron-programmer-tcan485)"
    echo ""
    echo -e "${CYAN}Options:${NC}"
    echo "  --build                   Force re-compile with PlatformIO before uploading"
    echo ""
    echo -e "${CYAN}Examples:${NC}"
    echo "  $0 emulator --build                   # Compile & flash emulator at ddsu666.local"
    echo "  $0 transmitter --build                # Compile & flash transmitter at chint-reader.local"
    echo "  $0 emulator 10.0.0.169                # Flash emulator directly via IP"
    echo "  $0 transmitter 10.0.0.110 --build     # Compile & flash transmitter via IP"
    exit 1
}

if [[ $# -lt 1 ]]; then
    usage
fi

TARGET="$1"
DEVICE_IP=""
DO_BUILD=false

# Parse second and third arguments flexibly
for arg in "${@:2}"; do
    if [[ "$arg" == "--build" ]]; then
        DO_BUILD=true
    elif [[ -z "$DEVICE_IP" ]]; then
        DEVICE_IP="$arg"
    fi
done

# Resolve environment name, default hostname, and binary path
case "$TARGET" in
    emulator|xiao|xiao-s3|seeed_xiao)
        ENV_NAME="xiao-esp32-s3"
        DEFAULT_HOST="ddsu666.local"
        BIN_PATH=".pio/build/xiao-esp32-s3/firmware.bin"
        ;;
    transmitter|chint-reader|chint|lilygo-reader|reader)
        ENV_NAME="chint-reader-lilygo-tcan485"
        DEFAULT_HOST="chint-reader.local"
        BIN_PATH=".pio/build/chint-reader-lilygo-tcan485/firmware.bin"
        ;;
    chint-reader-xiao)
        ENV_NAME="chint-reader-xiao-s3"
        DEFAULT_HOST="chint-reader.local"
        BIN_PATH=".pio/build/chint-reader-xiao-s3/firmware.bin"
        ;;
    emulator-lilygo|lilygo|tcan485)
        ENV_NAME="lilygo-t-can485"
        DEFAULT_HOST="ddsu666.local"
        BIN_PATH=".pio/build/lilygo-t-can485/firmware.bin"
        ;;
    emulator-waveshare|waveshare|s3)
        ENV_NAME="waveshare-esp32-s3"
        DEFAULT_HOST="ddsu666.local"
        BIN_PATH=".pio/build/waveshare-esp32-s3/firmware.bin"
        ;;
    eastron-programmer|programmer)
        ENV_NAME="eastron-programmer-tcan485"
        DEFAULT_HOST="10.0.0.110"
        BIN_PATH=".pio/build/eastron-programmer-tcan485/firmware.bin"
        ;;
    *)
        echo -e "${RED}Error: Unknown target '$TARGET'${NC}"
        usage
        ;;
esac

# Fallback to default hostname if not explicitly provided
if [[ -z "$DEVICE_IP" ]]; then
    DEVICE_IP="$DEFAULT_HOST"
fi

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

# Resolve OTA Password (default to "admin" which matches firmware defaults)
OTA_PASS="admin"
if [[ -f "include/secrets.h" ]]; then
    EXTRACTED_PASS=$(grep -E '^\s*#define\s+OTA_PASSWORD\s+' include/secrets.h | head -n1 | sed -E 's/.*"([^"]*)".*/\1/' || true)
    if [[ -n "$EXTRACTED_PASS" ]]; then
        OTA_PASS="$EXTRACTED_PASS"
    fi
fi
OTA_PASS="${OTA_PASSWORD:-$OTA_PASS}"

BIN_SIZE=$(wc -c < "$BIN_PATH")
echo -e "${CYAN}==> Target:${NC}      ${ENV_NAME}"
echo -e "${CYAN}==> Device Host:${NC} ${DEVICE_IP}"
echo -e "${CYAN}==> Binary:${NC}      ${BIN_PATH} (${BIN_SIZE} bytes)"
echo -e "${CYAN}==> Uploading OTA firmware to http://${DEVICE_IP}/update...${NC}"

# Perform OTA upload with curl
# Expect: header is mandatory to prevent 100-continue stalls on ESP32 WebServer
AUTH_ARGS=(-u "admin:${OTA_PASS}" -H "X-OTA-Password: ${OTA_PASS}")

HTTP_CODE=$(curl -s -o /tmp/ota_response.txt -w "%{http_code}" \
    "${AUTH_ARGS[@]}" \
    -F "file=@${BIN_PATH}" \
    -H "Expect:" \
    --connect-timeout 15 \
    --max-time 180 \
    "http://${DEVICE_IP}/update?password=${OTA_PASS}")

RESPONSE=$(cat /tmp/ota_response.txt 2>/dev/null || echo "")

if [[ "$HTTP_CODE" == "200" ]] && [[ "$RESPONSE" == *"OK"* ]]; then
    echo -e "${GREEN}✔ OTA Flash Successful!${NC} Device is rebooting into the new firmware."
    echo -e "${YELLOW}ℹ Waiting 5 seconds for reboot...${NC}"
    sleep 5
    if curl -s --connect-timeout 4 "http://${DEVICE_IP}/health" >/dev/null 2>&1; then
        echo -e "${GREEN}✔ Device is back online!${NC} (http://${DEVICE_IP}/health)"
    else
        echo -e "${YELLOW}ℹ Device is rebooting. Check http://${DEVICE_IP}/ in a few moments.${NC}"
    fi
else
    echo -e "${RED}✘ OTA Flash Failed! (HTTP Status: ${HTTP_CODE})${NC}"
    echo -e "Server response: ${RESPONSE}"
    exit 1
fi
