/**************************************************************
 * Class:  CSC-415-01
 * Name: Rupak Khatri
 * Student ID: 920605878,
 * Project: File System Project
 *
 * File: b_io.c
 *
 * Description: Interface of basic I/O functions
 *
 **************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "b_io.h"
#include "mfs.h"

#define MAXFCBS 20

/* For runtime in b_init from the value of getVCB () -> blockSize. */
uint64_t bufSize;

typedef enum {
    NoWRITE,
    WRITE
}
fileMode;

typedef struct b_fcb {
    int linuxFd; 
    char * buf; 
    int index; //holds the current position in the buffer
    int blockIndex; //holds the index of the block in the FCB
    int buflen; //holds how many valid bytes are in the buffer
    fileMode mode; // sets when file if open for write 
    fdDir * inode; //holdsa pointer to the inode associated with the file 
    int eof;

}
b_fcb;

b_fcb fcbArray[MAXFCBS]; 

int startup = 0; //Indicates that this has not been initialized

//Method to initialize our file system
void b_init() {

    /* Set buffer size equal to the block size of our file system. */
    bufSize = getVCB() -> blockSize;

    //init fcbArray to being all free
    for (int i = 0; i < MAXFCBS; i++) { //both indicating a free fcbArray
        fcbArray[i].linuxFd = -1;
        fcbArray[i].mode = NoWRITE;
    }

    startup = 1;
}

//for free element/file in the fcbArray
int b_getFCB() {
    for (int i = 0; i < MAXFCBS; i++) {
        if (fcbArray[i].linuxFd == -1) {
            fcbArray[i].linuxFd = -2; // used but not assigned
            return i; //Not thread safe
        }
    }
    return (-1); //all in use
}

int b_open(char * filename, int flags) {
    int fd;
    int returnFd;

    printf("b_open\n");

    if (startup == 0) b_init(); //Initialize our system

    //Should have a mutex here
    returnFd = b_getFCB(); // get our own file descriptor

    /* Return -1 if there was not. */
    b_fcb * fcb;
    if (returnFd < 0) {
        return -1;
    }

    fcb = & fcbArray[returnFd];
    fdDir * inode = getInode(filename);
    if (!inode) {

        printf("b_open: %s does not yet exist.\n", filename);

        /* CMD_CP2FS */
        /* Check for create mode. */
        if (flags & O_CREAT) {

            printf("Creating %s\n", filename);

            /* Create child, get parent and set parent. */
            inode = createInode(I_FILE, filename);
            char parentpath[MAX_FILENAME_SIZE];
            getParentPath(parentpath, filename);
            fdDir * parent = getInode(parentpath);
            setParent(parent, inode);
            writeInodes();

        } else {
            return -1;
        }
    }

    /* Save the inode in the FCB. */
    fcb -> inode = inode;
    fcb -> buf = malloc(bufSize + 1);
    if (fcb -> buf == NULL) {
        
        close(fd); // close linux file
        fcb -> linuxFd = -1; //Free FCB
        return -1;
    }

    fcb -> buflen = 0; // Not read 
    fcb -> index = 0; // Not read 

    printf("b_open: Opened file '%s' with fd %d\n", filename, fd);

    return (returnFd); // all set
}

// Interface to write a buffer	
int b_write(int fd, char * buffer, int count) {
    if (startup == 0) {
        b_init(); //Initialize our system
    }

    // check that fd is between 0 and (MAXFCBS - 1)
    if ((fd < 0) || (fd >= MAXFCBS)) {
        return (-1); //invalid file descriptor
    }

    if (fcbArray[fd].linuxFd == -1) //File don't open 
    {
        return -1;
    }

    

    b_fcb * fcb = & fcbArray[fd];
    int freeSpace = bufSize - fcb -> index;

    printf("b_write: count=%d, fcb->index=%d, freeSpace=%d\n", count, fcb -> index, freeSpace);

    fcb -> mode = WRITE; // set the file to write the last bytes in b_close
    int copyLength = freeSpace > count ? count : freeSpace;
    int secondCopyLength = count - copyLength;

    printf("Copying first segment to fcb->buf+%d: %d bytes\n", fcb -> index, copyLength);
    memcpy(fcb -> buf + fcb -> index, buffer, copyLength);
    fcb -> index += copyLength;
    fcb -> index %= bufSize;

    

    if (secondCopyLength != 0) {
        printf("Writing buffer to fd.\n");
        uint64_t indexOfBlock = getFreeBlock();

        //write(fcbArray[fd].linuxFd, fcb->buf, BUFSIZE);

        if (indexOfBlock == -1) {
            printf("There is not enough free space!");
            return 0;
        } else {

            /* Write block of data to disk. */
            printf("\n\nFCB buff:\n\n%s", fcb -> buf);
            writeBufferToInode(fcb -> inode, fcb -> buf, copyLength + secondCopyLength, indexOfBlock);
        }
        fcb -> index = 0;
        printf("Copying second segment to fcb->buf+%d: %d bytes\n", fcb -> index, secondCopyLength);
        memcpy(fcb -> buf + fcb -> index, buffer + copyLength, secondCopyLength);
        fcb -> index += secondCopyLength;
        fcb -> index %= bufSize;
    }

    // Copy above in b_close
    // Return total bytes copied to buffer.
    return copyLength + secondCopyLength;
}

