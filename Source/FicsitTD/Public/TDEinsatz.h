#pragma once

#include "CoreMinimal.h"
#include "FGConstructDisqualifier.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDEinsatz.generated.h"

class AFGHologram;
class APawn;
class ATDDefenseCore;
struct FTDSpielerPosten;

/** Eine Zeile im Laden des Kern-Menues */
struct FTDLadenZeile
{
	FString Key;
	int32 Paket = 1;
	int32 Preis = 0;
	bool bVerfuegbar = false;
	float Grundpreis = 0.f;
	float Aufschlag = 0.f;
	int32 Gekauft = 0;
	class UTexture2D* Symbol = nullptr;
	/** Spieler-Posten: Text hinter der Menge ("Inventarplaetze", "Ausruestung"), leer = Gebaeude/Stueck */
	FString Einheit;
	/** Hoechstzahl an Kaeufen, 0 = unbegrenzt */
	int32 Max = 0;
	/** 0.9.6 (Marius 11.11.): Abschnitt im Laden - 0 Tuerme, 1 Verteidigung, 2 Produktion, 3 Strom, 4 Logistik, 5 Pionier und Kern */
	int32 Kategorie = 0;
	static constexpr int32 Kategorien = 6;
	static FString KategorieName(int32 Index);
	/** Preis des Kaufs Nummer (bisher gekauft + Extra), fuer die Vorschau im Warenkorb */
	int32 PreisNach(int32 Extra) const { return FMath::RoundToInt(Grundpreis * (1.f + Aufschlag * (Gekauft + Extra))); }
};

/** Eine Zeile der Kontingent-Uebersicht */
struct FTDKontingentZeile
{
	FString Key;
	int32 Belegt = 0;
	int32 Grenze = 0;
	class UTexture2D* Symbol = nullptr;
};

/** Bau abgelehnt: Kontingent dieses Gebaeudetyps ist voll (Einsatz-Konzept Abschnitt 3) */
UCLASS()
class FICSITTD_API UTDCDKontingent : public UFGConstructDisqualifier
{
	GENERATED_BODY()
public:
	UTDCDKontingent();
};

/** 0.9.6 Bau abgelehnt: Blaupausen sind im Einsatz gesperrt (Marius 12.11.: in der ersten Version keine Blaupausen) */
UCLASS()
class FICSITTD_API UTDCDBlaupause : public UFGConstructDisqualifier
{
	GENERATED_BODY()
public:
	UTDCDBlaupause();
};

/** 0.84.4 Bau abgelehnt: AWESOME-Sink im Einsatz gesperrt (Coupons kommen nur vom Kern, Marius 08.11.) */
UCLASS()
class FICSITTD_API UTDCDSinkGesperrt : public UFGConstructDisqualifier
{
	GENERATED_BODY()
public:
	UTDCDSinkGesperrt();
};

/** Bau abgelehnt: im Verteidigungsfeld waehrend einer Welle (Einsatz-Konzept Abschnitt 2) */
UCLASS()
class FICSITTD_API UTDCDWellenSperre : public UFGConstructDisqualifier
{
	GENERATED_BODY()
public:
	UTDCDWellenSperre();
};

/**
 * Einsatz-Modus, Stufe 1 (Einsatz-Konzept): Startkit, Kontingente fuer Produktions- und Verteidigungsgebaeude
 * (Logistik frei), Bausperre im Verteidigungsfeld waehrend einer Welle, Schalter "freies Bauen" fuer Tests.
 * Der Zustand liegt im Abwehrkern (SaveGame) und wird beim Laden von dort uebernommen.
 */
