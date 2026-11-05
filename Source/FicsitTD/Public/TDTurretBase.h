#pragma once

#include <atomic>
#include "CoreMinimal.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "FGClearanceData.h"
#include "TDDamageable.h"
#include "TDTurretBase.generated.h"

class AFGCreature;
class ATDDecoy;
class UMaterialInstanceDynamic;

/** Schadensart eines Turms (Spielkonzept S4: Resistenzen, damit sich eine Turm-Mischung lohnt) */
enum class ETDSchadensart : uint8
{
	Durchschlag,   // Bolzen
	Kugel,         // Gewehr
	Feuer,         // Flammenwerfer
	Strom,         // Teslaspule
	Explosion,     // Nobelisk, Explosionsbolzen
	Gift           // 0.73: Gas-Nobelisk (keine Gegnerart geschuetzt)
};

/** Eine Zeile im Werte-Panel des Turm-Menues (UTDTurmInfo) */
struct FTDKennwert
{
	FString Name;
	FString Wert;
	/** Kampfwert: orange hervorheben */
	bool bHervor = false;
};

/**
 * Gemeinsamer Unterbau aller FicsitTD-Tuerme. Produktionsmaschine (Blueprint = Kopie des Konstruktors): Maschinen-Menue,
 * Rezepte, Standby, Uebertaktung und Strom kommen vom Spiel. Ein abgeschlossener Produktionszyklus wird auf dem Spiel-Thread
 * an Feuern() weitergereicht; produziert wird nur, solange BereitZumProduzieren() gilt (Ziel vorhanden, ausgerichtet ...).
 * Die Basis kuemmert sich um Zielsuche, Ausrichten von Drehteil (mYawMesh) und Kanzel (mHeadMesh), Turmlevel (Upgrades),
 * Lebenspunkte und Koeder, Statuslampe, Reichweitenkreis und Statustexte. Unterklassen setzen im Konstruktor die Masse
 * (m...-Konfiguration) und ueberschreiben Feuern()/Animieren().
 */
UCLASS(Abstract)
class FICSITTD_API ATDTurretBase : public AFGBuildableManufacturer, public ITDDamageable
{
	GENERATED_BODY()
public:
	ATDTurretBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Abriss im Einsatz: Abschuesse und Upgrades an den naechsten neuen Turm dieses Typs weitergeben */
	virtual void Dismantle_Implementation() override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const override;
	virtual FText GetLookAtDecription_Implementation(class AFGCharacterPlayer* byCharacter, const FUseState& state) const override;
	/** Feuerrate steigt mit der Stufe */
	virtual float GetProductionCycleTime() const override;

	// ITDDamageable
	virtual void ApplyTDDamage(float Amount, const AActor* Causer) override;
	virtual bool IsTDIntact() const override { return mHealth > 0.f; }
	virtual float GetTDHealth() const override { return mHealth; }
	virtual float GetTDMaxHealth() const override;
	/** Reparatur: heilt bis zum Maximum, liefert die tatsaechlich geheilten Lebenspunkte */
	virtual float HealTD(float Amount) override
	{
		if (!IsTDIntact() || Amount <= 0.f) return 0.f;
		const float Alt = mHealth;
		mHealth = FMath::Min(GetTDMaxHealth(), mHealth + Amount);
		return mHealth - Alt;
	}

	/** Munition im Eingang (Stueck bzw. bei Fluessigkeiten Liter) */
	virtual int32 GetAmmo() const;
	int32 GetShotsFired() const { return mSchuesse; }
	int32 GetKills() const { return mKills; }
	/** Turmlevel 1-5 (0.63, Marius 04.11.2026): nur per Upgrade im Turmfenster, sichtbar an den Anbauteilen _S<n> */
	static constexpr int32 TurmMaxLevel = 5;
	int32 GetLevel() const { return FMath::Clamp(mLevel, 1, TurmMaxLevel); }
	/** Test (/td turmlevel): Level direkt setzen */
	void SetzeLevelTest(int32 Level);
	FString GetStromText() const;
	FString GetMunitionsName() const;
	/** Kurzstatus: warum feuert er (nicht)? */
	virtual FString GetZustandText() const;
	/** Ohne Ziel: naechster Wellen-Gegner mit Abstand und Grund (Reichweite, Sicht durch X verdeckt, Mindestabstand) */
	FString KeinZielGrund() const;
	/** Aktuelle Reichweite in cm (mit Stufenbonus) */
	float GetReichweite() const;
	/** Reichweitenkreis am fertigen Turm ein-/ausblenden (/td reichweite) */
	void ZeigeReichweite(bool bAn);
	FString GetTurmName() const { return mTurmName; }
	/** 0.78.1 Pruefstand-Diagnose: worauf zielt der Turm gerade */
	AFGCreature* GetAktuellesZiel() const { return mAktuellesZiel.Get(); }
	/** 0.75 Selbsttest (UTDAutotest, Pruefung TurmWert): SchadenBonus, TaktBonus, Uebertakt, NennDPS, LevelPreis, LevelMax, Level,
	 *  Doktrin (0/1), KannUebertakten (0/1), SplitterPlaetze, StromLaden (MW beim Aufladen/Produzieren); unbekannt = -1 */
	float TestWert(const FString& Name) const;
	/** 0.77 Selbsttest Sichtlinie: freie Sicht von der Muendung zu einem Punkt? OutHindernis = was blockiert */
	bool FreieSichtZuPunkt(const FVector& Nach, FString& OutHindernis) const;

