#pragma once

// Firmware update over BLE (FRD-024), platform part. Protocol: docs/pager_config_protocol.md
//  - ESP32 (V3, V4): the image is streamed into the spare OTA app slot (Arduino Update).
//  - nRF52 (T114): the pager reboots into the bootloader's BLE DFU mode; the client then
//    sends the DFU .zip with Nordic's legacy DFU protocol.

#include <stddef.h>
#include <stdint.h>

#ifndef PAGER_BOARD
  #error "PAGER_BOARD (build env name) required, see [pager_common]"
#endif

// Embedded in every image. The ESP32 update refuses images without the own marker,
// and the web app checks it before sending anything, so a V4 image can't brick a V3.
#define PAGER_BOARD_MARKER  "MSPAGER-BOARD:" PAGER_BOARD

enum class PagerOtaKind { NONE, ESP32_APP, NRF_DFU };

PagerOtaKind pagerOtaKind();
const char* pagerOtaBoard();

// ESP32 only; msg receives the reason on failure
bool pagerOtaBegin(uint32_t size, const uint8_t md5[16], char* msg, size_t msg_size);
bool pagerOtaWrite(const uint8_t* data, size_t len, char* msg, size_t msg_size);
bool pagerOtaEnd(char* msg, size_t msg_size);   // verifies size, MD5 and board marker
void pagerOtaAbort();
bool pagerOtaActive();

void pagerOtaRestart();   // ESP32: boot the new image. nRF52: reboot into BLE DFU mode
