#pragma once

#include "CoreMinimal.h"
#include "Unlocks/FGUnlock.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "TDShop.generated.h"

class UFGSchematic;

/**
 * 0.12 E4 (Marius 14.11./21.11.: Kontingente, Ausruestung und Festplatten im AWESOME-Shop). Freischaltung eines Laden-Postens.
 * Darf beliebig oft gekauft werden. Das Spiel ruft Unlock() einmal je Kaufvorgang, nicht je Stueck (Fenster 1, Frage 3) -
 * die Wirkung steht deshalb im Hook auf die Kauffunktion (TDShop.cpp), nicht hier.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class FICSITTD_API UTDUnlockLaden : public UFGUnlock
{
	GENERATED_BODY()

public:
	/** Schluessel des Laden-Postens (wie in der Ladenliste, z. B. "Bolzenturm") */
	UPROPERTY(EditDefaultsOnly, Category = "FicsitTD")
	FString mSchluessel;

	virtual bool IsRepeatPurchasesAllowed_Implementation() const override { return true; }
};

/**
 * 0.12 E1: Sichtbarkeit am Experiment-Schalter (Balance "Experiment.<Name>" >= 1). Nur noch fuer alte Prototyp-Assets.
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class FICSITTD_API UTDExperimentAbhaengigkeit : public UFGAvailabilityDependency
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "FicsitTD")
	FString mSchalter;

	virtual bool AreDependenciesMet(UObject* worldContext) const override;
#if WITH_EDITOR
	virtual FString ToString() const override { return FString::Printf(TEXT("FicsitTD Experiment.%s an"), *mSchalter); }
#endif
};

/**
 * 0.12 E4: Ein Shop-Posten ist sichtbar, solange ein Einsatz laeuft, sein Bau-Rezept da ist, die Hoechstzahl nicht erreicht ist
 * und seine Sonderregel gilt (Klingenlaeufer nur als Ersatz, Nobelisken mit Zuender, Kernreparatur bei beschaedigtem Kern).
 */
UCLASS(EditInlineNew, DefaultToInstanced)
class FICSITTD_API UTDShopAbhaengigkeit : public UFGAvailabilityDependency
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "FicsitTD")
	FString mSchluessel;

	virtual bool AreDependenciesMet(UObject* worldContext) const override;
#if WITH_EDITOR
	virtual FString ToString() const override { return FString::Printf(TEXT("FicsitTD Shop-Posten %s"), *mSchluessel); }
#endif
};

namespace TDShop
{
	/** Experiment.ShopPrototyp >= 1 (nur noch Log; der Shop laeuft ohne Schalter) */
	FICSITTD_API bool PrototypAn();
	/** Name des Assets zu einem Laden-Schluessel: Umlaute umschreiben, nur Buchstaben und Ziffern (Inventarplätze -> Inventarplaetze) */
	FICSITTD_API FString AssetName(const FString& Schluessel);
	/** Schematic-Klasse eines Postens (/FicsitTD/Shop/Shop_TD_<AssetName>), nullptr wenn das Asset fehlt */
	FICSITTD_API UClass* Posten(const FString& Schluessel);
	/** Pfade aller Shop-Posten des Mods (fuer das Weltmodul), aus der Ladenliste */
	FICSITTD_API const TArray<FString>& PostenPfade();
	/** Coupon-Preis im CDO lesen bzw. setzen (mCost, ein Eintrag) */
	FICSITTD_API int32 LiesPreis(UClass* Schematic);
	FICSITTD_API bool SetzePreis(UClass* Schematic, int32 Preis);
	/** Je Welt: alle Posten auf ihren naechsten Preis (aus den Kaufzaehlern am Kern, ohne Einsatz Grundpreis). Das CDO lebt laenger als eine Welt. */
	FICSITTD_API void SetzePreise(const UObject* Kontext);
	/** Frueher: Prototyp zuruecksetzen. Jetzt SetzePreise ohne Welt (Grundpreise). */
	FICSITTD_API void SetzeZurueck();
	/** Frueher: Hooks des Prototyps. Jetzt ohne Wirkung (alle Hooks in RichteSperreEin). */
	FICSITTD_API void StelleHooksSicher();
	/** Hooks auf Shop und Schematic-Verwaltung einmal einrichten: Spiel-Artikel im Einsatz sperren, Posten des Mods pruefen und wirken */
	FICSITTD_API void RichteSperreEin();
	/** Einsatz oder Endlos aktiv und Einsatz.Shop.SpielArtikelSperren >= 1 */
	FICSITTD_API bool SpielArtikelGesperrt(const UObject* Kontext);
	/** Steht in der Welt ein AWESOME-Shop? (Kern-Menue verkauft nur, bis der erste steht) */
	FICSITTD_API bool ShopSteht(const UObject* Kontext);
}
