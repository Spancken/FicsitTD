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
		FrontenUngeprueft,     // Wellensystem: Welle startete mit ungeprueften Fronten (Netz nicht fertig, 18.11.)
		Abriegelung,           // 0.13 K1: Wellenstart wartete, weil ein Bau den Kern abriegelte (Ziel 0, sobald die Wegpruefung K4 steht)
		Anzahl
	};
	FICSITTD_API void Melde(EArt Art);
	FICSITTD_API int32 Zahl(EArt Art);
	FICSITTD_API const TCHAR* Name(EArt Art);
	FICSITTD_API void Zuruecksetzen();
	/** "Festgefahren 3, Absturz 1" oder "keine" */
	FICSITTD_API FString Text(int32* OutSumme = nullptr);

	/** 23.11.2026 (Gegencheck Konzept R3): Hilfen, die Gegner leise ueber Hindernisse bringen. Sie sind kein Fehler wie die
	 *  Sicherheitsnetze, aber jede Ausloesung zeigt eine Stelle, an der das Netz oder der Weg nicht passt - gezaehlt und im
	 *  Testbericht neben den Sicherheitsnetzen ausgewiesen (Pruefung "Sicherheitsnetze", ohne Einfluss auf bestanden). */
	enum class EHilfe : uint8
	{
		Bewuchs,        // Gegnerhirn: Pflanzen vor dem Gegner geraeumt
		Klippe,         // Gegnerhirn: Klippe aus dem Netz genommen
		StauGeduld,     // Gegnerhirn: nach Gegner.StauGeduld hinter einem Wellengegner versetzt
		Netzinsel,      // Gegnerhirn: Teilweg von einer Netzinsel verworfen, alter Weg bleibt
		FrontVerlegt,   // Wellensystem: Startpunkt einer Front hinter eine Klippe verlegt
		Rundungsrest,   // Gegnerhirn: Gegner mit Rest unter Treffer.Rundungsrest LP getoetet
		Anzahl
	};
	FICSITTD_API void MeldeHilfe(EHilfe Art);
	/** "Bewuchs 4, Klippe 1" oder "keine" */
	FICSITTD_API FString HilfenText();
}
