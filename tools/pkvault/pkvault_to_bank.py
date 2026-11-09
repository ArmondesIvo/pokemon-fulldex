#!/usr/bin/env python3
"""PKVault's boxes into the Hub's Bank (MY PC's Bank, frontend/pokemon_hub.cpp).

PKVault (github.com/Chnapy/PKVault, 2.x) keeps its Pokemon as PKHeX files (storage/<gen>/*.pkN)
and where each one is (its boxes and places) in a SQLite database, db/pkvault.db. Its backups
(backup/*.zip) hold the same: db/pkvault.db and main/<gen>/*.pkN.

The Bank is a folder of the same kind of files, one a place: <bank>/box-07/12.pk4 (box 7,
place 12, a Gen 4 Pokemon), and box-NN/name.txt for a box's name. Each PKVault box becomes a
Bank box, with its name and its Pokemon in the same places, after the Bank boxes already used.

Only each Pokemon's main version is brought (PKVault keeps converted copies of one beside it), and
none still attached to a save (those are in that game already). Generations 1 to 7 only: the Hub's.

    pkvault_to_bank.py <PKVault folder or backup .zip> <bank folder> [--dry-run]
"""
import argparse
import os
import re
import sqlite3
import sys
import tempfile
import zipfile

BANK_BOXES = 50


def used_boxes(bank):
    """The Bank boxes that have a Pokemon in them now (0-based)."""
    used = set()
    if not os.path.isdir(bank):
        return used
    for d in os.listdir(bank):
        m = re.fullmatch(r"box-(\d+)", d)
        if m and any(re.fullmatch(r"\d\d\.pk[1-7]", f) for f in os.listdir(os.path.join(bank, d))):
            used.add(int(m.group(1)) - 1)
    return used


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("source", help="PKVault's folder (with db/ and storage/) or one of its backups (.zip)")
    ap.add_argument("bank", help="the Bank's folder: <saves>/bank")
    ap.add_argument("--dry-run", action="store_true", help="only say what would be written")
    a = ap.parse_args()

    tmp = None
    zf = None
    if zipfile.is_zipfile(a.source):
        zf = zipfile.ZipFile(a.source)
        tmp = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        tmp.write(zf.read("db/pkvault.db"))
        tmp.close()
        dbpath = tmp.name
    else:
        dbpath = os.path.join(a.source, "db", "pkvault.db")
    if not os.path.isfile(dbpath):
        sys.exit(f"No PKVault database at {dbpath}")
    db = sqlite3.connect(f"file:{dbpath}?immutable=1", uri=True)

    def data_of(filepath, blob):
        if blob:
            return bytes(blob)
        if zf is not None:
            # (In a backup: main/<its folder's name>/<its name>.)
            parts = filepath.replace("\\", "/").split("/")
            try:
                return zf.read(f"main/{parts[-2]}/{parts[-1]}")
            except KeyError:
                return None
        p = filepath if os.path.isabs(filepath) else os.path.join(a.source, filepath)
        try:
            with open(p, "rb") as f:
                return f.read()
        except OSError:
            return None

    boxes = db.execute('SELECT Id, Name FROM Boxes WHERE Type = 0 ORDER BY "Order"').fetchall()
    used = used_boxes(a.bank)
    free = [b for b in range(BANK_BOXES) if b not in used]
    if used:
        # (After the last box used: the player's own Bank boxes stay first and as they are.)
        free = [b for b in free if b > max(used)]
    written = skipped = 0
    problems = []
    for box_id, box_name in boxes:
        rows = db.execute(
            "SELECT v.BoxSlot, v.Filepath, v.Species, f.Data FROM PkmVersions v "
            "LEFT JOIN PkmFiles f ON f.Filepath = v.Filepath "
            "WHERE v.BoxId = ? AND v.IsMain = 1 AND v.AttachedSaveId IS NULL ORDER BY v.BoxSlot",
            (box_id,),
        ).fetchall()
        if not rows:
            continue
        if not free:
            problems.append(f"no Bank box free for {box_name!r}")
            skipped += len(rows)
            continue
        dest = free.pop(0)
        bdir = os.path.join(a.bank, f"box-{dest + 1:02d}")
        print(f"{box_name!r} -> Bank box {dest + 1}")
        if not a.dry_run:
            os.makedirs(bdir, exist_ok=True)
            with open(os.path.join(bdir, "name.txt"), "w", encoding="utf-8") as f:
                f.write(box_name + "\n")
        places = list(range(30))
        for slot, filepath, species, blob in rows:
            m = re.search(r"\.pk([1-7])$", filepath, re.I)
            if not m:
                problems.append(f"{os.path.basename(filepath)}: not Gen 1 to 7")
                skipped += 1
                continue
            data = data_of(filepath, blob)
            if not data:
                problems.append(f"{os.path.basename(filepath)}: its file is missing")
                skipped += 1
                continue
            place = slot if slot in places else (places[0] if places else None)
            if place is None:
                problems.append(f"{os.path.basename(filepath)}: the box is full")
                skipped += 1
                continue
            places.remove(place)
            out = os.path.join(bdir, f"{place + 1:02d}.pk{m.group(1)}")
            print(f"   {place + 1:2d}: {os.path.basename(filepath)}")
            if not a.dry_run:
                with open(out, "wb") as f:
                    f.write(data)
            written += 1
    if tmp:
        os.unlink(tmp.name)
    print(f"{written} Pokemon {'would be ' if a.dry_run else ''}written, {skipped} left out.")
    for p in problems:
        print("  " + p)


if __name__ == "__main__":
    main()
