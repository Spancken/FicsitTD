#pragma once

#include "CoreMinimal.h"
#include "AI/FGCreatureController.h"
#include "Components/ActorComponent.h"
#include "TDGegnerHirn.generated.h"

class AFGCreature;
class ATDDecoy;
class ATDDefenseCore;
class UAnimMontage;

/**
 * Controller fuer Wellengegner: ein Spiel-Controller, damit der Koerper findet, was er erwartet
 * (Test 0.17.0: mit einem reinen AAIController stuerzte das Spiel in OnCreatureMovementModeChanged ab),
 * aber ohne Denken - kein Verhaltensbaum, kein KI-Tick. Das Verhalten kommt aus UTDGegnerHirn.
 */
UCLASS()
class FICSITTD_API ATDGegnerController : public AFGCreatureController
{
	GENERATED_BODY()
public:
	ATDGegnerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;
	/** Verhaltensbaum anhalten, falls er (wieder) laeuft */
	void Stillegen();
};

UENUM()
enum class ETDHirnPhase : uint8
{
	Marsch,
	Anlauf,
	Angriff,
	Tot
};

/** 0.9.5 Nachzuegler-Diagnose: Messpunkt des Restwegs (alle 2,5 s) oder Neuberechnung des Weges (NeuVor >= 0) */
struct FTDVerlaufPunkt
{
	float Zeit = 0.f;
	float Rest = 0.f;
	float Abseits = 0.f;
	float Hoehe = 0.f;   // Fuss des Gegners minus Wegpunkt (cm), + = ueber dem Weg
	float NeuVor = -1.f;
	FVector Ort = FVector::ZeroVector;
};

/** Kampfwerte je Gegnerart */
struct FTDGegnerWerte
{
	float Reichweite = 350.f;   // Abstand zum Koeder, ab dem angegriffen wird (cm)
	float Schaden = 25.f;       // je Angriff
	float Takt = 1.5f;          // Sekunden zwischen Angriffen
	float Tempo = 700.f;        // Lauftempo cm/s (Test 0.17.1: Spiel-Lauftempo 350 wirkte zu langsam)
	const TCHAR* Montage = nullptr;
	static FTDGegnerWerte Fuer(const UClass* Klasse);
};

/**
 * Gehirn eines Wellengegners (Weg C, Konzept-Gegnersteuerung.md): Koerper und Animationen kommen vom Spiel,
 * alles Verhalten von hier. Marsch auf dem Weg (ohne Wasser) mit Ausweichen vor Felsen, Zielwahl
 * (Mauer/Turm in 25 m, sonst der Kern), Anlauf und Angriff mit Schaden ueber ITDDamageable. Die Spiel-KI laeuft nicht mit.
 */
UCLASS()
class FICSITTD_API UTDGegnerHirn : public UActorComponent
{
	GENERATED_BODY()
public:
	UTDGegnerHirn();

	/** Faktor auf das Lauftempo aller Wellengegner (/td kern tempo), wirkt sofort */
	static float TempoFaktor;
	/** Stresstest-Vergleich: -1 = Balance-Datei (Leistung.*), sonst 0/1 fuer Animation nur sichtbar bzw. Kollision untereinander */
	static int32 AnimNurSichtbarErzwungen;
	static int32 KollisionErzwungen;
	static int32 RagdollErzwungen;
	/** Stresstest T2d: Gegner stehen am Ziel, ohne Angriffs-Montage und ohne Schaden (Gedraenge von Animation trennen) */
	static bool bAngriffAus;

