import ctypes
import ctypes.wintypes
import uuid
import os
import sys

# Define GUID for "UNKNOWN" device class (we'll iterate all)
# Alternatively, could use a specific GUID if known, e.g., for disk drives, etc.
# For device interfaces, SetupAPI typically doesn't use device class GUIDs for enumeration directly
# but rather interface class GUIDs. The driver provides a DeviceInterface GUID.
# DrvEye noted: DeviceInterface:{dd38f7fc-d7bd-488b-9242-7d8754cde80d}
# We need to parse this GUID string into a ctypes GUID structure.
CORSAIR_INTERFACE_GUID_STR = "{dd38f7fc-d7bd-488b-9242-7d8754cde80d}"

# --- SetupAPI constants and structures ---
# Source: https://learn.microsoft.com/en-us/windows/win32/api/setupapi/
#         https://learn.microsoft.com/en-us/windows-hardware/drivers/install/device-setup-api

INVALID_HANDLE_VALUE = -1
ERROR_INSUFFICIENT_BUFFER = 122
ERROR_NO_MORE_ITEMS = 259
DIGCF_PRESENT = 0x00000002
DIGCF_DEVICEINTERFACE = 0x00000010

LPVOID = ctypes.wintypes.LPVOID
class GUID(ctypes.Structure):
    _fields_ = [
        ("Data1", ctypes.wintypes.DWORD),
        ("Data2", ctypes.wintypes.WORD),
        ("Data3", ctypes.wintypes.WORD),
        ("Data4", ctypes.wintypes.BYTE * 8)
    ]

    def __init__(self, guid_str=None):
        super().__init__()
        if guid_str:
            self.from_string(guid_str)

    def from_string(self, guid_str):
        # Convert string GUID to native structure
        # Example: {dd38f7fc-d7bd-488b-9242-7d8754cde80d}
        u = uuid.UUID(guid_str)
        self.Data1 = u.time_low
        self.Data2 = u.time_mid
        self.Data3 = u.time_hi_version
        for i in range(8):
            self.Data4[i] = u.bytes[8+i]

    def __str__(self):
        # Convert GUID structure back to string format
        u = uuid.UUID(bytes_le=bytes(self.Data1.to_bytes(4, 'little') +
                                     self.Data2.to_bytes(2, 'little') +
                                     self.Data3.to_bytes(2, 'little') +
                                     bytes(self.Data4)))
        return str(u).upper()

class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
    _fields_ = [
        ("cbSize", ctypes.wintypes.DWORD),
        ("InterfaceClassGuid", GUID),
        ("Flags", ctypes.wintypes.DWORD),
                ("Reserved", ctypes.c_size_t) # Corrected type for pointer-sized integer
    ]

class SP_DEVICE_INTERFACE_DETAIL_DATA_W(ctypes.Structure):
    _fields_ = [
        ("cbSize", ctypes.wintypes.DWORD),
        ("DevicePath", ctypes.wintypes.WCHAR * 260) # MAX_PATH for device path
    ]

# --- SetupAPI functions ---
setupapi = ctypes.WinDLL('Setupapi', use_last_error=True)

setupapi.SetupDiGetClassDevsW.restype = ctypes.wintypes.HANDLE
setupapi.SetupDiGetClassDevsW.argtypes = [
    ctypes.POINTER(GUID),    # ClassGuid
    ctypes.wintypes.LPCWSTR, # Enumerator
    ctypes.wintypes.HWND,    # hwndParent
    ctypes.wintypes.DWORD    # Flags
]

setupapi.SetupDiEnumDeviceInterfaces.restype = ctypes.wintypes.BOOL
setupapi.SetupDiEnumDeviceInterfaces.argtypes = [
    ctypes.wintypes.HANDLE,           # hDevInfo
    LPVOID,                           # pDeviceInfoData
    ctypes.POINTER(GUID),             # pInterfaceClassGuid
    ctypes.wintypes.DWORD,            # dwMemberIndex
    ctypes.POINTER(SP_DEVICE_INTERFACE_DATA) # pDeviceInterfaceData
]

setupapi.SetupDiGetDeviceInterfaceDetailW.restype = ctypes.wintypes.BOOL
setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [
    ctypes.wintypes.HANDLE,                       # hDevInfo
    ctypes.POINTER(SP_DEVICE_INTERFACE_DATA),     # pDeviceInterfaceData
    ctypes.wintypes.LPVOID,                       # pDeviceInterfaceDetailData
    ctypes.wintypes.DWORD,                        # dwDeviceInterfaceDetailDataSize
    ctypes.POINTER(ctypes.wintypes.DWORD),        # pRequiredSize
    ctypes.wintypes.LPVOID                        # pDeviceInfoData
]

