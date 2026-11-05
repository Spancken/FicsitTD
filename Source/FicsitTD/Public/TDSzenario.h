#pragma once

#include "CoreMinimal.h"

/** 0.70 (Konzept-Kernplaetze S5, Marius: Testeinstiege zuerst festlegen, dann je Szenario eine Beispiel-Basis bauen) */
namespace TDSzenario
{
	/** Liste der Szenarien mit Einstieg und Anforderungen */
	FICSITTD_API TArray<FString> Liste();
	/** Einsatz auf das Szenario stellen: Forschung der Phasen davor, Sprung zur Einstiegswelle (EP/FP wie nach den Wellen), Freies Bauen an */
	FICSITTD_API TArray<FString> Anwenden(UObject* WorldContext, int32 Nr, class APawn* Spieler);
	/** Basis gegen die Anforderungen des zuletzt gewaehlten (oder angegebenen) Szenarios pruefen */
	FICSITTD_API TArray<FString> Pruefen(UObject* WorldContext, int32 Nr = 0);
	/** Aufbau beenden: Freies Bauen und Kostenloses Bauen wieder aus */
	FICSITTD_API FString Fertig(UObject* WorldContext);
	/** Munition aus allen Tuermen nehmen (Szenario T5: Nachschub nur aus der laufenden Fabrik) */
	FICSITTD_API FString TuermeLeeren(UObject* WorldContext);
}