	/** Zielwahl je Turm (Marius 03.11.2026): Reihenfolge der Ziele und bevorzugte Gegnerfamilie, im Maschinenfenster unten */
	static constexpr int32 ZielModusAnzahl = 5;
	static const TCHAR* ZielModusName(int32 Modus);
	/** Familien fuer die Vorzugswahl; "" = alle */
	static const TArray<FString>& ZielFamilien();
	static FString FamilienName(const FString& Familie);
	/** Familie eines Gegners fuer die Zielwahl (Spucker aller Biome zusammen) */
	static FString FamilieVon(const AActor* Ziel);
	int32 GetZielModus() const { return mZielModus; }
	const FString& GetZielFamilie() const { return mZielFamilie; }
	void SetzeZielwahl(int32 Modus, const FString& Familie);

	/** Turmlevel kaufen (0.63): Level 2 mit dem MAM-Knoten des Turmtyps (Baum Turm-Upgrades), 3/4 mit Turmfertigung II/III,
	 *  5 = Meisterstufe (0.64). Bezahlt mit EP. Hat dieser Turmtyp Level (Reparaturstation nicht)? */
	bool HatUpgrades() const;
	bool UpgradeFreigeschaltet() const;
	/** Hoechstes Level, das die Forschung gerade erlaubt */
	int32 LevelMax() const;
	/** Preis des naechsten Levels in EP (0 = keins mehr) */
	int32 LevelPreis() const;
	/** Warum das naechste Level (noch) nicht geht; leer = kaufbar, wenn das Konto reicht */
	FString LevelSperre() const;
	/** Was ein Level bringt, z. B. "Feuerrate +30 %"; LevelSumme = alles bis zum jetzigen Level */
	FString LevelText(int32 Level) const { return LevelDiffText(Level - 1, Level); }
	FString LevelSumme() const { return LevelDiffText(1, GetLevel()); }
	/** Kauft das naechste Level; liefert die Meldung fuers Turmfenster */
	FString KaufeLevel();
	/** Summe eines Level-Werts bis zum jetzigen Level (Schaden, Takt, Reichweite, Magazin, Radius, Kegel, Nachbrennen als Anteil;
	 *  Durchschlag, Spruenge als Anzahl). Balance.json: Tuerme.Level.<Turm>.L<n>.<Wert> */
	float LevelWert(const TCHAR* Wert) const { return LevelWertBis(GetLevel(), Wert); }
	float LevelWertBis(int32 Level, const TCHAR* Wert) const;

	/** Trefferbilanz je Welle (G2-Auswertung im Log): zu Wellenbeginn zuruecksetzen, nach der Welle ausgeben */
	struct FBilanz
	{
		int32 Schuesse = 0;
		int32 Treffer = 0;
		float Schaden = 0.f;
		/** Gegnerart -> (Schaden ohne Resistenz, Schaden mit Resistenz, Treffer) */
		TMap<FString, FVector> JeGegner;
		/** Messwerte (Recherche 3.8): Schaden ueber die Rest-LP hinaus, Treffer an anderen als dem Hauptziel
		 *  (Kette, Durchschlag, Flaeche, Kegel), Zeiten in s */
		float Overkill = 0.f;
		int32 Nebentreffer = 0;
		float Messzeit = 0.f;
		float ZeitMitZiel = 0.f;
		float ZeitOhneMunition = 0.f;
		/** Ziel da, aber keine Munition: die Fabrik kommt nicht nach */
		float ZeitHungrig = 0.f;
		float ZeitOhneStrom = 0.f;
	};
	const FBilanz& GetBilanz() const { return mBilanz; }
	ETDSchadensart GetSchadensart() const { return mSchadensart; }
	/** Nenn-Schaden pro Sekunde gegen ein Einzelziel mit der eingelegten Munition (wie "Schaden/s" im Werte-Panel),
	 *  Grundlage der Abwehrkapazitaet K in der Druckquote Q */
	virtual float NennDPS() const { return 0.f; }
	void StarteBilanz() { mBilanz = FBilanz(); mBilanz.Schuesse = -mSchuesse; }
	/** Eine Logzeile: Schuesse, Treffer, Schaden, Schaden/s, je Gegnerart mit wirksamem Faktor */
	FString BilanzText(float Sekunden) const;

