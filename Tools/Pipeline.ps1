<#
.SYNOPSIS
  Deploy-Strasse FicsitTD (ENTWICKLUNG.md): Build, Pruefungen, Simulation aller Modi, Selbsttests, Release-Zip.
.EXAMPLE
  & E:\Modding\SatisfactoryModLoader\Mods\FicsitTD\Tools\Pipeline.ps1 -Stufe Schnell
  & ...\Pipeline.ps1 -Stufe Voll
  & ...\Pipeline.ps1 -Stufe Release
  & ...\Pipeline.ps1 -Stufe Schnell -OhneSpiel     # Rechner belegt: nur Build, Pruefungen, Simulation
.NOTES
  Bericht: Satisfactory\Testberichte\<Zeit>_Pipeline_<Stufe>.md mit DEPLOYBAR ja/nein.
  Exit 0 = alle Schritte gruen, 1 = mindestens ein Schritt rot, 2 = ungueltig (Bildrate, Spiel lief schon).
#>
param(
    [ValidateSet("Schnell", "Voll", "Release")] [string] $Stufe = "Schnell",
    [switch] $OhneSpiel,
    [switch] $MitEditor
)
$ErrorActionPreference = "Continue"
$Mod = Split-Path $PSScriptRoot -Parent
$Skill = "D:\OneDrive\Claude\Claude Modding\.claude\skills\satisfactory-modding\scripts"
$Berichte = "D:\OneDrive\Claude\Claude Modding\Satisfactory\Testberichte"
$Stempel = Get-Date -Format "yyyy-MM-dd_HHmm"
$Aus = Join-Path $Mod "Saved\Pipeline\$Stempel"
New-Item -ItemType Directory -Force $Aus | Out-Null
$Schritte = New-Object System.Collections.Generic.List[object]
$Offen = New-Object System.Collections.Generic.List[string]

function Schritt([string] $Name, [scriptblock] $Tun) {
    $start = Get-Date
    Write-Host "== $Name"
    $ergebnis = & $Tun
    $ok = [bool]$ergebnis.Ok
    $Schritte.Add([pscustomobject]@{ Name = $Name; Ok = $ok; Ungueltig = [bool]$ergebnis.Ungueltig; Ist = $ergebnis.Ist; Minuten = [math]::Round(((Get-Date) - $start).TotalMinutes, 1) })
    Write-Host ("   -> " + $(if ($ok) { "ok" } elseif ($ergebnis.Ungueltig) { "UNGUELTIG" } else { "ROT" }) + " " + $ergebnis.Ist)
}

function Testlauf([hashtable] $Argumente) {
    $t = Join-Path $Mod "Tests\Teste-Mod.ps1"
    $zeilen = & $t @Argumente *>&1 | ForEach-Object { "$_" }
    $code = $LASTEXITCODE
    $bericht = ($zeilen | Select-String "== Bericht: (.*)" | Select-Object -Last 1)
    $ergebnis = ($zeilen | Select-String "\*\*Ergebnis: (.*?)\*\*" | Select-Object -Last 1)
    [pscustomobject]@{ Ok = ($code -eq 0); Ungueltig = ($code -eq 2); Ist = "$(if ($ergebnis) { $ergebnis.Matches[0].Groups[1].Value }) $(if ($bericht) { '(' + (Split-Path $bericht.Matches[0].Groups[1].Value -Leaf) + ')' })" }
}

# ---- S0 Stand
$zweig = git -C $Mod rev-parse --abbrev-ref HEAD
$commit = git -C $Mod rev-parse --short HEAD
$sauber = -not (git -C $Mod status --porcelain)
$version = (Get-Content (Join-Path $Mod "FicsitTD.uplugin") -Raw | ConvertFrom-Json).SemVersion
Write-Host "== Pipeline $Stufe auf $zweig @ $commit, Version $version, $(if ($sauber) { 'sauber' } else { 'mit ungesicherten Aenderungen' })"
if ($Stufe -eq "Release" -and -not $sauber) { $Offen.Add("Release nur von einem sauberen Stand (alles committet)") }
if ($Stufe -eq "Release" -and $zweig -notin @("entwicklung", "master")) { $Offen.Add("Release nur von 'entwicklung' oder 'master' (jetzt '$zweig')") }

# ---- S1 G0 Build (fuehrt pruefe_mod.py aus)
Schritt "G0 Build + statische Pruefung" {
    $buildArgs = @{ Mod = "FicsitTD" }; if (-not $MitEditor) { $buildArgs.SkipEditor = $true }
    # 16.11.: Build-Mod wirft bei Fehlern (ErrorActionPreference Stop) - abfangen, sonst bricht die ganze Pipeline ohne Bericht ab
    $abbruch = $null
    $ausgabe = try { & (Join-Path $Skill "Build-Mod.ps1") @buildArgs *>&1 | ForEach-Object { "$_" } } catch { $abbruch = "$_"; @() }
    $ok = -not $abbruch -and $LASTEXITCODE -eq 0
    $ausgabe | Set-Content (Join-Path $Aus "build.log") -Encoding utf8
    $fehlerZeilen = $ausgabe | Select-String "FEHLER:| error " | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }
    if (-not $ok) { $script:BuildRot = $true }
    [pscustomobject]@{ Ok = $ok; Ist = $(if ($ok) { "BUILD SUCCESSFUL" } elseif ($abbruch) { "$abbruch (Log: E:\Modding\Logs\package-FicsitTD.log)" } else { ($fehlerZeilen -join " | ") }) }
}
if ($script:BuildRot) { $Offen.Add("Build rot - Selbsttests uebersprungen (sie wuerden den alten Stand pruefen)"); $OhneSpiel = $true }

