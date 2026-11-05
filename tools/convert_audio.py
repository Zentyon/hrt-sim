#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Convierte audios .ogg (TTS ES/EN) a Opus 16 kHz 8 kbps mono 20 ms y los empaqueta
en un contenedor binario propio ".bop" listo para LittleFS de ESP32.

Formato .bop:
  magic  "BOP1" (4 bytes ASCII)
  u16 LE sampleRate (=16000)
  u16 LE flags      (=0)
  N veces: u16 LE packetLen + payload bytes
  EOF = fin de stream
"""
import os
import sys
import json
import struct
import subprocess
import tempfile
import shutil
from concurrent.futures import ThreadPoolExecutor, as_completed

import numpy as np
import soundfile as sf

FFMPEG = r"C:\Users\Zent\AppData\Local\Temp\opencode\ffmpeg\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe"
FFPROBE = os.path.join(os.path.dirname(FFMPEG), "ffprobe.exe")
PYTHON = sys.executable

ES_IN  = r"C:\Users\Zent\Downloads\AUDIOS PROYECTO ESPAÑOL REN\audios_tts_es"
EN_IN  = r"C:\Users\Zent\Downloads\AUDIOS PROYECTO INGLES REN\audio_ogg_english"
ES_OUT = r"C:\Users\Zent\Documents\hrt-sim\data\v\es"
EN_OUT = r"C:\Users\Zent\Documents\hrt-sim\data\v\en"

SAMPLE_RATE = 16000
FRAME_S     = 0.02          # 20 ms por packet
MAX_PACKET  = 256
MAX_PAR     = 8             # max 8 en paralelo

FF_CMD = [
    FFMPEG, "-y", "-hide_banner", "-loglevel", "error",
    "-i", "{inn}",
    "-c:a", "libopus", "-b:a", "8000", "-ac", "1", "-ar", "16000",
    "-frame_duration", "20", "-application", "voip", "-vbr", "on",
    "{out}",
]


def parse_ogg_packets(path):
    """Parsea el contenedor Ogg y devuelve la lista de packets Opus en orden,
    manejando packets que cruzan páginas (segmentos lacing 255 = continúa)."""
    with open(path, "rb") as f:
        data = f.read()
    packets = []
    partial = bytearray()
    pos = 0
    n = len(data)
    while pos < n:
        idx = data.find(b"OggS", pos)
        if idx == -1 or idx + 27 > n:
            break
        # version = data[idx+4]  (debe ser 0)
        # header_type = data[idx+5]
        # granule = struct.unpack_from('<q', data, idx+6)[0]
        # serial  = struct.unpack_from('<I', data, idx+14)[0]
        # seq     = struct.unpack_from('<I', data, idx+18)[0]
        # crc     = struct.unpack_from('<I', data, idx+22)[0]
        nsegs = data[idx + 26]
        seg_start = idx + 27
        if seg_start + nsegs > n:
            break
        seg_table = data[seg_start:seg_start + nsegs]
        payload_start = seg_start + nsegs
        payload_len = sum(seg_table)
        payload = data[payload_start:payload_start + payload_len]
        off = 0
        for lac in seg_table:
            partial.extend(payload[off:off + lac])
            off += lac
            if lac < 255:          # fin de packet
                packets.append(bytes(partial))
                partial = bytearray()
        pos = payload_start + payload_len
    return packets


def write_bop(audio_packets, path):
    with open(path, "wb") as f:
        f.write(b"BOP1")
        f.write(struct.pack("<HH", SAMPLE_RATE, 0))
        for p in audio_packets:
            f.write(struct.pack("<H", len(p)))
            f.write(p)


def read_bop(path):
    """Relee y valida la estructura de un .bop. Devuelve (sample_rate, packets)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"BOP1":
        raise ValueError("magic incorrecto")
    sr, flags = struct.unpack_from("<HH", data, 4)
    pos = 8
    packets = []
    while pos < len(data):
        if pos + 2 > len(data):
            raise ValueError(" truncado en longitud de packet")
        ln = struct.unpack_from("<H", data, pos)[0]
        pos += 2
        if pos + ln > len(data):
            raise ValueError(" truncado en payload")
        packets.append(data[pos:pos + ln])
        pos += ln
    return sr, flags, packets


