#include "diskio.h"
#include "sd_card.h"   // <-- Your SD driver
#include "stm32f4xx_hal.h"

// Physical drive number for SD card
#define SD_CARD  0

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv == SD_CARD) {
        return 0; // Always OK for now
    }
    return STA_NOINIT;
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv == SD_CARD) {
        return (SD_Init() == 0) ? 0 : STA_NOINIT;
    }
    return STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    if (pdrv == SD_CARD) {
        return (SD_ReadBlocks(buff, sector, count) == 0) ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
}

#if _USE_WRITE == 1
DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    if (pdrv == SD_CARD) {
        return (SD_WriteBlocks(buff, sector, count) == 0) ? RES_OK : RES_ERROR;
    }
    return RES_PARERR;
}
#endif

#if _USE_IOCTL == 1
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv == SD_CARD) {
        switch (cmd) {
            case CTRL_SYNC:
                return RES_OK;

            case GET_SECTOR_COUNT:
                *(DWORD*)buff = 32768; // Example: 16MB card (change for your card)
                return RES_OK;

            case GET_SECTOR_SIZE:
                *(WORD*)buff = 512;
                return RES_OK;

            case GET_BLOCK_SIZE:
                *(DWORD*)buff = 1;
                return RES_OK;

            default:
                return RES_PARERR;
        }
    }
    return RES_PARERR;
}
#endif

