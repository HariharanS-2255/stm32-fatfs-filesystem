#include "sd_card.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart2;
extern SPI_HandleTypeDef hspi2;

static uint8_t cardType = 0; // 0 = SDSC, 1 = SDHC

static uint8_t SPI_TxRx(uint8_t data) {
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi2, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

static void SD_SendDummyClocks(uint8_t count) {
    CS_HIGH();
    for (uint8_t i = 0; i < count; i++) SPI_TxRx(0xFF);
}

/**
 * Send SD command
 * @param cmd Command index with 0x40 offset
 * @param arg Command argument
 * @param crc Valid CRC for CMD0 & CMD8, else dummy
 * @param response Buffer for response bytes (NULL if only R1 is needed)
 * @param extraBytes How many extra bytes to read after R1 (R3/R7 = 4)
 * @return R1 byte
 */
static uint8_t SD_SendCommandEx(uint8_t cmd, uint32_t arg, uint8_t crc,
                                uint8_t *response, uint8_t extraBytes) {
    uint8_t buf[6];
    buf[0] = cmd;
    buf[1] = (arg >> 24) & 0xFF;
    buf[2] = (arg >> 16) & 0xFF;
    buf[3] = (arg >> 8) & 0xFF;
    buf[4] = arg & 0xFF;
    buf[5] = crc;

    CS_LOW();
    HAL_SPI_Transmit(&hspi2, buf, 6, HAL_MAX_DELAY);

    uint8_t r1;
    for (uint16_t i = 0; i < 500; i++) {
        r1 = SPI_TxRx(0xFF);
        if (!(r1 & 0x80)) break;
    }

    if (response && extraBytes > 0) {
        for (uint8_t i = 0; i < extraBytes; i++) {
            response[i] = SPI_TxRx(0xFF);
        }
    }

    return r1;
}

int SD_Init(void) {
    char buf[64];
    cardType = 0;

    // Slow SPI for init
    __HAL_SPI_DISABLE(&hspi2);
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    HAL_SPI_Init(&hspi2);

    // Dummy clocks
    SD_SendDummyClocks(10);

    // CMD0
    uint8_t r = SD_SendCommandEx(0x40 | 0, 0, 0x95, NULL, 0);
    sprintf(buf, "CMD0 resp: 0x%02X\r\n", r);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
    if (r != 0x01) return -1;

    // CMD8 (R7)
    uint8_t cmd8_resp[4];
    r = SD_SendCommandEx(0x40 | 8, 0x1AA, 0x87, cmd8_resp, 4);
    sprintf(buf, "CMD8 resp: 0x%02X\r\n", r);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
    sprintf(buf, "CMD8 R7: %02X %02X %02X %02X\r\n",
            cmd8_resp[0], cmd8_resp[1], cmd8_resp[2], cmd8_resp[3]);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    uint16_t retry = 0xFFFF;
    if (r == 0x01 && cmd8_resp[2] == 0x01 && cmd8_resp[3] == 0xAA) {
        // ACMD41 loop
        do {
            CS_HIGH(); SPI_TxRx(0xFF);
            SD_SendCommandEx(0x40 | 55, 0, 0x01, NULL, 0);
            CS_HIGH(); SPI_TxRx(0xFF);

            r = SD_SendCommandEx(0x40 | 41, 0x40000000, 0x01, NULL, 0);
            retry--;
        } while (r != 0x00 && retry);

        sprintf(buf, "ACMD41 resp: 0x%02X\r\n", r);
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

        // CMD58 (R3)
        uint8_t ocr[4];
        if (SD_SendCommandEx(0x40 | 58, 0, 0x01, ocr, 4) == 0x00) {
            sprintf(buf, "CMD58 OCR: %02X %02X %02X %02X\r\n",
                    ocr[0], ocr[1], ocr[2], ocr[3]);
            HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);
            cardType = (ocr[0] & 0x40) ? 1 : 0;
        }
    } else {
        // SDSC fallback
        sprintf(buf, "CMD8 fail, using SDSC init\r\n");
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

        do {
            CS_HIGH(); SPI_TxRx(0xFF);
            SD_SendCommandEx(0x40 | 55, 0, 0x01, NULL, 0);
            CS_HIGH(); SPI_TxRx(0xFF);

            r = SD_SendCommandEx(0x40 | 41, 0, 0x01, NULL, 0);
            retry--;
        } while (r != 0x00 && retry);
    }

    CS_HIGH();
    SPI_TxRx(0xFF);

    // Speed up SPI
    __HAL_SPI_DISABLE(&hspi2);
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
    HAL_SPI_Init(&hspi2);

    sprintf(buf, "Init OK, Card Type: %s\r\n", cardType ? "SDHC" : "SDSC");
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), HAL_MAX_DELAY);

    return 0;
}

int SD_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count) {
    while (count--) {
        uint32_t addr = (cardType ? sector : sector * 512);

        if (SD_SendCommandEx(0x40 | 17, addr, 0x01, NULL, 0) != 0x00) {
            CS_HIGH(); SPI_TxRx(0xFF);
            return -1;
        }

        // Wait for data token
        uint16_t timeout = 0xFFFF;
        while (SPI_TxRx(0xFF) != 0xFE && timeout--) ;
        if (timeout == 0) {
            CS_HIGH(); SPI_TxRx(0xFF);
            return -2;
        }

        for (uint16_t i = 0; i < 512; i++) {
            buff[i] = SPI_TxRx(0xFF);
        }

        SPI_TxRx(0xFF); // CRC
        SPI_TxRx(0xFF);

        CS_HIGH();
        SPI_TxRx(0xFF);

        buff += 512;
        sector++;
    }
    return 0;
}

int SD_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count) {
	while (count--) {
	        uint32_t addr = (cardType ? sector : sector * 512);

	        // Send CMD24 (write single block)
	        if (SD_SendCommandEx(0x40 | 24, addr, 0x01, NULL, 0) != 0x00) {
	            CS_HIGH(); SPI_TxRx(0xFF);
	            return -1;
	        }

	        SPI_TxRx(0xFF);
	        CS_LOW();

	        SPI_TxRx(0xFE); // Start block token

	        // Send data block
	        for (uint16_t i = 0; i < 512; i++) {
	            SPI_TxRx(buff[i]);
	        }

	        // Dummy CRC
	        SPI_TxRx(0xFF);
	        SPI_TxRx(0xFF);

	        // Receive data response
	        uint8_t resp = SPI_TxRx(0xFF);
	        if ((resp & 0x1F) != 0x05) {
	            CS_HIGH(); SPI_TxRx(0xFF);
	            return -2;
	        }

	        // Wait until not busy
	        while (SPI_TxRx(0xFF) == 0x00);

	        CS_HIGH();
	        SPI_TxRx(0xFF);

	        buff += 512;
	        sector++;
	    }

	    return 0;
}
