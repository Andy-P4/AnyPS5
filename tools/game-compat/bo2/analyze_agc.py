import struct
import subprocess

prx_path = r'S:\AnyPS5\extracted\libs\libSceAgcDriver.prx'

with open(prx_path, 'rb') as f:
    data = f.read()

# Parse DOS / PE header
e_lfanew = struct.unpack_from('<I', data, 0x3c)[0]
magic = data[e_lfanew:e_lfanew+4]
assert magic == b'PE\x00\x00'

num_sections = struct.unpack_from('<H', data, e_lfanew + 6)[0]
opt_header_size = struct.unpack_from('<H', data, e_lfanew + 20)[0]
opt_header_offset = e_lfanew + 24
opt_magic = struct.unpack_from('<H', data, opt_header_offset)[0]
assert opt_magic == 0x20b # PE32+

image_base = struct.unpack_from('<Q', data, opt_header_offset + 24)[0]
print(f"ImageBase: 0x{image_base:x}")

section_table_offset = opt_header_offset + opt_header_size
sections = []
for i in range(num_sections):
    sec_offset = section_table_offset + i * 40
    name = data[sec_offset:sec_offset+8].rstrip(b'\x00').decode('latin1', 'ignore')
    vsize, vaddr, raw_size, raw_ptr = struct.unpack_from('<IIII', data, sec_offset + 8)
    sections.append((name, vaddr, vsize, raw_ptr, raw_size))
    print(f"Section {name:8s}: VAddr=0x{vaddr:08x}, VSize=0x{vsize:08x}, RawPtr=0x{raw_ptr:08x}, RawSize=0x{raw_size:08x}")

def rva_to_offset(rva):
    for name, vaddr, vsize, raw_ptr, raw_size in sections:
        if vaddr <= rva < vaddr + vsize:
            return rva - vaddr + raw_ptr
    return None

def offset_to_rva(offset):
    for name, vaddr, vsize, raw_ptr, raw_size in sections:
        if raw_ptr <= offset < raw_ptr + raw_size:
            return offset - raw_ptr + vaddr
    return None

strings_to_find = [
    b"unclamped normalized color is unsupported",
    b"storage texture has no committed guest pages",
    b"linear surface pitch requires width aligned",
    b"unsupported color tile mode",
    b"writable shader buffer aliases"
]

print("\n=== STRING SEARCH ===")
for s in strings_to_find:
    idx = 0
    while True:
        pos = data.find(s, idx)
        if pos == -1:
            break
        rva = offset_to_rva(pos)
        va = image_base + rva if rva else 0
        print(f"Found '{s.decode()}' at file offset 0x{pos:x}, RVA=0x{rva:x}, VA=0x{va:x}")
        idx = pos + 1

# Check references to string RVA / VA in .text
text_sec = [s for s in sections if s[0] == '.text'][0]
print(f"\n.text section raw range: 0x{text_sec[3]:x} - 0x{text_sec[3] + text_sec[4]:x}")

# Search for RIP-relative references in code:
# opcode: [48 8d reg disp32] -> lea reg, [rip + disp32]
# displacement = target_rva - (insn_rva + insn_len)
# Target string:
for s in strings_to_find:
    pos = data.find(s)
    if pos == -1:
        continue
    target_rva = offset_to_rva(pos)
    print(f"\nSearching references to '{s.decode()}' (target_rva=0x{target_rva:x}):")
    # Scan .text
    text_start = text_sec[3]
    text_end = text_start + text_sec[4]
    text_rva_base = text_sec[1]
    
    # Check 4-byte LE displacement in code
    for off in range(text_start, text_end - 7):
        # Check for 48 8d (lea r64, [rip+disp])
        if data[off] == 0x48 and data[off+1] == 0x8d:
            disp = struct.unpack_from('<i', data, off + 3)[0]
            insn_rva = (off - text_start) + text_rva_base
            dest_rva = insn_rva + 7 + disp
            if dest_rva == target_rva:
                print(f"  LEA at file offset 0x{off:x} (RVA 0x{insn_rva:x}, VA 0x{image_base+insn_rva:x})")
        # Check for 8d (lea r32, [rip+disp]) or 4c 8d (lea r64, [rip+disp])
        elif data[off] in (0x4c, 0x48) and data[off+1] == 0x8d:
            disp = struct.unpack_from('<i', data, off + 3)[0]
            insn_rva = (off - text_start) + text_rva_base
            dest_rva = insn_rva + 7 + disp
            if dest_rva == target_rva:
                print(f"  LEA(REX) at file offset 0x{off:x} (RVA 0x{insn_rva:x}, VA 0x{image_base+insn_rva:x})")
