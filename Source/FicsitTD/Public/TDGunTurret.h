#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDGunTurret.generated.h"

/** Wirkung einer Gewehrmunition (1 Item = 1 Magazin, wie im Spiel) */
struct FTDPatronenWerte
{
	float Schaden = 12.f;
	float SchussProSekunde = 6.f;
	int32 Magazin = 15;
	/** 0.73: Panzerbrechend macht Durchschlag statt Kugel; Brandmunition zusaetzlich Feuer-Schaden je Treffer */
	ETDSchadensart Art = ETDSchadensart::Kugel;
	float Brand = 0.f;
};

/**
 * Gewehrturm: Rezept je Gewehrmunition, ein Zyklus laedt ein Magazin nach. Das Laufbuendel dreht hoch, ab 60 % Drehzahl
 * wird mit Muendungsfeuer geschossen (Treffer sofort). Nachgeladen wird erst, wenn das Magazin leer ist.
 */
UCLASS()
class FICSITTD_API ATDGunTurret : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDGunTurret();
	virtual void BeginPlay() override;

	static FTDPatronenWerte WerteFuer(const UClass* MunitionsKlasse);

protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual USceneComponent* StufenZiel(const FString& Teil) const override;
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override;
	virtual bool HatMunition() const override { return GetAmmo() > 0 || mMagazin > 0; }
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	virtual float NennDPS() const override;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mLaeufe;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mMuendungsfeuer;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> mFeuerMaterial;

	/** Schuss im geladenen Magazin */
	UPROPERTY(SaveGame)
	int32 mMagazin = 0;

	float mLaufTempo = 0.f;     // Grad/s
	float mFeuerTakt = 0.f;
	float mBlitzRest = 0.f;
};
