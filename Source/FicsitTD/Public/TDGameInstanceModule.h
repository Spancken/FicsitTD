#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "TDGameInstanceModule.generated.h"

class USMLSessionSetting;

/** Spieleinstellungen beim neuen Spiel (SML Session Settings).
 *  0.69.1: Ankreuzbox "FICSIT Tower Defense" (FicsitTD.TowerDefense, Bool).
 *  0.10.1 (Marius 15.11.): eigene Kategorie "FICSIT Tower Defense" mit Auswahl "Spielmodus" (FicsitTD.ModusInt, Ganzzahl),
 *  Erlaeuterung je Modus in Deutsch oder Englisch. Weitere Modi = weiterer Wert in ETDModus, im Asset und in den Texten. */
UCLASS()
class FICSITTD_API UTDGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()
public:
	UTDGameInstanceModule();
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
	/** Schluessel der alten Ankreuzbox (nur noch gelesen, damit aeltere Neustarts weiter erkannt werden) */
	static const TCHAR* TowerDefenseId;
	/** Schluessel der Modus-Auswahl */
	static const TCHAR* ModusId;
	/** 0.11 (Gegencheck Einsatz, Marius 16.11.): Auswahl "Stufe" - 0 nach Startort, 1..4 Erkundung..Albtraum */
	static const TCHAR* StufeId;
private:
	/** Anzeigetexte der Auswahl in der Spielsprache (das Asset kennt nur einen Text) */
	void SetzeTexte();
	UPROPERTY()
	TObjectPtr<USMLSessionSetting> mModus;
	UPROPERTY()
	TObjectPtr<USMLSessionSetting> mStufe;
};

/** Spielmodi der Auswahl "Spielmodus" (Wert der Sitzungseinstellung FicsitTD.ModusInt) */
namespace ETDModus
{
	constexpr int32 Aus = 0;
	constexpr int32 Einsatz = 1;
	/** 0.11 (Marius 15.11.): Wellen auf Zeit, endlos, eine Front */
	constexpr int32 Endlos = 2;
	/** 0.12-dev (Marius 15.11.): normales Satisfactory mit HUB, dazu Tuerme, Mauern und die Bohrung auf Knopfdruck */
	constexpr int32 Abwehr = 3;
}
