# 0.69.6: Projekt-ID im Kopf (BKHD) der erzeugten Soundbank auf die des Satisfactory-Wwise-Projekts setzen.
# Ohne das lehnt Wwise im Spiel die Bank ab: "Failed to load SoundBank: 92 (The Init bank was not loaded yet ...)"
# (Test 0.69.5; Loesung wie in FicsIt-Networks PR #543: Spiel-Init-Bank hat Projekt-ID 3134).
# Nach jedem "WwiseConsole generate-soundbank" und vor Build-Mod.ps1 ausfuehren.
param(
	[string]$Bank = "E:\Modding\SatisfactoryModLoader\SatisfactoryModLoader_WwiseProject\GeneratedSoundBanks\Windows\FicsitTD.bnk",
	[uint32]$ProjektId = 3134
)
$Daten = [IO.File]::ReadAllBytes($Bank)
if ([Text.Encoding]::ASCII.GetString($Daten, 0, 4) -ne "BKHD") { throw "$Bank beginnt nicht mit BKHD" }
# BKHD: Kennung(4) Groesse(4) | Version(4) BankID(4) SprachID(4) Werte(4) ProjektID(4)
$Stelle = 24
$Alt = [BitConverter]::ToUInt32($Daten, $Stelle)
[BitConverter]::GetBytes($ProjektId).CopyTo($Daten, $Stelle)
[IO.File]::WriteAllBytes($Bank, $Daten)
"$(Split-Path $Bank -Leaf): Projekt-ID $Alt -> $ProjektId"