	/** Spawnt eine Kreatur der Klasse mit Gehirn. Weg = Punkte von der Front zum Kern (BerechneWeg). */
	static AFGCreature* Spawne(UWorld* World, UClass* Klasse, const FVector& Ort, const TArray<FVector>& Weg, ATDDefenseCore* Kern, float LPFaktor);
	/** Weg von Ort zum Kern ueber das Navigationsnetz, Wasser gesperrt; leer, wenn keiner gefunden.
	 *  bVoll = false: der Weg endet vor dem Kern (versperrt, z. B. eingemauert) */
	static TArray<FVector> BerechneWeg(UWorld* World, const FVector& Ort, const ATDDefenseCore* Kern, bool* bVoll = nullptr, const APawn* Wer = nullptr);
	static UTDGegnerHirn* Von(const AActor* Kreatur);
	/** 0.9.5: nach unserem Treffer - bleibt ein Rest unter Treffer.Rundungsrest (Standard 1 LP), ist es der toedliche Treffer
	 *  (die Gesundheitskomponente des Spiels toetet erst bei 0). true, wenn getoetet. */
	static bool RestToeten(AActor* Ziel);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	ETDHirnPhase GetPhase() const { return mPhase; }
	/** 0.83.1: steht hinter einem anderen Wellengegner und wartet (das Nachzuegler-Netz laesst ihn dann in Ruhe) */
	bool StehtImStau() const { return mStau > 0.f; }
	int32 GetVersetzt() const { return mVersetzt; }
	/** Bisher angerichteter Schaden und Name des zuletzt angegriffenen Ziels (Diagnose) */
	float GetSchaden() const { return mSchaden; }
	const FString& GetLetztesOpfer() const { return mLetztesOpfer; }
	/** Restweg bis zum Kern (cm) entlang des Weges */
	float GetRestweg() const { return FMath::Max(0.f, mGesamt - mFortschritt); }
	/** Fuehrt der Weg bis zum Kern (false: Teilweg, endet vorher) */
	bool IstWegVoll() const { return mWegVoll; }
	FString Diagnose() const;
	/** 0.9.5: Verlauf von Restweg, Ort und Neuberechnungen der letzten ~40 s (Nachzuegler-Diagnose) */
	FString Verlauf() const;
	/** Nach dem Versetzen durch die Nachzuegler-Pruefung: Weg neu, ohne Netz an dieser Stelle den Ersatzweg der Front nehmen */
	void Neustart(const AFGCreature* Kreatur, const TArray<FVector>& Ersatz);
	/** Teslaspule Level 5 (Spannungsfeld, 0.64): Lauftempo fuer Dauer s auf Faktor senken; im Feld laufend erneuert */
	void Bremse(class AFGCreature* Kreatur, float Faktor, float Dauer);

private:
	void Marschiere(AFGCreature* Kreatur, float DeltaTime);
	void Greife(AFGCreature* Kreatur, float DeltaTime);
	/** Bewegungseingabe in Wunschrichtung, mit Ausweichen und Drehung */
	void Laufe(AFGCreature* Kreatur, const FVector& Wunsch, float DeltaTime);
	/** Zielwahl nach Konzept-Gegnersteuerung 7b: Kern (Regel 1), versperrende Mauer (2/3), sonst kein Ziel = Weg laufen (6) */
	ATDDecoy* SucheZiel(const AFGCreature* Kreatur);
	/** Naechster aktiver Koeder in MaxAbstand (cm, 2D): bHindernis = nur Mauern/Sperren, sonst nur Koeder des Kerns */
	ATDDecoy* KoederNahe(const FVector& Ort, float MaxAbstand, bool bHindernis) const;
	/** Weg ab der aktuellen Stelle neu berechnen (Mauern gebaut oder gefallen) */
	void NeuerWeg(const AFGCreature* Kreatur);
	/** Erste Mauer auf der Linie Gegner -> Kern (Regel 2); nullptr, wenn keine getroffen */
	ATDDecoy* MauerRichtungKern(const AFGCreature* Kreatur) const;
	/** Mauer als festes Ziel setzen (Regel 2/3), mit Logzeile */
	void SetzeBlockade(const AFGCreature* Kreatur, ATDDecoy* Mauer, const TCHAR* Grund);

