#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FGInteractWidget.h"
#include "TDEinsatz.h"
#include "TDKernTerminal.generated.h"

class ATDDefenseCore;
class ATDTurretBase;
class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UTDKernTerminal;
class UPanelWidget;
class UWidget;

/** +/- -Knopf einer Ladenzeile: merkt sich Schluessel und Schritt (dynamische Delegates tragen keine Parameter) */
UCLASS()
class FICSITTD_API UTDKaufKnopf : public UObject
{
	GENERATED_BODY()
public:
	FString Key;
	int32 Schritt = 1;
	TWeakObjectPtr<UTDKernTerminal> Menue;
	UFUNCTION()
	void Geklickt();
};

/** Pionier-Reiter: Talent eines Zweigs lernen */
UCLASS()
class FICSITTD_API UTDTalentKnopf : public UObject
{
	GENERATED_BODY()
public:
	int32 Zweig = 0;
	TWeakObjectPtr<UTDKernTerminal> Menue;
	UFUNCTION()
	void Geklickt();
};

/** T3: Klick in der Wellenvorschau (Gegnerkarte -> Steckbrief, Zeitleiste -> Welle) */
UCLASS()
class FICSITTD_API UTDVorschauKnopf : public UObject
{
	GENERATED_BODY()
public:
	FString Pfad;
	int32 Welle = -1;
	TWeakObjectPtr<UTDKernTerminal> Menue;
	UFUNCTION()
	void Geklickt();
};

/**
 * Menue des Abwehrkerns (E am Kern), Einsatz-Konzept: Uebersicht, Welle starten, Wegvorschau, freies Bauen,
 * Laden mit Kauf-Knoepfen und Kontingent-Uebersicht. Ganz in C++ aufgebaut (kein Widget-Blueprint), Optik an die
 * Maschinenfenster des Spiels angelehnt (Kopfleiste mit X, dunkle Felder, orange Beschriftung, Noto Sans).
 */
UCLASS()
class FICSITTD_API UTDKernTerminal : public UFGInteractWidget
{
	GENERATED_BODY()
public:
	UTDKernTerminal(const FObjectInitializer& ObjectInitializer);

	/** Aus UTDKaufKnopf: Warenkorb um Schritt aendern (nur Vorschau, gekauft wird erst mit "Bestaetigen") */
	void AendereKorb(const FString& Key, int32 Schritt);
	/** Aus UTDVorschauKnopf: Art (Steckbrief) bzw. Welle (Zeitleiste) in der Vorschau waehlen */
	void VorschauWahl(const FString& Pfad, int32 Welle);
	/** Aus UTDTalentKnopf: Talent lernen (Reiter Pionier) */
	void LerneTalent(int32 Zweig);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BaueOberflaeche();
	/** Zahlen und Knopftexte (alle 0,5 s) */
	void Aktualisiere();
	/** Laden- und Kontingentlisten neu aufbauen (beim Oeffnen und nach Kaeufen) */
	void BaueListen();
	void Melde(const FString& Text);
	ATDDefenseCore* Kern() const;

	UTextBlock* NeuerText(const FString& Text, int32 Groesse, const FLinearColor& Farbe, bool bFett = false);
	/** Knopf im Spielstil; bHaupt = orange (Hauptaktion), sonst grau */
	UButton* NeuerKnopf(const FString& Text, UTextBlock*& OutText, bool bHaupt = false);
	/** Dunkles Feld mit Innenabstand */
	UBorder* NeuesFeld(const FLinearColor& Farbe, const FMargin& Abstand);

