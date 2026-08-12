import requests
import json
import sys

# Helper to convert various types to string, handling lists, dicts, and None
def as_str(val):
    if isinstance(val, list):
        return ' | '.join(as_str(x) for x in val) if val else ''
    elif isinstance(val, dict):
        return json.dumps(val)
    elif val is None:
        return ''
    return str(val)

def fetch_and_filter_loldrivers():
    url = "https://www.loldrivers.io/api/drivers.json"
    print(f"Fetching LOLDrivers data from {url}...")
    try:
        response = requests.get(url)
        response.raise_for_status()  # Raise an exception for HTTP errors (4xx or 5xx)
        drivers_data = response.json()
    except requests.exceptions.RequestException as e:
        print(f"Error fetching LOLDrivers data: {e}")
        return
    except json.JSONDecodeError as e:
        print(f"Error decoding JSON response: {e}")
        return

    print(f"Successfully fetched {len(drivers_data)} drivers.")

    matching_drivers = []
    keywords = [
        "kva-to-pa", "virtual address translate", "kernel virtual read", "kernel virtual write",
        "map virtual", "map physical", "memory map", "pte", "page table entry", "cr3",
        "kva", "va to pa", "virtual to physical",
        "virtual memory", "map memory", "address translation", "pci access", "system memory",
        "kernel memory", "user-kernel mapping", "read/write any address"
    ]

    for driver in drivers_data:
        description = as_str(driver.get("Description", "")).lower()
        notes = as_str(driver.get("Notes", "")).lower()
        exploit_primitives = as_str(driver.get("Exploit Primitives", [])).lower()
        # IOCTLs field can be a list of dicts, let's process it more thoroughly
        ioctl_descriptions = []
        for ioctl_entry in driver.get("IOCTLs", []):
            if isinstance(ioctl_entry, dict):
                ioctl_descriptions.append(as_str(ioctl_entry.get("Description", "")).lower())
            else:
                ioctl_descriptions.append(as_str(ioctl_entry).lower())
        ioctl_text = " | ".join(ioctl_descriptions)

        combined_text = f"{description} {notes} {exploit_primitives} {ioctl_text}"

        if any(keyword in combined_text for keyword in keywords):
            matching_drivers.append(driver)

    if not matching_drivers:
        print("No drivers found matching KVA-to-PA translation or kernel virtual memory R/W keywords.")
        return

    output_file_path = r"C:\Users\jhern\VulnDriver\loldrivers_kva_pa_search_results.json"
    with open(output_file_path, "w", encoding="utf-8") as f:
        json.dump(matching_drivers, f, indent=4, ensure_ascii=False)

    print(f"Found {len(matching_drivers)} matching drivers. Details saved to {output_file_path}")

    # Also print a summary to console
    print("\n--- Summary of Matching Drivers ---")
    for i, driver in enumerate(matching_drivers):
        print(f"\nDriver {i+1}:")
        print(f"  Name: {driver.get('Name')}")
        print(f"  Filename: {driver.get('Filename')}")
        print(f"  SHA1: {driver.get('SHA1')}")
        print(f"  Exploit Primitives: {as_str(driver.get('Exploit Primitives'))}")
        print(f"  Description: {as_str(driver.get('Description'))}")
        print(f"  Notes: {as_str(driver.get('Notes'))}")
        print(f"  IOCTLs (relevant parts): {ioctl_text}")
        print(f"  Download URL: {driver.get('Download URL', 'N/A')}")

if __name__ == "__main__":
    # Ensure requests library is installed
    try:
        import requests
    except ImportError:
        print("The 'requests' library is not installed. Please install it using: pip install requests")
        sys.exit(1)
    fetch_and_filter_loldrivers()
