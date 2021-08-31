/**************************************************************
 * Class:  CSC-415-01
 * Name: Rupak Khatri
 * Student ID:920605878
 * Project: File System Project
 *
 * File: fsLowDriver.c
 *
 * Description: This program is used to create a volume for the
 *              Test File System. It holds the free-space bit vector
 *		and the inodes on disk.
 *
 **************************************************************/

#include "fsLowDriver.h"

int initialized = 0; //variable to check if the VCB is initialized

//Variables that hold the information of our volume control block
char header[16] = "*****TestFS****";
uint64_t volumeSize;
uint64_t blockSize;
uint64_t diskSizeBlocks;
uint32_t vcbStartBlock;
uint32_t totalVCBBlocks;
uint32_t inodeStartBlock;
uint32_t totalInodes;
uint32_t totalInodeBlocks;
uint32_t freeMapSize;

fs_VCB * openVCB_p; //pointer to our volume control block

/* Methods explained in the fsLowDriver.h file */

uint64_t ceilDiv(uint64_t a, uint64_t b) {
    /* Rounds up integer division. */
    return (a + b - 1) / b;
}

int allocateVCB(fs_VCB ** vcb_p) {
    * vcb_p = calloc(totalVCBBlocks, blockSize);
    return totalVCBBlocks;
}

uint64_t fsRead(void * buf, uint64_t blockCount, uint64_t blockPosition) {
    if (!initialized) { //make sure it's initialized or else we reach segmentation faults
        printf("fsRead: System not initialized.\n");
        return 0;
    }
    if (blockPosition + blockCount > openVCB_p -> diskSizeBlocks) {
        printf("fsRead: Invalid block range.\n");
        return 0;
    }
    LBAread(buf, blockCount, blockPosition);
    return blockCount;
}

uint64_t fsWrite(void * buf, uint64_t blockCount, uint64_t blockPosition) {
    if (!initialized) {
        printf("fsWrite: System not initialized.\n");
        return 0;
    }
    if (blockPosition + blockCount > openVCB_p -> diskSizeBlocks) {
        printf("fsWrite: Invalid block range.\n");
        return 0;
    }
    LBAwrite(buf, blockCount, blockPosition);
    for (int i = 0; i < blockCount; i++) {
        setBit(openVCB_p -> freeMap, blockPosition + i);
    }
    writeVCB();
    return blockCount;
}

void fsFree(void * buf, uint64_t blockCount, uint64_t blockPosition) {
    if (!initialized) {
        printf("fsFree: System not initialized.\n");
        return;
    }
    if (blockPosition + blockCount > openVCB_p -> diskSizeBlocks) {
        printf("fsFree: Invalid block range.\n");
        return;
    }
    for (int i = 0; i < blockCount; i++) {
        clearBit(openVCB_p -> freeMap, blockPosition + i);
    }
    writeVCB();
}

uint64_t getFreeBlock() {
    for (int index = 0; index < diskSizeBlocks; index++) {
        if (findBit(openVCB_p -> freeMap, index) == 0) {

            return index; //The position in the VolumeSpaceArray
        }

    }

    return -1;
}

uint64_t readVCB() {
    if (!initialized) {
        printf("readVCB: System not initialized.\n");
        return 0;
    }

    /* Read VCB from disk to openVCB_p */
    uint64_t blocksRead = LBAread(openVCB_p, totalVCBBlocks, VCB_START_BLOCK);
    printf("Read VCB in %d blocks starting at block %d.\n", totalVCBBlocks, VCB_START_BLOCK);
    return blocksRead;
}

uint64_t writeVCB() {
    if (!initialized) {
        printf("writeVCB: System not initialized.\n");
        return 0;
    }

    /* Write openVCB_p to disk. */
    uint64_t blocksWritten = LBAwrite(openVCB_p, totalVCBBlocks, VCB_START_BLOCK);
    printf("Wrote VCB in %d blocks starting at block %d.\n", totalVCBBlocks, VCB_START_BLOCK);
    return blocksWritten;
}

