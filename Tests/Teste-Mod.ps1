<#
.SYNOPSIS
  FicsitTD-Selbsttest (Konzept-Testautomatik): Testpaket oder einzelnen Plan im Spiel abarbeiten lassen, Sammelbericht schreiben.
.EXAMPLE
  .\Teste-Mod.ps1                          # Paket "Alles" (alle Plaene in einem Spielstart)
  .\Teste-Mod.ps1 -Paket Alles -Bereit      # danach bleibt das Spiel offen und wartet (Bereitschaft)
  .\Teste-Mod.ps1 -Plan Grundtest -Weiter   # an das offene Spiel in Bereitschaft schicken (ohne Neustart, Balance wird neu gelesen)
.NOTES
  Steam muss laufen. Waehrend des Tests nicht spielen. Ablauf: Test-Spielstaende als Autotest_<Name>.sav in den Speicherordner kopieren
  (eigene Spielstaende bleiben unberuehrt), Auftrag nach Saved\FicsitTD\Autotest.json, Spiel ueber Steam starten (ausser -Weiter),
  auf das Ergebnis warten, Bericht nach Satisfactory\Testberichte. Neuer Build (C++/Assets) braucht immer einen Neustart ohne -Weiter.
#>
param(
    [string] $Paket = "",
    [string] $Plan = "",
    [switch] $Bereit,
    [switch] $Weiter,
    [string] $Testspielstaende = "D:\OneDrive\Claude\Claude Modding\Satisfactory\Testspielstaende",
    [string] $Berichte = "D:\OneDrive\Claude\Claude Modding\Satisfactory\Testberichte",
    [string] $Steam = "C:\Program Files (x86)\Steam\steam.exe",
    [string] $Sprache = "",  # 0.85: "en" oder "de" erzwingt die Sprache der Mod-Texte (-TDSprache=...)
    [string] $Kultur = ""    # 0.10.2: "en" stellt auch die Spieloberflaeche um (-culture=en), fuer Screenshots
)
$ErrorActionPreference = "Stop"
$hier = Split-Path -Parent $MyInvocation.MyCommand.Path
$saved = Join-Path $env:LOCALAPPDATA "FactoryGame\Saved"
$td = Join-Path $saved "FicsitTD"
$spielProzess = "FactoryGameSteam-Win64-Shipping"
if (-not $Paket -and -not $Plan) { $Paket = "Alles" }

# Plaene einsammeln
$uplugin = Get-Content (Join-Path (Split-Path $hier) "FicsitTD.uplugin") -Raw | ConvertFrom-Json
$planNamen = if ($Plan) { @($Plan) } else { (Get-Content (Join-Path $hier "Pakete\$Paket.json") -Raw -Encoding UTF8 | ConvertFrom-Json).Plaene }
$name = if ($Plan) { $Plan } else { $Paket }
$plaene = foreach ($n in $planNamen) {
    (Get-Content (Join-Path $hier "Plaene\$n.json") -Raw -Encoding UTF8).Replace("{VERSION}", $uplugin.VersionName) | ConvertFrom-Json
}
$auftrag = [ordered]@{ Name = $name; Bereitschaft = [bool]($Bereit -or $Weiter); Plaene = @($plaene) }

# Test-Spielstaende kopieren (in den Steam-ID-Ordner mit den neuesten Spielstaenden)
$ordner = Get-ChildItem (Join-Path $saved "SaveGames") -Directory | Where-Object { $_.Name -match '^\d+$' } |
    Sort-Object { (Get-ChildItem $_.FullName -Filter *.sav | Sort-Object LastWriteTime -Descending | Select-Object -First 1).LastWriteTime } -Descending |
    Select-Object -First 1
foreach ($p in $plaene) { if ($p.Quelle) { Copy-Item (Join-Path $Testspielstaende $p.Quelle) (Join-Path $ordner.FullName ($p.Spielstand + ".sav")) -Force } }

$laeuft = Get-Process $spielProzess -ErrorAction SilentlyContinue | Select-Object -First 1
if ($laeuft -and -not $Weiter) { throw "Das Spiel laeuft schon - schliessen oder mit -Weiter an die Bereitschaft schicken." }
if (-not $laeuft -and $Weiter) { throw "Kein Spiel in Bereitschaft - ohne -Weiter starten." }

