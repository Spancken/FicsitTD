# 0.12-dev: ADA-Ansagen (TDAda.cpp) mit Stimme: Saetze -> Windows-Stimmen (TTS) -> Computerfilter -> Import im Editor.
# Danach Build-Mod.ps1 (paketiert die neuen Sounds). Kein Spiel noetig; der Editor-Lauf braucht einige Minuten.
$ErrorActionPreference = "Stop"
$hier = $PSScriptRoot
$editor = "E:\Unreal 5\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projekt = "E:\Modding\SatisfactoryModLoader\FactoryGame.uproject"
$schluessel = & python (Join-Path $hier "ada_saetze.py")
if ($LASTEXITCODE -ne 0 -or -not $schluessel) { throw "Saetze aus TDAda.cpp nicht gelesen" }
Write-Host "== $($schluessel.Count) Ansagen: $($schluessel -join ', ')"
foreach ($k in $schluessel) {
    & (Join-Path $hier "tts_erzeugen.ps1") -Nachricht "ada_$k" | Out-Null
    & python (Join-Path $hier "vo_filter.py") "ada_$k"
    if ($LASTEXITCODE -ne 0) { throw "Filter fuer $k fehlgeschlagen" }
}
Write-Host "== Import im Editor"
& $editor $projekt -run=pythonscript -script="$(Split-Path $hier)\import_ada_voice.py" -unattended -nosplash *> "E:\Modding\Logs\import_ada_voice_editor.log"
Get-Content "E:\Modding\Logs\import_ada_voice.txt" -Tail 3
