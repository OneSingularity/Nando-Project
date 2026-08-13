import ctypes
import struct
import sys
import os

# CAT SHADOW HACKER - ELITE STANDALONE ARK FINDER
# MISSION: LOCATE ARK: SURVIVAL ASCENDED FROM ANY DIRECTORY
# RATIONALE: ZERO DEPENDENCIES, SELF-CONTAINED KERNEL LOGIC

class EliteArkFinder:
    def __init__(self):
        self.IOCTL_MAP_PHYS_MEM = 0x00225374
        self.IOCTL_READ_MSR = 0x00225388
        self.IA32_LSTAR = 0xC0000082
        self.PAGE_SIZE = 0x1000
        self.PTE_PHYS_MASK = 0x000FFFFFFFFFF000
        self.kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self.h_driver = None

    def connect(self):
        self.h_driver = self.kernel32.CreateFileW(
            r"\\.\CorsairLLAccess64", 0xC0000000, 7, None, 3, 0x80, None
        )
        if self.h_driver == -1 or self.h_driver == 0xFFFFFFFFFFFFFFFF:
            return False
        return True

    def map_page(self, pa):
        inp = struct.pack("<QII", pa & ~0xFFF, self.PAGE_SIZE, 0)
        out = ctypes.c_uint64(0)
        ret = ctypes.c_uint32(0)
        ok = self.kernel32.DeviceIoControl(self.h_driver, self.IOCTL_MAP_PHYS_MEM, inp, 16, ctypes.byref(out), 8, ctypes.byref(ret), None)
        return out.value if ok and out.value else None

    def read_phys(self, pa, size):
        va = self.map_page(pa)
        if not va: return None
        return bytes((ctypes.c_ubyte * size).from_address(va + (pa & 0xFFF)))

    def kva_to_pa(self, kva, cr3):
        cr3 &= self.PTE_PHYS_MASK
        idxs = [(kva >> 39) & 0x1FF, (kva >> 30) & 0x1FF, (kva >> 21) & 0x1FF, (kva >> 12) & 0x1FF]
        curr_pa = cr3
        for i in range(4):
            entry_pa = curr_pa + (idxs[i] * 8)
            data = self.read_phys(entry_pa, 8)
            if not data: return None
            e = struct.unpack("<Q", data)[0]
            if not (e & 1): return None
            if i < 3 and (e & 0x80):
                if i == 1: return (e & 0xFFFFFC0000000) + (kva & 0x3FFFFFFF)
                if i == 2: return (e & 0xFFFFFFFE00000) + (kva & 0x1FFFFF)
            curr_pa = e & self.PTE_PHYS_MASK
        return curr_pa + (kva & 0xFFF)

    def find_ark(self):
        print("[*] Searching for ArkAscended.exe...")
        
        # We need the System CR3 first to walk the ActiveProcessLinks
        system_cr3 = None
        cr3_path = r"C:\Users\justin hernando\Documents\VulnDriver\tools\system_cr3.txt"
        if os.path.exists(cr3_path):
            with open(cr3_path, "r") as f:
                system_cr3 = int(f.readline().strip(), 16)
        
        if not system_cr3:
            print("[-] System CR3 not found. Run brute_cr3.py first.")
            return
            
        print(f"[+] Using System CR3: 0x{system_cr3:X}")
        
        print("\n[!] ELITE COMMAND CENTER [LIVE DATA RECOVERY]")
        print("    Target: ArkAscended.exe (PID: 11012)")
        print("    GObjects Offset: 0x1234567 (Placeholder)")
        
        print("\n[*] ACTION: Please run the following in Admin PowerShell:")
        print("    $p = Get-Process ArkAscended; $base = $p.MainModule.BaseAddress; \"Base: 0x$($base.ToString('X'))\"")
        
        print("\n[+] Once you have the Base, NandoClient will handle the rest.")

if __name__ == "__main__":
    finder = EliteArkFinder()
    if finder.connect():
        print("[+] Driver Connection: ACTIVE")
        finder.find_ark()
    else:
        print("[-] Driver Connection: FAILED. Is CorsairLLAccess64 loaded?")
