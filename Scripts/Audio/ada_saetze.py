# 0.12-dev (Marius 16.11.: "ADAs Ansagen passen"): Saetze der ADA-Ansagen aus Source/FicsitTD/Private/TDAda.cpp (eine Quelle)
# als saetze_ada_<Key>.json fuer tts_erzeugen.ps1 / vo_filter.py schreiben. Gibt die Schluessel zeilenweise aus.
import json, os, re

HIER = os.path.dirname(os.path.abspath(__file__))
QUELLE = os.path.join(HIER, "..", "..", "Source", "FicsitTD", "Private", "TDAda.cpp")
src = open(QUELLE, encoding="utf-8").read()
eintrag = re.compile(r'\{\s*TEXT\("([A-Za-z0-9]+)"\),\s*TEXT\("[^"]*"\),\s*TEXT\("((?:[^"\\]|\\.)*)"\),\s*TEXT\("((?:[^"\\]|\\.)*)"\)\s*\}', re.S)
for key, de, en in eintrag.findall(src):
    de, en = de.replace('\\"', '"'), en.replace('\\"', '"')
    with open(os.path.join(HIER, f"saetze_ada_{key}.json"), "w", encoding="utf-8") as fh:
        json.dump({"de": [de], "en": [en]}, fh, ensure_ascii=False, indent=1)
    print(key)
