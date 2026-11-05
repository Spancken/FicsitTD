# 0.69.5: Sprachdateien der ADA-Nachrichten ins Wwise-Projekt (WAAPI per HTTP, Server: WwiseConsole waapi-server --http-port 8095).
# Legt je Sprache einen Sound (SFX, nicht lokalisiert) und ein Play-Event an, speichert das Projekt.
import json, sys, time, urllib.request

URL = "http://127.0.0.1:8095/waapi"
NACHRICHTEN = sys.argv[1:] or ["begruessung"]

def waapi(uri, args=None, options=None):
    body = json.dumps({"uri": uri, "args": args or {}, "options": options or {}}).encode("utf-8")
    req = urllib.request.Request(URL, data=body, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=120) as r:
            return json.loads(r.read().decode("utf-8") or "{}")
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"{uri}: {e.code} {e.read().decode('utf-8', 'replace')}")

for versuch in range(60):
    try:
        info = waapi("ak.wwise.core.getInfo")
        print("Wwise", info.get("version", {}).get("displayName"), "bereit")
        break
    except Exception as e:
        time.sleep(2)
else:
    sys.exit("WAAPI-Server antwortet nicht")

wurzeln = waapi("ak.wwise.core.object.get", {"waql": "$ from type WorkUnit where parent = null or parent.type = \"Folder\""}, {"return": ["name", "path", "type"]})
print("Wurzeln:", [o.get("path") for o in wurzeln.get("return", [])][:20])

for n in NACHRICHTEN:
    name = "TD_" + n.capitalize()
    imports = []
    for sprache in ("de", "en"):
        imports.append({
            "audioFile": rf"E:\Modding\Audio\FicsitTD\VO\{sprache}\{n}\{name}_{sprache}.wav",
            "objectPath": rf"\Containers\Default Work Unit\<Folder>FicsitTD\<Sound>{name}_{sprache}",
            "event": rf"\Events\Default Work Unit\<Folder>FicsitTD\<Event>Play_{name}_{sprache}@Play",
        })
    r = waapi("ak.wwise.core.audio.import", {"importOperation": "replaceExisting", "default": {"importLanguage": "SFX"}, "imports": imports},
              {"return": ["name", "path", "id"]})
    print(n, json.dumps(r)[:800])

ev = waapi("ak.wwise.core.object.get", {"waql": "$ from type Event where name : \"Play_TD_\""}, {"return": ["name", "id", "shortId", "path"]})
print("Events:", json.dumps(ev.get("return", []), ensure_ascii=False))
waapi("ak.wwise.core.project.save")
print("gespeichert")
