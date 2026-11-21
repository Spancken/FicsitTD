#pragma once

#include "CoreMinimal.h"

class AFGPlayerController;
class UWorld;

/**
 * 0.12 Etappe E1, Spike S0 Kern-Zugang - nur die Messungen (Fahrplan-0.12 Abschnitt 4, Bericht A1 Pruefpunkt 4):
 *   a0  Wem gehoert die Kollision der Verteidigungsmauer, und teilt sie ihren Kollisionstopf mit Spielwaenden?
 *   b   Laesst sich ein Treffer auf ein Lightweight-Bauteil (Fundament, Rampe, Wand) in das Bauteil aufloesen?
 *   c   Laesst sich ein fremdes Lightweight-Bauteil per Code entfernen - ohne Geister-Kollision, auch nach Speichern und Laden?
 * Kein Umbau der Mauer (Teil a), keine Spiellogik. Alles Entwicklerbefehle /td s0 ..., jede Antwort steht auch im Log ("S0...").
 */
namespace TDSpikeS0
{
	/** Args ohne das fuehrende "s0". Antwortzeilen nach Out (der Aufrufer schreibt sie in Chat und Log). */
	FICSITTD_API void Befehl(UWorld* World, AFGPlayerController* Player, const TArray<FString>& Args, TArray<FString>& Out);
}
