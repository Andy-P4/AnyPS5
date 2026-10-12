# S:\AnyPS5\extracted\shorten_intros.py
import struct, shutil, os

video_dir = r'S:\AnyPS5\extracted\app0\zone\video'

for name in ['dolby.bk2', 'treyarch.bk2', 'intro.bk2']:
    path = os.path.join(video_dir, name)
    bak = path + '.orig'
    if not os.path.exists(bak):
        shutil.copyfile(path, bak)
        print(f"Backed up {name} to {name}.orig")
    
    with open(path, 'r+b') as f:
        hdr = f.read(48)
        old_frames = struct.unpack('<I', hdr[8:12])[0]
        f.seek(8)
        f.write(struct.pack('<I', 2))
        print(f"Patched {name}: frames {old_frames} -> 2")