New-Item -ItemType Directory -Force $td | Out-Null
$ergebnisDatei = Join-Path $td "Autotest-Ergebnis.json"
Remove-Item $ergebnisDatei, (Join-Path $td "Autotest.laeuft.json") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $td "Autotest\Bilder\*") -ErrorAction SilentlyContinue
# mit BOM: ohne liest das Spiel die Datei als ANSI (Test 0.74: "Ã¼" statt "ü" im Bericht)
[IO.File]::WriteAllText((Join-Path $td "Autotest.json"), ($auftrag | ConvertTo-Json -Depth 12), (New-Object Text.UTF8Encoding $true))

$start = Get-Date
$max = ($plaene | Measure-Object -Property MaxMinuten -Sum).Sum + 5
if (-not $laeuft) {
    Write-Host "== Starte Satisfactory ueber Steam (${name}: $($planNamen -join ', '))"
    $startArgs = @()
    if ($Sprache) { $startArgs += "-TDSprache=$Sprache" }
    if ($Kultur) { $startArgs += "-culture=$Kultur" }
    & $Steam -applaunch 526870 @startArgs
    while (-not $laeuft -and ((Get-Date) - $start).TotalMinutes -lt 3) { Start-Sleep 5; $laeuft = Get-Process $spielProzess -ErrorAction SilentlyContinue | Select-Object -First 1 }
    if (-not $laeuft) { Remove-Item (Join-Path $td "Autotest.json") -ErrorAction SilentlyContinue; throw "Spiel ist nach 3 min nicht gestartet." }
} else {
    Write-Host "== Auftrag an das offene Spiel (${name}: $($planNamen -join ', '))"
}
Write-Host "== warte auf das Ergebnis (hoechstens $max min, Stillstand nach 12 min ohne Fortschritt)"
# 15.11. (Endlos-Lang hing 2 h im falschen Modus, MaxMinuten war gross): Stillstandswaechter. Kommt 12 min lang
# keine neue Autotest-/Wellen-Zeile ins Spiellog, wird das Spiel beendet und der Bericht nennt den Stillstand.
$spielLog = Join-Path $env:LOCALAPPDATA "FactoryGame\Saved\Logs\FactoryGame.log"
$fortschrittMuster = 'LogFicsitTD: (Autotest|Welle|Endlos|Lauf|Ansage)'
$letzteZeile = ""; $letzterFortschritt = Get-Date; $stillstand = $false; $naechstePruefung = Get-Date
while ((Get-Process -Id $laeuft.Id -ErrorAction SilentlyContinue) -and ((Get-Date) - $start).TotalMinutes -lt $max) {
    if ($auftrag.Bereitschaft -and (Test-Path $ergebnisDatei)) { break }
    if ((Get-Date) -ge $naechstePruefung) {
        $naechstePruefung = (Get-Date).AddSeconds(60)
        $treffer = Get-Content $spielLog -Tail 3000 -ErrorAction SilentlyContinue | Select-String $fortschrittMuster | Select-Object -Last 1
        $zeile = if ($treffer) { $treffer.Line } else { "" }
        if ($zeile -ne $letzteZeile) { $letzteZeile = $zeile; $letzterFortschritt = Get-Date }
        elseif (((Get-Date) - $letzterFortschritt).TotalMinutes -ge 12) { $stillstand = $true; break }
    }
    Start-Sleep 5
}
if ($stillstand -and (Get-Process -Id $laeuft.Id -ErrorAction SilentlyContinue)) {
    Write-Host "== Stillstand: 12 min kein Fortschritt im Log (letzte Zeile: $letzteZeile) - Spiel wird beendet"
    Stop-Process -Id $laeuft.Id -Force
}
if ((Get-Process -Id $laeuft.Id -ErrorAction SilentlyContinue) -and -not $auftrag.Bereitschaft) {
    Write-Host "== Zeitlimit: Spiel wird beendet"
    Stop-Process -Id $laeuft.Id -Force
}
Remove-Item (Join-Path $td "Autotest.json") -ErrorAction SilentlyContinue

