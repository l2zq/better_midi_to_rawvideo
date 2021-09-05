#!/bin/env python3
import sys
import struct

ui32le = struct.Struct('<I')

offs = [ None ] * 65536
data = [ None ] * 65536

def test(glyph_data):
    assert len(glyph_data) in (32, 64)
    line_len = len(glyph_data) // 16
    for line_data in map(lambda i: glyph_data[i: i+line_len], range(0, len(glyph_data), line_len)):
        for seg_data in map(lambda i: int(line_data[i: i+2], 16), range(0, len(line_data), 2)):
            print(bin(0x100+seg_data)[3:].replace('0', '\033[47m  ').replace('1', '\033[40m  '), end='')
        print('\033[0m')

def glyph_line_to_bytes(glyph_data):
    assert len(glyph_data) in (32, 64)
    return bytes(map(lambda i: int(glyph_data[i: i+2], 16), range(0, len(glyph_data), 2)))

with open(sys.argv[1], 'r') as fp:
    for cp, glyph_data in map(lambda x: (int(x[0], 16), x[2]),
            map(lambda l: l.strip().partition(':'),
                filter(len, fp))):
        data[cp] = glyph_line_to_bytes(glyph_data)

offset = 0
for i in range(65536):
    if data[i] == None:
        offs[i] = 0xffff_ffff
    else:
        assert offset % 16 == 0
        assert len(data[i]) in (16, 32)
        offs[i] = offset
        if len(data[i]) == 32:
            offs[i] |= 1<<31
        offset += len(data[i])

with open('unifont.bin', 'wb') as fp:
    for i in range(65536):
        fp.write(ui32le.pack(offs[i]))
    for glyph_bytes in data:
        if glyph_bytes != None:
            fp.write(glyph_bytes)
