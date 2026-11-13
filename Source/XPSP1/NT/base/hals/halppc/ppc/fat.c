/*
 *  fat.c - Minimal FAT12/16/32 filesystem reader
 *
 *  Windows XP (NT 5.1) port for Broadway/PowerPC G3 (GameCube/Wii).
 *
 *  Reads files from a FAT12/FAT16/FAT32 volume on the SD card using the
 *  HAL SD block driver (HalpSdReadSectors / HalpSdWriteSectors).
 *
 *  Supported operations:
 *    HalpFatInit            - mount the FAT volume (read BPB, locate FAT/root)
 *    HalpFatOpen            - open a file by path (e.g. "\\nt\\arcfw.elf")
 *    HalpFatRead            - read bytes from an open file
 *    HalpFatGetFileSize     - return file size
 *    HalpFatClose           - close an open file
 *
 *  Build: powerpc-linux-gnu-gcc -mcpu=750 -mno-altivec -c fat.c
 */

#include "ppctx.h"
#include <stddef.h>
#include <string.h>

/* Pointer types not defined in ppctx.h for this freestanding module. */
#ifndef PULONG
#define PULONG ULONG *
#endif
#ifndef PUCHAR
#define PUCHAR UCHAR *
#endif
#ifndef PCHAR
#define PCHAR CHAR *
#endif

/*--------------------------------------------------------------------------
    Freestanding C library: memset/memcpy are declared in inc_stub/string.h
    but never implemented elsewhere in this port, so we provide local
    implementations here (used by the FAT reader).
--------------------------------------------------------------------------*/
void *
memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned char uc = (unsigned char)c;
    while (n-- > 0) {
        *p++ = uc;
    }
    return s;
}

void *
memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n-- > 0) {
        *d++ = *s++;
    }
    return dest;
}

/* inc_stub/string.h only declares memset/memcpy; provide memcmp locally. */
static int
HalpFatMemCmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    while (n-- > 0) {
        if (*p1 != *p2) {
            return (int)(*p1 - *p2);
        }
        p1++;
        p2++;
    }
    return 0;
}

/*--------------------------------------------------------------------------
    Constants.
--------------------------------------------------------------------------*/
#define FAT_SECTOR_SIZE 512
#define FAT_MAX_PATH    128
#define FAT_MAX_OPEN    4

/* Directory entry attributes. */
#define FAT_ATTR_READ_ONLY   0x01
#define FAT_ATTR_HIDDEN      0x02
#define FAT_ATTR_SYSTEM      0x04
#define FAT_ATTR_VOLUME_ID   0x08
#define FAT_ATTR_DIRECTORY   0x10
#define FAT_ATTR_ARCHIVE     0x20
#define FAT_ATTR_LONG_NAME   0x0F

/* Directory entry name terminators. */
#define FAT_DIR_FREE         0x00  /* entry never used */
#define FAT_DIR_DELETED      0xE5  /* entry deleted */

/*--------------------------------------------------------------------------
    On-disk structures (packed, little-endian).
--------------------------------------------------------------------------*/

/* BIOS Parameter Block (FAT12/16/16 common part). */
typedef struct _FAT_BPB {
    UCHAR  JumpBoot[3];
    CHAR   OemName[8];
    USHORT BytesPerSector;
    UCHAR  SectorsPerCluster;
    USHORT ReservedSectors;
    UCHAR  NumFats;
    USHORT RootDirEntries;
    USHORT TotalSectors16;
    UCHAR  Media;
    USHORT SectorsPerFat16;
    USHORT SectorsPerTrack;
    USHORT Heads;
    ULONG  HiddenSectors;
    ULONG  TotalSectors32;
} FAT_BPB, *PFAT_BPB;

/* FAT32 extended BPB (follows the common BPB). */
typedef struct _FAT_BPB_FAT32 {
    ULONG  SectorsPerFat32;
    USHORT ExtFlags;
    USHORT FsVersion;
    ULONG  RootCluster;
    USHORT FsInfo;
    USHORT BackupBootSector;
    UCHAR  Reserved[12];
    UCHAR  DriveNumber;
    UCHAR  Reserved1;
    UCHAR  BootSignature;
    ULONG  VolumeId;
    UCHAR  VolumeLabel[11];
    UCHAR  FileSystemType[8];
} FAT_BPB_FAT32, *PFAT_BPB_FAT32;

