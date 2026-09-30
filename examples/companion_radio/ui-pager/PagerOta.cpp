#include "PagerOta.h"
#include <Arduino.h>
#include <string.h>

// referenced below, so the marker string stays in the image
static const char board_marker[] = PAGER_BOARD_MARKER;

const char* pagerOtaBoard() { return board_marker + strlen("MSPAGER-BOARD:"); }

#if defined(ESP32)

#include <Update.h>

static bool ota_active = false;
static uint32_t ota_size = 0, ota_written = 0;
static size_t marker_pos = 0;     // matched bytes of board_marker incl. its NUL
static bool marker_found = false;

PagerOtaKind pagerOtaKind() { return PagerOtaKind::ESP32_APP; }
bool pagerOtaActive() { return ota_active; }

// board_marker[0] ('M') occurs only once in the marker, so a mismatch restarts at 0 or 1
static void scanMarker(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len && !marker_found; i++) {
    char c = (char)data[i];
    if (c == board_marker[marker_pos]) {
      if (++marker_pos == sizeof(board_marker)) marker_found = true;   // incl. NUL: exact board
    } else {
      marker_pos = (c == board_marker[0]) ? 1 : 0;
    }
  }
}

bool pagerOtaBegin(uint32_t size, const uint8_t md5[16], char* msg, size_t msg_size) {
  pagerOtaAbort();
  if (!Update.begin(size, U_FLASH)) {
    snprintf(msg, msg_size, "begin: %s", Update.errorString());
    return false;
  }
  char hex[33];
  for (int i = 0; i < 16; i++) sprintf(&hex[i * 2], "%02x", md5[i]);
  Update.setMD5(hex);
  ota_active = true;
  ota_size = size;
  ota_written = 0;
  marker_pos = 0;
  marker_found = false;
  return true;
}

bool pagerOtaWrite(const uint8_t* data, size_t len, char* msg, size_t msg_size) {
  if (!ota_active || ota_written + len > ota_size) {
    snprintf(msg, msg_size, "no update in progress");
    return false;
  }
  scanMarker(data, len);
  if (Update.write((uint8_t*)data, len) != len) {
    snprintf(msg, msg_size, "write: %s", Update.errorString());
    pagerOtaAbort();
    return false;
  }
  ota_written += len;
  return true;
}

bool pagerOtaEnd(char* msg, size_t msg_size) {
  if (!ota_active || ota_written != ota_size) {
    snprintf(msg, msg_size, "incomplete: %lu of %lu bytes", (unsigned long)ota_written, (unsigned long)ota_size);
    return false;
  }
  if (!marker_found) {
    snprintf(msg, msg_size, "not a pager image for %s", pagerOtaBoard());
    pagerOtaAbort();
    return false;
  }
  if (!Update.end()) {   // checks the MD5, then marks the new slot bootable
    snprintf(msg, msg_size, "verify: %s", Update.errorString());
    pagerOtaAbort();
    return false;
  }
  ota_active = false;
  snprintf(msg, msg_size, "OK, restarting");
  return true;
}

void pagerOtaAbort() {
  if (ota_active) Update.abort();
  ota_active = false;
}

void pagerOtaRestart() { ESP.restart(); }

#elif defined(NRF52_PLATFORM)

PagerOtaKind pagerOtaKind() { return PagerOtaKind::NRF_DFU; }
bool pagerOtaActive() { return false; }

bool pagerOtaBegin(uint32_t, const uint8_t*, char* msg, size_t msg_size) {
  snprintf(msg, msg_size, "use DFU on this board");
  return false;
}
bool pagerOtaWrite(const uint8_t*, size_t, char* msg, size_t msg_size) {
  snprintf(msg, msg_size, "use DFU on this board");
  return false;
}
bool pagerOtaEnd(char* msg, size_t msg_size) {
  snprintf(msg, msg_size, "use DFU on this board");
  return false;
}
void pagerOtaAbort() { }

void pagerOtaRestart() { enterOTADfu(); }   // Adafruit bootloader, BLE DFU mode (GPREGRET 0xA8)

#else

PagerOtaKind pagerOtaKind() { return PagerOtaKind::NONE; }
bool pagerOtaActive() { return false; }
bool pagerOtaBegin(uint32_t, const uint8_t*, char* msg, size_t msg_size) { snprintf(msg, msg_size, "not supported"); return false; }
bool pagerOtaWrite(const uint8_t*, size_t, char* msg, size_t msg_size) { snprintf(msg, msg_size, "not supported"); return false; }
bool pagerOtaEnd(char* msg, size_t msg_size) { snprintf(msg, msg_size, "not supported"); return false; }
void pagerOtaAbort() { }
void pagerOtaRestart() { }

#endif