int b_read(int fd, char * buffer, int count) {
    struct b_fcb * fcb = & fcbArray[fd];
    //fcb->mode = NoWRITE;
    int bytesRemaining = fcb -> buflen - fcb -> index;

    printf("b_read: index = %d\n", fcb -> index);
    printf("b_read: buflen = %d\n", fcb -> buflen);

    if (bytesRemaining > count) {

        printf("Existing: Copying to %ld from %ld for %ld bytes.\n", buffer, fcb -> buf + fcb -> index, count);
        memcpy(buffer, fcb -> buf + fcb -> index, count);
        fcb -> index += count;
        return count;
    } else {
        /* Copy tail-end of existing buffer */
        printf("Tail: Copying to %ld from %ld for %ld bytes.\n", buffer, fcb -> buf + fcb -> index, bytesRemaining);
        memcpy(buffer, fcb -> buf + fcb -> index, bytesRemaining);

        if (fcb -> eof) {
            printf("EOF reached. Exiting.\n");
            fcb -> index += bytesRemaining;
            return bytesRemaining;
        }

        /* Check that the inode has a direct block pointer to read from. */
        if (fcb -> blockIndex > fcb -> inode -> numDirectBlockPointers - 1) {
            printf("Block Index out-of-bounds.\n");
            return 0;
        }

        int blockNumber = fcb -> inode -> directBlockPointers[fcb -> blockIndex];
        LBAread(fcb -> buf, 1, blockNumber);
        //read(fcb->linuxFileDescriptor, fcb->buffer, FILE_BUFFER_SIZE);
        fcb -> blockIndex++;

        printf("Read new data.\n");
        
        printf("%s\n", fcb -> buf);
        
        fcb -> index = 0;
        //fcb->readMarker = fcb->buffer;
        int newBufferSize = fcb -> buflen = strlen(fcb -> buf);
        printf("%d bytes read.\n", newBufferSize);

        /* Set end of file if # bytes read is less than the bufferSize. */
        if (newBufferSize < bufSize) {
            /* End of file read to buffer*/
            fcb -> eof = 1;
        }
        int remainderOfCount = count - bytesRemaining;
        int secondSegmentCount = newBufferSize > remainderOfCount ? remainderOfCount : newBufferSize;

        printf("Second: Copying to %ld from %ld for %ld bytes.\n", buffer + bytesRemaining, fcb -> buf + fcb -> index, secondSegmentCount);
        memcpy(buffer + bytesRemaining, fcb -> buf + fcb -> index, secondSegmentCount);

        fcb -> index += secondSegmentCount;
        return bytesRemaining + secondSegmentCount;
    }
}

// Interface to Close the file	
void b_close(int fd) {
    b_fcb * fcb = & fcbArray[fd];
    printf("Closing file %d.\n", fd);

    //write(fcb->linuxFd, fcb->buf, fcb->index);	

    if (fcb -> mode == WRITE && fcb -> index > 0) { 
        printf("File was in write mode.\n");
        uint64_t indexOfBlock = getFreeBlock();
        if (indexOfBlock == -1) {
            printf("There is not enough free space!");
            return;
        } else {
            /* Write any remaining bytes (index) to a new block. */
            printf("Writing remaining bytes.\n");
            writeBufferToInode(fcb -> inode, fcb -> buf, fcb -> index, indexOfBlock);
        }

    }

    close(fcb -> linuxFd); // close the linux file handle
    free(fcb -> buf); // free the associated buffer
    fcb -> buf = NULL; // Safety First
    fcb -> linuxFd = -1; 
}
