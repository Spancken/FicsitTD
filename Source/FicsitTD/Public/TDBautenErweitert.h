#pragma once

#include "CoreMinimal.h"
#include "TDTurretBase.h"
#include "TDBautenErweitert.generated.h"

/*
 * 0.83 weitere Bauten P2/P3 (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07, Modelle aus Blender). Alle sind Konstruktor-Kopien auf
 * ATDTurretBase (Strom, Rezepte, Lebenspunkte, Koeder); die Wirkung steht hier. Werte in der Balance-Datei unter Tuerme.<Name>.*.
 */

/** Elektrozaun (P2, Gabel G mit Oelschlick): 8 x 4 m Mauerstueck mit Kupferstaeben. Fuer Gegner eine Mauer; wer ihn angreift,
 *  bekommt einen Schlag (Strom-Schaden, 1 s Betaeubung), solange er Strom hat. */
UCLASS()
class FICSITTD_API ATDConductorWall : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDConductorWall();
	virtual void BeginPlay() override;
	/** Aus UTDGegnerHirn::Greife: Angreifer bekommt einen Schlag */
	static void Schlag(AActor* Zaun, class AFGCreature* Angreifer);
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Schlaege") ? mSchlaege : -1.f; }
	virtual bool HatMunition() const override { return true; }
protected:
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return Ziel != nullptr; }
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const override { return true; }
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mTuer2;
private:
	int32 mSchlaege = 0;
};

/** Funkmast (P2, Radarmast + Leitsender): laeuft dauernd mit Strom, Tuerme in 30 m bekommen +10 % Reichweite.
 *  Die Richtantenne zeigt auf den naechsten Gegner, die grosse Schuessel dreht. */
UCLASS()
class FICSITTD_API ATDRadioMast : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDRadioMast();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool HatMunition() const override { return true; }
	/** Faktor auf die Reichweite eines Turms (1 ohne Funkmast in Reichweite) */
	static float ReichweitenFaktor(const AActor* Turm);
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Sendet") ? (IsProducing() ? 1.f : 0.f) : -1.f; }
protected:
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return true; }
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mAntenne;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKleineSchuessel;
private:
	float mZeit = 0.f;
	static TArray<TWeakObjectPtr<ATDRadioMast>> sAlle;
};

/** Schildgenerator (P2, Gabel H mit Stoerfeld): Kuppel 20 m schluckt Schaden an Bauten (nicht am Kern), 3000 Schild-LP,
 *  gebrochen laedt er 20 s nach. Braucht dauernd Strom. */
UCLASS()
class FICSITTD_API ATDShieldGenerator : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDShieldGenerator();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool HatMunition() const override { return true; }
	/** Schaden an Ziel durch die Schilde in Reichweite fuehren; liefert, was durchkommt */
	static float Schlucke(const UWorld* Welt, const AActor* Ziel, float Schaden);
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Geschluckt") ? mGeschluckt : Name == TEXT("Schild") ? mSchild : -1.f; }
protected:
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return true; }
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRing1;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRing2;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mDeckel0;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mDeckel1;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mDeckel2;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mDeckel3;
private:
	bool IstAktiv() const;
	float SchildMax() const;
	double Jetzt() const;
	float mSchild = -1.f;
	double mLadenBis = 0.0;
	double mLetzterTreffer = 0.0;
	float mGeschluckt = 0.f;
	float mDeckelWinkel = 0.f;
	static TArray<TWeakObjectPtr<ATDShieldGenerator>> sAlle;
};

/** Himmelsturm / Flak (P3): schiesst nur auf Flieger (fliegende Krabben), Flak-Kanister vom Band. */
UCLASS()
class FICSITTD_API ATDFlakTurret : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDFlakTurret();
	virtual void BeginPlay() override;
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Treffer") ? mTreffer : -1.f; }
protected:
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override;
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const override { return false; }   // keine Bodenziele
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRadar;
private:
	AActor* SucheFlieger() const;
	TWeakObjectPtr<AActor> mFlieger;
	float mSuchTakt = 0.f;
	int32 mTreffer = 0;
};

/** Stoerfeld-Projektor (P3, Gabel H mit Schildgenerator): verbraucht reanimiertes SAM und bremst alle Gegner in 15 m um 40 %. */
UCLASS()
class FICSITTD_API ATDFieldProjector : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDFieldProjector();
	virtual void BeginPlay() override;
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Gebremst") ? mGebremst : -1.f; }
protected:
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override { return Ziel != nullptr; }
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const override { return true; }
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRing0;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRing1;
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRing2;
private:
	float mZeit = 0.f;
	float mWirkTakt = 0.f;
	int32 mGebremst = 0;
};

/** Singularitaetswerfer (P3, Freies Spiel): Ficsonium-Ladung zieht Gegner in 12 m zusammen und sprengt sie nach 1,5 s. */
UCLASS()
class FICSITTD_API ATDSingularityLauncher : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDSingularityLauncher();
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Getroffen") ? mGetroffen : -1.f; }
protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
private:
	int32 mGetroffen = 0;
};
