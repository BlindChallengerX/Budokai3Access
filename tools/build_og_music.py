"""Build the og_music mod for the DBZ Budokai 3 HD port from your own PS2 disc.

Usage: python build_og_music.py <game folder> <PS2 ADX_USA.AFS> [<PS2 OPENING.SFD>]

Writes <game folder>/mods/og_music/us/adx_usa.afs (+ opening.sfd and
manifest.txt). The game folder must hold the extracted HD data in us/.

The HD archive is kept and only the music entries are replaced with the PS2
(US) ones, so voices and new HD sounds stay as they are. Mapping:
- the 23-track music table is the same in both executables (HD 0x82327d50,
  PS2 ELF 0x34ef70) with every index 2 higher in HD: HD 124-146 = PS2 122-144
  (HD 122 and 123 are tracks the HD release added; they are kept);
- HD 307 = PS2 305 (jingle);
- stage music HD 4187-4215 = PS2 4179-4207;
- 48 kHz themes HD 4267-4276 = PS2 4259-4268.
"""
import struct, os, sys, shutil

def toc(f):
    f.seek(0); h = f.read(8); assert h[:4] == b'AFS\0', 'not an AFS archive'
    n = struct.unpack_from('<I', h, 4)[0]
    t = f.read(8 * n + 8)
    return [struct.unpack_from('<II', t, 8 * i) for i in range(n)], struct.unpack_from('<II', t, 8 * n)

def al(x): return (x + 0x7ff) & ~0x7ff

def main():
    game, ps2 = sys.argv[1], sys.argv[2]
    hd = os.path.join(game, 'us', 'adx_usa.afs')
    out_dir = os.path.join(game, 'mods', 'og_music', 'us')
    os.makedirs(out_dir, exist_ok=True)
    hf, pf = open(hd, 'rb'), open(ps2, 'rb')
    he, hmeta = toc(hf); pe, _ = toc(pf)
    if len(he) != 4549 or len(pe) != 4541:
        sys.exit('unexpected archives (%d / %d entries): need the US HD and US PS2 versions' % (len(he), len(pe)))
    m = {124 + k: 122 + k for k in range(23)}
    m[307] = 305
    m.update({4187 + k: 4179 + k for k in range(29)})
    m.update({4267 + k: 4259 + k for k in range(10)})
    n = len(he)
    first = max(al(8 + 8 * n + 8), he[0][0])
    with open(os.path.join(out_dir, 'adx_usa.afs'), 'wb') as o:
        o.write(b'\0' * first)
        ent = []
        for i in range(n):
            src, (off, size) = (pf, pe[m[i]]) if i in m else (hf, he[i])
            src.seek(off); data = src.read(size)
            pos = o.tell(); ent.append((pos, size))
            o.write(data); o.write(b'\0' * (al(pos + size) - pos - size))
        mpos, msize = 0, 0
        if hmeta[0] and hmeta[1]:          # name table, copied unchanged
            hf.seek(hmeta[0]); md = hf.read(hmeta[1]); mpos, msize = o.tell(), len(md)
            o.write(md); o.write(b'\0' * (al(mpos + msize) - mpos - msize))
        o.seek(0)
        o.write(b'AFS\0' + struct.pack('<I', n))
        for e in ent: o.write(struct.pack('<II', *e))
        o.write(struct.pack('<II', mpos, msize))
    if len(sys.argv) > 3:
        shutil.copy(sys.argv[3], os.path.join(out_dir, 'opening.sfd'))
    with open(os.path.join(game, 'mods', 'og_music', 'manifest.txt'), 'w') as f:
        f.write('name=OG Music\ndescription=Original PS2 (US) Budokai 3 soundtrack by Kenji Yamamoto, put back into '
                'the HD game\'s music slots. Built from your own PS2 disc; voices are unchanged.\n'
                'author=Built with DBZ 3 Access tools\nversion=1.0\n')
    print('og_music built:', len(m), 'music entries replaced')

if __name__ == '__main__':
    main()