	/** Schadens-Faktor einer Schadensart gegen eine Kreatur (nach Gegnerfamilie, siehe Spielkonzept S4) */
	static float Resistenz(ETDSchadensart Art, const AFGCreature* Ziel);
	static const TCHAR* SchadensartName(ETDSchadensart Art);

	/** Alle Werte fuer das Panel im Maschinen-Menue: Stufe, Lebenspunkte, Kampfwerte, Munition, Statistik */
	void Kennwerte(TArray<FTDKennwert>& Out) const;
	/** Maschinen-Menue auf/zu: Werte-Panel (UTDTurmInfo) ein- und ausblenden */
	virtual void RegisterInteractingPlayer_Implementation(class AFGCharacterPlayer* player) override;
	virtual void UnregisterInteractingPlayer_Implementation(class AFGCharacterPlayer* player) override;

protected:
	/** Kampfwerte der eingelegten Munition (Schaden, Feuerrate, Schaden pro Sekunde ...) */
	virtual void Kampfwerte(TArray<FTDKennwert>& Out) const {}
	/** Sekunden je Produktionszyklus inkl. Stufenbonus und Uebertaktung, 0 ohne Rezept */
	float Zykluszeit() const;

	virtual bool CanProduce_Implementation() const override;
	virtual void Factory_ProductionCycleCompleted(float overProductionRate) override;

	/** Ein Produktionszyklus ist fertig (Spiel-Thread). Ziel kann nullptr sein, wenn es inzwischen verschwunden ist. */
	virtual void Feuern(AFGCreature* Ziel) {}
	/** ~30 Mal pro Sekunde fuer Animationen und Dauereffekte */
	virtual void Animieren(float DeltaSeconds, AFGCreature* Ziel) {}
	/** Stufen-Optik (0.62): Komponente, an die die Anbauteile eines Teils gehaengt werden (Base, Yaw, Head; Unterklassen z. B. Barrels) */
	virtual USceneComponent* StufenZiel(const FString& Teil) const;
	/** Darf ein neuer Produktionszyklus beginnen? Standard: Ziel vorhanden und ausgerichtet */
	virtual bool BereitZumProduzieren(AFGCreature* Ziel) const;
	/** Kommt die Kreatur als Ziel in Frage (zusaetzlich zu Reichweite und freier Sicht)? */
	virtual bool ZielErlaubt(const AFGCreature* Ziel, float Abstand) const { return true; }
public:
	/** Hat der Turm Munition fuer den naechsten Zyklus? (Teslaspule: immer) */
	virtual bool HatMunition() const { return GetAmmo() > 0; }
protected:
	/** Direkt nach dem Spielstart der Maschine, bevor das Standardrezept gesetzt wird (z. B. Rohranschluss einhaengen) */
	virtual void NachMaschinenStart() {}
	/** Zusatzzeile fuer den Hinweistext */
	virtual FString ZusatzInfo() const { return FString(); }
	/** Munitionszeile im Werte-Panel (Fluessigkeiten in m3) */
	virtual FString MunitionsText() const;
	/** ZusatzInfo auch im Werte-Panel zeigen (nicht, wenn sie nur die Munition wiederholt) */
	virtual bool ZusatzImPanel() const { return true; }

