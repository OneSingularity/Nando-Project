#pragma once
#include <ntddk.h>

// Communication Structure
typedef struct _NANDO_COMMAND {
    unsigned long long Magic;       // 0x4E414E444F (NANDO)
    unsigned int CommandID;         // 1: Read, 2: Write, 3: Unload
    unsigned long long Address;     // Virtual or Physical Address
    unsigned int Size;              // Size of R/W
    unsigned long long Buffer;      // Pointer to usermode buffer (physical or mapped)
    int Status;                     // Result code
} NANDO_COMMAND, *PNANDO_COMMAND;

#define NANDO_MAGIC 0x4E414E444F

// Protocol IDs
#define CMD_READ  1
#define CMD_WRITE 2
#define CMD_PING  3

// Section markers for shellcode generation
#pragma code_seg(".text")
#define NANDO_CODE
