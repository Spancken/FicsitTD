# 0.9.3 Drohnenplatz (ersetzt die Reparaturstation) und Radar: kompletter Ablauf ohne Spiel.
# 1) Editor mit den neuen Klassen bauen  2) Gebaeude, Rezepte, Menue anlegen  3) Forschung neu  4) gebackene Modelle  5) paketieren
$ErrorActionPreference = "Stop"
$editor = "E:\Unreal 5\Unreal Engine - CSS\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$projekt = "E:\Modding\SatisfactoryModLoader\FactoryGame.uproject"
$skripte = "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Scripts"
$inhalt = "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Content"
$logs = "E:\Modding\Logs"
$build = "D:\OneDrive\Claude\Claude Modding\.claude\skills\satisfactory-modding\scripts\Build-Mod.ps1"

function Schritt([string] $Name, [string] $Skript, [hashtable] $Umgebung = @{}) {
    foreach ($k in $Umgebung.Keys) { Set-Item "env:$k" $Umgebung[$k] }
    Write-Host "== $Name ($Skript)"
    & $editor $projekt -run=pythonscript -script="$skripte\$Skript" -unattended -nosplash *> "$logs\dp_$Name.log"
    foreach ($k in $Umgebung.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue }
}

Write-Host "== Editor bauen"
& $build -Mod FicsitTD *> "$logs\dp_build1.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$logs\dp_build1.log" -Tail 20; throw "Build 1 fehlgeschlagen" }

$nur = @{ FICSITTD_NUR = "DronePort" }
$o = Join-Path $inhalt "Buildings\DronePort"
if (Test-Path $o) { Get-ChildItem $o -Filter "*.uasset" | Where-Object { $_.Name -match '^(Build|Desc|Recipe)_TD_' } | Remove-Item -Force }
Schritt "modelle" "create_p1_bauten.py" ($nur + @{ FICSITTD_SCHRITT = "modelle" })
Schritt "gebaeude" "create_building.py" $nur
Schritt "bauplan" "setup_turret_blueprint.py" $nur
Schritt "rezepte" "create_turret_recipes.py"
Schritt "lack" "create_factory_materials.py"
Schritt "menue" "create_p1_bauten.py" ($nur + @{ FICSITTD_SCHRITT = "menue" })

$sicherung = "E:\Modding\Backup\Research_vor_093d"
New-Item -ItemType Directory -Force $sicherung | Out-Null
Copy-Item (Join-Path $inhalt "Research\*") $sicherung -Recurse -Force
Remove-Item (Join-Path $inhalt "Research\*") -Recurse -Force
Schritt "forschung" "create_research.py"

# gebackene Modelle ersetzen die rohen (der Import ueberschreibt bestehende Modelle nicht)
Get-ChildItem (Join-Path $o "Mesh") -Filter "SM_TD_DronePort_*.uasset" -ErrorAction SilentlyContinue | Remove-Item -Force
Schritt "gebacken" "import_gebacken.py" @{ TD_GEBACKEN_NUR = "DronePort" }
Schritt "bauplan2" "setup_turret_blueprint.py" $nur

Write-Host "== paketieren"
& $build -Mod FicsitTD -SkipEditor *> "$logs\dp_build2.log"
if ($LASTEXITCODE -ne 0) { Get-Content "$logs\dp_build2.log" -Tail 20; throw "Build 2 fehlgeschlagen" }
Write-Host "FERTIG"
