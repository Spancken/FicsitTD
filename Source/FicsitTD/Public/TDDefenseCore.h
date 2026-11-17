#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGClearanceData.h"
#include "TDDamageable.h"
#include "TDWellenStatistik.h"
#include "TDDefenseCore.generated.h"

class ATDDecoy;

/**
 * Abwehrkern (Spielkonzept S1): das Ziel der Wellen im Modus Kern-Verteidigung. Gebaeude mit Lebenspunkten und Koedern
 * (Blueprint = Kopie des Aussichtsturms, reines AFGBuildable). Faellt er auf 0, ist der Lauf verloren - das Gebaeude bleibt
 * stehen und wird beim naechsten Start wieder aufgeladen. Der Energiekristall dreht sich und pulsiert; bei Schaden flackert er.
 * Steuerung vorerst per Chat: /td kern start|weiter|stop|status.
 */
UCLASS()
class FICSITTD_API ATDDefenseCore : public AFGBuildable, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDDefenseCore();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const override;
	virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) const override;
	/** Im Einsatz oder laufender Kern-Verteidigung nicht abreissbar (Test 0.55) */
	virtual bool CanDismantle_Implementation() const override { return !mEinsatzAktiv && !mLaufAktiv && Super::CanDismantle_Implementation(); }

	// ITDDamageable
	virtual void ApplyTDDamage(float Amount, const AActor* Causer) override;
	virtual bool IsTDIntact() const override { return mHealth > 0.f; }
	virtual float GetTDHealth() const override { return mHealth; }
	/** mit Forschung Kern-Panzerung x1,3 */
	virtual float GetTDMaxHealth() const override;
	/** Reparatur: heilt bis zum Maximum, liefert die tatsaechlich geheilten Lebenspunkte */
	virtual float HealTD(float Amount) override
	{
		if (!IsTDIntact() || Amount <= 0.f) return 0.f;
		const float Alt = mHealth;
		mHealth = FMath::Min(GetTDMaxHealth(), mHealth + Amount);
		return mHealth - Alt;
	}

	/** Volle Lebenspunkte (Start eines Laufs) */
	void Aufladen();

	/** Gespeicherter Stand eines laufenden Durchgangs (das Wellen-System schreibt ihn laufend, nach dem Laden setzt es fort) */
	UPROPERTY(SaveGame)
	bool mLaufAktiv = false;
	UPROPERTY(SaveGame)
	int32 mLaufWelle = 0;
	UPROPERTY(SaveGame)
	int32 mLaufWellenGesamt = 30;
	UPROPERTY(SaveGame)
	float mLaufCountdown = 0.f;
	UPROPERTY(SaveGame)
	float mLaufZwischen = 600.f;
	UPROPERTY(SaveGame)
	int32 mLaufStufe = 1;
	/** Wellen nur auf Befehl (Einsatz: Planung ohne Zeitdruck) */
	UPROPERTY(SaveGame)
	bool mLaufManuell = false;
	/** 0.11 (Gegencheck 15.11.): Lauf im Spielmodus Endlos-Wellen - am Kern gemerkt, damit ein spaeterer Moduswechsel ihn nicht kippt */
	UPROPERTY(SaveGame)
	bool mLaufEndlos = false;
	/** 0.11 (Marius 15.11.): nach einer Endlos-Bosswelle wartet eine Bonuswahl (Coupons, Kernreparatur, Festplatten) */
	UPROPERTY(SaveGame)
	bool mEndlosBonusOffen = false;
	/** 16.11. (Nachpruefung E5): Zahl der offenen Bonuswahlen - ein nicht abgeholter Bonus geht nicht mehr verloren */
	UPROPERTY(SaveGame)
	int32 mBonusOffen = 0;
	/** 16.11. (Nachpruefung E4, Marius): gewaehlte Boni "Kern" - je Wahl dauerhaft +10 % Kern-LP fuer diesen Lauf */
	UPROPERTY(SaveGame)
	int32 mKernBonusStufen = 0;
	/** 0.12-dev Freies Spiel mit Abwehr: laeuft am Kern gerade eine Bohrung (wird nach dem Laden ohne Strafe beendet) */
	UPROPERTY(SaveGame)
	bool mLaufBohrung = false;
	/** 0.12-dev: ADA-Ansagen, die in diesem Lauf schon kamen (TDAda) */
	UPROPERTY(SaveGame)
	TArray<FString> mAdaGesagt;
	/** Erste Erfolge der Bohrung je Rohstoff und Stufe ("<Rohstoff-Klassenpfad>|<Stufe>") - Erstlohn nur einmal */
	UPROPERTY(SaveGame)
	TArray<FString> mBohrErfolge;
	/** Talentpunkte aus Erstlohn der Bohrung (zusaetzlich zu den Punkten aus dem Rang) */
	UPROPERTY(SaveGame)
	int32 mPionierBonusPunkte = 0;
	/** 0.9.6 (Gesamtreview 12.11.: nach Sieg oder Niederlage kein Ausweg): Ende des letzten Durchgangs - 0 offen, 1 Sieg, 2 Niederlage,
	 *  dazu die Welle. Das Kern-Menue bietet dann Welle wiederholen, neu starten oder Einsatz beenden. */
	UPROPERTY(SaveGame)
	int32 mLaufEnde = 0;
	UPROPERTY(SaveGame)
	int32 mLaufEndWelle = 0;
	/** 0.11 (Gegencheck Einsatz, Marius 16.11.: Bestwert je Kernplatz und Stufe): Kernplatz des Laufs ("" bei alten Staenden) */
	UPROPERTY(SaveGame)
	FString mLaufPlatz;
	/** Eingriffe in diesem Lauf (Testsprung, /gib, Freies Bauen ...): ein Lauf mit Eingriffen bekommt keinen Bestwert */
	UPROPERTY(SaveGame)
	int32 mLaufEingriffe = 0;
	/** Einsatz-Modus (TDEinsatz.h): aktiv, freies Bauen (Test), Startkit schon ausgegeben */
	UPROPERTY(SaveGame)
	bool mEinsatzAktiv = false;
	UPROPERTY(SaveGame)
	bool mEinsatzFrei = false;
	UPROPERTY(SaveGame)
	bool mEinsatzStartkit = false;
	/** 0.84 (Marius 08.11.: gleiche Kernplaetze ergeben gleiche Wellen): Zufallszahl je Einsatz fuer den Wellenplan,
	 *  0 = keine Variation (Standard, Balance Wellen.VariationJeEinsatz = 0) */
	UPROPERTY(SaveGame)
	int32 mLaufSaat = 0;
	/** Gekaufte Kontingente je Schluessel (Anzahl Kaeufe). mEinsatzPunkte: bis 0.9.8 das ganze Konto; seit 0.10.0 nur die
	 *  FICSIT-Coupons, die noch am Kern warten (Inventar voll) - der Rest liegt als Item im Inventar. Alte Staende: das alte
	 *  Konto wartet am Kern und wandert beim naechsten Nachreichen als Coupons ins Inventar. */
	UPROPERTY(SaveGame)
	int32 mEinsatzPunkte = 0;
	UPROPERTY(SaveGame)
	TMap<FString, int32> mKontingentGekauft;
	/** Forschungspunkte (seit 0.10.0 Festplatten), die nicht ins volle Inventar passten - werden spaeter nachgereicht */
	UPROPERTY(SaveGame)
	int32 mFPOffen = 0;
	/** 0.66: Statistik je abgeschlossener Welle (Kern-Menue, Reiter Statistik) */
	UPROPERTY(SaveGame)
	TArray<FTDWellenStat> mStatistik;
	/** Baumaterial aus Startkit/Kaeufen, das nicht ins volle Inventar passte (Item-Klassenpfad -> Anzahl) */
	UPROPERTY(SaveGame)
	TMap<FString, int32> mMaterialOffen;
	/** Balancing-Lauf (/td lauf balancing): Testsprung aus, Eingriffe werden vermerkt (UTDLaufProtokoll) */
	UPROPERTY(SaveGame)
	bool mBalancingLauf = false;
	/** Feste Fronten des Laufs (Ort, Richtung, Weglaenge; 0 = ungeprueft) */
	UPROPERTY(SaveGame)
	TArray<FVector> mLaufFrontOrte;
	UPROPERTY(SaveGame)
	TArray<float> mLaufFrontWinkel;
	UPROPERTY(SaveGame)
	TArray<float> mLaufFrontWeg;
	/** Erbe abgerissener Tuerme "Typ|Abschuesse|L<Level>" (UTDEinsatzSubsystem::MerkeTurmErbe, alt "Typ|K|R|F|S|Sp") */
	UPROPERTY(SaveGame)
	TArray<FString> mTurmErbe;
	/** Pionier-Rang (UTDPionier): Erfahrung und gelernte Talente je Zweig */
	UPROPERTY(SaveGame)
	int32 mPionierXP = 0;
	UPROPERTY(SaveGame)
	TMap<FString, int32> mTalente;
	/** Kern-Schild (Forschung KernSchild): Rest in dieser Welle, wird zum Wellenstart geladen */
	float mSchild = 0.f;
	void LadeSchild();

	/** Maximale LP je nach Schwierigkeit (wird mit dem Spielstand gesichert) */
	void SetzeMaxLP(float LP) { mMaxHealth = FMath::Max(1000.f, LP); }

	/** Wegvorschau ein-/ausschalten: Leuchtringe entlang der Wege von den Fronten zum Kern */
	void ZeigeWege(const TArray<TArray<FVector>>& Wege);
	void VersteckeWege();
	bool ZeigtWege() const { return mWegMarker.Num() > 0; }