	UFUNCTION()
	void WelleStarten();
	UFUNCTION()
	void WegeUmschalten();
	UFUNCTION()
	void FreiUmschalten();
	UFUNCTION()
	void EinsatzStarten();
	/** 0.9.6 (Marius 12.11.): nach Sieg oder Niederlage - Welle wiederholen, neu starten, Einsatz beenden */
	UFUNCTION()
	void EndeWiederholen();
	UFUNCTION()
	void EndeNeustart();
	UFUNCTION()
	void EndeBeenden();
	/** Wellen-Progression: Einsatzlaenge Kurz 20 / Standard 30 / Lang 50 / Endlos */
	UFUNCTION()
	void LaengeWechseln();
	UFUNCTION()
	void Schliessen();
	UFUNCTION()
	void Bestaetigen();
	UFUNCTION()
	void Verwerfen();
	/** T3: zwischen Laden/Kontingent und Wellenvorschau umschalten */
	UFUNCTION()
	void VorschauUmschalten();
	/** 0.58: Reiter Forschung (alle MAM-Knoten mit Zustand) und Pionier (Rang, Talente) */
	UFUNCTION()
	void ForschungUmschalten();
	UFUNCTION()
	void PionierUmschalten();
	UFUNCTION()
	void TalenteNeu();
	/** 0 Laden, 1 Wellenvorschau, 2 Forschung, 3 Pionier */
	void ZeigeAnsicht(int32 Ansicht);
	void BaueForschung();
	void BauePionier();
	/** 0.60 (Skill satisfactory-design): Reiter in der Kopfleiste statt Knoepfen */
	UFUNCTION()
	void ReiterLaden();
	UFUNCTION()
	void ReiterVorschau();
	UFUNCTION()
	void ReiterForschung();
	UFUNCTION()
	void ReiterPionier();
	/** 0.66 (Marius: Verlauf je Welle fuer die Transparenz): Reiter Statistik */
	UFUNCTION()
	void ReiterStatistik();
	void BaueStatistik();
	/** Gegenstands-Kachel wie im Spiel: hellgraues Feld mit Bild, unten rechts orange Mengen-Plakette (leer = keine) */
	UWidget* NeueKachel(class UTexture2D* Bild, float Kante, const FString& Plakette);
	/** Testphase (Balance Test.WellenSprung): zur in der Zeitleiste gewaehlten Welle springen */
	UFUNCTION()
	void Springen();
	/** Knopf ohne eigene Flaeche (Karten, Zeitleiste): leichtes Aufhellen beim Darueberfahren */
	UButton* NeueKlickflaeche(UWidget* Inhalt, const FString& Pfad, int32 Welle);
	/** Zeitleiste aller Wellen des Laufs */
	UWidget* NeueZeitleiste(int32 Gezeigt);
	/** T3: Wellenvorschau aufbauen (Kopf, neue Arten als Steckbrief, alle Arten als Karten, Boss) */
	void BaueVorschau();
	/** Steckbrief-Karte einer Art (gross, fuer NEU und Boss) */
	UWidget* NeuerSteckbrief(const struct FTDPlanGruppe& G, bool bZahlen);
	/** kleine Karte einer Art fuer die Uebersicht */
	UWidget* NeueGegnerKarte(const struct FTDPlanGruppe& G, bool bZahlen);
	/** Bild einer Kreatur (Spiel-Icon ueber den Kreatur-Deskriptor) mit optionalem Abzeichen oben links */
	UWidget* NeuesKreaturBild(const FString& Deskriptor, float Kante, const FString& Abzeichen, const FLinearColor& AbzeichenFarbe);
	/** Kosten des Warenkorbs (mit steigenden Preisen je weiterem Kauf) */
	int32 KorbSumme() const;
	/** Gebaeudesymbol 48 px (leer, wenn keins) */
	UWidget* NeuesSymbol(class UTexture2D* Bild, float Groesse);

