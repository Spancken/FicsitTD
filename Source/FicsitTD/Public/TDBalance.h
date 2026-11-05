#pragma once

#include "CoreMinimal.h"

/**
 * Balance-Datei (B1): alle Stellschrauben fuer Tuerme, Munition, Gegner, Wellen, Resistenzen und Einsatz an einer Stelle.
 * Standardwerte stehen in TDBalance.cpp (Tabelle). Beim ersten Zugriff wird
 *   %LOCALAPPDATA%\FactoryGame\Saved\FicsitTD\Balance.json
 * gelesen (fehlt sie, wird sie mit den Standardwerten angelegt) und daneben Balance-Standard.json zum Nachschlagen
 * geschrieben. Schluessel sind Punkt-Pfade ("Gegner.Hog.Schaden"), in der Datei verschachtelte Objekte.
 * Neu laden im Spiel: /td balance neu (wirkt sofort; Gegnerwerte ab der naechsten Welle).
 */
namespace TDBalance
{
	/** Wert zum Schluessel: aus der Datei, sonst der Standard aus der Tabelle; unbekannter Schluessel -> Ersatz (und Warnung) */
	FICSITTD_API float Wert(const FString& Schluessel, float Ersatz = 0.f);
	inline int32 Ganz(const FString& Schluessel, int32 Ersatz = 0) { return FMath::RoundToInt(Wert(Schluessel, float(Ersatz))); }
	/** Datei (neu) lesen; liefert eine Zusammenfassung fuer Chat und Log */
	FICSITTD_API FString Lade();
	/** Gibt es den Schluessel in Tabelle oder Datei? (ohne Warnung) */
	FICSITTD_API bool Hat(const FString& Schluessel);
	/** Pfad der Balance-Datei */
	FICSITTD_API FString Pfad();
}
