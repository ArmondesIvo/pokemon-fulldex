#!/usr/bin/env python3
"""Builds the Pokémon box sprites into the program (the MY PC tab, frontend/launcher.h):

  embed_sprites.py SPRITES_DIR OUT.cpp

SPRITES_DIR holds regular/001.png ... and shiny/001.png ... (the National Dex number, three
digits): pokesprite's Generation 8 box sprites (68 x 56), as PKVault shows them. None come with
the source. OUT.cpp holds each PNG as it is, one blob and a table by number, and
BoxSprite(species, shiny, data, size) to find one."""
import os
import sys


def main():
    src, out = sys.argv[1], sys.argv[2]
    blob = bytearray()
    table = []
    for kind in ('regular', 'shiny'):
        d = os.path.join(src, kind)
        for n in range(1, 1000):
            p = os.path.join(d, '%03d.png' % n)
            if not os.path.isfile(p):
                continue
            data = open(p, 'rb').read()
            if data[:8] != b'\x89PNG\r\n\x1a\n':
                sys.exit('%s is not a PNG' % p)
            table.append((n, kind == 'shiny', len(blob), len(data)))
            blob += data
    lines = ['// Made by tools/boxsprites/embed_sprites.py: do not edit.', '#include <cstddef>', '#include <cstdint>', '',
             'namespace BoxSprites', '{', 'static const unsigned char kBlob[] = {']
    for i in range(0, len(blob), 32):
        lines.append(','.join(str(b) for b in blob[i:i + 32]) + ',')
    if not blob:
        lines.append('0')
    lines += ['};', 'struct Entry { uint16_t n; bool shiny; uint32_t at, size; };', 'static const Entry kTable[] = {']
    lines += ['    { %d, %s, %d, %d },' % (n, 'true' if s else 'false', at, size) for n, s, at, size in table] or ['    { 0, false, 0, 0 },']
    lines += ['};', '}', '',
              '// A species\' box sprite (a PNG), shiny or not; false when there is none.',
              'bool BoxSprite(int species, bool shiny, const unsigned char*& data, size_t& size)', '{',
              '    for (const BoxSprites::Entry& e : BoxSprites::kTable)',
              '        if (e.n == species && e.shiny == shiny && e.size) { data = BoxSprites::kBlob + e.at; size = e.size; return true; }',
              '    return false;', '}', '']
    tmp = out + '.tmp'
    with open(tmp, 'w') as f:
        f.write('\n'.join(lines))
    os.replace(tmp, out)
    print('box sprites: %d, %d bytes' % (len(table), len(blob)))


if __name__ == '__main__':
    main()