/* Directory entry (32 bytes). */
typedef struct _FAT_DIR_ENTRY {
    CHAR   Name[11];
    UCHAR  Attributes;
    UCHAR  Reserved[10];
    USHORT Time;
    USHORT Date;
    USHORT FirstClusterHigh;
    USHORT FirstClusterLow;
    ULONG  FileSize;
} FAT_DIR_ENTRY, *PFAT_DIR_ENTRY;

/*--------------------------------------------------------------------------
    Filesystem volume state.
--------------------------------------------------------------------------*/
typedef struct _FAT_VOLUME {
    BOOLEAN Mounted;
    BOOLEAN IsFat32;

    ULONG BytesPerSector;
    ULONG SectorsPerCluster;
    ULONG ClusterSize;        /* bytes per cluster */
    ULONG ReservedSectors;
    ULONG NumFats;
    ULONG SectorsPerFat;
    ULONG RootDirEntries;     /* FAT12/16 only */
    ULONG TotalSectors;
    ULONG FirstDataSector;
    ULONG FirstFatSector;
    ULONG RootDirSector;      /* FAT12/16 root dir start sector */
    ULONG RootCluster;        /* FAT32 root cluster */
} FAT_VOLUME, *PFAT_VOLUME;

static FAT_VOLUME HalpFatVolume;

/* Sector read cache (single sector). */
static UCHAR HalpFatSectorCache[FAT_SECTOR_SIZE];
static ULONG HalpFatCachedSector = 0xFFFFFFFFUL;
static BOOLEAN HalpFatCacheValid = FALSE;

/*--------------------------------------------------------------------------
    Forward declarations (SD block driver, disk.c).
--------------------------------------------------------------------------*/
extern BOOLEAN HalpSdIsInitialized(VOID);
extern ULONG   HalpSdReadSectors(IN ULONG Sector, IN ULONG SectorCount, OUT PVOID Buffer);
extern ULONG   HalpSdGetSectorSize(VOID);

