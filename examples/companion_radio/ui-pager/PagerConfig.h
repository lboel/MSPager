#pragma once

// MSPager runtime configuration (FRD-021): nickname, pager channel and extra
// question/answer sets. Uploaded as YAML over BLE, parsed on the device and
// stored as a compact binary blob. Values given here override the build config
// (pager.ini / pager.secret.ini). Plain C++ (no Arduino), so it runs in the native tests.

#include <stddef.h>
#include <stdint.h>

#define PAGER_CFG_VERSION        1
#define PAGER_CFG_NAME_MAX       31     // nickname / channel name bytes, like node_name and ChannelDetails.name
#define PAGER_CFG_TEXT_MAX       40     // bytes (UTF-8) per question or reply text (FRD-012 length budget)
#define PAGER_CFG_MAX_QUESTIONS  12
#define PAGER_CFG_MAX_REPLIES    6
#define PAGER_CFG_YAML_MAX       4096   // largest accepted YAML upload
#define PAGER_CFG_BLOB_MAX       (6 + 2 * (1 + PAGER_CFG_NAME_MAX) + 16 + 11 + 1 \
                                  + PAGER_CFG_MAX_QUESTIONS * (2 + PAGER_CFG_TEXT_MAX + PAGER_CFG_MAX_REPLIES * (1 + PAGER_CFG_TEXT_MAX)))

#define PAGER_CFG_HAS_NICKNAME   0x01
#define PAGER_CFG_HAS_CHANNEL    0x02   // channel name
#define PAGER_CFG_HAS_KEY        0x04   // channel key
#define PAGER_CFG_HAS_RADIO      0x08   // region + LoRa settings (FRD-023)

struct PagerQuestion {
  char text[PAGER_CFG_TEXT_MAX + 1];
  uint8_t num_replies;
  char replies[PAGER_CFG_MAX_REPLIES][PAGER_CFG_TEXT_MAX + 1];
};

struct PagerConfig {
  uint8_t flags;   // PAGER_CFG_HAS_*: which values override the build config
  char nickname[PAGER_CFG_NAME_MAX + 1];
  char channel_name[PAGER_CFG_NAME_MAX + 1];
  uint8_t channel_key[16];
  // radio (FRD-023): region = index + 1 into the region table, values always complete
  uint8_t region;
  uint32_t freq_khz;     // 869618 = 869.618 MHz
  uint32_t bw_hz;        // 62500 = 62.5 kHz
  uint8_t sf, cr;        // spreading factor 5..12, coding rate 5..8 (4/5..4/8)
  uint8_t num_questions;
  PagerQuestion questions[PAGER_CFG_MAX_QUESTIONS];
};

struct PagerCfgError {
  int line;        // 1-based YAML line, 0 = whole document
  char msg[64];
};

void pagerCfgClear(PagerConfig& cfg);

// Parses the YAML subset described in FRD-021. On failure, err holds line + reason and cfg is undefined.
bool pagerCfgParseYaml(const char* yaml, size_t len, PagerConfig& cfg, PagerCfgError& err);

// Compact flash format. Serialize returns the byte count, 0 if dest is too small.
int  pagerCfgSerialize(const PagerConfig& cfg, uint8_t* dest, int dest_size);
bool pagerCfgDeserialize(const uint8_t* src, int len, PagerConfig& cfg);

// region code ("EU", "US", ...) for PagerConfig::region, "" if none
const char* pagerCfgRegionName(uint8_t region);

// question with exactly this text, or NULL
const PagerQuestion* pagerCfgFindQuestion(const PagerConfig& cfg, const char* text);