UCLASS()
class FICSITTD_API UTDEinsatzSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** Kontingent-Schluessel eines Gebaeudes ("Konstruktor", "Bolzenturm", ...); leer = nicht begrenzt (Logistik u. a.) */
	static FString Schluessel(const UClass* BauKlasse);
	/** Baender, Lifte, Masten, Stromleitungen, Rohre, Splitter/Verbinder */
	static bool IstLogistik(const UClass* BauKlasse);
	/** Im Einsatz ohne Materialkosten (und ohne Rueckerstattung beim Abriss): Logistik sowie HUB und MAM (Marius 02.11.) */
	bool Kostenlos(const UClass* BauKlasse) const;
	/** Rezepte der kostenlosen Gebaeude (Logistik, HUB, MAM) ohne Zutaten: Bauen kostet nichts, Abriss gibt nichts zurueck
	 *  (Test 0.27.0: die Hooks auf Kosten/Rueckerstattung allein liessen Abriss weiter Material bringen). Ohne Einsatz: Originale zurueck. */
	void AktualisiereGratisRezepte();
	static void StelleRezepteWiederHer();
	/** Ressourcenscanner (V): jeder Rohstoff, den ein verfuegbares Rezept braucht, wird suchbar; Kohle auch mit dem Kohlekraftwerk
	 *  (Marius 02.11.: im Einsatz fehlen die HUB-Meilensteine, die das sonst freischalten) */
	void SchalteScannerFrei();
	/** Ballistik erscheint erst nach Produktion -> Assembler (Baum-Abhaengigkeit im Asset). Absicherung: Pruefung anstossen
	 *  und den Baum notfalls selbst freischalten */
	void PruefeForschungsbaeume();
	virtual void Deinitialize() override;

	/** Startet einen Einsatz am Kern (fehlt er, wird er 10 m vor den Spieler gesetzt): Startgebaeude freischalten,
	 *  Startkit ins Inventar (einmal je Einsatz), Kontingente an, Wellen ohne Countdown */
	void Starte(ATDDefenseCore* Kern, int32 Stufe, int32 Wellen, APawn* Spieler, TArray<FString>& Meldungen);
	void Beende();
	/** Nach dem Laden eines Spielstands (Kern-BeginPlay) */
	void Lade(ATDDefenseCore* Kern);
	void SetzeFrei(bool bFrei);
	bool IstAktiv() const { return mAktiv; }
	bool IstFrei() const { return mFrei; }

	int32 Grenze(const FString& Key) const;
	/** Nach jeder Welle: Einsatzpunkte gutschreiben (Einsatz-Konzept Abschnitt 4) */
	void WelleBeendet(int32 Welle, float KernAnteil);
	TArray<FTDLadenZeile> LadenZeilen() const;
	TArray<FTDKontingentZeile> KontingentZeilen();
	/** 0.10.0: FICSIT-Coupons sind echte Items (Desc_ResourceSinkCoupon). Konto = Coupons im Inventar + am Kern wartende */
	int32 Konto() const;
	/** EP ausserhalb der Wellenabrechnung gutschreiben (Beute) */
	void GibEP(int32 Punkte, const TCHAR* Grund);
	/** EP fuer etwas anderes als den Laden ausgeben (Turm-Upgrades): nur im Einsatz, ohne bAuchInWelle nur in der Bauphase. Leer = bezahlt, sonst Grund */
	FString ZahleEP(int32 Preis, const FString& Posten, bool bAuchInWelle = false);
	/** Coupon-Item des Spiels (auch die Waehrung des AWESOME-Shops) */
	static UClass* CouponKlasse();
	/** Am Kern wartende Coupons (Kern->mEinsatzPunkte) ins Inventar legen; Rueckgabe: wie viele es geschafft haben */
	int32 GibOffeneCoupons();
	/** 0.10.0: Spielstaende vor 0.10 - FICSIT-Kampfdaten im Inventar werden zu Festplatten. Rueckgabe: getauschte Anzahl */
	int32 TauscheAlteKampfdaten();
	/** Abgerissener Turm: Abschuesse und Upgrades gehen an den naechsten neu gebauten Turm desselben Typs (Marius 03.11.2026) */
	void MerkeTurmErbe(const FString& Turm, int32 Kills, int32 Level);
	bool HoleTurmErbe(const FString& Turm, int32& Kills, int32& Level);
	/** Spieler-Posten (Inventar, Handplatz, Ausruestung): kein Kontingent, wirkt sofort */
	static bool IstSpielerPosten(const FString& Key);
	/** 0.9.6: Anzeigename eines Laden-/Kontingent-Schluessels in der Spielsprache (Schluessel selbst bleiben intern) */
	static FString Anzeige(const FString& Key);
	/** Forschungspunkte (0.10.0: Festplatte des Spiels, Desc_HardDrive) im Inventar des lokalen Spielers */
	int32 FPImInventar() const;
	/** Offene FP (Inventar war voll) ins Inventar legen; Rueckgabe: wie viele es geschafft haben */
	int32 GibOffeneFP();
	/** Wartendes Baumaterial nachreichen; Rueckgabe: Anzahl Teile */
	int32 GibOffenesMaterial();
	/** Von Gegnern zerstoert (Marius 02.11.): Baumaterial zurueck ins Inventar (sonst an den Kern), Meldung gesammelt nach der Welle */
	void GebaeudeZerstoert(const UClass* BauKlasse);
	static UClass* FPKlasse();
	/** Items ins Inventar des lokalen Spielers; was nicht passt, merkt sich der Kern (mMaterialOffen). Rueckgabe: Text fuer den Chat */
	FString GibMaterial(const TMap<UClass*, int32>& Items);
	/** Baukosten (Rezeptzutaten) von Anzahl Gebaeuden eines Kontingent-Schluessels sammeln */
	void SammleBaumaterial(const FString& Key, int32 Anzahl, TMap<UClass*, int32>& Out) const;
	ATDDefenseCore* GetKern() const { return mKern.Get(); }
	/** /td laden: Konto, kaufbare Kontingente mit Preis */
	TArray<FString> LadenText() const;
	/** /td kaufe <Name> [Anzahl]: Antwortzeile */
	FString Kaufe(const FString& Name, int32 Anzahl);
	/** Warenkorb aus dem Kern-Menue: alles oder nichts (Summe muss ins Konto passen) */
	FString KaufeKorb(const TMap<FString, int32>& Korb);
	/** Gebaeudesymbol zum Kontingent-Schluessel (aus dem Bau-Rezept) */
	static class UTexture2D* Symbol(const FString& Key);
	/** Paketgroesse je Kauf (Mauer 5 ...) */
	static int32 Paket(const FString& Key);
	int32 Belegt(const FString& Key);
	TArray<FString> StatusText();

	/** Aus dem Hook auf AFGHologram::ValidatePlacementAndCost: Kontingent und Wellensperre pruefen */
	void PruefeHologramm(AFGHologram* Hologramm);
	/** 0.9.1: Entscheidung von PruefeHologramm ohne Hologramm (Selbsttest "Bausperre"). Leer = erlaubt, sonst
	 *  "Welle", "Sink" oder "Kontingent". */
	FString SperrGrund(UClass* BauKlasse, const FVector& Ort, int32 Anzahl = 1);
	/** To-do-Liste des Spiels: offene Kontingente als Bau-Rezepte in die Einkaufsliste, Einsatz-Block in die private Notiz
	 *  (eigener Text des Spielers bleibt). Alle 3 s, solange ein Einsatz laeuft; beim Ende wird alles entfernt. */
	void AktualisiereToDo();
	/** Zu Beginn jeder Welle: alle Verteidigungsanlagen auf 100 % (Marius: kein Reparieren waehrend der Welle) */
	void HeileVerteidigung();

