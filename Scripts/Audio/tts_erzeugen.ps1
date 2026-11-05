# 0.69.5: Sprachausgabe fuer ADA-Nachrichten mit den Windows-Stimmen (Hedda de-DE, Zira en-US), je Satz eine WAV.
# Danach Scripts/Audio/vo_filter.py (Computerfilter, Zusammenfuegen, Zeitstempel).
param([string]$Nachricht = "begruessung")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Speech
$Basis = "E:\Modding\Audio\FicsitTD\VO"
$Saetze = Get-Content -Raw -Encoding UTF8 "$PSScriptRoot\saetze_$Nachricht.json" | ConvertFrom-Json
$Stimmen = @{ de = "Microsoft Hedda Desktop"; en = "Microsoft Zira Desktop" }
$Format = New-Object System.Speech.AudioFormat.SpeechAudioFormatInfo(22050, [System.Speech.AudioFormat.AudioBitsPerSample]::Sixteen, [System.Speech.AudioFormat.AudioChannel]::Mono)
foreach ($Sprache in @("de", "en")) {
    $Ordner = Join-Path $Basis "$Sprache\$Nachricht\roh"
    New-Item -ItemType Directory -Force $Ordner | Out-Null
    $s = New-Object System.Speech.Synthesis.SpeechSynthesizer
    $s.SelectVoice($Stimmen[$Sprache])
    $s.Rate = 0
    $i = 0
    foreach ($Satz in $Saetze.$Sprache) {
        $i++
        $Datei = Join-Path $Ordner ("{0:D2}.wav" -f $i)
        $s.SetOutputToWaveFile($Datei, $Format)
        # Marius 04.11.: schneller, heller, freundlicher -> etwas schneller und hoeher (SSML prosody)
        $Kultur = if ($Sprache -eq "de") { "de-DE" } else { "en-US" }
        $Text = [System.Security.SecurityElement]::Escape($Satz)
        $s.SpeakSsml("<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='$Kultur'><prosody rate='+12%' pitch='+10%'>$Text</prosody></speak>")
        $s.SetOutputToNull()
        "$Sprache $i -> $Datei"
    }
    $s.Dispose()
}
