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
	static TArray<FVector> BerechneWeg(UWorld* World, const FVector& Ort, const ATDDefenseCore* Kern, bool* bVoll = nullptr);
	static UTDGegnerHirn* Von(const AActor* Kreatur);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	ETDHirnPhase GetPhase() const { return mPhase; }
	int32 GetVersetzt() const { return mVersetzt; }
	/** Bisher angerichteter Schaden und Name des zuletzt angegriffenen Ziels (Diagnose) */
	float GetSchaden() const { return mSchaden; }
	const FString& GetLetztesOpfer() const { return mLetztesOpfer; }
	/** Restweg bis zum Kern (cm) entlang des Weges */
	float GetRestweg() const { return FMath::Max(0.f, mGesamt - mFortschritt); }
	FString Diagnose() const;
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
	FVector mAusweich = FVector::ZeroVector;
	/** geglaettete Laufrichtung (keine Spruenge beim Ausweichen) */
	FVector mRichtung = FVector::ZeroVector;
	float mAusweichZeit = 0.f;
	float mSeite = 1.f;
	float mZielSuche = 0.f;
	float mAngriffTakt = 0.f;
	int32 mVersetzt = 0;
	/** Weg fuehrt bis zum Kern (false = versperrt, er endet an einer Mauer) */
	bool mWegVoll = true;
	/** Sekunden bis zur naechsten Neuberechnung des Weges */
	float mNeuWeg = 0.f;
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
	float mBremseFaktor = 1.f;
	double mBremseBis = 0.0;
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