private:
	void ZaehleNeu();
	/** Abwehrkern kostenlos 10 m vor den Spieler setzen (Einstieg im normalen Spiel, bis es feste Einsatzkarten gibt) */
	ATDDefenseCore* SetzeKern(APawn* Spieler);
	/** Gebaeude und Rezepte des Startkontingents freischalten (neuer Spielstand ohne HUB-Fortschritt) */
	void SchalteStartFrei();
	/** Nach jedem Bau: Kontingent dieses Typs in den Chat */
	UFUNCTION()
	void GebaeudeGebaut(class AFGBuildable* Gebaeude);
public:
	/** Chat-Meldung des Einsatzes (0.9.6 oeffentlich: Meilenstein-Sperre im Modul) */
	void Melde(const FString& Text) const;
	/** 0.9.6 (Marius 12.11.): nach Sieg oder Niederlage - Ende des Durchgangs (0 offen, 1 Sieg, 2 Niederlage) und die drei Wege */
	int32 LaufEnde() const;
	/** Welle der Niederlage noch einmal: laedt den Spielstand vom Start dieser Welle (FicsitTD_Wxx_Start) */
	FString WiederholeWelle(APlayerController* Spieler);
	/** Neuer Durchgang am selben Ort ab Welle 1, Fabrik bleibt, Kern voll */
	FString StarteNeu(APawn* Spieler);
	/** Einsatz beenden: Kontingente und Sperren fallen weg, Kern voll und abreissbar */
	FString BeendeNachEnde();
private:
	FString SpielerWirkung(const FTDSpielerPosten& P, int32 Menge);
	void Verbinde();
	/** 0.10.0: nach dem Laden auf das Spielerinventar warten, dann Kampfdaten -> Festplatten und wartende Coupons ins Inventar */
	void UmstellungNachLaden();
	FTimerHandle mUmstellTimer;
	int32 mUmstellVersuche = 0;
	/** Statuszeile unten rechts fuer den lokalen Spieler (UTDHud) */
	void ZeigeHud();
	void VersteckeHud();
	UPROPERTY()
	TObjectPtr<class UTDHud> mHud;

	/** Einen Bau-Rezept-Eintrag der Einkaufsliste auf Soll setzen (0 = entfernen) */
	void SetzeEinkauf(class UFGShoppingListComponent* Liste, UClass* Rezept, int32 Soll);
	/** Von uns gesetzte Einkaufslisten-Eintraege (Kontingent-Schluessel) */
	TSet<FString> mToDoGesetzt;
	/** In dieser Welle zerstoert: Kontingent-Schluessel -> Anzahl */
	TMap<FString, int32> mZerstoert;
	FTimerHandle mToDoTimer;

	TWeakObjectPtr<ATDDefenseCore> mKern;
	bool mAktiv = false;
	bool mFrei = false;
	TMap<FString, int32> mBelegt;
	float mBelegtZeit = -1.f;
};
