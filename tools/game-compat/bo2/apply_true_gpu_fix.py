# S:\AnyPS5\extracted\apply_true_gpu_fix.py
import os

src_path = r'S:\AnyPS5\extracted\libs\libSceAgcDriver.prx.bak_master'
dst_path = r'S:\AnyPS5\extracted\libs\libSceAgcDriver.prx'

with open(src_path, 'rb') as f:
    data = bytearray(f.read())

patches = [
    # 1. Format 14 number type check in DecodeState -> NOP cmp+jne
    (0x113bad,
     bytes.fromhex('83f8040f854c503300'),
     b'\x90' * 9,
     "Format 14 number type check NOP"),

    # 2. Format 14 bpp: change 0x10 (16) to 0x04 (4)
    (0x113bbd,
     bytes.fromhex('10'),
     bytes.fromhex('04'),
     "Format 14 bpp = 4"),

    # 3. Format 14 VkFormat: change 0x6b to 0x25 (VK_FORMAT_R8G8B8A8_UNORM = 37)
    (0x113bc5,
     bytes.fromhex('6b'),
     bytes.fromhex('25'),
     "Format 14 VkFormat = 37"),

    # 4. Unclamped normalized color exception -> NOP jne
    (0x113be9,
     bytes.fromhex('0f853c573300'),
     b'\x90' * 6,
     "Bypass unclamped normalized color exception"),

    # 5. Invalid color address extension check -> NOP ja
    (0x113d60,
     bytes.fromhex('0f87a6543300'),
     b'\x90' * 6,
     "Bypass invalid color address extension exception"),

    # 6. Color target base address extension (ext << 40) -> xor rax, rax; nop
    # Keeps color targets inside the 40-bit guest memory space
    (0x113d6d,
     bytes.fromhex('48c1e028'),
     bytes.fromhex('4831c090'),
     "Zero color target base address extension (mask bit 40)"),

    # 7. CheckBufferAliases -> ret immediately
    # Eliminates 'writable shader buffer aliases the index buffer' draw rejection
    (0x8cc50,
     bytes.fromhex('554889e5'),
     bytes.fromhex('c3909090'),
     "CheckBufferAliases instant ret"),

    # 8. ShaderResources::buildPrepare aliases check 1 -> NOP jb
    (0xe6bcd,
     bytes.fromhex('0f8275ab3500'),
     b'\x90' * 6,
     "buildPrepare buffer alias check 1 NOP"),

    # 9. ShaderResources::buildPrepare aliases check 2 -> NOP jb
    (0xe6e41,
     bytes.fromhex('0f8201a93500'),
     b'\x90' * 6,
     "buildPrepare buffer alias check 2 NOP"),

    # 10. AgcDriver::Graphics::Require -> ret immediately
    # Bypasses graphics require assertions
    (0x3fe040,
     bytes.fromhex('84c97401c3'),
     bytes.fromhex('c390909090'),
     "Graphics::Require instant ret"),

    # 11. GuestMemory::CheckRange null-safe alignment bypass
    # If rbx == 0, jumps to epilogue (success ret).
    # If rbx != 0, jumps past div rdi to verify check.
    # Prevents 'null or misaligned address' draw rejections.
    (0x54933,
     bytes.fromhex('4885db741031d24889d831c948f7f74885d20f94c1488d1568d84500e89cffffff'),
     bytes.fromhex('4885db746ceb1a') + b'\x90' * 26,
     "CheckRange null-safe alignment bypass"),

    # 12. GuestMemory::CheckRange unreadable page bypass
    # NOPs jne so 'guest memory is not readable at ...' exception is never thrown.
    (0x54985,
     bytes.fromhex('7529'),
     b'\x90\x90',
     "CheckRange unreadable page bypass")
]

for offset, expected, replacement, name in patches:
    actual = bytes(data[offset:offset+len(expected)])
    if actual != expected:
        raise ValueError(f"Mismatch at {name} (offset {hex(offset)}): expected {expected.hex()}, found {actual.hex()}")
    assert len(expected) == len(replacement), f"Length mismatch for {name}"
    data[offset:offset+len(replacement)] = replacement
    print(f"Applied patch: {name} at {hex(offset)}")

with open(dst_path, 'wb') as f:
    f.write(data)

print(f"\nSuccessfully created patched {dst_path} with all {len(patches)} master GPU fixes!")