	UPROPERTY()
	TObjectPtr<UTextBlock> mTitel;
	UPROPERTY()
	TObjectPtr<UTextBlock> mWelle;
	UPROPERTY()
	TObjectPtr<UTextBlock> mKernLP;
	UPROPERTY()
	TObjectPtr<UTextBlock> mKonto;
	UPROPERTY()
	TObjectPtr<UTextBlock> mFronten;
	UPROPERTY()
	TObjectPtr<UTextBlock> mMeldung;
	UPROPERTY()
	TObjectPtr<UTextBlock> mWelleText;
	UPROPERTY()
	TObjectPtr<UTextBlock> mWegeText;
	UPROPERTY()
	TObjectPtr<UTextBlock> mFreiText;
	UPROPERTY()
	TObjectPtr<UButton> mStartKnopf;
	UPROPERTY()
	TObjectPtr<UButton> mWelleKnopf;
	UPROPERTY()
	TObjectPtr<UButton> mEndeWiederholen;
	UPROPERTY()
	TObjectPtr<UButton> mEndeNeustart;
	UPROPERTY()
	TObjectPtr<UButton> mEndeBeenden;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mLaden;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mKontingent;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mKorbListe;
	UPROPERTY()
	TObjectPtr<UTextBlock> mKorbSumme;
	UPROPERTY()
	TObjectPtr<UTextBlock> mKorbRest;
	UPROPERTY()
	TObjectPtr<UButton> mBestaetigen;
	UPROPERTY()
	TArray<TObjectPtr<UTDKaufKnopf>> mKaufKnoepfe;
	/** T3: Laden/Kontingent-Bereich und Vorschau-Bereich (gleiche Hoehe, einer sichtbar) */
	UPROPERTY()
	TObjectPtr<UWidget> mSpalten;
	UPROPERTY()
	TObjectPtr<UWidget> mVorschau;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mVorschauInhalt;
	UPROPERTY()
	TObjectPtr<UTextBlock> mVorschauKnopfText;
	UPROPERTY()
	TObjectPtr<UTextBlock> mVorschauCountdown;
	bool mZeigtVorschau = false;
	int32 mAnsicht = 0;
	UPROPERTY()
	TArray<TObjectPtr<UButton>> mReiter;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mReiterTexte;
	UPROPERTY()
	TObjectPtr<UWidget> mForschung;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mForschungInhalt;
	UPROPERTY()
	TObjectPtr<UTextBlock> mForschungKnopfText;
	UPROPERTY()
	TObjectPtr<UWidget> mPionier;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mPionierInhalt;
	UPROPERTY()
	TObjectPtr<UWidget> mStatistik;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mStatistikInhalt;
	UPROPERTY()
	TObjectPtr<UTextBlock> mPionierKnopfText;
	UPROPERTY()
	TArray<TObjectPtr<UTDTalentKnopf>> mTalentKnoepfe;
	int32 mLaenge = 2;   // 0.77: Standard 50 Wellen
	UPROPERTY()
	TObjectPtr<UTextBlock> mStartText;
	UPROPERTY()
	TObjectPtr<UButton> mLaengeKnopf;
	UPROPERTY()
	TObjectPtr<UTextBlock> mLaengeText;
	/** naechste bzw. laufende Welle beim letzten Aufbau (Wechsel -> neu aufbauen) */
	int32 mVorschauNaechste = -1;
	/** in der Zeitleiste gewaehlte Welle (-1 = naechste) und angeklickte Art */
	int32 mAuswahlWelle = -1;
	FString mAuswahlArt;
	UPROPERTY()
	TArray<TObjectPtr<UTDVorschauKnopf>> mVorschauKnoepfe;

	/** Auswahl im Laden: Schluessel -> Anzahl Kaeufe */
	TMap<FString, int32> mKorb;
	TArray<FTDLadenZeile> mLadenZeilen;
	/** 0.9.6: zugeklappte Laden-Abschnitte (Kategorie) */
	TSet<int32> mLadenZu;
	FTimerHandle mTimer;
	/** Mauszeiger selbst eingeschaltet (das Spiel hat es nicht getan) -> beim Schliessen zuruecksetzen */
	bool mZeigerSelbst = false;
};

/**
 * Werte-Panel im Maschinen-Menue eines Turms: liegt ueber dem (fuer Tuerme nutzlosen) Ausgabe-Feld des Spielfensters und
 * zeigt Stufe, Lebenspunkte, Schaden, Feuerrate, Schaden pro Sekunde, Munition usw. (ATDTurretBase::Kennwerte).
 */
class UTDTurmInfo;

/** Knopf der Zielwahl im Turm-Fenster: Modus (Familie leer lassen) oder Familie (Modus -1) */
UCLASS()
class FICSITTD_API UTDZielKnopf : public UObject
{
	GENERATED_BODY()
public:
	int32 Modus = -1;
	FString Familie;
	bool bFamilie = false;
	TWeakObjectPtr<UTDTurmInfo> Info;
	UFUNCTION()
	void Geklickt();
};

/** Level-Knopf im Turm-Fenster (0.63: nur Art 0 = naechstes Level kaufen) */
UCLASS()
class FICSITTD_API UTDUpgradeKnopf : public UObject
{
	GENERATED_BODY()
public:
	int32 Art = 0;
	TWeakObjectPtr<UTDTurmInfo> Info;
	UFUNCTION()
	void Geklickt();
};

