/*
 * storage.h - the saved setup, kept across power cycles.
 *
 * Without this the product does not do the thing it claims. "Save your setup and
 * reproduce it next week" fails at the first unplug, and worse, it fails
 * circularly: to re-save the reference you must already have the rig set up
 * correctly, which is precisely what the device is for.
 *
 * WHERE IT LIVES
 * The last sector of internal flash. The sketch occupies about 11% from the
 * bottom and nothing else claims the top, so one 4 KB sector at the end is free.
 * Only one setup is stored - recalling one of several presets is future work.
 *
 * WHY A CHECKSUM
 * Flash is erased before it is written. Losing power between the erase and the
 * program leaves a sector of 0xFF, and losing it mid-program leaves half a
 * record. Either would otherwise be read back as a reference and silently
 * corrupt every verdict afterwards. A CRC32 over the record means a partial
 * write fails to verify and the device honestly reports NO SETUP SAVED, which is
 * the only safe answer.
 *
 * WEAR
 * The nRF52840 is rated for 10,000 erase cycles per page. One erase per save,
 * and a save is a deliberate two-second button hold, so this is not a concern.
 */
#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino_CRC32.h>
#include <FlashIAP.h>
#include <string.h>

struct StoredSetup {
  uint32_t magic;
  uint32_t version;
  float    refMm;
  int32_t  refLight;
  uint32_t crc;        // over every field above
};

// "LGES" - LightGuide Edge Setup. Distinguishes a real record from erased flash
// (all 0xFF) or from whatever happened to be there before.
const uint32_t SETUP_MAGIC   = 0x4C474553UL;
const uint32_t SETUP_VERSION = 1;

inline uint32_t setupCrc(const StoredSetup &s) {
  Arduino_CRC32 crc;
  return crc.calc((uint8_t const *)&s, sizeof(StoredSetup) - sizeof(uint32_t));
}

inline uint32_t storageAddress(mbed::FlashIAP &flash) {
  const uint32_t end = flash.get_flash_start() + flash.get_flash_size();
  return end - flash.get_sector_size(end - 1);
}

// Returns true only if a complete, uncorrupted record of the expected version
// was found. Every other outcome - erased flash, a half-written record, a record
// from an older firmware - returns false and leaves the outputs untouched.
inline bool loadSetup(float &refMm, int &refLight) {
  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  StoredSetup s;
  const int rc = flash.read(&s, storageAddress(flash), sizeof(s));
  flash.deinit();

  if (rc != 0)                        return false;
  if (s.magic   != SETUP_MAGIC)       return false;
  if (s.version != SETUP_VERSION)     return false;
  if (s.crc     != setupCrc(s))       return false;

  refMm    = s.refMm;
  refLight = s.refLight;
  return true;
}

inline bool saveSetup(float refMm, int refLight) {
  StoredSetup s;
  s.magic    = SETUP_MAGIC;
  s.version  = SETUP_VERSION;
  s.refMm    = refMm;
  s.refLight = (int32_t)refLight;
  s.crc      = setupCrc(s);

  mbed::FlashIAP flash;
  if (flash.init() != 0) return false;

  const uint32_t addr   = storageAddress(flash);
  const uint32_t sector = flash.get_sector_size(addr);
  const uint32_t page   = flash.get_page_size();

  // program() writes whole pages, so the record is padded out to a page
  // boundary with 0xFF - the same value erased flash already holds.
  uint8_t buf[64];
  const uint32_t n = ((sizeof(StoredSetup) + page - 1) / page) * page;
  if (n > sizeof(buf)) { flash.deinit(); return false; }
  memset(buf, 0xFF, sizeof(buf));
  memcpy(buf, &s, sizeof(s));

  int rc = flash.erase(addr, sector);
  if (rc == 0) rc = flash.program(buf, addr, n);
  flash.deinit();
  return rc == 0;
}

#endif