	FVector GetMuzzleLocation() const;
	bool IstAusgerichtet(const AActor* Ziel) const;
	void SammleFeinde(TArray<AFGCreature*>& OutFeinde) const;
	/** Schaden (nach Verzoegerung, z. B. Flugzeit) mit Abschusszaehlung; Betaeubung in Sekunden */
	void Treffer(AFGCreature* Ziel, float Schaden, const FVector& Richtung, float Betaeubung, float Verzoegerung = 0.f);
	/** Wie Treffer, aber mit eigener Schadensart (z. B. Explosionsbolzen am Bolzenturm) */
	void TrefferArt(AFGCreature* Ziel, float Schaden, const FVector& Richtung, float Betaeubung, float Verzoegerung, ETDSchadensart Art);
	TSubclassOf<class UFGItemDescriptor> AktuelleMunition() const;
	/** Level-Boni (0.63): 1 + Summe der Level-Werte Schaden bzw. Takt */
	float SchadenBonus() const;
	float TaktBonus() const;
	/** 0.73 Doktrin (Konzept-Spezialisierung 4): ist dieser Turmtyp die gewaehlte Hauptlinie? */
	bool HatDoktrin() const;
	/** 0.73 Energiesplitter: Uebertaktung des Spiels (1 = 100 %), fuer Tuerme, deren Feuertakt nicht am Produktionszyklus haengt */
	float Uebertakt() const { return FMath::Max(GetCurrentPotential(), 0.1f); }
	/** Schluessel-Anfang in der Balance-Datei: "Tuerme.<Turmname>." */
	FString BalanceTurm() const { return TEXT("Tuerme.") + mTurmName + TEXT("."); }
	/** Effekte (0.64, Marius: Munition sichtbar am Effekt): farbige Spur von-nach (Leuchtspur, Splitter, Feldblitz; Dicke 1 = Blitz)
	 *  und Leuchtpunkt (Einschlag, Wolke, Brand; Groesse 1 = 0,6 m Stern). Verzoegerung in s (Flugzeit), Puls = langsam pulsierend */
	void ZeigeSpur(const FVector& Von, const FVector& Nach, const FLinearColor& Farbe, float Dauer = 0.06f, float Dicke = 1.f, float Verzoegerung = 0.f);
	void ZeigeLeuchten(const FVector& Ort, const FLinearColor& Farbe, float Groesse, float Dauer, float Verzoegerung = 0.f, bool bPuls = false, float Staerke = 25.f);
	/** Laedt Modell in die Komponente, falls die Blueprint keins gesetzt hat */
	static void SetzeModell(UStaticMeshComponent* Teil, const FString& Pfad);

	// ---- Konfiguration (im Konstruktor der Unterklasse setzen) ----
	FString mTurmName = TEXT("Turm");
	ETDSchadensart mSchadensart = ETDSchadensart::Durchschlag;
	/** Drehteil auf dem Deck (relativ zum Turm) */
	FVector mDrehpunkt = FVector(0.f, 0.f, 225.f);
	/** Neigeachse der Kanzel (relativ zum Drehteil) */
	FVector mNeigeachse = FVector(5.f, 0.f, 190.f);
	/** Muendung im Kanzel-Raum */
	FVector mMuendung = FVector(385.f, 0.f, 5.f);
	/** Halbmasse des Sockels (Koeder) */
	FVector mHalbmasse = FVector(180.f, 180.f, 112.f);
	/** Ort der Statuslampe am Sockel (Unterklassen mit anderem Sockel setzen ihn im Konstruktor) */
	FVector mLampenOrt = FVector(150.f, -150.f, 225.f);
	/** Freiraum-Hoehe in cm */
	float mKlarraumHoehe = 560.f;
	float mNeigungMin = -30.f;   // 0.64.1: vorher -10, Tuerme auf Felsen kamen nicht an Gegner direkt darunter
	float mNeigungMax = 40.f;
	/** false = feststehender Turm (Teslaspule): kein Drehen/Neigen, immer ausgerichtet */
	bool mDreht = true;
	/** Ausrichtungs-Toleranz in Grad */
	float mZielToleranz = 8.f;
	FString mYawModell;
	FString mHeadModell;
	/** Rezept, das ein frisch gebauter Turm bekommt */
	FString mStandardRezept;

	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	float mBaseMaxHealth = 3000.f;

	/** Reichweite in cm (Stufe 0) */
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	float mRange = 4500.f;

	/** Drehgeschwindigkeit von Drehteil und Kanzel in Grad/s */
	UPROPERTY(EditDefaultsOnly, Category = "Tower Defense")
	float mTurnSpeed = 160.f;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mYawMesh;

	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mHeadMesh;

	/** Reichweitenkreis (Modell Radius 1 m, auf die Reichweite skaliert). Sichtbar im Hologramm, am fertigen Turm nur per /td reichweite */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mRangeRing;

