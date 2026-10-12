# Call of Duty: Black Ops II Compatibility & Patches for AnyPS5

This directory contains the compatibility implementations, automated patchers, multi-monitor launcher, and diagnostics used to run *Call of Duty: Black Ops II* on AnyPS5.

## Components

### 1. `apply_true_gpu_fix.py`
Automated master patcher for `libSceAgcDriver.prx` addressing 11 key graphics & memory checks:
1. **Format 14 Type Check**: NOPs invalid type restriction (`0x113bad`).
2. **Format 14 Bytes Per Pixel**: Corrects bpp to 4 (`0x113bbd`).
3. **Format 14 VkFormat**: Maps format 14 to `VK_FORMAT_R8G8B8A8_UNORM` (`0x113bc5`).
4. **Unclamped Normalized Color**: Bypasses restriction for normalized post-processing (`0x113be9`).
5. **Color Address Extension**: NOPs address extension check (`0x113d60`).
6. **Address Space Aliasing**: Zeroes `CB_COLOR_BASE_EXT` (`shl rax, 0x28` -> `xor rax, rax; nop`) preventing 48-bit aliasing (`0x113d6d`).
7. **Buffer Alias Check**: Bypasses `CheckBufferAliases` entry (`0x8cc50`).
8. **Shader Resource Alias Jumps**: NOPs index buffer aliasing checks in `ShaderResources::buildPrepare` (`0xe6bcd`, `0xe6e41`).
9. **AgcDriver Graphics Require**: Bypasses unhandled graphics assertion (`0x3fe040`).
10. **Null / Misaligned Address**: Safe alignment bypass in `GuestMemory::CheckRange` (`0x54933`).
11. **Guest Memory Readability**: NOPs unreadable memory check in `GuestMemory::CheckRange` (`0x54985`).

### 2. `make_compat.py` & `compat_all.c`
Compatibility stub generator and source for unresolved SCE NIDs (NpCommerce, WebBrowserDialog, semaphores, user IDs, flex mappings).

### 3. `launch_game.c`
Custom process launcher that:
- Spawns `eboot.exe` on interactive display station `WinSta0\Default`.
- Locates the game window and automatically moves it to Monitor 2 (`X=2560..5120`).
- Dispatches discrete Enter/Space keystrokes at 4s, 7s, 10s, and 14s to advance intros without accidentally triggering the main menu "Quit" option.

### 4. `screenshot.c` & `take_screenshot.ps1`
Direct GDI/PrintWindow screen capture utility capturing the live game window buffer for verification.

### 5. `shorten_intros.py`
Replaces lengthy intro video containers (`dolby.bk2`, `treyarch.bk2`) with lightweight Bink stubs to accelerate game boot times.
