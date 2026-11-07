# 0.83 Bauten P2/P3: Editor-Schritte nacheinander (headless). Danach Build-Mod.ps1 -Mod FicsitTD -SkipEditor.
# Voraussetzung: Editor mit den Klassen aus TDBautenErweitert.h und TDDimensionalDepot.h gebaut (Build-Mod.ps1 ohne -SkipEditor),
# Icons unter E:\Modding\Export\Icons\Icon_TD_<Bau>_512.png.
param([switch] $OhneForschung)
$ErrorActionPreference = "Stop"
$editor = "E:\Unreal 5\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projekt = "E:\Modding\SatisfactoryModLoader\FactoryGame.uproject"
$skripte = "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Scripts"
$inhalt = "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
$logs = "E:\Modding\Logs"
$bauten = "ConductorWall", "RadioMast", "ShieldGenerator", "FlakTurret", "FieldProjector", "SingularityLauncher", "DimensionalDepot"

function Schritt([string] $Name, [string] $Skript, [hashtable] $Umgebung = @{}) {
    foreach ($k in $Umgebung.Keys) { Set-Item "env:$k" $Umgebung[$k] }
    Write-Host "== $Name ($Skript)"
    $start = Get-Date
    & $editor $projekt -run=pythonscript -script="$skripte\$Skript" -unattended -nosplash *> "$logs\p2_$Name.log"
    foreach ($k in $Umgebung.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue }
    Write-Host ("   fertig in {0:N0} s, Exit {1}" -f ((Get-Date) - $start).TotalSeconds, $LASTEXITCODE)
}

foreach ($b in $bauten) {
    if (-not (Test-Path "E:\Modding\Export\Icons\Icon_TD_${b}_512.png")) { Write-Warning "Icon fehlt: Icon_TD_${b}_512.png" }
}
$nur = @{ FICSITTD_NUR = ($bauten -join ",") }
# alte Fassungen der neuen Bauten entfernen (Duplizieren scheitert sonst)
foreach ($b in $bauten) {
    $o = Join-Path $inhalt "Buildings\$b"
    if (Test-Path $o) { Get-ChildItem $o -Filter "*.uasset" | Where-Object { $_.Name -match '^(Build|Desc|Recipe)_TD_' } | Remove-Item -Force }
}
Schritt "modelle" "create_p1_bauten.py" ($nur + @{ FICSITTD_SCHRITT = "modelle" })
Schritt "gegenstaende" "create_ammo_items.py"
Schritt "gebaeude" "create_building.py" $nur
Schritt "bauplan" "setup_turret_blueprint.py" $nur
Schritt "einsatzrezepte" "create_einsatz_recipes.py"
Schritt "rezepte" "create_turret_recipes.py"
Schritt "lack" "create_factory_materials.py"
Schritt "menue" "create_p1_bauten.py" ($nur + @{ FICSITTD_SCHRITT = "menue" })
if (-not $OhneForschung) {
    $sicherung = "E:\Modding\Backup\Research_vor_083_" + (Get-Date -Format "yyyyMMdd_HHmm")
    New-Item -ItemType Directory -Force $sicherung | Out-Null
    Copy-Item (Join-Path $inhalt "Research\*") $sicherung -Recurse -Force
    Remove-Item (Join-Path $inhalt "Research\*") -Recurse -Force
    Write-Host "   Forschung gesichert nach $sicherung"
    Schritt "forschung" "create_research.py"
}
Write-Host "== Logs: $logs\create_p1_bauten_*.txt, create_ammo_items.txt, create_building.txt, setup_turret_blueprint.txt, create_einsatz_recipes.txt, create_turret_recipes.txt, create_research*.txt"
