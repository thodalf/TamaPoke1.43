#!/usr/bin/env python3
"""Extracts web/sprites-<region>.pak back into tools/sdcard/mons/*.bin.

The reverse of pack_bundle.py. The .pak files ARE committed (see the .gitignore
comment on tools/sdcard/mons/*.bin: they have to be, since GitHub release assets
send no CORS headers and the web installer needs same-origin files), so this is
a fast, offline way to get real sprites for the emulator and sprite_test on a
fresh checkout -- no PMD SpriteCollab fetch (pack_pmd.py) required.

Same TPAK format pack_bundle.py writes:
  char[4]  "TPAK"
  uint16   count
  count x { uint8 nameLen; char name[nameLen]; uint32 size }   (index)
  ...each file's bytes, in the same order...

  python3 tools/unpack_bundle.py            # every web/sprites-*.pak
  python3 tools/unpack_bundle.py kanto       # just one region
"""
import glob
import os
import struct
import sys

HERE = os.path.dirname(__file__)
MONS = os.path.join(HERE, 'sdcard', 'mons')
WEB = os.path.join(HERE, '..', 'web')


def unpack(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] != b'TPAK':
        raise SystemExit(f'{path}: not a TPAK file')
    (count,) = struct.unpack_from('<H', data, 4)
    pos = 6
    entries = []
    for _ in range(count):
        (name_len,) = struct.unpack_from('<B', data, pos)
        pos += 1
        name = data[pos:pos + name_len].decode()
        pos += name_len
        (size,) = struct.unpack_from('<I', data, pos)
        pos += 4
        entries.append((name, size))
    written = 0
    for name, size in entries:
        blob = data[pos:pos + size]
        pos += size
        # names are stored as "mons/<file>" -- only the basename matters here
        out = os.path.join(MONS, os.path.basename(name))
        if os.path.exists(out) and os.path.getsize(out) == size:
            continue  # already extracted (e.g. a shared file from another pack)
        with open(out, 'wb') as o:
            o.write(blob)
        written += 1
    return len(entries), written


def main():
    wanted = set(a.lower() for a in sys.argv[1:])
    os.makedirs(MONS, exist_ok=True)
    paks = sorted(glob.glob(os.path.join(WEB, 'sprites-*.pak')))
    if wanted:
        paks = [p for p in paks
                if os.path.basename(p)[len('sprites-'):-len('.pak')] in wanted]
    if not paks:
        raise SystemExit('no matching web/sprites-*.pak found')
    total_written = 0
    for p in paks:
        n, written = unpack(p)
        total_written += written
        print(f'{os.path.basename(p)}: {n} files, {written} new')
    print(f'{total_written} file(s) written to {os.path.normpath(MONS)}')


if __name__ == '__main__':
    main()
