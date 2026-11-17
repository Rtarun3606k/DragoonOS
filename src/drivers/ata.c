#include "ata.h"
#include "io.h"
#include "log.h"

#define ATA_PRIMARY_DATA 0x1F0
#define ATA_PRIMARY_ERR 0x1F1
#define ATA_PRIMARY_SECCOUNT 0x1F2
#define ATA_PRIMARY_LBA_LO 0x1F3
#define ATA_PRIMARY_LBA_MID 0x1F4
#define ATA_PRIMARY_LBA_HI 0x1F5
#define ATA_PRIMARY_DRIVE_HEAD 0x1F6
#define ATA_PRIMARY_STATUS 0x1F7
#define ATA_PRIMARY_COMMAND 0x1F7
#define ATA_PRIMARY_ALT_STATUS 0x3F6

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_BSY 0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DF 0x20
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01

static int ata_present = 0;

static inline void insw(uint16_t port, void *addr, uint32_t count) {
  __asm__ volatile("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void *addr, uint32_t count) {
  __asm__ volatile("rep outsw"
                   : "+S"(addr), "+c"(count)
                   : "d"(port)
                   : "memory");
}

static void ata_400ns_delay(void) {
  inb(ATA_PRIMARY_ALT_STATUS);
  inb(ATA_PRIMARY_ALT_STATUS);
  inb(ATA_PRIMARY_ALT_STATUS);
  inb(ATA_PRIMARY_ALT_STATUS);
}

static int ata_wait_ready(void) {
  ata_400ns_delay();

  for (uint32_t i = 0; i < 1000000U; i++) {
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (!(status & ATA_SR_BSY)) {
      if (status & (ATA_SR_ERR | ATA_SR_DF)) {
        return -1;
      }
      if (status & ATA_SR_DRQ) {
        return 0;
      }
    }
  }
  return -1;
}

int ata_init(void) {
  outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
  ata_400ns_delay();

  outb(ATA_PRIMARY_SECCOUNT, 0);
  outb(ATA_PRIMARY_LBA_LO, 0);
  outb(ATA_PRIMARY_LBA_MID, 0);
  outb(ATA_PRIMARY_LBA_HI, 0);
  outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);

  uint8_t status = inb(ATA_PRIMARY_STATUS);
  if (status == 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "ATA: no primary master disk found");
    return -1;
  }

  while (status & ATA_SR_BSY) {
    status = inb(ATA_PRIMARY_STATUS);
  }

  if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL,
                "ATA: unsupported ATAPI/SATA device");
    return -1;
  }

  if (ata_wait_ready() != 0) {
    log_message(LOG_ERROR, LOG_OUTPUT_SERIAL, "ATA: IDENTIFY failed");
    return -1;
  }

  uint16_t identify_data[256];
  insw(ATA_PRIMARY_DATA, identify_data, 256);

  ata_present = 1;
  log_message(LOG_INFO, LOG_OUTPUT_SERIAL,
              "ATA: primary master disk initialized");
  return 0;
}

int ata_read_sectors(uint32_t lba, uint8_t sector_count, void *buffer) {
  if (!ata_present || sector_count == 0 || !buffer) {
    return -1;
  }

  outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_PRIMARY_SECCOUNT, sector_count);
  outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
  outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
  outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
  outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);

  uint16_t *dst = (uint16_t *)buffer;
  for (uint8_t s = 0; s < sector_count; s++) {
    if (ata_wait_ready() != 0) {
      return -1;
    }
    insw(ATA_PRIMARY_DATA, dst + (s * 256U), 256);
  }

  return 0;
}

int ata_write_sectors(uint32_t lba, uint8_t sector_count, const void *buffer) {
  if (!ata_present || sector_count == 0 || !buffer) {
    return -1;
  }

  outb(ATA_PRIMARY_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_PRIMARY_SECCOUNT, sector_count);
  outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
  outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
  outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
  outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);

  const uint16_t *src = (const uint16_t *)buffer;
  for (uint8_t s = 0; s < sector_count; s++) {
    if (ata_wait_ready() != 0) {
      return -1;
    }
    outsw(ATA_PRIMARY_DATA, src + (s * 256U), 256);
  }

  outb(ATA_PRIMARY_COMMAND, ATA_CMD_CACHE_FLUSH);
  ata_400ns_delay();
  return 0;
}