/*--------------------------------------------------------------------------
    HalpFatReadSector - read one sector from the SD card, using cache.
--------------------------------------------------------------------------*/
static BOOLEAN
HalpFatReadSector(IN ULONG Sector, OUT PVOID Buffer)
{
    if (Sector == HalpFatCachedSector && HalpFatCacheValid) {
        memcpy(Buffer, HalpFatSectorCache, FAT_SECTOR_SIZE);
        return TRUE;
    }

    ULONG read = HalpSdReadSectors(Sector, 1, HalpFatSectorCache);
    if (read != 1) {
        return FALSE;
    }

    HalpFatCachedSector = Sector;
    HalpFatCacheValid = TRUE;
    memcpy(Buffer, HalpFatSectorCache, FAT_SECTOR_SIZE);
    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpFatClusterToSector - convert cluster number to sector number.
--------------------------------------------------------------------------*/
static ULONG
HalpFatClusterToSector(IN ULONG Cluster)
{
    PFAT_VOLUME vol = &HalpFatVolume;
    return vol->FirstDataSector + (Cluster - 2) * vol->SectorsPerCluster;
}

/*--------------------------------------------------------------------------
    HalpFatReadFatEntry - read a FAT entry for the given cluster.
    
    Returns the next cluster in the chain, or 0xFFFFFFFF on end-of-chain/error.
--------------------------------------------------------------------------*/
static ULONG
HalpFatReadFatEntry(IN ULONG Cluster)
{
    PFAT_VOLUME vol = &HalpFatVolume;
    ULONG fatOffset, fatSector, fatIndex;
    UCHAR sector[FAT_SECTOR_SIZE];
    ULONG next;

    if (vol->IsFat32) {
        fatOffset = Cluster * 4;
        fatSector = vol->FirstFatSector + (fatOffset / vol->BytesPerSector);
        fatIndex  = fatOffset % vol->BytesPerSector;

        if (!HalpFatReadSector(fatSector, sector)) {
            return 0xFFFFFFFFUL;
        }

        next = *(ULONG *)(sector + fatIndex) & 0x0FFFFFFFUL;
        if (next >= 0x0FFFFFF8UL) {
            return 0xFFFFFFFFUL; /* end of chain */
        }
        return next;
    } else {
        /* FAT16: 2 bytes per entry. */
        fatOffset = Cluster * 2;
        fatSector = vol->FirstFatSector + (fatOffset / vol->BytesPerSector);
        fatIndex  = fatOffset % vol->BytesPerSector;

        if (!HalpFatReadSector(fatSector, sector)) {
            return 0xFFFFFFFFUL;
        }

        next = *(USHORT *)(sector + fatIndex);
        if (next >= 0xFFF8UL) {
            return 0xFFFFFFFFUL; /* end of chain */
        }
        return next;
    }
}

/*--------------------------------------------------------------------------
    HalpFatInit - mount the FAT volume.
    
    Reads sector 0, parses the BPB, locates the FAT and root directory.
    Returns TRUE on success.
--------------------------------------------------------------------------*/
BOOLEAN
HalpFatInit(VOID)
{
    PFAT_VOLUME vol = &HalpFatVolume;
    UCHAR sector[FAT_SECTOR_SIZE];
    PFAT_BPB bpb;
    PFAT_BPB_FAT32 bpb32;

    memset(vol, 0, sizeof(*vol));

    /* The SD card must be initialized first. */
    if (!HalpSdIsInitialized()) {
        return FALSE;
    }

    /* Read sector 0 (boot sector). */
    if (!HalpFatReadSector(0, sector)) {
        return FALSE;
    }

    bpb = (PFAT_BPB)sector;

    vol->BytesPerSector   = bpb->BytesPerSector;
    vol->SectorsPerCluster= bpb->SectorsPerCluster;
    vol->ReservedSectors  = bpb->ReservedSectors;
    vol->NumFats          = bpb->NumFats;
    vol->RootDirEntries   = bpb->RootDirEntries;
    vol->TotalSectors     = bpb->TotalSectors16 ? bpb->TotalSectors16 : bpb->TotalSectors32;

    /* Determine FAT12/16 vs FAT32. */
    vol->SectorsPerFat = bpb->SectorsPerFat16 ? bpb->SectorsPerFat16 : 0;

    /* Check the FAT32 signature at offset 0x52. */
    {
        CHAR fsType[9];
        memcpy(fsType, sector + 0x52, 8);
        fsType[8] = '\0';

        /* FAT32 volumes have "FAT32" in the BPB extended area. */
        if (HalpFatMemCmp(fsType, "FAT32", 5) == 0) {
            vol->IsFat32 = TRUE;
            bpb32 = (PFAT_BPB_FAT32)(sector + sizeof(FAT_BPB));
            vol->SectorsPerFat = bpb32->SectorsPerFat32;
            vol->RootCluster   = bpb32->RootCluster;
        } else {
            vol->IsFat32 = FALSE;
            vol->RootCluster = 0;
        }
    }

    vol->ClusterSize = vol->BytesPerSector * vol->SectorsPerCluster;
    vol->FirstFatSector = vol->ReservedSectors;
    vol->FirstDataSector = vol->ReservedSectors + (vol->NumFats * vol->SectorsPerFat);

    if (!vol->IsFat32) {
        /* Root directory is at Fixed location for FAT12/16. */
        ULONG rootDirSectors = ((vol->RootDirEntries * 32) + (vol->BytesPerSector - 1)) / vol->BytesPerSector;
        vol->RootDirSector = vol->FirstDataSector - rootDirSectors;
        vol->FirstDataSector += rootDirSectors;
    }

    vol->Mounted = TRUE;
    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpFatIsMounted - query volume mount state.
--------------------------------------------------------------------------*/
BOOLEAN
HalpFatIsMounted(VOID)
{
    return HalpFatVolume.Mounted;
}

/*--------------------------------------------------------------------------
    HalpFatGetSectorSize - return sector size.
--------------------------------------------------------------------------*/
ULONG
HalpFatGetSectorSize(VOID)
{
    return FAT_SECTOR_SIZE;
}

/*--------------------------------------------------------------------------
    HalpFatMatchName - compare an 8.3 name against a directory entry name.
    
    name:  null-terminated string, may contain '.' (e.g. "README.TXT")
    entry: pointer to a FAT_DIR_ENTRY
    Returns TRUE if they match (case-insensitive).
--------------------------------------------------------------------------*/
static BOOLEAN
HalpFatMatchName(IN const CHAR *Name, IN const FAT_DIR_ENTRY *Entry)
{
    CHAR fatName[12];
    ULONG i, j;

    /* Build the 8.3 name from the entry. */
    for (i = 0; i < 8; i++) {
        fatName[i] = Entry->Name[i];
    }
    fatName[8] = '.';
    for (i = 0; i < 3; i++) {
        fatName[9 + i] = Entry->Name[8 + i];
    }
    fatName[12] = '\0';

    /* Trim trailing spaces. */
    for (i = 11; i > 0 && fatName[i - 1] == ' '; i--) {
        fatName[i - 1] = '\0';
    }

    /* Compare case-insensitively. */
    for (i = 0, j = 0; fatName[i] != '\0' && Name[j] != '\0'; i++, j++) {
        CHAR a = fatName[i];
        CHAR b = Name[j];
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b) {
            return FALSE;
        }
    }

    return fatName[i] == '\0' && Name[j] == '\0';
}

/*--------------------------------------------------------------------------
    HalpFatFindFile - find a file/directory entry in a directory sector.
    
    dirSector: sector containing directory entries
    name:      8.3 name to find (e.g. "NTLDR   SYS")
    entry:     output directory entry (if found)
    Returns:   sector offset within dirSector where entry was found, or 0xFFFFFFFF.
--------------------------------------------------------------------------*/
static ULONG
HalpFatFindFileInSector(IN ULONG DirSector, IN const CHAR *Name, OUT PFAT_DIR_ENTRY Entry)
{
    UCHAR sector[FAT_SECTOR_SIZE];
    ULONG entriesPerSector = FAT_SECTOR_SIZE / sizeof(FAT_DIR_ENTRY);
    ULONG i;

    if (!HalpFatReadSector(DirSector, sector)) {
        return 0xFFFFFFFFUL;
    }

    for (i = 0; i < entriesPerSector; i++) {
        const FAT_DIR_ENTRY *ent = (const FAT_DIR_ENTRY *)(sector + i * sizeof(FAT_DIR_ENTRY));

        /* Free/unused entry - stop scanning (entries are contiguous). */
        if (ent->Name[0] == FAT_DIR_FREE) {
            return 0xFFFFFFFFUL;
        }

        /* Deleted entry. */
        if (ent->Name[0] == FAT_DIR_DELETED) {
            continue;
        }

        /* Volume label / long-name entry - skip. */
        if ((ent->Attributes & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
            continue;
        }

        if (HalpFatMatchName(Name, ent)) {
            if (Entry != NULL) {
                *Entry = *ent;
            }
            return i; /* entry index within sector */
        }
    }

    return 0xFFFFFFFFUL;
}

/*--------------------------------------------------------------------------
    HalpFatParsePath - split a path into components.
    
    path:    null-terminated path (e.g. "\\nt\\arcfw.elf")
    comps:   output array of component names (8.3 format, space-padded)
    maxComps: max number of components
    Returns: number of components found.
--------------------------------------------------------------------------*/
static ULONG
HalpFatParsePath(IN const CHAR *Path, OUT CHAR Comps[][12], IN ULONG MaxComps)
{
    ULONG n = 0;
    const CHAR *p = Path;

    /* Skip leading slashes. */
    while (*p == '\\' || *p == '/') {
        p++;
    }

    while (*p != '\0' && n < MaxComps) {
        const CHAR *start = p;
        ULONG len = 0;

        /* Extract component up to next slash. */
        while (*p != '\0' && *p != '\\' && *p != '/') {
            p++;
            len++;
        }

        /* Copy component (max 11 chars + null). */
        if (len > 11) {
            len = 11;
        }
        memcpy(Comps[n], start, len);
        Comps[n][len] = '\0';
        n++;

        /* Skip slashes. */
        while (*p == '\\' || *p == '/') {
            p++;
        }
    }

    return n;
}

/*--------------------------------------------------------------------------
    HalpFatNameTo83 - convert a component name to 8.3 space-padded form.
    
    src:  source name (e.g. "ARC.FW" or "ARC" or "FW.ELF")
    dst:  12-byte output buffer (11 chars + null), space-padded
--------------------------------------------------------------------------*/
static VOID
HalpFatNameTo83(IN const CHAR *Src, OUT CHAR Dst[12])
{
    ULONG i;
    CHAR base[9];
    CHAR ext[4];
    ULONG baseLen = 0, extLen = 0;

    memset(Dst, ' ', 11);
    Dst[11] = '\0';

    /* Split on '.'. */
    for (i = 0; Src[i] != '\0' && Src[i] != '.' && i < 8; i++) {
        base[i] = Src[i];
        baseLen++;
    }
    base[baseLen] = '\0';

    if (Src[i] == '.') {
        i++;
        for (; Src[i] != '\0' && i < 8 + 3; i++) {
            ext[extLen] = Src[i];
            extLen++;
        }
        ext[extLen] = '\0';
    }

    /* Copy base (8 chars). */
    for (i = 0; i < 8; i++) {
        Dst[i] = (i < baseLen) ? base[i] : ' ';
    }

    /* Copy extension (3 chars). */
    for (i = 0; i < 3; i++) {
        Dst[8 + i] = (i < extLen) ? ext[i] : ' ';
    }
}

/*--------------------------------------------------------------------------
    HalpFatOpenDirectory - open a directory and return its starting sector/cluster.
    
    pathComponents: array of 8.3 component names
    nComps:         number of components
    startCluster:   starting cluster (for FAT32) or sector (FAT12/16) of the parent dir
    outCluster:     output starting cluster/sector of the target dir
    Returns:        TRUE on success.
--------------------------------------------------------------------------*/
static BOOLEAN
HalpFatOpenDirectory(IN const CHAR Comps[][12], IN ULONG NComps,
                     IN ULONG StartCluster, OUT PULONG OutCluster)
{
    PFAT_VOLUME vol = &HalpFatVolume;
    ULONG current = StartCluster;
    ULONG i;

    for (i = 0; i < NComps; i++) {
        CHAR name83[12];
        HalpFatNameTo83(Comps[i], name83);

        ULONG dirSector;
        if (vol->IsFat32) {
            dirSector = HalpFatClusterToSector(current);
        } else {
            dirSector = current; /* current is a sector for FAT12/16 root */
        }

        BOOLEAN found = FALSE;
        ULONG sectorsToScan = vol->IsFat32 ? vol->SectorsPerCluster : 1;

        ULONG s;
        for (s = 0; s < sectorsToScan; s++) {
            FAT_DIR_ENTRY entry;
            ULONG idx = HalpFatFindFileInSector(dirSector + s, name83, &entry);
            if (idx != 0xFFFFFFFFUL) {
                found = TRUE;
                if (i == NComps - 1) {
                    /* Last component: return its cluster. */
                    *OutCluster = ((ULONG)entry.FirstClusterHigh << 16) | entry.FirstClusterLow;
                } else {
                    /* Intermediate dir: descend into it. */
                    current = ((ULONG)entry.FirstClusterHigh << 16) | entry.FirstClusterLow;
                }
                break;
            }
        }

        if (!found) {
            return FALSE;
        }
    }

    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpFatOpen - open a file by path.
    
    path:     null-terminated path (e.g. "\\nt\\arcfw.elf")
    fileSize: output file size in bytes
    startCluster: output starting cluster of the file
    Returns:  TRUE on success.
--------------------------------------------------------------------------*/
BOOLEAN
HalpFatOpen(IN const CHAR *Path, OUT PULONG FileSize, OUT PULONG StartCluster)
{
    CHAR comps[FAT_MAX_PATH / 12][12];
    ULONG nComps;
    PFAT_VOLUME vol = &HalpFatVolume;
    ULONG start;

    if (!vol->Mounted) {
        return FALSE;
    }

    nComps = HalpFatParsePath(Path, comps, FAT_MAX_PATH / 12);
    if (nComps == 0) {
        return FALSE;
    }

    /* Determine starting directory. */
    if (vol->IsFat32) {
        start = vol->RootCluster;
    } else {
        start = vol->RootDirSector;
    }

    /* The last component is the file; all preceding are directories. */
    ULONG fileCluster = 0;
    ULONG fileSize = 0;

    if (nComps == 1) {
        /* File at root. */
        CHAR name83[12];
        HalpFatNameTo83(comps[0], name83);

        ULONG dirSector = vol->IsFat32 ? HalpFatClusterToSector(start) : start;
        ULONG sectorsToScan = vol->IsFat32 ? vol->SectorsPerCluster : 1;

        ULONG s;
        for (s = 0; s < sectorsToScan; s++) {
            FAT_DIR_ENTRY entry;
            if (HalpFatFindFileInSector(dirSector + s, name83, &entry) != 0xFFFFFFFFUL) {
                fileCluster = ((ULONG)entry.FirstClusterHigh << 16) | entry.FirstClusterLow;
                fileSize = entry.FileSize;
                break;
            }
        }
    } else {
        /* Navigate directories, then find the file. */
        ULONG dirCluster = start;
        if (!HalpFatOpenDirectory(comps, nComps - 1, start, &dirCluster)) {
            return FALSE;
        }

        CHAR name83[12];
        HalpFatNameTo83(comps[nComps - 1], name83);

        ULONG dirSector = vol->IsFat32 ? HalpFatClusterToSector(dirCluster) : dirCluster;
        ULONG sectorsToScan = vol->IsFat32 ? vol->SectorsPerCluster : 1;

        ULONG s;
        for (s = 0; s < sectorsToScan; s++) {
            FAT_DIR_ENTRY entry;
            if (HalpFatFindFileInSector(dirSector + s, name83, &entry) != 0xFFFFFFFFUL) {
                fileCluster = ((ULONG)entry.FirstClusterHigh << 16) | entry.FirstClusterLow;
                fileSize = entry.FileSize;
                break;
            }
        }
    }

    if (fileCluster == 0 && fileSize == 0) {
        /* Could be a zero-length file at cluster 0; treat as not found. */
        return FALSE;
    }

    *FileSize = fileSize;
    *StartCluster = fileCluster;
    return TRUE;
}

/*--------------------------------------------------------------------------
    HalpFatReadClusters - read file data following the cluster chain.
    
    startCluster: first cluster of the file
    byteOffset:   offset within the file to start reading
    byteCount:    number of bytes to read
    buffer:       destination buffer
    Returns:      number of bytes actually read.
--------------------------------------------------------------------------*/
ULONG
HalpFatReadClusters(IN ULONG StartCluster, IN ULONG ByteOffset,
                    IN ULONG ByteCount, OUT PVOID Buffer)
{
    PFAT_VOLUME vol = &HalpFatVolume;
    UCHAR *dst = (UCHAR *)Buffer;
    ULONG cluster = StartCluster;
    ULONG clusterOffset = ByteOffset % vol->ClusterSize;
    ULONG totalRead = 0;
    ULONG clusterSector;

    if (!vol->Mounted || ByteCount == 0) {
        return 0;
    }

    /* Advance to the cluster containing ByteOffset. */
    ULONG clustersToSkip = ByteOffset / vol->ClusterSize;
    while (clustersToSkip-- > 0) {
        cluster = HalpFatReadFatEntry(cluster);
        if (cluster == 0xFFFFFFFFUL) {
            break;
        }
    }

    while (totalRead < ByteCount && cluster != 0xFFFFFFFFUL) {
        clusterSector = HalpFatClusterToSector(cluster);
        ULONG remainingInCluster = vol->ClusterSize - clusterOffset;
        ULONG toRead = ByteCount - totalRead;
        if (toRead > remainingInCluster) {
            toRead = remainingInCluster;
        }

        /* Read sectors covering this portion of the cluster. */
        ULONG startSector = clusterSector + (clusterOffset / vol->BytesPerSector);

        /* We read sector-by-sector into the cache, then copy. */
        ULONG copied = 0;
        ULONG sec = startSector;
        ULONG secOffset = clusterOffset % vol->BytesPerSector;
        while (copied < toRead) {
            UCHAR sectorBuf[FAT_SECTOR_SIZE];
            if (!HalpFatReadSector(sec, sectorBuf)) {
                break;
            }
            ULONG avail = vol->BytesPerSector - secOffset;
            ULONG chunk = toRead - copied;
            if (chunk > avail) {
                chunk = avail;
            }
            memcpy(dst + copied, sectorBuf + secOffset, chunk);
            copied += chunk;
            sec++;
            secOffset = 0;
        }

        dst += copied;
        totalRead += copied;
        clusterOffset = 0;

        /* Advance to next cluster. */
        cluster = HalpFatReadFatEntry(cluster);
    }

    return totalRead;
}

/*--------------------------------------------------------------------------
    HalpFatRead - read bytes from an open file.
    
    startCluster: first cluster of the file
    fileSize:     total file size
    byteOffset:   offset within the file
    byteCount:    number of bytes to read
    buffer:       destination buffer
    Returns:      number of bytes actually read.
--------------------------------------------------------------------------*/
ULONG
HalpFatRead(IN ULONG StartCluster, IN ULONG FileSize,
            IN ULONG ByteOffset, IN ULONG ByteCount, OUT PVOID Buffer)
{
    if (ByteOffset >= FileSize) {
        return 0;
    }

    ULONG remaining = FileSize - ByteOffset;
    if (ByteCount > remaining) {
        ByteCount = remaining;
    }

    return HalpFatReadClusters(StartCluster, ByteOffset, ByteCount, Buffer);
}

/*--------------------------------------------------------------------------
    HalpFatGetFileSize - return file size (caller tracks it).
--------------------------------------------------------------------------*/
ULONG
HalpFatGetFileSize(IN ULONG FileSize)
{
    return FileSize;
}

/*--------------------------------------------------------------------------
    HalpFatClose - close an open file (no-op in this simple reader).
--------------------------------------------------------------------------*/
VOID
HalpFatClose(VOID)
{
    /* No state to release in the single-file reader. */
}

/*--------------------------------------------------------------------------
    HalpFatListDirectory - list entries in a directory.
    
    Path: null-terminated directory path (e.g. "\\nt" or "\\").
    Returns number of entries listed (printed via HalpDisplayPrintf).
--------------------------------------------------------------------------*/
ULONG
HalpFatListDirectory(IN const CHAR *Path)
{
    CHAR comps[FAT_MAX_PATH / 12][12];
    ULONG nComps;
    PFAT_VOLUME vol = &HalpFatVolume;
    ULONG start;
    ULONG dirCluster;
    ULONG dirSector;
    ULONG sectorsToScan;
    ULONG s;
    ULONG entriesListed = 0;

    if (!vol->Mounted) {
        return 0;
    }

    nComps = HalpFatParsePath(Path, comps, FAT_MAX_PATH / 12);
    if (nComps == 0) {
        /* Listing root directory. */
        dirCluster = vol->IsFat32 ? vol->RootCluster : 0;
        start = vol->RootCluster;
    } else {
        /* Navigate to the target directory first. */
        if (!HalpFatOpenDirectory(comps, nComps, vol->IsFat32 ? vol->RootCluster : vol->RootDirSector, &dirCluster)) {
            return 0;
        }
        start = dirCluster;
    }

    if (vol->IsFat32) {
        dirSector = HalpFatClusterToSector(start);
        sectorsToScan = vol->SectorsPerCluster;
    } else {
        /* FAT12/16: root directory is a fixed sector range. */
        dirSector = vol->RootDirSector;
        sectorsToScan = ((vol->RootDirEntries * 32) + (vol->BytesPerSector - 1)) / vol->BytesPerSector;
        if (nComps > 0) {
            /* Non-root subdirectory: dirCluster is the sector. */
            dirSector = dirCluster;
            sectorsToScan = 1;
        }
    }

    for (s = 0; s < sectorsToScan; s++) {
        UCHAR sector[FAT_SECTOR_SIZE];
        ULONG entriesPerSector = FAT_SECTOR_SIZE / sizeof(FAT_DIR_ENTRY);
        ULONG i;

        if (!HalpFatReadSector(dirSector + s, sector)) {
            break;
        }

        for (i = 0; i < entriesPerSector; i++) {
            const FAT_DIR_ENTRY *ent = (const FAT_DIR_ENTRY *)(sector + i * sizeof(FAT_DIR_ENTRY));
            CHAR name83[12];

            /* Free entry - stop scanning (entries contiguous in root). */
            if (ent->Name[0] == FAT_DIR_FREE) {
                break;
            }

            /* Deleted entry - skip. */
            if (ent->Name[0] == FAT_DIR_DELETED) {
                continue;
            }

            /* Volume label / long-name entry - skip. */
            if ((ent->Attributes & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
                continue;
            }

            /* Skip volume label (non-directory, no extension). */
            if ((ent->Attributes & FAT_ATTR_VOLUME_ID) == FAT_ATTR_VOLUME_ID) {
                continue;
            }

            HalpFatNameTo83(ent->Name, name83);

            HalpDisplayPrintf("%s%c  %10lu bytes\r\n",
                              name83,
                              (ent->Attributes & FAT_ATTR_DIRECTORY) ? '/' : ' ',
                              ent->FileSize);
            entriesListed++;
        }
    }

    return entriesListed;
}
