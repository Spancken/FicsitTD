#pragma once

#include "CoreMinimal.h"
#include "FGConstructDisqualifier.h"

class AActor;

/**
 * 17.11. (P16/A12, Marius: kein Abriss waehrend der Welle). Gleiche Regel wie die Bausperre (UTDEinsatzSubsystem::SperrGrund
 * "Welle"): im Einsatz, nicht im Endlos-Modus, im Verteidigungsfeld, fuer Gebaeude mit Kontingent. So rettet ein Abriss in der
 * Welle kein Turmlevel mehr, das bei einer Zerstoerung verloren ginge.
 */
FICSITTD_API bool TDAbrissInWelleGesperrt(const AActor* Bau);
/** Grund fuer die Anzeige der Bau-Pistole (Abriss-Modus) */
FICSITTD_API void TDAbrissSperreGruende(const AActor* Bau, TArray<TSubclassOf<UFGConstructDisqualifier>>& Out);
