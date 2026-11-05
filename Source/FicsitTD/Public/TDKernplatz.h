#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDKernplatz.generated.h"

/** Kandidat fuer einen festen Kernplatz (Konzept-Kernplaetze.md, Abschnitt 2: aus den Kartendaten von satisfactory-calculator.com) */
struct FTDPlatzKandidat
{
	const TCHAR* Id;
	const TCHAR* Biom;
	float X;
	float Y;
	/** Hoehe der Knoten in der Umgebung (cm), nur Startwert fuer die Bodensuche */
	float ZHinweis;
	/** Stufe fest am Biom: 0 Erkundung, 1 Normal, 2 Hart, 3 Albtraum */
	int32 Stufe;
};

/** 0.67 (Kernplaetze S1): /td platz pruefen <Kandidat|hier [Biom]> - Gelaende, Fronten, Rohstoffe pruefen; /td platz merken <Biom> - Platz speichern */
UCLASS()
class FICSITTD_API UTDKernplatz : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDKernplatz* Get(const UObject* WorldContext);
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	/** 0.69.1 (S4): ist die Spieleinstellung "FICSIT Tower Defense" an? */
	static bool TowerDefenseAn(const UObject* WorldContext);
	/** 0.69.4 (Marius: Begruessung durch ADA): ADA-Nachricht "Einsatzbeginn" abspielen (Nachrichtensystem des Spiels, Untertitel) */
	static bool SpieleBegruessung(const UObject* WorldContext, class APawn* Spieler);
	/** 0.69.7: Projekt-ID der Init-Bank des Spiels ins Log (eigene Soundbanks brauchen sie im Kopf, sonst Fehler 92) */
	static FString PruefeWwiseInitBank();
	static const TArray<FTDPlatzKandidat>& Kandidaten();
	static const FTDPlatzKandidat* Finde(const FString& Id);
	/** Biom aus einer Eingabe (grasfelder, felsenwueste, wald, duenen); Stufe -1 = unbekannt */
	static int32 BiomAus(const FString& Eingabe, FString& OutName);
	/** Startet die Pruefung; Meldungen kommen im Chat. Id "hier" = Spielerort, Biom optional (bestimmt die Stufe) */
	FString Pruefe(const FString& Id, class APawn* Spieler, const FString& Biom = FString());
	/** Zurueck an den Ort vor der ersten Pruefung */
	FString Zurueck(class APawn* Spieler);
	/** 0.67.1: letzten gepruefte Platz als Kernplatz des Bioms speichern (Saved/FicsitTD/Kernplaetze-Auswahl.json) */
	FString Merke(const FString& Biom, const FString& Notiz, class APawn* Spieler);
	/** Gespeicherte Plaetze als Text */
	TArray<FString> Gemerkt() const;
	/** 0.68 (Marius: selbst nach Plaetzen suchen): Biom im Raster abfliegen, freie ebene Flaechen bewerten, die besten mit Fronten pruefen */
	FString StarteScan(const FString& Biom, class APawn* Spieler);
	FString StoppeScan();
	/** 0.69 (S3): festen Platz des Bioms (zufaellig oder Nr) anfliegen, Plattform aus Fundamenten und Kern bauen, Einsatz starten */
	FString BauePlatz(const FString& Biom, const FString& Nr, class APawn* Spieler);

private:
	void SucheBoden();
	void WarteAufNetz();
	void Auswerten();
	void Melde(const FString& Text) const;
	/** Anteil der 16 Messpunkte in Front-Entfernung, die schon auf dem Navigationsnetz liegen */
	int32 NetzPunkte() const;
	/** Boden unter X/Y (Landschaft, sonst ebene Flaeche wie Duenen); bHindernis = darueber steht etwas (Baum, Fels, Gebaeude) */
	bool Boden(float X, float Y, float ZMitte, FVector& Out, bool& bHindernis, FString& Hindernis) const;
	static FString AuswahlDatei();
	/** 0.67.2: Navigationsnetz-Invoker (320 m) auf der Platzmitte, wie ihn der Kern hat - sonst gibt es Netz nur um den Spieler (~100 m) */
	void SetzeInvoker(const FVector& Ort);
	void EntferneInvoker();
	/** 0.68.2 (Test: in den Duenen Schaden bis zum Tod): waehrend Scan und Pruefung unverwundbar und volle Gesundheit */
	void Schuetze(bool bAn);
	void BaueJetzt();
	bool mBauen = false;
	/** S4: beim ersten Laden eines TD-Spielstands automatisch bauen, sobald der Spieler steht */
	void PruefeTDStart();
	FTimerHandle mStartTimer;
	int32 mStehtSeit = 0;
	/** 0.69.1: Einsatz erst starten, wenn das Navigationsnetz um den neuen Kern steht (Test 0.69.0: Ansage "0 von 16") */
	void WarteNetzFuerEinsatz();
	TWeakObjectPtr<class ATDDefenseCore> mNeuerKern;
	int32 mEinsatzWarten = 0;
	TWeakObjectPtr<AActor> mInvoker;

	// ---- Scan (0.68) ----
	struct FScanPunkt { float Z = 0.f; bool bHindernis = false; bool bWasser = false; };
	struct FScanTreffer { FVector Ort; float Hub = 0.f; int32 Hindernisse = 0; float Frei = 0.f; float Eben = 0.f; float Rau = 0.f; float Wert = 0.f; int32 Richtungen = -1; FString Fronten; };
	void ScanSchritt();
	void ScanMessen();
	void ScanBewerten();
	/** Phase 2: naechsten Treffer mit voller Pruefung (Netz, Fronten) ansehen */
	void ScanNaechstePruefung();
	bool mScanLaeuft = false;
	int32 mScanPhase = 0;
	FString mScanBiom;
	int32 mScanStufe = 1;
	TArray<FVector2D> mScanStationen;
	int32 mScanIndex = 0;
	int32 mScanWarten = 0;
	float mScanZ = 0.f;
	double mScanStart = 0.0;
	TMap<FIntPoint, FScanPunkt> mScanKarte;
	TArray<FScanTreffer> mScanTreffer;
	int32 mScanPruefIndex = 0;
	FTimerHandle mScanTimer;

	TWeakObjectPtr<class APawn> mSpieler;
	FString mId;
	FString mBiom;
	int32 mStufe = 1;
	FVector mZiel = FVector::ZeroVector;
	FVector mBodenMitte = FVector::ZeroVector;
	bool mHatRueckweg = false;
	FVector mRueckweg = FVector::ZeroVector;
	int32 mVersuche = 0;
	int32 mNetzVersuche = 0;
	int32 mNetzLetzte = -1;
	int32 mNetzGleich = 0;
	FTimerHandle mTimer;
	/** Ergebnis der letzten Pruefung (fuer merken) */
	bool mHatErgebnis = false;
	FVector mErgebnisOrt = FVector::ZeroVector;
	float mErgebnisHub = 0.f;
	int32 mErgebnisHindernisse = 0;
	int32 mErgebnisRichtungen = 0;
	int32 mErgebnisNetz = 0;
	FString mErgebnisFronten;
	FString mErgebnisRohstoffe;
};
