#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include <atomic>

/** T1 Leistungsmessung (Gegnertakt-Recherche 2.3): Rechenzeit des eigenen Codes in CPU-Zyklen, nur Spiel-Thread.
 *  Das Wellen-Subsystem liest die Zaehler zu Beginn und Ende einer Messung und rechnet in ms je Bild um. */
namespace TDLeistung
{
	inline uint64 HirnZyklen = 0;
	inline uint64 TurmZyklen = 0;
	/** T2c: Schadensverarbeitung des Spiels je Treffer (ApplyPointDamage + Betaeubung) und Erzeugen der sichtbaren Geschosse */
	inline uint64 SchadenZyklen = 0;
	inline uint64 GeschossZyklen = 0;
	/** T2j: inklusive Tick-Zeit unserer Wellengegner (Tag FicsitTD) je Teil, per SML-Hook gemessen; atomar, falls die Engine parallel tickt */
	inline std::atomic<uint64> BewegungZyklen{ 0 };
	inline std::atomic<uint64> ControllerZyklen{ 0 };
	inline std::atomic<uint64> KreaturZyklen{ 0 };
	inline std::atomic<uint64> SkelettZyklen{ 0 };
	inline std::atomic<int32> BewegungTicks{ 0 };
	/** T2j: Aufrufe von ResolvePenetrationImpl (aus Geometrie geschoben) und OnCharacterStuckInGeometry (festgesteckt) */
	inline std::atomic<int32> Durchdringung{ 0 };
	inline std::atomic<int32> Feststecken{ 0 };
	/** T2k: inklusive Tick-Zeit je Klasse des Tick-Ziels im laufenden Bild (nur Spiel-Thread, nur waehrend einer Bildmessung) */
	inline std::atomic<bool> bTickProfil{ false };
	inline TMap<const UClass*, uint64> TickJeKlasse;

	/** Misst den umgebenden Block und addiert ihn auf den Zaehler */
	struct FUhr
	{
		uint64& Ziel;
		const uint64 Start;
		explicit FUhr(uint64& InZiel) : Ziel(InZiel), Start(FPlatformTime::Cycles64()) {}
		~FUhr() { Ziel += FPlatformTime::Cycles64() - Start; }
	};
}
