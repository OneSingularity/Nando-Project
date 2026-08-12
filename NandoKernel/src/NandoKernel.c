#include "Common.h"

// Shared command buffer
PNANDO_COMMAND g_CommandBuffer = (PNANDO_COMMAND)0xFFFFF80580E42F00;

NANDO_CODE NTSTATUS HandleCommand(PNANDO_COMMAND Cmd) {
    if (Cmd->Magic != NANDO_MAGIC) return STATUS_INVALID_PARAMETER;

    switch (Cmd->CommandID) {
        case CMD_PING:
            Cmd->Status = 1337; // Pong
            break;
            
        case CMD_READ:
            // TODO: Physical Read
            Cmd->Status = 0;
            break;

        default:
            Cmd->Status = -1;
            break;
    }

    // Reset Magic to signal completion
    Cmd->Magic = 0;
    return STATUS_SUCCESS;
}

// Hook handler
NANDO_CODE void KernelHeartbeat() {
    if (g_CommandBuffer && g_CommandBuffer->Magic == NANDO_MAGIC) {
        HandleCommand(g_CommandBuffer);
    }
}

// Minimal Entry for compiler (not used in shellcode)
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);
    return STATUS_SUCCESS;
}

// The actual entry called by the hijack
// fffff805`810d7190 (DxgkSubmitCommand) will jump here.
NANDO_CODE void HijackHandler() {
    // x64 Register Save Preamble (Manual Bytecode)
    // 50 51 52 53 55 56 57 41 50 41 51 41 52 41 53 41 54 41 55 41 56 41 57
    // push rax, rcx, rdx, rbx, rbp, rsi, rdi, r8-r15
    
    // Call HandleCommand if Magic matches
    if (g_CommandBuffer && g_CommandBuffer->Magic == NANDO_MAGIC) {
        HandleCommand(g_CommandBuffer);
    }
    
    // Restore and Execute original preamble...
}
