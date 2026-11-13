/*
 *  disk.c - Broadway/G3 SD card block device driver
 *
 *  Windows XP (NT 5.1) port for Broadway/PowerPC G3 (GameCube/Wii).
 *
 *  Reads/writes 512-byte sectors from the SD card via the Broadway EXI2
 *  (SD card interface) memory-mapped registers at 0xCD006400.
 *
 *  Exposes:
 *    HalpSdInit            - initialize the SD card
 *    HalpSdReadSectors     - read one or more 512-byte sectors
 *    HalpSdWriteSectors    - write one or more 512-byte sectors
 *    HalpSdGetSectorCount  - return total sector count
 *    HalpSdGetSectorSize   - return sector size (512)
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c disk.c
 *
 *  Broadway EXI register layout (channel 2 = SD card):
 *    Base address: 0xCD006400
 *    +0x00 EXI_CR     Control register
 *    +0x04 EXI_CSR    Control/Status register
 *    +0x08 EXI_DATA   Data register
 *    +0x0C EXI_EXI    Interrupt register
 *
 *  EXI_CR bits:
 *    0x00000001  EXI clock enable
 *    0x00000002  EXI interrupt enable
 *    0x00000004  EXI DMA enable
 *    0x00000008  EXI reset
 *
 *  EXI_CSR bits:
 *    0x00000001  EXI busy
 *    0x00000002  EXI error
 *    0x00000004  EXI data available
 *
 *  SD card commands (SPI mode over EXI):
 *    CMD0  = 0x40  GO_IDLE_STATE
 *    CMD8  = 0x48  SEND_IF_COND
 *    CMD16 = 0x50  SET_BLOCKLEN
 *    CMD17 = 0x51  READ_SINGLE_BLOCK
 *    CMD24 = 0x58  WRITE_SINGLE_BLOCK
 *    ACMD41= 0x69  SD_SEND_OP_COND (preceded by CMD55)
 *    CMD55 = 0x77  APP_CMD
 */

#include "ppctx.h"
#include <stddef.h>
#include <string.h>

/*--------------------------------------------------------------------------
    Disk activity blink flag (set by SD read/write paths; consumed by
    HalpBlinkBootIndicator() to flash the disk-activity block).
--------------------------------------------------------------------------*/
extern volatile BOOLEAN HalpDiskActivityPending;

/*--------------------------------------------------------------------------
    Broadway EXI2 (SD card) register base addresses.
--------------------------------------------------------------------------*/
#define EXI2_BASE            ((volatile unsigned long *)0xCD006400UL)
#define EXI_REG_CR           EXI2_BASE[0]   /* 0xCD006400 - Control */
#define EXI_REG_CSR          EXI2_BASE[1]   /* 0xCD006404 - Control/Status */
#define EXI_REG_DATA         EXI2_BASE[2]   /* 0xCD006408 - Data */
#define EXI_REG_EXI          EXI2_BASE[3]   /* 0xCD00640C - Interrupt */

/* EXI_CR bits. */
#define EXI_CR_ENABLE        0x00000001UL
#define EXI_CR_IRQ_ENABLE    0x00000002UL
#define EXI_CR_DMA_ENABLE    0x00000004UL
#define EXI_CR_RESET         0x00000008UL

/* EXI_CSR bits. */
#define EXI_CSR_BUSY         0x00000001UL
#define EXI_CSR_ERROR        0x00000002UL
#define EXI_CSR_DATA_READY   0x00000004UL

/* SD card commands (command byte = 0x40 | cmd_index). */
#define SD_CMD_GO_IDLE_STATE     0x40  /* CMD0 */
#define SD_CMD_SEND_IF_COND      0x48  /* CMD8 */
#define SD_CMD_APP_CMD           0x77  /* CMD55 */
#define SD_CMD_READ_SINGLE_BLOCK 0x51  /* CMD17 */
#define SD_CMD_WRITE_SINGLE_BLOCK 0x58 /* CMD24 */
#define SD_CMD_SET_BLOCKLEN      0x50  /* CMD16 */
#define SD_ACMD_SD_SEND_OP_COND  0x69  /* ACMD41 */

