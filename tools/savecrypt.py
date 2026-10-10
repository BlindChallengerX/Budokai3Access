"""Budokai 3 HD save file (data.bin, "#SPF 1.0") decrypt / encrypt.

Reimplemented from the game code (821d34a8 decrypt, 821d33c0 encrypt,
82084a78 random). Layout:
  0x00 "#SPF 1.0"   0x0c seed   0x10 16-byte column sum of the bytes from 0x20 (stored form)
  0x20 16-byte column sum of the plain bytes from 0x30, then the plain data from 0x30.
Each stored byte = plain - previous plain + rand((previous plain & 15) + 8).
Usage: savecrypt.py dec IN OUT  |  savecrypt.py enc IN OUT
"""
import sys, struct

class Rng:
    def __init__(self, seed): self.x = seed & 0xFFFFFFFF
    def rand(self, n):
        x = self.x
        for _ in range(n + 1):
            bit = (x >> 31) ^ ((x >> 4) & 1)
            x = ((x << 1) | bit) & 0xFFFFFFFF
        self.x = x
        return (x - 1) & 0xFFFFFFFF

MAGIC = b"#SPF 1.0"

def decrypt(data):
    b = bytearray(data)
    assert b[:8] == MAGIC, "not a #SPF save"
    s = [0] * 16
    for i in range(len(b) - 0x20): s[i & 15] = (s[i & 15] + b[0x20 + i]) & 0xFF
    if bytes(s) != bytes(b[0x10:0x20]): raise ValueError("stored checksum mismatch")
    r = Rng(struct.unpack_from(">I", b, 0xC)[0]); r.rand(0x20)
    prev = 0
    for i in range(0x20, len(b)):
        prev = (b[i] + prev - r.rand((prev & 15) + 8)) & 0xFF
        b[i] = prev
    s = [0] * 16
    for i in range(len(b) - 0x30): s[i & 15] = (s[i & 15] + b[0x30 + i]) & 0xFF
    if bytes(s) != bytes(b[0x20:0x30]): raise ValueError("plain checksum mismatch")
    return bytes(b)

def encrypt(plain, seed=None):
    b = bytearray(plain)
    b[:8] = MAGIC
    if seed is None: seed = struct.unpack_from(">I", plain, 0xC)[0]
    struct.pack_into(">I", b, 0xC, seed)
    s = [0] * 16
    for i in range(len(b) - 0x30): s[i & 15] = (s[i & 15] + b[0x30 + i]) & 0xFF
    b[0x20:0x30] = bytes(s)
    r = Rng(seed); r.rand(0x20)
    prev = 0; c = [0] * 16
    for i in range(0x20, len(b)):
        p = b[i]
        b[i] = (p - prev + r.rand((prev & 15) + 8)) & 0xFF
        prev = p
        c[(i - 0x20) & 15] = (c[(i - 0x20) & 15] + b[i]) & 0xFF
    b[0x10:0x20] = bytes(c)
    return bytes(b)

if __name__ == "__main__":
    mode, src, dst = sys.argv[1:4]
    d = open(src, "rb").read()
    open(dst, "wb").write(decrypt(d) if mode == "dec" else encrypt(d))