# Sammelbericht
New-Item -ItemType Directory -Force $Berichte | Out-Null
$stempel = Get-Date -Format "yyyy-MM-dd_HHmm"
$bericht = Join-Path $Berichte "${stempel}_$name.md"
$zeilen = @("# Testbericht $name ($($uplugin.VersionName))", "", "Start $($start.ToString('dd.MM.yyyy HH:mm')), Dauer $([int]((Get-Date) - $start).TotalMinutes) min, $(if ($Weiter) { 'ohne Neustart (Bereitschaft)' } else { 'ein Spielstart' }).", "")
$bestanden = $false
if (Test-Path $ergebnisDatei) {
    $e = Get-Content $ergebnisDatei -Raw -Encoding UTF8 | ConvertFrom-Json
    $bestanden = [bool]$e.Bestanden
    $zeilen += "**Ergebnis: $(if ($bestanden) { 'alles bestanden' } else { "$($e.Fehler) Fehler" })** ($($e.Plaene.Count) Plaene, $($e.Sekunden) s)", ""
    $zeilen += "| Plan | Ergebnis | Schritte | Dauer |", "|---|---|---|---|"
    foreach ($p in $e.Plaene) { $zeilen += "| $($p.Plan) | $(if ($p.Bestanden) { 'bestanden' } else { "**$($p.Fehler) Fehler**" }) | $($p.Schritte.Count) | $($p.Sekunden) s |" }
    foreach ($p in $e.Plaene) {
        $zeilen += "", "## $($p.Plan)$(if (-not $p.Bestanden) { ' - FEHLER' })", "", "Ende: $($p.Ende)", "", "| # | Art | Schritt | Ok | Ist |", "|---|---|---|---|---|"
        $i = 0
        foreach ($s in $p.Schritte) {
            $i++
            $ist = ($s.Ist -replace '\|', '/')
            if ($s.Antworten.Count -gt 0) { $ist += " <br>" + (($s.Antworten | Select-Object -First 3) -join " <br>" -replace '\|', '/') }
            $zeilen += "| $i | $($s.Art) | $($s.Text -replace '\|', '/') | $(if ($s.Ok) { 'ja' } else { '**NEIN**' }) | $ist |"
        }
        if ($p.Warnungen.Count -gt 0) { $zeilen += "", "Warnungen im Log:", ""; $zeilen += ($p.Warnungen | ForEach-Object { "- $_" }) }
    }
} else {
    $zeilen += "**Kein Ergebnis** - der Test ist nicht bis zum Ende gekommen (Absturz, Spielstand nicht geladen oder Zeitlimit)."
    if ($stillstand) { $zeilen += "", "**Stillstand:** 12 min kein Fortschritt im Log. Letzte Zeile: ``$letzteZeile``" }
}
# Bildschirmfotos (bei Marius 5120x1440, 12 MB) auf 1920 Breite als JPG verkleinern
$bilder = Get-ChildItem (Join-Path $td "Autotest\Bilder") -Filter *.png -ErrorAction SilentlyContinue
if ($bilder) {
    $bildOrdner = Join-Path $Berichte "${stempel}_$name"
    New-Item -ItemType Directory -Force $bildOrdner | Out-Null
    Add-Type -AssemblyName System.Drawing
    foreach ($b in $bilder) {
        $bild = [System.Drawing.Image]::FromFile($b.FullName)
        $klein = New-Object System.Drawing.Bitmap $bild, 1920, ([int](1920 * $bild.Height / $bild.Width))
        $klein.Save((Join-Path $bildOrdner ($b.BaseName + ".jpg")), [System.Drawing.Imaging.ImageFormat]::Jpeg)
        $bild.Dispose(); $klein.Dispose()
    }
    $zeilen += "", "## Bilder", ""; $zeilen += (Get-ChildItem $bildOrdner -Filter *.jpg | ForEach-Object { "![$($_.BaseName)](${stempel}_$name/$($_.Name))" })
}
[IO.File]::WriteAllLines($bericht, $zeilen, (New-Object Text.UTF8Encoding $false))
# Pruefstand (M1): Kennzahlen je Welle seit dem Start gegen die Zielbaender
& python (Join-Path $hier "werte_pruefstand.py") $start.ToString("yyyy-MM-dd HH:mm:ss") $bericht ($planNamen -join ",")
# 14.11.2026: Laeuft der Rechner unter Last (Bildrate unter 15/s), ist der Lauf ungueltig statt bestanden/durchgefallen (Exit 2)
& python (Join-Path $hier "pruefe_bildrate.py") $start.ToUniversalTime().ToString("yyyy-MM-dd HH:mm:ss") $bericht
$ungueltig = ($LASTEXITCODE -eq 2)
Write-Host "== Bericht: $bericht"
Write-Host $zeilen[4]
if ($ungueltig) { Write-Host "== LAUF UNGUELTIG (Rechner ausgelastet) - Ergebnis nicht werten, wiederholen"; exit 2 }
if (-not $bestanden) { exit 1 }