protected:
	UPROPERTY(EditDefaultsOnly, SaveGame, Category = "Tower Defense")
	float mMaxHealth = 20000.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> mWegMarker;

	/** Feste Aufbauten (Saeulen, Kaefig, Mast) */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mAufbau;

	/** Energiekristall (dreht sich) - seit 0.61 ohne Modell (Tiefbohrer) */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mKristall;

	/** Tiefbohrer (0.61, Satisfactory\Modelle\TD_DefenseCore_Bohrer.blend): alle Teile im Ursprung des Gebaeudes modelliert */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mBohrer;      // Stange und Schnecke: dreht, faehrt auf und ab
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mHub;         // Querhaupt und Kolben: faehrt mit, dreht nicht
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mZahnkranz;   // dreht gegenlaeufig
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mLeuchtring;  // Zustand: Farbe nach Kern-LP

private:
	void Animieren();
	/** Kern-Geschuetz (Forschung KernGeschuetz): alle 1,5 s ein Bolzen auf den naechsten Gegner */
	void Geschuetz();
	FTimerHandle mGeschuetzTimer;

	UPROPERTY(SaveGame)
	float mHealth = -1.f;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;

	UPROPERTY(Transient)
	TArray<TObjectPtr<class UMaterialInstanceDynamic>> mGluehen;

	FTimerHandle mAnimTimer;
	float mZeit = 0.f;
	float mTrefferBlitz = 0.f;
	/** Hoehe des Bohrers ueber der Bohrstellung (cm): in der Bauphase oben, in der Welle unten */
	float mBohrHoehe = 150.f;
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> mRingMaterial;
};
