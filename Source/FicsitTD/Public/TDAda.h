#pragma once

#include "CoreMinimal.h"

class ATDDefenseCore;
struct FTDPlanGruppe;

/**
 * 0.12-dev (Marius 16.11.): ADA meldet sich bei wichtigen Wellen - neue Gegnerart oder Boss. Texte: Notizen\ADA-Ansagen-Entwurf-2026-11-16.md.
 * Zu Beginn der Bauphase vor der Welle, je Lauf jede Ansage einmal (Kern->mAdaGesagt). Text immer als ADA-Zeile im Chat, dazu die Stimme
 * (/FicsitTD/Audio/SW_TD_ADA_<Key>_<de|en>, Scripts/Audio/Erzeuge-ADA-Ansagen.ps1), wenn sie erzeugt ist.
 * Abschaltbar mit Balance ADA.Ansagen 0.
 */
namespace TDAda
{
	/** Ansagen fuer die naechste Welle (Plan schon gerechnet). Rueckgabe: Zahl der Ansagen */
	FICSITTD_API int32 VorWelle(const UObject* WorldContext, ATDDefenseCore* Kern, int32 Welle, const TArray<FTDPlanGruppe>& Plan);
	/** Ansage nach Schluessel (Gegnerart, z. B. "Hog", oder "Boss10"); false, wenn unbekannt oder schon gesagt */
	FICSITTD_API bool Sage(const UObject* WorldContext, ATDDefenseCore* Kern, const FString& Key);
	/** ADA ist so lange belegt (z. B. Begruessung): spaetere Ansagen warten, bis sie fertig ist */
	FICSITTD_API void Belege(const UObject* WorldContext, float Sekunden);
	/** Alle Schluessel (Selbsttest, /td ada liste) */
	FICSITTD_API TArray<FString> Schluessel();
}