fs_VCB * getVCB() {
    return openVCB_p;
}

void initializeVCB() {
    if (!initialized) {
        printf("initializeVCB: System not initialized.\n");
        return;
    }
    printf("---Initializing VCB---\n");

    sprintf(openVCB_p -> header, "%s", header);

    /* Set information on volume sizes and block locations. */
    openVCB_p -> volumeSize = volumeSize;
    openVCB_p -> blockSize = blockSize;
    openVCB_p -> diskSizeBlocks = diskSizeBlocks;
    openVCB_p -> vcbStartBlock = VCB_START_BLOCK;
    openVCB_p -> totalVCBBlocks = totalVCBBlocks;
    openVCB_p -> inodeStartBlock = inodeStartBlock;
    openVCB_p -> totalInodes = totalInodes;
    openVCB_p -> totalInodeBlocks = totalInodeBlocks;
    printf("initVCB: totalInodeBlocks %ld", openVCB_p -> totalInodeBlocks);

    openVCB_p -> freeMapSize = freeMapSize;

    /* Initialize freeBlockMap to all 0's. */
    for (int i = 0; i < freeMapSize; i++) {
        openVCB_p -> freeMap[i] = 0;
    }

    /* Set bits in freeMap for VCB and inodes. */
    for (int i = 0; i < inodeStartBlock + totalInodeBlocks; i++) {
        setBit(openVCB_p -> freeMap, i);
    }

    printVCB();
    writeVCB();
}

void initializeInodes() {
    if (!initialized) {
        printf("initializeInodes: System not initialized.\n");
        return;
    }

    printf("---Initializing inodes---\n");

    printf("Total disk blocks: %ld, total inodes: %d, total inode blocks: %d\n", diskSizeBlocks, totalInodes, totalInodeBlocks);

    /* Allocate and initialize inodes. First inode is root directory and has id=1. */
    fdDir * inodes = calloc(totalInodeBlocks, blockSize);
    inodes[0].id = 0;
    inodes[0].inUse = 1;
    inodes[0].type = I_DIR;
    strcpy(inodes[0].name, "root");
    strcpy(inodes[0].path, "/root");
    inodes[0].lastAccessTime = time(0);
    inodes[0].lastModificationTime = time(0);
    inodes[0].numDirectBlockPointers = 0;

    /* Do all other inodes. */
    for (int i = 1; i < totalInodes; i++) {
        inodes[i].id = i;
        inodes[i].inUse = 0;
        inodes[i].type = I_UNUSED;
        strcpy(inodes[i].parent, "");
        strcpy(inodes[i].name, "");
        inodes[i].lastAccessTime = 0;
        inodes[i].lastModificationTime = 0;

        /* Set all direct block pointers to -1 (invalid). */
        for (int j = 0; j < MAX_DATABLOCK_POINTERS; j++) {
            inodes[i].directBlockPointers[j] = INVALID_DATABLOCK_POINTER;
        }
        inodes[i].numDirectBlockPointers = 0;

    }

    /* Write inodes to disk. */
    char * char_p = (char * ) inodes;
    LBAwrite(char_p, totalInodeBlocks, inodeStartBlock);
    printf("Wrote %d inodes of size %ld bytes each starting at block %d.\n", totalInodes, sizeof(fdDir), inodeStartBlock);
    free(inodes);
}

void printVCB() {
    int size = openVCB_p -> totalVCBBlocks * (openVCB_p -> blockSize);
    int width = 16;
    char * char_p = (char * ) openVCB_p;
    char ascii[width + 1];
    sprintf(ascii, "%s", "................");
    //printf("--------------------------Printing VCB--------------------------\n");
    for (int i = 0; i < size; i++) {
        //printf("%02x ", char_p[i] & 0xff);
        if (char_p[i]) {
            ascii[i % width] = char_p[i];
        }
        if ((i + 1) % width == 0 && i > 0) {
            ascii[i % width + 1] = '\0';
            //printf("%s\n", ascii);
            sprintf(ascii, "%s", "................");
        } else if (i == size - 1) {
            for (int j = 0; j < width - (i % (width - 1)); j++) {
                printf("   ");
            }
            ascii[i % width + 1] = '\0';
            //printf("%s\n", ascii);
            sprintf(ascii, "%s", "................");
        }
    }
    printf("VCB Size: %d bytes\n", size);
}

