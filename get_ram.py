
import winreg
import struct

def get_ram_ranges():
    try:
        key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\RESOURCEMAP\System Resources\Physical Memory")
        val, typ = winreg.QueryValueEx(key, ".Translated")
        winreg.CloseKey(key)
        
        # CM_RESOURCE_LIST (header = 4 bytes for count)
        #   CM_FULL_RESOURCE_DESCRIPTOR[count]
        #     InterfaceType (4), BusNumber (4)
        #     CM_PARTIAL_RESOURCE_LIST
        #       Version (2), Revision (2), Count (4)
        #       CM_PARTIAL_RESOURCE_DESCRIPTOR[Count]
        
        full_count = struct.unpack_from("<I", val, 0)[0]
        off = 4
        ranges = []
        for i in range(full_count):
            # InterfaceType(4), BusNumber(4)
            off += 8
            # Partial List header: Version(2), Revision(2), Count(4)
            partial_count = struct.unpack_from("<I", val, off + 4)[0]
            off += 8
            for j in range(partial_count):
                # CM_PARTIAL_RESOURCE_DESCRIPTOR is 20 bytes
                # Type(1), Share(1), Flags(2), ...
                type_byte = val[off]
                if type_byte == 3: # CmResourceTypeMemory
                    # Memory descriptor: Start(8), Length(4) (standard)
                    # BUT on 64-bit it might be different or have padding
                    # Let's try to find 0x03 followed by memory-like values
                    start = struct.unpack_from("<Q", val, off + 4)[0]
                    length = struct.unpack_from("<I", val, off + 12)[0]
                    # Check if it looks like a valid PA range
                    if length > 0 and start < 0x10000000000:
                         ranges.append((start, length))
                off += 20
        return ranges
    except Exception as e:
        with open(r"C:\Users\justin hernando\Documents\VulnDriver\tools\brute_error.txt", "w") as f:
            f.write(str(e))
        return None

print(get_ram_ranges())