UCLASS()
class FICSITTD_API UTDTurmInfo : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetzeTurm(ATDTurretBase* Turm) { mTurm = Turm; }
	/** Zielwahl am Turm setzen (Knopf), Anzeige sofort nachziehen */
	void WaehleZiel(int32 Modus, const FString& Familie, bool bFamilie);
	void KaufeUpgrade(int32 Art);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	void Fuelle();
	/** Ueber das Ausgabe-Feld des Maschinenfensters legen; die Zielwahl ueber die beiden gesperrten Felder unten */
	void Platziere(const FGeometry& Eigene);
	void SetzeFeld(UWidget* Panel, const FGeometry& Eigene, const FGeometry& Bezug, const FVector2D& Von, const FVector2D& Bis);
	UBorder* NeuesZielPanel(UPanelWidget* Flaeche, const TCHAR* Titel, class UPanelWidget*& OutInhalt);
	void FuelleZielwahl();
	void FuelleUpgrades();

	TWeakObjectPtr<ATDTurretBase> mTurm;
	TWeakObjectPtr<UUserWidget> mFenster;
	UPROPERTY()
	TObjectPtr<UBorder> mPanel;
	UPROPERTY()
	TObjectPtr<UTextBlock> mKopf;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mListe;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mNamen;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mWerte;
	UPROPERTY()
	TObjectPtr<UBorder> mModusPanel;
	/** 0.78.5: Seitenfeld links neben dem Maschinenfenster (Kopfleiste und Koerper wie ein Spielfenster) */
	UPROPERTY()
	TObjectPtr<UBorder> mSeite;
	UPROPERTY()
	TObjectPtr<UBorder> mFamilienPanel;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mModusTexte;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mFamilienTexte;
	UPROPERTY()
	TArray<TObjectPtr<UTDZielKnopf>> mZielKnoepfe;
	/** Auswahlpunkte der Zielwahl (gezeichnet: die Spielschrift hat keine Kreiszeichen, Test 0.56 zeigte "?") */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> mModusPunkte;
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> mFamilienPunkte;
	/** Level-Zeile (0.63): Kaestchen je Level und Vorschau des naechsten Levels */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> mUpgradeStufen;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mUpgradeWirkung;
	int32 mUpgradeStufenMax = 0;
	UPROPERTY()
	TObjectPtr<UWidget> mUpgradeReihe;
	UPROPERTY()
	TArray<TObjectPtr<UButton>> mUpgradeKnoepfe;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mUpgradeTexte;
	UPROPERTY()
	TArray<TObjectPtr<UTDUpgradeKnopf>> mUpgradeObjekte;
	UPROPERTY()
	TObjectPtr<UTextBlock> mUpgradeHinweis;
	/** Turm-Spezialisierung (0.58): Hinweis und zwei Knoepfe (UTDUpgradeKnopf Art 10/11) */
	UPROPERTY()
	TObjectPtr<UTextBlock> mSpezHinweis;
	UPROPERTY()
	TObjectPtr<UWidget> mSpezReihe;
	UPROPERTY()
	TArray<TObjectPtr<UButton>> mSpezKnoepfe;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mSpezTexte;
	FString mUpgradeMeldung;
	float mUpgradeMeldungZeit = 0.f;
	float mTakt = 0.f;
	bool mGemeldet = false;
	/** 0.64.2 (Marius: Turmwerte lagen ueber der Rezeptauswahl): ist im Maschinenfenster der Reiter "Rezept auswaehlen" offen? */
	bool RezeptAuswahlOffen();
	float mVerdecktTakt = 0.f;
	bool mVerdeckt = false;
	FString mFensterKlassen;
};

/** T4: Lebensbalken der Bosse oben in der Mitte (Name, LP aller Bosse der Welle, lebend/gesamt), solange ein Boss lebt */
UCLASS()
class FICSITTD_API UTDBossBalken : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	UPROPERTY()
	TObjectPtr<UTextBlock> mName;
	UPROPERTY()
	TObjectPtr<UTextBlock> mWerte;
	UPROPERTY()
	TObjectPtr<class UProgressBar> mBalken;
	float mTakt = 0.f;
};

/** Seitenleiste rechts waehrend einer Welle: Fortschritt (besiegt / gesamt), lebend und noch kommend, je Gegnerart mit Bild */
UCLASS()
class FICSITTD_API UTDWellenLeiste : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	void Fuelle();
	UPROPERTY()
	TObjectPtr<UTextBlock> mKopf;
	UPROPERTY()
	TObjectPtr<UTextBlock> mZahlen;
	UPROPERTY()
	TObjectPtr<class UProgressBar> mBalken;
	UPROPERTY()
	TObjectPtr<UVerticalBox> mListe;
	/** Zeilen je Art: Text "lebend / noch" (Reihenfolge wie mArten) */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mZeilen;
	TArray<FString> mArten;
	float mTakt = 0.f;
	/** 0.66 (Marius: welche Tuerme machen in der Welle welchen Schaden): Schaden je Turmart live */
	void FuelleSchaden();
	UPROPERTY()
	TObjectPtr<UVerticalBox> mSchadenListe;
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> mSchadenTexte;
	UPROPERTY()
	TArray<TObjectPtr<class UProgressBar>> mSchadenBalken;
	TArray<FString> mSchadenArten;
};

/** Kleine Statuszeile unten rechts (Welle, Kern, Konto), solange ein Einsatz laeuft */
UCLASS()
class FICSITTD_API UTDHud : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	void Aktualisiere();
	UPROPERTY()
	TObjectPtr<UTextBlock> mText;
	FTimerHandle mTimer;
};