	/** Eigene Statuslampe (ersetzt BP_ProductionIndicatorInstanced, dessen Schild am Turm schwarz war) */
	UPROPERTY(VisibleAnywhere, Category = "Tower Defense")
	TObjectPtr<UStaticMeshComponent> mStatusLampe;

	TWeakObjectPtr<AFGCreature> mAktuellesZiel;
	int32 mSchuesse = 0;
	FBilanz mBilanz;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UTDTurmInfo> mInfo;

	void Spielschritt();
	void Zielen();
	AFGCreature* SucheZiel(const FVector& Achse) const;
	bool FreieSicht(const FVector& Von, const AActor* Ziel) const;
	void SetzeErkennungsfarbe();
	void AktualisiereLampe();
	/** Schwarze Flaechen (Test 0.9.1): einmalig alles Sichtbare im Umkreis von 6 m mit Lage im Turm-Raum ins Log */
	void DiagnoseUmgebung();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> mLampenMaterial;
	int32 mLampenZustand = -1;
	FTimerHandle mDiagnoseTimer;

	UPROPERTY(SaveGame)
	float mHealth = -1.f;

	UPROPERTY(SaveGame)
	bool mFarbeGesetzt = false;

	/** Abschuesse dieses Turms (bleiben bis zur Zerstoerung) */
	UPROPERTY(SaveGame)
	int32 mKills = 0;

	/** Zielwahl: 0 naechster zum Turm, 1 naechster zum Kern, 2 wenigste LP, 3 meiste LP, 4 Elite und Boss zuerst */
	UPROPERTY(SaveGame)
	int32 mZielModus = 0;
	/** Bevorzugte Gegnerfamilie (FamilieVon), leer = alle gleich */
	UPROPERTY(SaveGame)
	FString mZielFamilie;

	/** Turmlevel (0 = neu oder Spielstand vor 0.63: BeginPlay setzt es) */
	UPROPERTY(SaveGame)
	int32 mLevel = 0;
	/** Nur Spielstaende vor 0.63 (Einzel-Upgrades, Spezialisierung): werden beim Laden in mLevel umgerechnet */
	UPROPERTY(SaveGame)
	int32 mUpgReichweite = 0;
	UPROPERTY(SaveGame)
	int32 mUpgTakt = 0;
	UPROPERTY(SaveGame)
	int32 mUpgSchaden = 0;
	UPROPERTY(SaveGame)
	int32 mSpezial = 0;
	/** Munitionspuffer: fuer welche Munition der Eingang vergroessert wurde */
	TWeakObjectPtr<UClass> mPufferFuer;

	UPROPERTY()
	TArray<TObjectPtr<ATDDecoy>> mDecoys;

	/** Vom Spiel-Thread gesetzt, vom Fabrik-Thread in CanProduce gelesen */
	std::atomic<bool> mBereit{ false };
	/** Abgeschlossene Zyklen (Fabrik-Thread) -> Feuern() auf dem Spiel-Thread */
	std::atomic<int32> mOffeneZyklen{ 0 };
	FTimerHandle mSpielTimer;
	FTimerHandle mZielTimer;
	/** T2f: seit der letzten Drehung vergangene Zeit (Drehtakt) */
	float mDrehZeit = 0.f;
	FTimerHandle mFarbTimer;
	FString LevelDiffText(int32 Von, int32 Bis) const;
	struct FTDEffekt
	{
		TWeakObjectPtr<UStaticMeshComponent> K;
		TWeakObjectPtr<UMaterialInstanceDynamic> M;
		float Warten = 0.f;
		float Rest = 0.f;
		float Dauer = 0.1f;
		float Staerke = 25.f;
		bool bPuls = false;
		bool bSpur = false;
	};
	TArray<FTDEffekt> mEffekte;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> mEffektObjekte;
	FTDEffekt* HoleEffekt(bool bSpur);
	void TickeEffekte(float DeltaSeconds);
private:
	/** Stufen-Optik (Marius 04.11.2026: Level am Turm sichtbar): Anbauteile SM_TD_<Turm>_<Teil>_S<n>, sichtbar ab Level n */
	void BaueStufenTeile();
	void ZeigeStufenTeile();
	/** Lack (Primitivdaten) des Drehteils auf die Anbauteile uebertragen */
	void GleicheLackAb();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> mStufenTeile;
	TArray<int32> mStufenTeilStufe;
	int32 mGezeigteStufe = -1;
	FTimerHandle mLackTimer;
};
