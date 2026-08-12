
import ctypes
import struct

ntdll = ctypes.WinDLL("ntdll")
ntdll.NtSystemDebugControl.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]

def test_cmd(cmd_id):
    ret = ntdll.NtSystemDebugControl(cmd_id, None, 0, None, 0, None)
    return ret & 0xFFFFFFFF

print(f"Build: 26200")
for i in range(20):
    res = test_cmd(i)
    # STATUS_INFO_LENGTH_MISMATCH = 0xC0000004
    # STATUS_NOT_IMPLEMENTED = 0xC0000002
    # STATUS_ACCESS_DENIED = 0xC0000022
    # STATUS_INVALID_INFO_CLASS = 0xC0000003
    status_map = {
        0xC0000004: "LENGTH_MISMATCH",
        0xC0000002: "NOT_IMPLEMENTED",
        0xC0000022: "ACCESS_DENIED",
        0xC0000003: "INVALID_INFO_CLASS",
        0: "SUCCESS (unexpected)"
    }
    msg = status_map.get(res, hex(res))
    print(f"Cmd {i:2}: {msg}")
