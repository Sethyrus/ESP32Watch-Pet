"""Recomprime los PNG del simulador (que salen sin comprimir). Uso: python3 -I pngpack.py f.png..."""
import struct, sys, zlib

for path in sys.argv[1:]:
    d = open(path, "rb").read()
    pos, idat, chunks = 8, b"", []
    while pos < len(d):
        n, t = struct.unpack(">I4s", d[pos:pos + 8])
        body = d[pos + 8:pos + 8 + n]
        if t == b"IDAT":
            idat += body
        else:
            chunks.append((t, body))
        pos += 12 + n
    z = zlib.compress(zlib.decompress(idat), 9)
    out = d[:8]
    for t, body in chunks:
        if t == b"IEND":
            out += struct.pack(">I4s", len(z), b"IDAT") + z + struct.pack(">I", zlib.crc32(b"IDAT" + z))
        out += struct.pack(">I4s", len(body), t) + body + struct.pack(">I", zlib.crc32(t + body))
    open(path, "wb").write(out)