def get_duration(path):
    """Duración en segundos. Prioriza soundfile; si libsndfile no puede parsear
    el .ogg (algunos TTS vorbis lo marcan como 'malformed'), usa ffprobe."""
    try:
        info = sf.info(path)
        return info.frames / info.samplerate
    except Exception:
        p = subprocess.run(
            [FFPROBE, "-v", "error", "-show_entries", "format=duration",
             "-of", "default=noprint_wrappers=1:nokey=1", path],
            capture_output=True, text=True)
        s = (p.stdout or "").strip()
        if p.returncode == 0 and s:
            return float(s)
        raise RuntimeError("no pude obtener duracion de %s" % path)


def decode_rms(path):
    """RMS decodificando a PCM s16le con ffmpeg (robusto, sin depender de libsndfile)."""
    p = subprocess.run(
        [FFMPEG, "-v", "error", "-i", path, "-f", "s16le", "-ac", "1", "-"],
        capture_output=True)
    if p.returncode != 0 or len(p.stdout) < 2:
        raise RuntimeError("ffmpeg decode fallo: %s" % (p.stderr or b"").decode()[:200])
    a = np.frombuffer(p.stdout, dtype="<i2").astype(np.float64) / 32768.0
    return float(np.sqrt(np.mean(np.square(a)))) if a.size else 0.0


def process_file(in_path, out_bop, tmpdir):
    stem = os.path.splitext(os.path.basename(in_path))[0]
    tmp_ogg = os.path.join(tmpdir, stem + ".opus.ogg")
    result = {"id": stem, "in": in_path, "out": out_bop, "ok": True, "err": None}
    try:
        orig_dur = get_duration(in_path)
        # 1) codificar
        cmd = [c.replace("{inn}", in_path).replace("{out}", tmp_ogg) for c in FF_CMD]
        p = subprocess.run(cmd, capture_output=True, text=True)
        if p.returncode != 0 or not os.path.exists(tmp_ogg):
            result["ok"] = False
            result["err"] = "ffmpeg rc=%d: %s" % (p.returncode, (p.stderr or "").strip()[:300])
            return result
        # 2) parsear packets Opus
        all_pk = parse_ogg_packets(tmp_ogg)
        if len(all_pk) < 2:
            result["ok"] = False
            result["err"] = "menos de 2 packets (head/tags)"
            return result
        # descartar OpusHead y OpusTags
        if not all_pk[0].startswith(b"OpusHead"):
            result["ok"] = False; result["err"] = "packet0 no es OpusHead"; return result
        if not all_pk[1].startswith(b"OpusTags"):
            result["ok"] = False; result["err"] = "packet1 no es OpusTags"; return result
        audio = all_pk[2:]
        # 3) validar
        if len(audio) < 10:
            result["ok"] = False; result["err"] = "<10 packets (%d)" % len(audio); return result
        maxlen = max(len(x) for x in audio)
        if maxlen > MAX_PACKET:
            result["ok"] = False; result["err"] = "packetLen max %d > %d" % (maxlen, MAX_PACKET); return result
        bop_dur = len(audio) * FRAME_S
        if orig_dur <= 0 or abs(bop_dur - orig_dur) / orig_dur > 0.15:
            result["ok"] = False
            result["err"] = "dur mismatch bop=%.3f orig=%.3f" % (bop_dur, orig_dur)
            return result
        # decodificar y comprobar RMS > 0 (via ffmpeg, robusto)
        rms = decode_rms(tmp_ogg)
        if rms <= 0:
            result["ok"] = False; result["err"] = "RMS=0"; return result
        # 4) escribir .bop
        write_bop(audio, out_bop)
        # 5) re-leer .bop para validar estructura
        rsr, rflags, rpk = read_bop(out_bop)
        if rsr != SAMPLE_RATE or rflags != 0 or len(rpk) != len(audio):
            result["ok"] = False; result["err"] = "relectura .bop inconsistente"; return result
        result.update({"orig_dur": orig_dur, "bop_dur": bop_dur, "n_packets": len(audio),
                       "max_len": maxlen, "rms": rms, "bop_size": os.path.getsize(out_bop)})
        return result
    except Exception as e:
        result["ok"] = False
        result["err"] = "%s: %s" % (type(e).__name__, e)
        return result
    finally:
        try:
            if os.path.exists(tmp_ogg):
                os.remove(tmp_ogg)
        except OSError:
            pass


