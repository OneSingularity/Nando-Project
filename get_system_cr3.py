"""
Get System CR3 via NtSystemDebugControl(SysDbgReadVirtual).
Requires: bcdedit /debug on (already set), Admin + SeDebugPrivilege.
No physical-memory driver scans. No WinDbg attach.
"""

import ctypes
import ctypes.wintypes
import struct
import sys

SystemModuleInformation = 11
SysDbgReadVirtual = 8
SE_DEBUG_NAME = "SeDebugPrivilege"
TOKEN_ADJUST_PRIVILEGES = 0x20
TOKEN_QUERY = 0x8
SE_PRIVILEGE_ENABLED = 0x2

OFF_DIR_TABLE_BASE = 0x28  # KPROCESS.DirectoryTableBase

ntdll = ctypes.WinDLL("ntdll", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)

NTSTATUS = ctypes.wintypes.LONG
HANDLE = ctypes.c_void_p
DWORD = ctypes.wintypes.DWORD
BOOL = ctypes.wintypes.BOOL
LPVOID = ctypes.c_void_p
ULONG_PTR = ctypes.c_size_t


class LUID(ctypes.Structure):
    _fields_ = [("LowPart", DWORD), ("HighPart", ctypes.wintypes.LONG)]


class LUID_AND_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("Luid", LUID), ("Attributes", DWORD)]


class TOKEN_PRIVILEGES(ctypes.Structure):
    _fields_ = [("PrivilegeCount", DWORD), ("Privileges", LUID_AND_ATTRIBUTES * 1)]


class SYSDBG_VIRTUAL(ctypes.Structure):
    _fields_ = [
        ("Address", LPVOID),
        ("Buffer", LPVOID),
        ("Request", DWORD),
    ]


ntdll.NtQuerySystemInformation.restype = NTSTATUS
ntdll.NtQuerySystemInformation.argtypes = [DWORD, LPVOID, ULONG_PTR, LPVOID]
ntdll.NtSystemDebugControl.restype = NTSTATUS
ntdll.NtSystemDebugControl.argtypes = [
    DWORD, LPVOID, DWORD, LPVOID, DWORD, ctypes.POINTER(DWORD)
]

kernel32.GetCurrentProcess.restype = HANDLE
kernel32.GetCurrentProcess.argtypes = []
advapi32.OpenProcessToken.restype = BOOL
advapi32.OpenProcessToken.argtypes = [HANDLE, DWORD, ctypes.POINTER(HANDLE)]
advapi32.LookupPrivilegeValueW.restype = BOOL
advapi32.LookupPrivilegeValueW.argtypes = [
    ctypes.wintypes.LPCWSTR, ctypes.wintypes.LPCWSTR, ctypes.POINTER(LUID)
]
advapi32.AdjustTokenPrivileges.restype = BOOL
advapi32.AdjustTokenPrivileges.argtypes = [
    HANDLE, BOOL, ctypes.POINTER(TOKEN_PRIVILEGES), DWORD, LPVOID, LPVOID
]


def enable_debug_privilege():
    tok = HANDLE()
    proc = kernel32.GetCurrentProcess()
    ctypes.set_last_error(0)
    if not advapi32.OpenProcessToken(
        proc, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, ctypes.byref(tok)
    ):
        print(f"[-] OpenProcessToken failed: {ctypes.get_last_error()}")
        return False
    luid = LUID()
    if not advapi32.LookupPrivilegeValueW(None, SE_DEBUG_NAME, ctypes.byref(luid)):
        print(f"[-] LookupPrivilegeValue failed: {ctypes.get_last_error()}")
        return False
    tp = TOKEN_PRIVILEGES()
    tp.PrivilegeCount = 1
    tp.Privileges[0].Luid = luid
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED
    ctypes.set_last_error(0)
    if not advapi32.AdjustTokenPrivileges(tok, False, ctypes.byref(tp), 0, None, None):
        print(f"[-] AdjustTokenPrivileges failed: {ctypes.get_last_error()}")
        return False
    err = ctypes.get_last_error()
    if err == 1300:  # ERROR_NOT_ALL_ASSIGNED
        print("[-] SeDebugPrivilege not assigned (need elevated Admin).")
        return False
    print("[+] SeDebugPrivilege enabled")
    return True


def sysdbg_read(address, size):
    buf = ctypes.create_string_buffer(size)
    req = SYSDBG_VIRTUAL()
    req.Address = ctypes.c_void_p(address)
    req.Buffer = ctypes.cast(buf, LPVOID)
    req.Request = size
    retlen = DWORD(0)
    status = ntdll.NtSystemDebugControl(
        SysDbgReadVirtual,
        ctypes.byref(req),
        ctypes.sizeof(req),
        None,
        0,
        ctypes.byref(retlen),
    )
    status &= 0xFFFFFFFF
    if status != 0:
        return None, status
    return buf.raw[:size], 0


def get_ntoskrnl_base():
    size = DWORD(0)
    ntdll.NtQuerySystemInformation(SystemModuleInformation, None, 0, ctypes.byref(size))
    buf = ctypes.create_string_buffer(size.value + 0x1000)
    status = ntdll.NtQuerySystemInformation(
        SystemModuleInformation, buf, len(buf), ctypes.byref(size)
    )
    if status != 0:
        print(f"[-] SystemModuleInformation: 0x{status & 0xFFFFFFFF:X}")
        return None
    data = buf.raw[: size.value]
    for name in (b"ntoskrnl.exe", b"ntkrnlmp.exe"):
        idx = data.lower().find(name)
        if idx < 0:
            continue
        for entry_off in range(max(0, idx - 0x28 - 255), idx - 0x27):
            image_base = struct.unpack_from("<Q", data, entry_off + 0x10)[0]
            image_size = struct.unpack_from("<I", data, entry_off + 0x18)[0]
            if image_base >= 0xFFFF800000000000 and 0x100000 <= image_size <= 0x2000000:
                path = data[entry_off + 0x28: entry_off + 0x28 + 256]
                if name in path.lower():
                    return image_base
    return None