void init(uint64_t _volumeSize, uint64_t _blockSize) {
    printf("-Init-\n");
    printf("volumeSize: %ld\n", volumeSize = _volumeSize);
    printf("blockSize: %ld\n", blockSize = _blockSize);
    printf("diskSizeBlocks: %ld\n", diskSizeBlocks = ceilDiv(volumeSize, blockSize));
    printf("freeMapSize: %d\n", freeMapSize = diskSizeBlocks <= sizeof(uint32_t) * 8 ? 1 : diskSizeBlocks / sizeof(uint32_t) / 8);
    printf("totalVCBBlocks: %d\n", totalVCBBlocks = ceilDiv(sizeof(fs_VCB) + sizeof(uint32_t[freeMapSize]), blockSize));
    printf("inodeStartBlock: %d\n", inodeStartBlock = VCB_START_BLOCK + totalVCBBlocks);
    printf("totalInodes: %d\n", totalInodes = (diskSizeBlocks - inodeStartBlock) / (DATA_BLOCKS_PER_INODE + ceilDiv(sizeof(fdDir), blockSize)));
    printf("totalInodeBlocks: %d\n", totalInodeBlocks = ceilDiv(totalInodes * sizeof(fdDir), blockSize));
    printf("inodeSizeBytes: %ld\n", sizeof(fdDir));
    printf("inodeSizeBlocks: %ld\n", ceilDiv(sizeof(fdDir), blockSize));

    /* Allocate the VCB in memory. */
    int vcbSize = allocateVCB( & openVCB_p);
    printf("VCB allocated in %d blocks.\n", vcbSize);

    initialized = 1;
    printf("-End Init-\n");
}

int createVolume(char * volumeName, uint64_t _volumeSize, uint64_t _blockSize) {
    printf("-Creating Volume-\n");
    /* Check whether volume exists already. */
    if (access(volumeName, F_OK) != -1) {
        printf("Cannot create volume '%s'. Volume already exists.\n", volumeName);
        return -3;
    }

    uint64_t existingVolumeSize = _volumeSize;
    uint64_t existingBlockSize = _blockSize;

    /* Initialize the volume with the fsLow library. */
    int retVal = startPartitionSystem(volumeName, & existingVolumeSize, & existingBlockSize);

    printf("Opened %s, Volume Size: %llu;  BlockSize: %llu; Return %d\n", volumeName, (ull_t) existingVolumeSize, (ull_t) existingBlockSize, retVal);

    /* Format the disk if the volume was opened properly. */
    if (!retVal) {
        init(_volumeSize, _blockSize);
        initializeVCB();
        initializeInodes();
    }

    closeVolume();
    return retVal;
}

void openVolume(char * volumeName) {
    printf("-Opening Volume-\n");
    if (!initialized) {
        uint64_t existingVolumeSize;
        uint64_t existingBlockSize;

        int retVal = startPartitionSystem(volumeName, & existingVolumeSize, & existingBlockSize);
        if (!retVal) {
            init(existingVolumeSize, existingBlockSize);
            readVCB();

            printVCB();
        }
    } else {
        printf("Can't open volume '%s'. Another volume is already open.\n", volumeName);
    }
}

void closeVolume() {
    printf("-Closing Volume-\n");
    if (initialized) {
        closePartitionSystem();
        free(openVCB_p);
        initialized = 0;
    } else {
        printf("Can't close volume. Volume not open.\n");
    }
}
