#pragma once

#include "CoreMinimal.h"

/** Ein MAM-Knoten des Einsatzes mit allem, was die Forschungsuebersicht im Kern-Menue zeigt (Daten aus Scripts/create_research.py) */
struct FTDForschungKnoten
{
	FString Baum;
	FString BaumName;
	FString Key;
	FString Name;
	FString Text;
	FString Laden;
	FString Gegen;
	int32 FP = 0;
	/** Standard-Welle aus dem Skript; wirksam ist TDForschung::AbWelle (Balance Forschung.<Key>.AbWelle) */
	int32 StandardWelle = 0;
	/** Voraussetzungen (Schluessel), Eltern im Baum und Knoten aus anderen Baeumen */
	TArray<FString> Braucht;
	/** Andere Seite einer Abzweigung, sonst leer */
	FString Andere;
	FString Pfad;
};

/** Zustand eines Knotens fuer die Anzeige */
enum class ETDForschungZustand : uint8
{
	Erforscht,
	Moeglich,
	ZuFrueh,
	Gesperrt,
	Ausgeschlossen
};

namespace TDForschung
{
	FICSITTD_API const TArray<FTDForschungKnoten>& Alle();
	FICSITTD_API const FTDForschungKnoten* Finde(const FString& Key);
	/** Ist der Knoten erforscht? Schluessel wie im Skript (z. B. KernPanzer) */
	FICSITTD_API bool Hat(const UObject* WorldContext, const TCHAR* Key);
	FICSITTD_API int32 AbWelle(const FTDForschungKnoten& K);
	FICSITTD_API ETDForschungZustand Zustand(const UObject* WorldContext, const FTDForschungKnoten& K);
	/** Schluessel der schon gewaehlten anderen Seite einer Gabelung, sonst leer */
	FICSITTD_API FString GewaehlteAndere(const UObject* WorldContext, const FTDForschungKnoten& K);
	/** 0.65 (Marius 04.11.: MAM zeigt nicht, was freischaltbar ist): Statuszeile eines Knotens, z. B. "Gesperrt: ab Welle 10" */
	FICSITTD_API FString StatusTitel(const UObject* WorldContext, const FTDForschungKnoten& K, FString& OutText);
	/** 0.72.1 (Marius: Status nicht als Kachel): schreibt den Status in den Knotentitel, z. B. "Nobelisk-Werfer (ab Welle 15)" */
	FICSITTD_API void AktualisiereMamKarten(const UObject* WorldContext);
	/** Name des Knotens ohne Status-Zusatz (fuer Chat und Protokoll) */
	FICSITTD_API FString Basisname(const UClass* Schematic);
	/** 0.72.1: Turm-Aktionen erforschter Knoten freischalten (stehen nicht mehr im MAM), Panzersperre entfernen; in FicsitTDModule.cpp */
	FICSITTD_API void GibStilleRezepte(const UObject* WorldContext);
}