def pe_export_rva(path, export_name):
    with open(path, "rb") as f:
        pe = f.read()
    if pe[:2] != b"MZ":
        return None
    e_lfanew = struct.unpack_from("<I", pe, 0x3C)[0]
    # PE32+ optional header
    magic = struct.unpack_from("<H", pe, e_lfanew + 24)[0]
    if magic != 0x20B:
        print(f"[-] Unexpected optional magic 0x{magic:X}")
        return None
    export_rva = struct.unpack_from("<I", pe, e_lfanew + 24 + 112)[0]
    export_size = struct.unpack_from("<I", pe, e_lfanew + 24 + 116)[0]
    if not export_rva:
        return None

    def rva_to_off(rva):
        coff = e_lfanew + 4
        num_sections = struct.unpack_from("<H", pe, coff + 2)[0]
        size_opt = struct.unpack_from("<H", pe, coff + 16)[0]
        sec = coff + 20 + size_opt
        for i in range(num_sections):
            off = sec + i * 40
            va = struct.unpack_from("<I", pe, off + 12)[0]
            vsz = struct.unpack_from("<I", pe, off + 8)[0]
            raw = struct.unpack_from("<I", pe, off + 20)[0]
            rsz = struct.unpack_from("<I", pe, off + 16)[0]
            if va <= rva < va + max(vsz, rsz):
                return raw + (rva - va)
        return None

    exp_off = rva_to_off(export_rva)
    if exp_off is None:
        return None
    num_names = struct.unpack_from("<I", pe, exp_off + 24)[0]
    addr_of_funcs = struct.unpack_from("<I", pe, exp_off + 28)[0]
    addr_of_names = struct.unpack_from("<I", pe, exp_off + 32)[0]
    addr_of_ords = struct.unpack_from("<I", pe, exp_off + 36)[0]
    names_off = rva_to_off(addr_of_names)
    funcs_off = rva_to_off(addr_of_funcs)
    ords_off = rva_to_off(addr_of_ords)
    target = export_name.encode("ascii") + b"\x00"
    for i in range(num_names):
        name_rva = struct.unpack_from("<I", pe, names_off + i * 4)[0]
        name_off = rva_to_off(name_rva)
        if name_off is None:
            continue
        end = pe.index(b"\x00", name_off) + 1
        if pe[name_off:end] == target:
            ordinal = struct.unpack_from("<H", pe, ords_off + i * 2)[0]
            func_rva = struct.unpack_from("<I", pe, funcs_off + ordinal * 4)[0]
            return func_rva
    return None


def main():
    if sys.platform != "win32":
        print("Windows only")
        return 1

    print("=== System CR3 via NtSystemDebugControl ===")
    print("Needs: debug=Yes boot + Admin (SeDebugPrivilege)\n")

    if not enable_debug_privilege():
        return 1

    # Sanity: can we read anything?
    ntos = get_ntoskrnl_base()
    if not ntos:
        print("[-] ntoskrnl base not found")
        return 1
    print(f"[+] ntoskrnl base: 0x{ntos:X}")

    # Probe read MZ header via SysDbgReadVirtual
    data, st = sysdbg_read(ntos, 2)
    if st != 0:
        print(f"[-] SysDbgReadVirtual failed: 0x{st:X}")
        print("    Is bcdedit /debug on + reboot done? Running as Admin?")
        return 1
    if data != b"MZ":
        print(f"[-] Unexpected bytes at ntoskrnl: {data.hex()}")
        return 1
    print("[+] SysDbgReadVirtual works (read MZ)")

    rva = pe_export_rva(r"C:\Windows\System32\ntoskrnl.exe", "PsInitialSystemProcess")
    if rva is None:
        print("[-] PsInitialSystemProcess export not found in on-disk ntoskrnl")
        return 1
    print(f"[+] PsInitialSystemProcess RVA: 0x{rva:X}")

    ptr_addr = ntos + rva
    raw, st = sysdbg_read(ptr_addr, 8)
    if st != 0:
        print(f"[-] Failed reading PsInitialSystemProcess ptr: 0x{st:X}")
        return 1
    eprocess = struct.unpack("<Q", raw)[0]
    print(f"[+] System EPROCESS: 0x{eprocess:X}")

    raw, st = sysdbg_read(eprocess + OFF_DIR_TABLE_BASE, 8)
    if st != 0:
        print(f"[-] Failed reading DirectoryTableBase: 0x{st:X}")
        return 1
    cr3 = struct.unpack("<Q", raw)[0]
    cr3_page = cr3 & 0xFFFFFFFFF000
    print(f"[+] DirectoryTableBase (CR3) = 0x{cr3:X}")
    print(f"[+] CR3 page base          = 0x{cr3_page:X}")
    print("\nNext (optional, uses Corsair maps carefully):")
    print(
        f'  python driver_interact.py --cr3 0x{cr3_page:X} --translate-lstar'
    )
    # Also write for scripts
    out = r"C:\Users\justin hernando\Documents\VulnDriver\tools\system_cr3.txt"
    with open(out, "w", encoding="utf-8") as f:
        f.write(f"0x{cr3_page:X}\n")
        f.write(f"raw=0x{cr3:X}\n")
        f.write(f"eprocess=0x{eprocess:X}\n")
    print(f"[+] Wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
