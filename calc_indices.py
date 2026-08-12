
va = 0xFFFFF8049EA00000
pml4_idx = (va >> 39) & 0x1FF
pdpt_idx = (va >> 30) & 0x1FF
pd_idx = (va >> 21) & 0x1FF
pt_idx = (va >> 12) & 0x1FF

print(f"VA: {hex(va)}")
print(f"PML4 Index: {pml4_idx}")
print(f"PDPT Index: {pdpt_idx}")
print(f"PD Index:   {pd_idx}")
print(f"PT Index:   {pt_idx}")
