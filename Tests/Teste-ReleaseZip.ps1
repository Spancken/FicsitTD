<#
.SYNOPSIS
  Pruefstufe G5 (Satisfactory\Notizen\Release-Prozess.md): genau die Release-Zip fuer ficsit.app im Spiel testen.
  1. installierten Mod-Ordner beiseite legen (umbenennen), 2. Ordner Windows\ aus der Zip als Mod einsetzen,
  3. Teste-Mod.ps1 mit dem Paket ReleaseZip, 4. vorherigen Stand zurueck (auch bei Fehlern).
.EXAMPLE
  & "E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Tests\Teste-ReleaseZip.ps1"
#>
param(
    [string] $Zip = "E:\Modding\SatisfactoryModLoader\Saved\ArchivedPlugins\FicsitTD\FicsitTD.zip",
    [string] $Spiel = "C:\Program Files (x86)\Steam\steamapps\common\Satisfactory",
    [string] $Paket = "ReleaseZip"
)
$ErrorActionPreference = "Stop"
if (Get-Process FactoryGame* -ErrorAction SilentlyContinue) { throw "Das Spiel laeuft - erst beenden." }
if (-not (Test-Path $Zip)) { throw "Release-Zip fehlt: $Zip (erst Release-Mod.ps1)" }
$mod = Join-Path $Spiel "FactoryGame\Mods\FicsitTD"
$beiseite = "$mod.vorG5"
$entpackt = Join-Path $env:TEMP "FicsitTD_G5_$(Get-Date -Format yyyyMMddHHmmss)"
if (Test-Path $beiseite) { throw "$beiseite existiert noch (alter G5-Lauf?) - von Hand pruefen." }
Write-Host "== Zip: $Zip ($([int]((Get-Item $Zip).Length / 1MB)) MB, $((Get-Item $Zip).LastWriteTime))"
Expand-Archive -Path $Zip -DestinationPath $entpackt
$quelle = Join-Path $entpackt "Windows"
if (-not (Test-Path (Join-Path $quelle "FicsitTD.uplugin"))) { throw "In der Zip fehlt Windows\FicsitTD.uplugin" }
$version = (Get-Content (Join-Path $quelle "FicsitTD.uplugin") -Raw | ConvertFrom-Json).VersionName
Write-Host "== Version in der Zip: $version"
Rename-Item $mod $beiseite
$code = 1
try {
    Copy-Item $quelle $mod -Recurse
    & (Join-Path $PSScriptRoot "Teste-Mod.ps1") -Paket $Paket
    $code = $LASTEXITCODE
} finally {
    if (Get-Process FactoryGame* -ErrorAction SilentlyContinue) { Start-Sleep 10 }
    Remove-Item $mod -Recurse -Force
    Rename-Item $beiseite $mod
    Remove-Item $entpackt -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "== Build-Stand wiederhergestellt: $mod"
}
Write-Host "== G5 mit Zip-Version $version : $(if ($code -eq 0) { 'bestanden' } elseif ($code -eq 2) { 'UNGUELTIG' } else { 'FEHLER' })"
exit $code