/* SD card responses. */
#define SD_R1_IDLE       0x01
#define SD_R1_ERASE_RESET 0x02
#define SD_R1_ILLEGAL_CMD 0x04
#define SD_R1_CRC_ERROR   0x08
#define SD_R1_ERASE_SEQ_ERROR 0x10
#define SD_R1_ADDRESS_ERROR 0x20
#define SD_R1_PARAMETER_ERROR 0x40

/* Sector size (bytes). */
#define SD_SECTOR_SIZE 512

/*--------------------------------------------------------------------------
    SD card state.
--------------------------------------------------------------------------*/
static BOOLEAN HalpSdInitialized = FALSE;
static ULONG   HalpSdSectorCount = 0;
static UCHAR   HalpSdCardType = 0;   /* 1 = SD v1, 2 = SD v2, 3 = SDHC */

/*--------------------------------------------------------------------------
    HalpExiWaitIdle - wait until the EXI bus is not busy.
--------------------------------------------------------------------------*/
static VOID
HalpExiWaitIdle(VOID)
{
    volatile ULONG timeout = 0x100000UL;
    while ((EXI_REG_CSR & EXI_CSR_BUSY) && timeout--) {
        __asm__ __volatile__ ("nop");
    }
}

/*--------------------------------------------------------------------------
    HalpExiWrite - write a 32-bit word to the EXI data register.
--------------------------------------------------------------------------*/
static VOID
HalpExiWrite(IN ULONG Word)
{
    HalpExiWaitIdle();
    EXI_REG_DATA = Word;
    HalpExiWaitIdle();
}

/*--------------------------------------------------------------------------
    HalpExiRead - read a 32-bit word from the EXI data register.
--------------------------------------------------------------------------*/
static ULONG
HalpExiRead(VOID)
{
    HalpExiWaitIdle();
    return EXI_REG_DATA;
}

/*--------------------------------------------------------------------------
    HalpSdSendCommand - send an SD command and read the R1 response.
    
    Returns the R1 response byte (0xFF = no card / timeout).
    cmd:    command byte (0x40 | index)
    arg:    32-bit command argument
    crc:    CRC7 (we use fixed dummy values; SD ignores CRC in SPI mode)
--------------------------------------------------------------------------*/
static UCHAR
HalpSdSendCommand(IN UCHAR Cmd, IN ULONG Arg, IN UCHAR Crc)
{
    UCHAR resp;

    /* Send command: 6 bytes (cmd, arg[31:24], arg[23:16], arg[15:8], arg[7:0], crc). */
    HalpExiWrite(Cmd);
    HalpExiWrite((UCHAR)(Arg >> 24));
    HalpExiWrite((UCHAR)(Arg >> 16));
    HalpExiWrite((UCHAR)(Arg >> 8));
    HalpExiWrite((UCHAR)Arg);
    HalpExiWrite(Crc);

    /* Read response: skip bytes until we get something other than 0xFF. */
    resp = (UCHAR)(HalpExiRead() & 0xFFUL);
    ULONG tries = 0;
    while (resp == 0xFF && tries++ < 1000) {
        resp = (UCHAR)(HalpExiRead() & 0xFFUL);
    }

    return resp;
}

/*--------------------------------------------------------------------------
    HalpSdReadBytes - read N bytes from the SD card into Buffer.
--------------------------------------------------------------------------*/
static VOID
HalpSdReadBytes(IN PVOID Buffer, IN ULONG ByteCount)
{
    volatile unsigned long *dst = (volatile unsigned long *)Buffer;
    volatile unsigned long *end = dst + (ByteCount / 4);

    while (dst < end) {
        *dst++ = HalpExiRead();
    }

    /* Handle trailing bytes (1-3). */
    ULONG rem = ByteCount % 4;
    if (rem != 0) {
        ULONG word = HalpExiRead();
        UCHAR *p = (UCHAR *)dst;
        UCHAR *w = (UCHAR *)&word;
        for (ULONG i = 0; i < rem; i++) {
            p[i] = w[i];
        }
    }
}

