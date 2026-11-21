#pragma once

#include "CoreMinimal.h"
#include "Unlocks/FGUnlock.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "TDShop.generated.h"

class UFGSchematic;

/**
 * 0.12 Etappe E1, Shop-Prototyp (Fahrplan-0.12, Bericht A2 Abschnitt 5.3): Freischaltung eines Laden-Postens im AWESOME-Shop.
 * Darf beliebig oft gekauft werden. Im Prototyp schreibt sie nur Logzeilen ("ShopProto: Unlock <Schluessel> Nr. n") - die
 * Frage ist, ob das Spiel Unlock() bei jedem Kauf ruft oder nur beim ersten (Kommentar in FGUnlock.h: "first time").
 * Die Wirkung eines Kaufs (Kontingent, Preis) gehoert deshalb in den Hook auf die Kauffunktion, nicht hierher.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class FICSITTD_API UTDUnlockLaden : public UFGUnlock
{
	GENERATED_BODY()

public:
	/** Schluessel des Laden-Postens (wie in der Ladenliste, z. B. "Bolzenturm") */
	UPROPERTY(EditDefaultsOnly, Category = "FicsitTD")
	FString mSchluessel;

	virtual void Unlock(class AFGUnlockSubsystem* unlockSubsystem) override;
	virtual void Apply(class AFGUnlockSubsystem* unlockSubsystem) override;
	virtual bool IsRepeatPurchasesAllowed_Implementation() const override { return true; }
};

/**
 * 0.12 E1: Sichtbarkeit eines Postens am Experiment-Schalter (Balance "Experiment.<Name>" >= 1). Mit Standard 0 bleibt der
 * Prototyp-Posten in jedem normalen Spiel verborgen und nicht kaufbar.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class FICSITTD_API UTDExperimentAbhaengigkeit : public UFGAvailabilityDependency
{
	GENERATED_BODY()

public:
	/** Name des Schalters ohne "Experiment." (z. B. "ShopPrototyp") */
	UPROPERTY(EditDefaultsOnly, Category = "FicsitTD")
	FString mSchalter;

	virtual bool AreDependenciesMet(UObject* worldContext) const override;
#if WITH_EDITOR
	virtual FString ToString() const override { return FString::Printf(TEXT("FicsitTD Experiment.%s an"), *mSchalter); }
#endif
};

namespace TDShop
{
	/** Experiment.ShopPrototyp >= 1 */
	FICSITTD_API bool PrototypAn();
	/** Schematic-Klasse eines Postens (/FicsitTD/Shop/Shop_TD_<Schluessel>), nullptr wenn das Asset fehlt */
	FICSITTD_API UClass* Posten(const FString& Schluessel);
	/** Pfade aller Shop-Posten des Mods (fuer das Weltmodul) */
	FICSITTD_API const TArray<FString>& PostenPfade();
	/** Coupon-Preis im CDO lesen bzw. setzen (mCost, ein Eintrag) */
	FICSITTD_API int32 LiesPreis(UClass* Schematic);
	FICSITTD_API bool SetzePreis(UClass* Schematic, int32 Preis);
	/** Je Welt: Kaufzaehler leeren und alle Posten auf den Grundpreis (das CDO lebt laenger als eine Welt) */
	FICSITTD_API void SetzeZurueck();
	/** Hooks des Prototyps einmal einrichten - nur wenn der Schalter an ist. Ohne Schalter bleibt das Spiel unberuehrt. */
	FICSITTD_API void StelleHooksSicher();
	/** 21.11. (A5 Nr. 1): Artikel des Spiels im Einsatz und Endlos aus der Shop-Liste nehmen und ihren Kauf ablehnen (immer aktiv) */
	FICSITTD_API void RichteSperreEin();
	/** Einsatz oder Endlos aktiv und Einsatz.Shop.SpielArtikelSperren >= 1 */
	FICSITTD_API bool SpielArtikelGesperrt(const UObject* Kontext);
}
