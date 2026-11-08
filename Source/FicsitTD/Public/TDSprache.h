#pragma once

#include "CoreMinimal.h"

/**
 * 0.85 (L1, Marius 07.11.: "Das Spiel muss auf Deutsch und auf Englisch funktionieren"): Spieltexte folgen der Spielsprache.
 * Deutsch bei deutscher Spielsprache, sonst Englisch.
 *
 * C++-Texte stehen in beiden Sprachen direkt am Aufruf:
 *   TDT(TEXT("Kern zerstoert"), TEXT("Core destroyed"))                -> FString
 *   TDF(TEXT("Welle %d"), TEXT("Wave %d"), Nr)                          -> FString (beide Formate vom Compiler geprueft)
 *   TDTX(...) / TDFX(...)                                               -> FText
 * Asset-Texte (Forschung, Rezepte, Gegenstaende, Gebaeude) sind deutsch erzeugt; bei englischer Spielsprache ersetzt
 * TDSprache::UebersetzeAssets() sie beim Laden der Welt ueber die Tabelle TDTexteEn.inl (erzeugt von Scripts/erzeuge_uebersetzung.py).
 * Logs, Balance-Schluessel und Testausgaben bleiben deutsch.
 */
namespace TDSprache
{
	/** true, wenn die Spielsprache nicht Deutsch ist (dann Englisch) */
	FICSITTD_API bool Englisch();
	/** Englische Fassung eines deutschen Asset-Textes aus der Tabelle; nullptr, wenn keine da ist */
	FICSITTD_API const TCHAR* Nachschlagen(const FString& Deutsch);
	/** Alle Assets unter /FicsitTD laden und ihre Text-Felder bei englischer Spielsprache ersetzen; liefert die Zahl ersetzter Felder */
	FICSITTD_API int32 UebersetzeAssets();
}

#define TDT(De, En) (TDSprache::Englisch() ? FString(En) : FString(De))
// TDF/TDFX brauchen mindestens ein Argument (sonst TDT/TDTX); ohne ## werden auch verschachtelte TDF-Argumente aufgeloest
#define TDF(De, En, ...) (TDSprache::Englisch() ? FString::Printf(En, __VA_ARGS__) : FString::Printf(De, __VA_ARGS__))
#define TDTX(De, En) FText::FromString(TDT(De, En))
#define TDFX(De, En, ...) FText::FromString(TDF(De, En, __VA_ARGS__))
