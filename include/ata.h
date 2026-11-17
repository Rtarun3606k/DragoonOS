#ifndef ATA_H
#define ATA_H

#include <stdint.h>

int ata_init(void);
int ata_read_sectors(uint32_t lba, uint8_t sector_count, void *buffer);
int ata_write_sectors(uint32_t lba, uint8_t sector_count, const void *buffer);

#endif
