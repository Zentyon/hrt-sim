#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Validacion global independiente:
1) cuenta y suma .bop en disco
2) para A01 (ES y EN): parsea el .bop, reconstruye un Ogg-Opus minimo
   (OpusHead + OpusTags sinteticos + packets de audio) y lo decodifica con
   ffmpeg para confirmar duracion y RMS>0  -> prueba de recomposicion real.
"""
import os, struct, subprocess
import numpy as np

FFMPEG = r"C:\Users\Zent\AppData\Local\Temp\opencode\ffmpeg\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe"
ES_OUT = r"C:\Users\Zent\Documents\hrt-sim\data\v\es"
EN_OUT = r"C:\Users\Zent\Documents\hrt-sim\data\v\en"


def ogg_crc(data):
    crc = 0
    for b in data:
        crc ^= (b << 24) & 0xFFFFFFFF
        for _ in range(8):
            if crc & 0x80000000:
                crc = ((crc << 1) ^ 0x04c11db7) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    return crc


def build_page(serial, seq, granule, packets, bos=False, eos=False, continued=False):
    seg_table = bytearray()
    payload = b""
    for pkt in packets:
        payload += pkt
        L = len(pkt)
        while L >= 255:
            seg_table.append(255); L -= 255
        seg_table.append(L)
    ht = (1 if continued else 0) | (2 if bos else 0) | (4 if eos else 0)
    header = b"OggS" + bytes([0, ht]) + struct.pack("<q", granule) + \
             struct.pack("<I", serial) + struct.pack("<I", seq) + \
             b"\x00\x00\x00\x00" + bytes([len(seg_table)]) + bytes(seg_table)
    page = header + payload
    crc = ogg_crc(page)
    page = page[:22] + struct.pack("<I", crc) + page[26:]
    return page


def opus_head():
    # 'OpusHead' v1, 1 ch, pre-skip 0, input rate 16000, gain 0, mapping 0
    return b"OpusHead" + bytes([1, 1]) + struct.pack("<H", 0) + \
           struct.pack("<I", 16000) + struct.pack("<h", 0) + bytes([0])


def opus_tags():
    vendor = b"bop-recon"
    return b"OpusTags" + struct.pack("<I", len(vendor)) + vendor + struct.pack("<I", 0)


def read_bop(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"BOP1"
    sr, flags = struct.unpack_from("<HH", data, 4)
    pos = 8; pk = []
    while pos < len(data):
        ln = struct.unpack_from("<H", data, pos)[0]; pos += 2
        pk.append(data[pos:pos+ln]); pos += ln
    return sr, flags, pk


def recompose_and_decode(bop_path, tag):
    sr, flags, audio = read_bop(bop_path)
    serial = 0x1234ABCD
    out = b""
    # pagina 1: BOS con OpusHead
    out += build_page(serial, 0, 0, [opus_head()], bos=True)
    # pagina 2: OpusTags
    out += build_page(serial, 1, 0, [opus_tags()])
    # paginas de audio: ~20 packets por pagina
    seq = 2
    per = 20
    for i in range(0, len(audio), per):
        chunk = audio[i:i+per]
        granule = (i + len(chunk)) * 320   # muestras a 16 kHz
        last = (i + per >= len(audio))
        out += build_page(serial, seq, granule, chunk, eos=last)
        seq += 1
    tmp = os.path.join(os.environ.get("TEMP", "."), "recomp_%s.ogg" % tag)
    with open(tmp, "wb") as f:
        f.write(out)
    # decodificar a PCM y medir
    p = subprocess.run([FFMPEG, "-v", "error", "-i", tmp, "-ar", "16000",
                        "-f", "s16le", "-ac", "1", "-"],
                       capture_output=True)
    ok = p.returncode == 0 and len(p.stdout) > 2
    dur = rms = 0.0
    if ok:
        a = np.frombuffer(p.stdout, dtype="<i2").astype(np.float64) / 32768.0
        dur = a.size / 16000.0
        rms = float(np.sqrt(np.mean(np.square(a)))) if a.size else 0.0
    try: os.remove(tmp)
    except OSError: pass
    print("  [%s] packets=%d  dur_recomp=%.3fs  rms=%.4f  -> %s" %
          (tag, len(audio), dur, rms, "OK" if (ok and rms > 0) else "FALLO"))
    return ok and rms > 0


def tally(d, label):
    files = sorted(f for f in os.listdir(d) if f.lower().endswith(".bop"))
    sizes = [os.path.getsize(os.path.join(d, f)) for f in files]
    # re-parse todos para asegurar estructura y duracion
    bad = []
    tot_packets = 0
    for f in files:
        try:
            sr, flags, pk = read_bop(os.path.join(d, f))
            if sr != 16000 or flags != 0 or len(pk) < 10 or max(map(len, pk)) > 256:
                bad.append(f)
            tot_packets += len(pk)
        except Exception as e:
            bad.append("%s(%s)" % (f, e))
    print("[%s] archivos .bop=%d  bytes=%d (%.2f MB)  packets=%d  dur=%.2fs  estructura_mala=%d" %
          (label, len(files), sum(sizes), sum(sizes)/1048576, tot_packets,
           tot_packets*0.02, len(bad)))
    if bad:
        print("   MALOS:", bad)
    return len(files), sum(sizes), tot_packets*0.02


if __name__ == "__main__":
    print("=== TALLY EN DISCO ===")
    n1, b1, d1 = tally(ES_OUT, "ES")
    n2, b2, d2 = tally(EN_OUT, "EN")
    n = n1 + n2; b = b1 + b2; dur = d1 + d2
    print("TOTAL: archivos=%d  bytes=%d (%.2f MB)  duracion=%.2fs  media/frasa=%.1f bytes" %
          (n, b, b/1048576, dur, b/n if n else 0))
    print("\n=== RECOMPOSICION (A01) ===")
    r1 = recompose_and_decode(os.path.join(ES_OUT, "A01.bop"), "ES-A01")
    r2 = recompose_and_decode(os.path.join(EN_OUT, "A01.bop"), "EN-A01")
    print("RECOMPOSICION:", "OK" if (r1 and r2) else "FALLO")
