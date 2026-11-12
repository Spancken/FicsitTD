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
	virtual float ZusatzTestWert(const FString& Name) const override
	{
		// 0.9.4 Diagnose (Leiter loest nie aus): naechster Abstand eines Ziels zum Zaun (m), Takte mit Ziel, Takte mit Ziel unter 4 m
		return Name == TEXT("Schlaege") ? mSchlaege : Name == TEXT("NahMin") ? mDiagNahMin / 100.f : Name == TEXT("ZielTakte") ? float(mDiagZiel)
			: Name == TEXT("NahTakte") ? float(mDiagNah) : Name == TEXT("Intakt") ? (IsTDIntact() ? 1.f : 0.f) : -1.f;
	}
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override
	{
		Super::Animieren(DeltaSeconds, Ziel);
		if (Ziel)
		{
			const float D = FVector::Dist(Ziel->GetActorLocation(), GetActorLocation());
			mDiagNahMin = FMath::Min(mDiagNahMin, D);
			++mDiagZiel;
			mDiagNah += D < 400.f ? 1 : 0;
		}
	}
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
	float mDiagNahMin = 1.e7f;
	int32 mDiagZiel = 0;
	int32 mDiagNah = 0;
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
	/** 0.9.3 Radar (Marius 09.11.2026): Wellen-Markierungen an den Fronten und Rohstoffe der Umgebung auf Karte und Kompass */
	void RadarTakt();
	void RaeumeWellenMarker();
	FTimerHandle mRadarTimer;
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UFGActorRepresentation>> mWellenMarker;
	int32 mMarkerWelle = -1;
	float mRessourcenRest = 0.f;
	int32 mGescannt = 0;
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
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
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
	/** 0.9.6 (Gesamtreview 12.11., D): Ladestand gespeichert (-1 = neu gebaut, BeginPlay fuellt auf) */
	UPROPERTY(SaveGame)
	float mSchild = -1.f;
	/** 0.9.6: Rest der Ladezeit nach einem Bruch in s, in PreSaveGame aus mLadenBis (Weltzeit beginnt nach dem Laden neu) */
	UPROPERTY(SaveGame)
	float mLadenRest = 0.f;
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
	TObjectPtr<UStaticMeshComponent> mRadar;   // 0.9.3: Radar entfernt (Marius), Komponente bleibt leer fuer alte Spielstaende
	/** 0.9.3 sichtbare Munition: 6 Flak-Kanister in den Rohren, je Schuss einer weg, nach dem Nachladen gleitet er wieder hinein */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> mKanister;
	float mKanisterRest[6] = { 0.f, 0.f, 0.f, 0.f, 0.f, 0.f };
	int32 mKanisterNaechster = 0;
private:
	AActor* SucheFlieger() const;
	TWeakObjectPtr<AActor> mFlieger;
	float mSuchTakt = 0.f;
	int32 mTreffer = 0;
};

/**
 * 0.9.3 Drohnenplatz (Marius 09.11.2026, ersetzt die Reparaturstation): Landeplattform mit 2-4 Drohnen. Gabelung im MAM:
 * Reparaturdrohnen (Platten, fliegen zu beschaedigten Bauten in 40 m) oder Kampfdrohnen (Gewehrmunition, fliegen zu Gegnern in 40 m).
 * Ein Rezeptzyklus = ein Einsatz einer Drohne. Drohnen fliegen ohne Navigationsnetz: steigen, Reiseflug in 15 m, sinken zum Ziel.
 */
UCLASS()
class FICSITTD_API ATDDronePort : public ATDTurretBase
{
	GENERATED_BODY()
public:
	ATDDronePort();
	virtual void BeginPlay() override;
	virtual float ZusatzTestWert(const FString& Name) const override
	{
		return Name == TEXT("Einsaetze") ? float(mEinsaetze) : Name == TEXT("Geheilt") ? mGeheilt : Name == TEXT("Drohnen") ? float(mDrohnen.Num()) : -1.f;
	}
	/** 0.9.6 (Gesamtreview 12.11., A2, Marius 12.11.): Level 1-3 = 2/3/4 Drohnen, gekauft mit Coupons im Turmfenster */
	virtual int32 MaxLevel() const override { return 3; }
	virtual int32 LevelPreis() const override;
protected:
	virtual void LevelZusatzText(int32 Von, int32 Bis, TArray<FString>& Teile) const override;
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const override;
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) override;
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const override { return IstKampf(); }
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
private:
	struct FDrohne
	{
		int32 Zustand = 0;   // 0 parkt, 1 Aufstieg, 2 Hinflug, 3 Arbeit, 4 Rueckflug, 5 Landung
		FVector Ort = FVector::ZeroVector;
		FVector Platz = FVector::ZeroVector;
		TWeakObjectPtr<AActor> Ziel;
		float Arbeit = 0.f;
		float Takt = 0.f;
		float Menge = 0.f;
	};
	bool IstKampf() const;
	int32 SollDrohnen() const { return DrohnenFuer(GetLevel()); }
	static int32 DrohnenFuer(int32 Level) { return FMath::Clamp(1 + Level, 2, 4); }
	AActor* SucheBeschaedigt() const;
	int32 FreieDrohne() const;
	void BaueDrohnen();
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> mDrohnenTeile;
	TArray<FDrohne> mDrohnen;
	float mSuchTakt = 0.f;
	bool mGibtArbeit = false;
	int32 mEinsaetze = 0;
	float mGeheilt = 0.f;
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
	virtual void BeginPlay() override;
	virtual float ZusatzTestWert(const FString& Name) const override { return Name == TEXT("Getroffen") ? mGetroffen : -1.f; }
protected:
	virtual void Feuern(AFGCreature* Ziel) override;
	virtual FString ZusatzInfo() const override;
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const override;
private:
	int32 mGetroffen = 0;
};