setupapi.SetupDiDestroyDeviceInfoList.restype = ctypes.wintypes.BOOL
setupapi.SetupDiDestroyDeviceInfoList.argtypes = [
    ctypes.wintypes.HANDLE # hDevInfo
]

def find_corsair_driver_path():
    """
    Enumerates device interfaces and tries to find the Corsair driver's path.
    """
    print(f"Searching for device interfaces matching '{CORSAIR_INTERFACE_GUID_STR}' or containing 'corsair'/'llaccess'...")

    # Initialize the GUID structure for the Corsair interface
    corsair_guid = GUID(CORSAIR_INTERFACE_GUID_STR)

    # Get a device information set for all device interfaces
    hDevInfo = setupapi.SetupDiGetClassDevsW(
        ctypes.byref(corsair_guid), # Pass the specific interface GUID
        None,                       # Enumerator (all)
        None,                       # No parent window
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE # Only active interfaces
    )

    if hDevInfo == INVALID_HANDLE_VALUE:
        error_code = ctypes.get_last_error()
        print(f"SetupDiGetClassDevsW failed. Error: {error_code} (0x{error_code:X})")
        return None

    try:
        device_interface_data = SP_DEVICE_INTERFACE_DATA()
        device_interface_data.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DATA)
        
        # Enumerate device interfaces
        member_index = 0
        found_paths = []
        while True:
            success = setupapi.SetupDiEnumDeviceInterfaces(
                hDevInfo,
                None,                       # No device info data
                ctypes.byref(corsair_guid), # Filter by the specific GUID
                member_index,
                ctypes.byref(device_interface_data)
            )

            if not success:
                error_code = ctypes.get_last_error()
                if error_code == ERROR_NO_MORE_ITEMS:
                    break # No more interfaces
                print(f"SetupDiEnumDeviceInterfaces failed. Error: {error_code} (0x{error_code:X})")
                return None

            # Get interface detail data (device path)
            required_size = ctypes.wintypes.DWORD(0)
            setupapi.SetupDiGetDeviceInterfaceDetailW(
                hDevInfo,
                ctypes.byref(device_interface_data),
                None,  # Pass NULL to get required size
                0,
                ctypes.byref(required_size),
                None
            )
            
            error_code = ctypes.get_last_error()
            if error_code != ERROR_INSUFFICIENT_BUFFER:
                print(f"SetupDiGetDeviceInterfaceDetailW (get size) failed. Error: {error_code} (0x{error_code:X})")
                member_index += 1
                continue

            # Allocate buffer for detail data
            detail_buffer = ctypes.create_string_buffer(required_size.value)
            
            detail_data = SP_DEVICE_INTERFACE_DETAIL_DATA_W()
            detail_data.cbSize = ctypes.sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) # Correct cbSize for the structure itself

            success = setupapi.SetupDiGetDeviceInterfaceDetailW(
                hDevInfo,
                ctypes.byref(device_interface_data),
                ctypes.byref(detail_data),
                required_size.value, # Total buffer size from previous call
                None,                # Not interested in required size again
                None                 # No device info data
            )

            if success:
                device_path = detail_data.DevicePath
                print(f"Found Device Path: {device_path}")
                found_paths.append(device_path)
            else:
                error_code = ctypes.get_last_error()
                print(f"SetupDiGetDeviceInterfaceDetailW failed. Error: {error_code} (0x{error_code:X})")
            
            member_index += 1
    finally:
        setupapi.SetupDiDestroyDeviceInfoList(hDevInfo)
    
    return found_paths

def main():
    if sys.platform != "win32":
        print("This script is designed for Windows systems only.")
        return

    # Find the device paths for CorsairLLAccess64.sys
    paths = find_corsair_driver_path()

    if not paths:
        print("\nNo device interfaces found matching the Corsair GUID or general criteria.")
        print("Please ensure the Corsair driver (CorsairLLAccess64.sys) is installed and running.")
        print("You might also need to install the iCUE software from Corsair to ensure the driver is active.")
        print("If you know a different specific device interface GUID or a unique part of the symbolic link, update CORSAIR_INTERFACE_GUID_STR or extend the search logic.")
        return

    print("\n--- Identified Driver Paths ---")
    for p in paths:
        print(f"Potential DRIVER_SYMBOLIC_LINK: {p}")
        print(f"Use this in driver_interact.py: DRIVER_SYMBOLIC_LINK = r\"{p}\"")

if __name__ == "__main__":
    main()
