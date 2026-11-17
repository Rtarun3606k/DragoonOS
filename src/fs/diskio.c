#include "ff.h"
#include "diskio.h"
#include "ata.h"

static DSTATUS stat = STA_NOINIT;

DSTATUS disk_status(BYTE pdrv) {
  if (pdrv != 0)
    return STA_NOINIT;
  return stat;
}

DSTATUS disk_initialize(BYTE pdrv) {
  if (pdrv != 0)
    return STA_NOINIT;
  if (ata_init() == 0) {
    stat &= ~STA_NOINIT;
  }
  return stat;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
  if (pdrv != 0 || (stat & STA_NOINIT))
    return RES_NOTRDY;
  while (count > 0) {
    uint8_t chunk = count > 255 ? 255 : (uint8_t)count;
    if (ata_read_sectors((uint32_t)sector, chunk, buff) != 0) {
      return RES_ERROR;
    }
    sector += chunk;
    buff += chunk * 512U;
    count -= chunk;
  }
  return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
  if (pdrv != 0 || (stat & STA_NOINIT))
    return RES_NOTRDY;
  while (count > 0) {
    uint8_t chunk = count > 255 ? 255 : (uint8_t)count;
    if (ata_write_sectors((uint32_t)sector, chunk, buff) != 0) {
      return RES_ERROR;
    }
    sector += chunk;
    buff += chunk * 512U;
    count -= chunk;
  }
  return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
  if (pdrv != 0 || (stat & STA_NOINIT))
    return RES_NOTRDY;
  switch (cmd) {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_SIZE:
    *(WORD *)buff = 512;
    return RES_OK;
  case GET_BLOCK_SIZE:
    *(DWORD *)buff = 1;
    return RES_OK;
  case GET_SECTOR_COUNT:
    *(LBA_t *)buff = 131040;
    return RES_OK;
  }
  return RES_PARERR;
}

DWORD get_fattime(void) {
  return ((DWORD)(2026 - 1980) << 25) | ((DWORD)11 << 21) | ((DWORD)15 << 16);
}