def run_language(in_dir, out_dir, label):
    os.makedirs(out_dir, exist_ok=True)
    files = sorted(f for f in os.listdir(in_dir) if f.lower().endswith(".ogg"))
    # validar ids contra manifiesto si existe
    manifest_path = os.path.join(in_dir, "manifiesto.json")
    if os.path.exists(manifest_path):
        try:
            with open(manifest_path, "r", encoding="utf-8") as mf:
                mj = json.load(mf)
            mids = {a["id"] for a in mj.get("archivos", [])}
            fids = {os.path.splitext(f)[0] for f in files}
            missing = mids - fids
            extra = fids - mids
            print("[%s] manifiesto: %d ids; faltan=%s sobran=%s" %
                  (label, len(mids), sorted(missing) or "-", sorted(extra) or "-"))
        except Exception as e:
            print("[%s] aviso: no pude leer manifiesto: %s" % (label, e))
    else:
        print("[%s] sin manifiesto.json; validando solo por extension .ogg" % label)

    tmpdir = tempfile.mkdtemp(prefix="bop_", dir=r"C:\Users\Zent\AppData\Local\Temp\opencode")
    results = []
    print("[%s] procesando %d archivos con %d workers..." % (label, len(files), MAX_PAR))
    try:
        with ThreadPoolExecutor(max_workers=MAX_PAR) as ex:
            futs = {ex.submit(process_file, os.path.join(in_dir, f),
                              os.path.join(out_dir, os.path.splitext(f)[0] + ".bop"),
                              tmpdir): f for f in files}
            done = 0
            for fut in as_completed(futs):
                results.append(fut.result())
                done += 1
                if done % 25 == 0 or done == len(files):
                    print("[%s] %d/%d" % (label, done, len(files)))
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)
    return results


def main():
    for p in (FFMPEG,):
        if not os.path.exists(p):
            print("ERROR: ffmpeg no existe:", p); sys.exit(1)
    os.makedirs(ES_OUT, exist_ok=True)
    os.makedirs(EN_OUT, exist_ok=True)

    summary = {}
    allres = {}
    for label, indir, outdir in (("ES", ES_IN, ES_OUT), ("EN", EN_IN, EN_OUT)):
        res = run_language(indir, outdir, label)
        allres[label] = res
        ok = [r for r in res if r["ok"]]
        bad = [r for r in res if not r["ok"]]
        total_bop = sum(r["bop_size"] for r in ok)
        total_dur = sum(r["bop_dur"] for r in ok)
        total_orig = sum(r["orig_dur"] for r in ok)
        summary[label] = {
            "total": len(res), "ok": len(ok), "failed": len(bad),
            "total_bop_bytes": total_bop, "total_bop_dur_s": total_dur,
            "total_orig_dur_s": total_orig,
            "failures": [{"id": r["id"], "err": r["err"]} for r in bad],
        }
        print("\n=== %s ===" % label)
        print("  archivos       : %d (ok=%d, fallidos=%d)" % (len(res), len(ok), len(bad)))
        print("  duracion total : %.2f s (bop) / %.2f s (origen)" % (total_dur, total_orig))
        print("  bytes .bop     : %d (%.2f MB)" % (total_bop, total_bop / 1048576))
        if bad:
            print("  FALLOS:")
            for b in bad:
                print("    %s -> %s" % (b["id"], b["err"]))

    comb_bytes = summary["ES"]["total_bop_bytes"] + summary["EN"]["total_bop_bytes"]
    n_ok = summary["ES"]["ok"] + summary["EN"]["ok"]
    print("\n=== GLOBAL ===")
    print("  total archivos ok : %d / %d" % (n_ok, summary["ES"]["total"] + summary["EN"]["total"]))
    print("  bytes combinados  : %d (%.2f MB)" % (comb_bytes, comb_bytes / 1048576))
    if n_ok:
        print("  tamanio medio/frase: %.1f bytes" % (comb_bytes / n_ok))
    # dump json
    with open(r"C:\Users\Zent\AppData\Local\Temp\opencode\bop_summary.json", "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=2)
    print("  resumen -> C:\\Users\\Zent\\AppData\\Local\\Temp\\opencode\\bop_summary.json")


if __name__ == "__main__":
    main()