	TArray<FVector> mWeg;
	float mGesamt = 0.f;
	float mFortschritt = 0.f;
	float mBester = 0.f;
	float mStill = 0.f;
	float mVordermannSumme = 0.f;   // 0.9.4 Diagnose: Sekunden insgesamt hinter einem Wellengegner gewartet
	float mStau = 0.f;   // 0.83.1: Sekunden im Stau hinter einem Wellengegner
	FVector mAusweich = FVector::ZeroVector;
	/** geglaettete Laufrichtung (keine Spruenge beim Ausweichen) */
	FVector mRichtung = FVector::ZeroVector;
	float mAusweichZeit = 0.f;
	/** 0.79 "Breite Front" (Option Wellen.BreiteFront, Konzept-Wellen-und-Spielvarianten A1-A3): fester Querversatz zum Weg (cm),
	 *  aktuelle Skala (halbiert, wenn der versetzte Punkt nicht auf dem Netz liegt), Takt der Netzpruefung, Abstossung von Nachbarn */
	float mVersatz = 0.f;
	float mVersatzSkala = 1.f;
	float mVersatzTakt = 0.f;
	FVector mAbstoss = FVector::ZeroVector;
	float mAbstossTakt = 0.f;
	/** Ziel 6 m voraus mit Querversatz (ohne Option: der Wegpunkt selbst) */
	FVector WegZiel(const AFGCreature* Kreatur, float Voraus);
	/** Abstossung von nahen Wellengegnern (ohne Option: null) */
	FVector Abstossung(const AFGCreature* Kreatur, float DeltaTime);
	float mSeite = 1.f;
	float mZielSuche = 0.f;
	float mAngriffTakt = 0.f;
	int32 mVersetzt = 0;
	/** 0.78.1: aus der Welt gefallen und zurueckgesetzt (Pruefstand: Gegner fielen bis -100 km und griffen den Kern von unten an) */
	int32 mAbgestuerzt = 0;
	/** 0.78.1: Sekunden mit 0 LP ohne Tod (Zombie-Hog) */
	float mOhneLeben = 0.f;
	/** 0.78.2: letzter Ort mit Bodenkontakt (Diagnose: wo faellt er durch?) */
	FVector mLetzterBoden = FVector::ZeroVector;
	/** Weg fuehrt bis zum Kern (false = versperrt, er endet an einer Mauer) */
	bool mWegVoll = true;
	/** Sekunden bis zur naechsten Neuberechnung des Weges */
	float mNeuWeg = 0.f;
	/** 0.9.5: Ring der letzten Messpunkte (Verlauf) und Takt bis zum naechsten */
	TArray<FTDVerlaufPunkt> mVerlauf;
	float mVerlaufTakt = 0.f;
	void Merke(const AFGCreature* Kreatur, float NeuVor);
	/** Anlauf: bester Abstand zum Ziel und Sekunden ohne Annaeherung (festgefahren vor einer Mauer?) */
	float mAnlaufBest = 0.f;
	float mAnlaufStill = 0.f;
	/** 0.65: greift den Kern aus dem Gedraenge an (Kern.Gedraenge) */
	bool mGedraenge = false;
	float mSchaden = 0.f;
	FString mLetztesOpfer;
	/** Lauftempo der Art (cm/s, ohne TempoFaktor); das Gehirn haelt es, weil das Spiel es zuruecksetzt (Test 0.17.0) */
	float mLaufTempo = 0.f;
	/** Stand der Logistik-Liste des Wellen-Subsystems, die die Kapsel ignoriert */
	int32 mLogistikVersion = -1;
	float mPflege = 0.f;
	/** 0.9.1: Bremsen stapeln nicht mehr "letzte gewinnt". Bis zu drei Bremsen laufen nebeneinander, es wirkt die staerkste.
	 *  Endet sie, greift die naechstschwaechere, die noch laeuft (Review 09.11.: Stoerfeld/Oelschlick/Tesla hoben den Haltegriff der Singularitaet auf). */
	static constexpr int32 BremsPlaetze = 3;
	float mBremseFaktor[BremsPlaetze] = { 1.f, 1.f, 1.f };
	double mBremseBis[BremsPlaetze] = { 0.0, 0.0, 0.0 };
	float BremsFaktor() const;
public:
	/** T4c: Toenungskopie des Modells (Leader-Pose). Sie wird nur gezeigt, solange das Original selbst gezeichnet wird -
	 *  sonst sah man sie jenseits der Sichtweite des Originals als gleitenden Geist (Test 0.49.1). */
	TWeakObjectPtr<USkeletalMeshComponent> mToenung;
private:
	float mToenungTakt = 0.f;
	ETDHirnPhase mPhase = ETDHirnPhase::Marsch;
	FTDGegnerWerte mWerte;

	UPROPERTY()
	TObjectPtr<UAnimMontage> mMontage;
	TWeakObjectPtr<ATDDefenseCore> mKern;
	TWeakObjectPtr<ATDDecoy> mZiel;
	/** Mauer, die angegriffen wird, weil sie den Weg versperrt (Regel 2) oder der Gegner an ihr festhaengt (Regel 3); gilt bis sie faellt */
	TWeakObjectPtr<ATDDecoy> mBlockade;
};
