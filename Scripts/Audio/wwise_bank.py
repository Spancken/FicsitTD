# 0.69.5: Soundbank "FicsitTD" mit allen Play_TD_*-Events (SML-Doku Audio: Events muessen in einer Soundbank stecken).
import json, sys, time, urllib.request

URL = "http://127.0.0.1:8095/waapi"

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
        waapi("ak.wwise.core.getInfo"); break
    except Exception:
        time.sleep(2)
else:
    sys.exit("WAAPI-Server antwortet nicht")

bank = waapi("ak.wwise.core.object.create", {"parent": r"\SoundBanks\Default Work Unit", "type": "SoundBank", "name": "FicsitTD", "onNameConflict": "merge"})
print("Bank:", bank)
events = waapi("ak.wwise.core.object.get", {"waql": "$ from type Event where name : \"Play_TD_\""}, {"return": ["name", "id", "path"]}).get("return", [])
print("Events:", [e["name"] for e in events])
r = waapi("ak.wwise.core.soundbank.setInclusions", {"soundbank": r"\SoundBanks\Default Work Unit\FicsitTD", "operation": "add",
          "inclusions": [{"object": e["id"], "filter": ["events", "structures", "media"]} for e in events]})
print("Inclusions:", r)
waapi("ak.wwise.core.project.save")
print("gespeichert")
