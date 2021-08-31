/**************************************************************
 * Class:  CSC-415-01
 * Name: Rupak Khatri
 * Student ID:920605878 
 * Project: File System Project
 *
 * File: freeBit.c
 *
 * Description: This file defines the methods that change our free-space bit vector.
 *
 **************************************************************/

#include "freeBit.h"

void setBit(int A[], int k) {
    A[k / 32] |= 1 << (k % 32);
}

void clearBit(int A[], int k) {
    A[k / 32] &= ~(1 << (k % 32));
}

int findBit(int A[], int k) {
    return ((A[k / 32] & (1 << (k % 32))) != 0);
}

