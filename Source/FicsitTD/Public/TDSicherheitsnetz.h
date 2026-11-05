#pragma once
#include "CoreMinimal.h"

/**
 * 0.78.2: Zaehler der Sicherheitsnetze. Marius 05.11.2026: Netze duerfen Spieler vor Sackgassen schuetzen,
 * jede Ausloesung ist aber ein Fehler, dessen Ursache gesucht wird. Ziel im Test: 0 (Pruefung "Sicherheitsnetze").
 */
namespace TDNetz
{
	enum class EArt : uint8
	{
		Festgefahren,          // Gegnerhirn: 8 s ohne Fortschritt, auf den Weg versetzt
		KeinBoden,             // Gegnerhirn: festgefahren, am Wegpunkt kein Boden - nicht versetzt
		NachzueglerVersetzt,   // Wellensystem: 25 s ohne Annaeherung, auf den Frontweg versetzt
		NachzueglerEntfernt,   // Wellensystem: nach 3 Versuchen entfernt
		Absturz,               // Gegnerhirn: im freien Fall unter dem Kern, zurueck auf den Weg
		AbsturzEntfernt,       // Gegnerhirn: dritter Absturz, getoetet
		OhneLeben,             // Gegnerhirn: lebte mit 0 LP weiter, getoetet
		Anzahl
	};
	FICSITTD_API void Melde(EArt Art);
	FICSITTD_API int32 Zahl(EArt Art);
	FICSITTD_API const TCHAR* Name(EArt Art);
	FICSITTD_API void Zuruecksetzen();
	/** "Festgefahren 3, Absturz 1" oder "keine" */
	FICSITTD_API FString Text(int32* OutSumme = nullptr);
}