# ---- S2 Konsistenz Code/Doku/Simulation/Tests
Schritt "Konsistenz (Zettel, Doku, Simulation, Experimente$(if ($Stufe -eq 'Release') { ', Versionen' }))" {
    $ausgabe = & python (Join-Path $Mod "Tests\pruefe_konsistenz.py") --stufe $Stufe.ToLower() 2>&1 | ForEach-Object { "$_" }
    $ausgabe | Set-Content (Join-Path $Aus "konsistenz.txt") -Encoding utf8
    $ausgabe | Select-String "^(FEHLER|HINWEIS):" | ForEach-Object { $Offen.Add($_.Line) }
    [pscustomobject]@{ Ok = ($LASTEXITCODE -eq 0); Ist = ($ausgabe | Select-Object -First 1) }
}

# ---- S3 Simulation aller Modi
Schritt "Simulation aller Spielmodi" {
    $sim = Join-Path $Mod "Tools\Simulator\sim_alle_modi.py"
    if (-not (Test-Path $sim)) { return [pscustomobject]@{ Ok = $false; Ist = "sim_alle_modi.py fehlt" } }
    $ausgabe = & python $sim --aus (Join-Path $Aus "Simulation") 2>&1 | ForEach-Object { "$_" }
    $ausgabe | Set-Content (Join-Path $Aus "simulation.txt") -Encoding utf8
    $code = $LASTEXITCODE
    if ($code -eq 1) { $Offen.Add("Simulation: Zielband verletzt (siehe $Aus\Simulation\sim_ergebnis.md)") }
    if ($code -ge 2) { $Offen.Add("Simulation: Modell passt nicht zum Code (simulation.txt)") }
    [pscustomobject]@{ Ok = ($code -eq 0); Ist = ($ausgabe | Select-Object -Last 1) }
}

# ---- S4 Selbsttests
if ($OhneSpiel) {
    $Offen.Add("Selbsttests uebersprungen (-OhneSpiel) - nicht deploybar")
} else {
    Schritt "G1 Paket Schnell (je Spielmodus ein Plan)" { Testlauf @{ Paket = "Schnell" } }
    if ($Stufe -ne "Schnell") {
        Schritt "G2 Regression Release (DE)" { Testlauf @{ Paket = "Release" } }
        Schritt "G2 Regression ReleaseEn (EN)" { Testlauf @{ Paket = "ReleaseEn"; Sprache = "en" } }
    }
    if ($Stufe -eq "Release") {
        Schritt "G3 Langlauf ReleaseLang" { Testlauf @{ Paket = "ReleaseLang" } }
        Schritt "Release-Zip bauen" {
            $ausgabe = & (Join-Path $Skill "Release-Mod.ps1") -Mod FicsitTD *>&1 | ForEach-Object { "$_" }
            $ausgabe | Set-Content (Join-Path $Aus "release-zip.log") -Encoding utf8
            [pscustomobject]@{ Ok = ($LASTEXITCODE -eq 0); Ist = ($ausgabe | Select-Object -Last 1) }
        }
        Schritt "G5 Release-Zip im Spiel" {
            $zeilen = & (Join-Path $Mod "Tests\Teste-ReleaseZip.ps1") *>&1 | ForEach-Object { "$_" }
            [pscustomobject]@{ Ok = ($LASTEXITCODE -eq 0); Ungueltig = ($LASTEXITCODE -eq 2); Ist = ($zeilen | Select-String "Ergebnis" | Select-Object -Last 1) }
        }
        $Offen.Add("G4 Handspiel Marius (neues Spiel + alter Spielstand) - Handarbeit")
        $Offen.Add("G6 Merge nach master, Tag v$version, Push und Upload nur mit Marius' Freigabe")
    }
}

# ---- Bericht
$alleOk = -not ($Schritte | Where-Object { -not $_.Ok })
$ungueltig = [bool]($Schritte | Where-Object { $_.Ungueltig })
$deploybar = $Stufe -eq "Release" -and $alleOk -and -not $OhneSpiel -and -not ($Offen | Where-Object { $_ -like "FEHLER*" -or $_ -like "Release nur*" })
$z = @("# Pipeline $Stufe - $version ($zweig @ $commit)", "",
    "Start $Stempel, $(if ($sauber) { 'sauberer Stand' } else { 'mit ungesicherten Aenderungen' }). Ablauf: ``ENTWICKLUNG.md``. Rohdaten: ``$Aus``.", "",
    "**DEPLOYBAR: $(if ($deploybar) { 'ja (nach G4 und Freigabe)' } else { 'nein' })**$(if ($Stufe -ne 'Release') { ' - nur die Stufe Release kann deploybar sein' })", "",
    "| Schritt | Ergebnis | Ist | min |", "|---|---|---|---|")
foreach ($s in $Schritte) { $z += "| $($s.Name) | $(if ($s.Ok) { 'gruen' } elseif ($s.Ungueltig) { '**ungueltig**' } else { '**ROT**' }) | $(($s.Ist -replace '\|', '/')) | $($s.Minuten) |" }
if ($Offen.Count) { $z += "", "## Offen", ""; $Offen | ForEach-Object { $z += "- $_" } }
$bericht = Join-Path $Berichte "${Stempel}_Pipeline_$Stufe.md"
$z | Set-Content $bericht -Encoding utf8
Write-Host "== Bericht: $bericht"
Write-Host "**Pipeline ${Stufe}: $(if ($alleOk) { 'alle Schritte gruen' } else { 'ROT' }), DEPLOYBAR $(if ($deploybar) { 'ja' } else { 'nein' })**"
exit $(if ($ungueltig) { 2 } elseif ($alleOk) { 0 } else { 1 })
