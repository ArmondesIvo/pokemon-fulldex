#!/usr/bin/env python3
"""The arena's moving pictures: PMD SpriteCollab's (github.com/PMDCollab/SpriteCollab, CC BY-NC 4.0),
four animations of each species, packed one file a species for the app to fetch when it needs one.

Usage: fetch_pmd.py <out folder> [first] [last]      (species 1 to 809 by default)

Each <out>/NNNN.pmd:
    PMD1
    <Name> <frame width> <frame height> <rows> <durations, comma-separated> <png bytes>   (a line each)
    (an empty line)
    the PNGs, one after the other, in the same order
Names: Idle, Walk, Attack, Hurt (one missing takes another's: Idle Walk's, Hurt Idle's). Rows are
the eight directions, from facing down round to down-left, as SpriteCollab draws them (one when only
one). Durations are in frames of a 60-a-second clock.
"""
import concurrent.futures, os, sys, urllib.request, xml.etree.ElementTree as ET

BASE = "https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite/%04d/"
WANT = ["Idle", "Walk", "Attack", "Hurt"]


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "PokeHub-sprite-fetch"})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=30) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
        except Exception:
            pass
    return None


def png_size(data):
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")


def one(sp, out):
    path = os.path.join(out, "%04d.pmd" % sp)
    if os.path.exists(path):
        return sp, "kept"
    xml = get(BASE % sp + "AnimData.xml")
    if not xml:
        return sp, "none"
    anims = {}
    for a in ET.fromstring(xml).find("Anims"):
        name = a.findtext("Name")
        anims[name] = a
    def resolve(name, depth=0):
        a = anims.get(name)
        if a is None or depth > 4:
            return None, None
        copy = a.findtext("CopyOf")
        if copy:
            return resolve(copy, depth + 1)
        return name, a
    fallback = {"Idle": "Walk", "Hurt": "Idle", "Attack": "Walk", "Walk": "Idle"}
    lines, pngs = [], []
    for want in WANT:
        name, a = resolve(want)
        if a is None:
            name, a = resolve(fallback[want])
        if a is None:
            return sp, "missing " + want
        png = get(BASE % sp + name + "-Anim.png")
        if not png:
            return sp, "no png " + name
        fw, fh = int(a.findtext("FrameWidth")), int(a.findtext("FrameHeight"))
        durs = [int(d.text) for d in a.find("Durations")]
        w, h = png_size(png)
        rows = max(1, h // fh)
        lines.append("%s %d %d %d %s %d" % (want, fw, fh, rows, ",".join(map(str, durs)), len(png)))
        pngs.append(png)
    data = ("PMD1\n" + "\n".join(lines) + "\n\n").encode() + b"".join(pngs)
    with open(path + ".tmp", "wb") as f:
        f.write(data)
    os.replace(path + ".tmp", path)
    return sp, "ok %d" % len(data)


def main():
    out = sys.argv[1]
    first = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    last = int(sys.argv[3]) if len(sys.argv) > 3 else 809
    os.makedirs(out, exist_ok=True)
    bad = []
    with concurrent.futures.ThreadPoolExecutor(12) as ex:
        for sp, what in ex.map(lambda s: one(s, out), range(first, last + 1)):
            if not what.startswith(("ok", "kept")):
                bad.append((sp, what))
            if sp % 50 == 0:
                print(sp, what, flush=True)
    print("done; without:", bad)


main()