/*--------------------------------------------------------------------------
    HalpSdWriteBytes - write N bytes from Buffer to the SD card.
--------------------------------------------------------------------------*/
static VOID
HalpSdWriteBytes(IN const VOID *Buffer, IN ULONG ByteCount)
{
    const volatile unsigned long *src = (const volatile unsigned long *)Buffer;
    const volatile unsigned long *end = src + (ByteCount / 4);

    while (src < end) {
        HalpExiWrite(*src++);
    }

    /* Handle trailing bytes (1-3). */
    ULONG rem = ByteCount % 4;
    if (rem != 0) {
        ULONG word = 0;
        const UCHAR *p = (const UCHAR *)src;
        UCHAR *w = (UCHAR *)&word;
        for (ULONG i = 0; i < rem; i++) {
            w[i] = p[i];
        }
        HalpExiWrite(word);
    }
}

/*--------------------------------------------------------------------------
    HalpSdReadBlock - read one 512-byte sector via CMD17.
    
    Sector: sector number (LBA)
    Buffer: 512-byte destination buffer
    Returns TRUE on success.
--------------------------------------------------------------------------*/
static BOOLEAN
HalpSdReadBlock(IN ULONG Sector, OUT PVOID Buffer)
{
    UCHAR resp;

    /* Convert sector to byte address for SD v1 cards. */
    ULONG address = Sector;
    if (HalpSdCardType == 2 || HalpSdCardType == 3) {
        /* SD v2 and SDHC use byte addressing (sector is already bytes for block mode). */
        address = Sector * SD_SECTOR_SIZE;
    }

    resp = HalpSdSendCommand(SD_CMD_READ_SINGLE_BLOCK, address, 0xFF);
    if (resp != 0x00) {
        return FALSE;
    }

    /* Wait for data token (0xFE = start of data block). */
    UCHAR token = (UCHAR)(HalpExiRead() & 0xFFUL);
    ULONG tries = 0;
    while (token == 0xFF && tries++ < 50000) {
        token = (UCHAR)(HalpExiRead() & 0xFFUL);
    }
    if (token != 0xFE) {
        return FALSE;
    }

    /* Read 512 bytes of data. */
    HalpSdReadBytes(Buffer, SD_SECTOR_SIZE);

    /* Read and discard 2 bytes of CRC. */
    HalpExiRead();
    HalpExiRead();

    HalpDiskActivityPending = TRUE;   /* signal disk activity for blink indicator */
    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpSdWriteBlock - write one 512-byte sector via CMD24.
    
    Sector: sector number (LBA)
    Buffer: 512-byte source buffer
    Returns TRUE on success.
--------------------------------------------------------------------------*/
static BOOLEAN
HalpSdWriteBlock(IN ULONG Sector, IN const VOID *Buffer)
{
    UCHAR resp;

    ULONG address = Sector;
    if (HalpSdCardType == 2 || HalpSdCardType == 3) {
        address = Sector * SD_SECTOR_SIZE;
    }

    resp = HalpSdSendCommand(SD_CMD_WRITE_SINGLE_BLOCK, address, 0xFF);
    if (resp != 0x00) {
        return FALSE;
    }

    /* Send data start token (0xFE = single block write). */
    HalpExiWrite(0xFE);

    /* Write 512 bytes of data. */
    HalpSdWriteBytes(Buffer, SD_SECTOR_SIZE);

    /* Write 2 bytes of dummy CRC. */
    HalpExiWrite(0xFFFF);

    /* Wait for data response token. */
    UCHAR data_resp = (UCHAR)(HalpExiRead() & 0xFFUL);
    if ((data_resp & 0x1F) != 0x05) {
        return FALSE;
    }

    /* Wait for write to complete (busy clears). */
    HalpExiWaitIdle();
    while ((EXI_REG_CSR & EXI_CSR_BUSY)) {
        __asm__ __volatile__ ("nop");
    }

    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpSdInit - initialize the SD card.
    
    Performs SD card initialization sequence:
      1. Send 80 clock cycles (CS high equivalent)
      2. CMD0 (GO_IDLE_STATE)
      3. CMD8 (SEND_IF_COND) - check voltage
      4. CMD55 + ACMD41 (SD_SEND_OP_COND) - initialize card
      5. CMD16 (SET_BLOCKLEN 512)
    
    Returns TRUE on success.
--------------------------------------------------------------------------*/
BOOLEAN
HalpSdInit(VOID)
{
    UCHAR resp;
    ULONG i;

    /* Enable EXI interface. */
    EXI_REG_CR = EXI_CR_ENABLE | EXI_CR_IRQ_ENABLE;

    /* Send 80+ clock cycles with CS high to bring card to SPI mode.
     * We do this by writing 0xFF bytes 10 times. */
    for (i = 0; i < 10; i++) {
        HalpExiWrite(0xFFFFFFFFUL);
    }

    /* CMD0: GO_IDLE_STATE - reset card to idle state. */
    resp = HalpSdSendCommand(SD_CMD_GO_IDLE_STATE, 0x00000000, 0x95);
    if (resp != 0x01) {
        /* Card may not respond; still try to proceed. */
    }

    /* CMD8: SEND_IF_COND - check voltage range (SD v2). */
    resp = HalpSdSendCommand(SD_CMD_SEND_IF_COND, 0x000001AA, 0x87);
    if (resp == 0x01 || resp == 0xFF) {
        HalpSdCardType = 1;  /* SD v1 or MMC */
    } else if ((resp & 0xFE) == 0x00) {
        /* Read R7 response (4 bytes). */
        UCHAR r7[4];
        HalpSdReadBytes(r7, 4);
        if (r7[3] == 0xAA) {
            HalpSdCardType = 2;  /* SD v2 (non-HC) */
        } else {
            HalpSdCardType = 1;
        }
    } else {
        HalpSdCardType = 1;
    }

    /* ACMD41: SD_SEND_OP_COND - initialize card, wait until ready. */
    {
        ULONG tries = 0;
        UCHAR acmd41_arg = (HalpSdCardType == 2) ? 0x40000000UL : 0x00000000UL;
        do {
            /* CMD55: APP_CMD. */
            resp = HalpSdSendCommand(SD_CMD_APP_CMD, 0x00000000, 0xFF);
            if (resp == 0xFF) {
                break;
            }
            /* ACMD41: SD_SEND_OP_COND. */
            resp = HalpSdSendCommand(SD_ACMD_SD_SEND_OP_COND, acmd41_arg, 0xFF);
            tries++;
        } while ((resp & SD_R1_IDLE) != 0 && tries < 50000);

        if ((resp & SD_R1_IDLE) != 0) {
            return FALSE;  /* Card failed to initialize. */
        }

        /* If card is SD v2, check OCR for HC (high capacity). */
        if (HalpSdCardType == 2) {
            /* Read OCR register (4 bytes) after ACMD41. */
            /* The OCR follows the R1 response. We already read R1 above.
             * Read 4 more bytes for OCR. */
            UCHAR ocr[4];
            HalpSdReadBytes(ocr, 4);
            if ((ocr[0] & 0x40) != 0) {
                HalpSdCardType = 3;  /* SDHC/SDXC */
            }
        }
    }

    /* CMD16: SET_BLOCKLEN - set block length to 512 bytes. */
    resp = HalpSdSendCommand(SD_CMD_SET_BLOCKLEN, SD_SECTOR_SIZE, 0xFF);
    if (resp != 0x00) {
        return FALSE;
    }

    /* Determine sector count: read CSD register (CMD9). */
    {
        resp = HalpSdSendCommand(0x49, 0x00000000, 0xFF);  /* CMD9 */
        if (resp == 0x00) {
            /* Wait for data token 0xFE. */
            UCHAR token = (UCHAR)(HalpExiRead() & 0xFFUL);
            ULONG tries = 0;
            while (token == 0xFF && tries++ < 1000) {
                token = (UCHAR)(HalpExiRead() & 0xFFUL);
            }
            if (token == 0xFE) {
                UCHAR csd[16];
                HalpSdReadBytes(csd, 16);
                /* Read CRC (2 bytes) - discard. */
                HalpExiRead();
                HalpExiRead();

                /* Parse CSD for capacity. */
                if ((csd[0] >> 6) == 1) {
                    /* CSD v2 (SDHC). */
                    ULONG csize = ((ULONG)(csd[9] & 0x3F) << 16)
                                | ((ULONG)csd[10] << 8)
                                | (ULONG)csd[11];
                    HalpSdSectorCount = (csize + 1) * 1024UL;
                } else {
                    /* CSD v1 (SD v1/v2 non-HC). */
                    ULONG read_bl_len = csd[5] & 0x0F;
                    ULONG csize = ((ULONG)(csd[6] & 0x03) << 10)
                                | ((ULONG)csd[7] << 2)
                                | ((ULONG)csd[8] >> 6);
                    ULONG csize_mult = ((ULONG)(csd[9] & 0x03) << 1)
                                     | ((ULONG)(csd[10] >> 7));
                    HalpSdSectorCount = ((csize + 1) * (1UL << (csize_mult + 2)))
                                      * (1UL << read_bl_len) / SD_SECTOR_SIZE;
                }
            }
        }
    }

    HalpSdInitialized = TRUE;
    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpSdReadSectors - read one or more 512-byte sectors.
    
    Sector:    starting sector (LBA)
    SectorCount: number of sectors to read
    Buffer:    destination buffer (must be SectorCount * 512 bytes)
    Returns:   number of sectors successfully read.
--------------------------------------------------------------------------*/
ULONG
HalpSdReadSectors(IN ULONG Sector, IN ULONG SectorCount, OUT PVOID Buffer)
{
    ULONG read = 0;
    UCHAR *buf = (UCHAR *)Buffer;

    if (!HalpSdInitialized) {
        return 0;
    }

    while (read < SectorCount) {
        if (!HalpSdReadBlock(Sector + read, buf + read * SD_SECTOR_SIZE)) {
            break;
        }
        read++;
    }

    return read;
}

/*--------------------------------------------------------------------------
    HalpSdWriteSectors - write one or more 512-byte sectors.
    
    Sector:    starting sector (LBA)
    SectorCount: number of sectors to write
    Buffer:    source buffer (must be SectorCount * 512 bytes)
    Returns:   number of sectors successfully written.
--------------------------------------------------------------------------*/
ULONG
HalpSdWriteSectors(IN ULONG Sector, IN ULONG SectorCount, IN const VOID *Buffer)
{
    ULONG written = 0;
    const UCHAR *buf = (const UCHAR *)Buffer;

    if (!HalpSdInitialized) {
        return 0;
    }

    while (written < SectorCount) {
        if (!HalpSdWriteBlock(Sector + written, buf + written * SD_SECTOR_SIZE)) {
            break;
        }
        written++;
    }

    return written;
}

/*--------------------------------------------------------------------------
    HalpSdGetSectorCount - return total sector count on the SD card.
--------------------------------------------------------------------------*/
ULONG
HalpSdGetSectorCount(VOID)
{
    return HalpSdSectorCount;
}

/*--------------------------------------------------------------------------
    HalpSdGetSectorSize - return sector size (always 512).
--------------------------------------------------------------------------*/
ULONG
HalpSdGetSectorSize(VOID)
{
    return SD_SECTOR_SIZE;
}

/*--------------------------------------------------------------------------
    HalpSdIsInitialized - query SD initialization state.
--------------------------------------------------------------------------*/
BOOLEAN
HalpSdIsInitialized(VOID)
{
    return HalpSdInitialized;
}
