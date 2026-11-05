# 0.69.5: Computerfilter fuer die TTS-Saetze (reines Python, kein numpy):
# Hochpass 280 Hz + Tiefpass 4800 Hz (Funkband), kurzer Kammfilter (metallisch), leichte Ringmodulation, kleiner Raum.
# Saetze mit Pausen zusammenfuegen, Zeitstempel je Satz fuer die Untertitel schreiben.
# Aufruf: python vo_filter.py [nachricht]   (Eingang: E:\Modding\Audio\FicsitTD\VO\<sprache>\<nachricht>\roh\NN.wav)
import json, math, os, struct, sys, wave

BASIS = r"E:\Modding\Audio\FicsitTD\VO"
NACHRICHT = sys.argv[1] if len(sys.argv) > 1 else "begruessung"
VORLAUF, PAUSE = 0.3, 0.5

def lies(pfad):
    with wave.open(pfad, "rb") as w:
        rate, n = w.getframerate(), w.getnframes()
        roh = w.readframes(n)
    return rate, [s / 32768.0 for s in struct.unpack("<%dh" % n, roh)]

def biquad(x, rate, art, f0, q=0.707):
    w0 = 2 * math.pi * f0 / rate
    a = math.sin(w0) / (2 * q)
    c = math.cos(w0)
    if art == "hp":
        b0, b1, b2 = (1 + c) / 2, -(1 + c), (1 + c) / 2
    else:
        b0, b1, b2 = (1 - c) / 2, 1 - c, (1 - c) / 2
    a0, a1, a2 = 1 + a, -2 * c, 1 - a
    b0, b1, b2, a1, a2 = b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0
    y, x1, x2, y1, y2 = [], 0.0, 0.0, 0.0, 0.0
    for s in x:
        o = b0 * s + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, s, y1, o
        y.append(o)
    return y

def filtern(x, rate):
    # Marius 04.11.: heller und freundlicher -> breiteres Band, weniger Metall und Modulation, weniger Hall
    x = biquad(x, rate, "hp", 180.0)
    x = biquad(x, rate, "lp", 8000.0)
    # Marius 04.11. (3. Probe): mehr Computer bei hellem, schnellem Klang -> zwei Kammfilter (3 ms und 1,6 ms), deutlichere Modulation
    y = list(x)
    for verz, anteil in ((0.003, 0.30), (0.0016, 0.18)):
        d = int(verz * rate)
        for i in range(len(y) - 1, d - 1, -1):
            y[i] += anteil * y[i - d]
    y = [s * (0.90 + 0.10 * math.sin(2 * math.pi * 85.0 * i / rate)) for i, s in enumerate(y)]
    # kleiner Raum (zwei Echos)
    for verz, anteil in ((0.027, 0.08), (0.047, 0.05)):
        k = int(verz * rate)
        y = [y[i] + (anteil * y[i - k] if i >= k else 0.0) for i in range(len(y))]
    return y

def schreibe(pfad, rate, x):
    spitze = max(1e-6, max(abs(s) for s in x))
    g = 0.89 / spitze   # -1 dBFS
    daten = struct.pack("<%dh" % len(x), *[int(max(-1.0, min(1.0, s * g)) * 32767) for s in x])
    with wave.open(pfad, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate); w.writeframes(daten)

for sprache in ("de", "en"):
    ordner = os.path.join(BASIS, sprache, NACHRICHT)
    roh = os.path.join(ordner, "roh")
    dateien = sorted(f for f in os.listdir(roh) if f.endswith(".wav"))
    rate, gesamt, zeiten = None, [], []
    for f in dateien:
        r, x = lies(os.path.join(roh, f))
        rate = rate or r
        if not gesamt:
            gesamt = [0.0] * int(VORLAUF * rate)
        zeiten.append(round(len(gesamt) / rate, 2))
        gesamt += filtern(x, rate) + [0.0] * int(PAUSE * rate)
    ziel = os.path.join(ordner, f"TD_{NACHRICHT.capitalize()}_{sprache}.wav")
    schreibe(ziel, rate, gesamt)
    with open(os.path.join(ordner, "zeiten.json"), "w", encoding="utf-8") as fh:
        json.dump({"zeiten": zeiten, "dauer": round(len(gesamt) / rate, 2)}, fh)
    print(sprache, ziel, f"{len(gesamt) / rate:.1f} s", zeiten)
