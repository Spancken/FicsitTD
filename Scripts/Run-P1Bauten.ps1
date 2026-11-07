# 0.81 passive Bauten P1: Editor-Schritte nacheinander (headless). Danach Build-Mod.ps1 -Mod FicsitTD -SkipEditor.
# Voraussetzung: Editor mit den Klassen TDGate, TDLureBeacon, TDSlickPlate gebaut (Build-Mod.ps1 ohne -SkipEditor).
param([switch] $OhneForschung)
$ErrorActionPreference = "Stop"
$editor = "E:\Unreal 5\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projekt = "E:\Modding\SatisfactoryModLoader\FactoryGame.uproject"
$skripte = "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Scripts"
$inhalt = "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
$logs = "E:\Modding\Logs"

function Schritt([string] $Name, [string] $Skript, [hashtable] $Umgebung = @{}) {
    foreach ($k in $Umgebung.Keys) { Set-Item "env:$k" $Umgebung[$k] }
    Write-Host "== $Name ($Skript)"
    $start = Get-Date
    & $editor $projekt -run=pythonscript -script="$skripte\$Skript" -unattended -nosplash *> "$logs\p1_$Name.log"
    foreach ($k in $Umgebung.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue }
    Write-Host ("   fertig in {0:N0} s, Exit {1}" -f ((Get-Date) - $start).TotalSeconds, $LASTEXITCODE)
}

$nur = @{ FICSITTD_NUR = "Gate,LureBeacon,SlickPlate" }
# alte Fassungen der neuen Bauten entfernen (Duplizieren scheitert sonst)
foreach ($b in "Gate", "LureBeacon", "SlickPlate") {
    $o = Join-Path $inhalt "Buildings\$b"
    if (Test-Path $o) { Get-ChildItem $o -Filter "*.uasset" | Where-Object { $_.Name -match '^(Build|Desc|Recipe)_TD_' } | Remove-Item -Force }
}
Schritt "modelle" "create_p1_bauten.py" @{ FICSITTD_SCHRITT = "modelle" }
Schritt "gebaeude" "create_building.py" $nur
Schritt "bauplan" "setup_turret_blueprint.py" $nur
Schritt "rezepte" "create_turret_recipes.py"
Schritt "lack" "create_factory_materials.py"
Schritt "menue" "create_p1_bauten.py" @{ FICSITTD_SCHRITT = "menue" }
if (-not $OhneForschung) {
    $sicherung = "E:\Modding\Backup\Research_vor_081_" + (Get-Date -Format "yyyyMMdd_HHmm")
    New-Item -ItemType Directory -Force $sicherung | Out-Null
    Copy-Item (Join-Path $inhalt "Research\*") $sicherung -Recurse -Force
    Remove-Item (Join-Path $inhalt "Research\*") -Recurse -Force
    Write-Host "   Forschung gesichert nach $sicherung"
    Schritt "forschung" "create_research.py"
}
Write-Host "== Logs: $logs\create_p1_bauten_*.txt, create_building.txt, setup_turret_blueprint.txt, create_turret_recipes*.txt, create_factory_materials.txt, create_research*.txt"
