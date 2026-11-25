#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDWaveSubsystem.generated.h"

class AFGCreature;
class ATDTestWall;
class ATDDefenseCore;
class AFGBuildable;

/** Ablauf des Modus Kern-Verteidigung (Spielkonzept S2/S3) */
UENUM()
enum class ETDLauf : uint8
{
	Aus,
	Aufbau,     // Countdown bis zur naechsten Welle
	Welle       // Welle laeuft, bis alle Gegner tot sind
};

UENUM()
enum class ETDSteuerModus : uint8
{
	/** Nur Aggro/Ziel der Kreatur-KI setzen - die KI entscheidet selbst, wie sie hinkommt. */
	Aggro,
	/** Zusaetzlich per MoveToActor zum Ziel laufen lassen, solange die KI nicht kaempft. */
	Laufen
};

/** 0.13 K1 (Fahrplan-0.13, Bericht A1 5.1): Ist der Kern fuer Wellengegner erreichbar? Schlechtester Zustand ueber die aktiven Fronten. */
enum class ETDKernZugang : uint8
{
	Frei,              // voller Weg
	Durchbrechbar,     // Teilweg, die Sperre sind nur TD-Hindernisse (Mauer, Sperre, Tor, Zaun) - die Gegner greifen sie an
	NetzNichtBereit,   // Wegsuche ohne Punkte (Netz um den Kern noch im Bau)
	Unpassierbar       // Teilweg, ein Spielbau, eine Maschine oder ein Turm sperrt (oder Gelaende ohne erkannten Bau)
};

/** Ergebnis einer Pruefung "Kern erreichbar" */
struct FTDKernZugang
{
	ETDKernZugang Zustand = ETDKernZugang::NetzNichtBereit;
	/** Front mit dem schlechtesten Zustand (1-basiert), 0 = Ringprobe ohne Fronten */
	int32 Front = 0;
	/** Ein Bau des Spielers sperrt (Sperre, Sperrstelle gesetzt). false bei Unpassierbar = Gelaende oder unbekannt */
	bool bBau = false;
	FVector Sperrstelle = FVector::ZeroVector;
	/** Anzeigename des sperrenden Baus (Spielsprache) und Klasse fuer das Log */
	FString Sperre;
	FString SperreKlasse;
	int32 Wegsuchen = 0;
	double Ms = 0.0;
	/** FPlatformTime::Seconds der Pruefung (Zwischenspeicher KernZugang.Haltezeit) */
	double Zeit = -1000.0;
	bool IstGesperrt() const { return Zustand == ETDKernZugang::Unpassierbar; }
};

/** Schwierigkeitsstufe eines Laufs (Level-Konzept Abschnitt 5). Zeiten in Sekunden, Abstaende in cm. */
struct FTDStufe
{
	const TCHAR* Name;
	float Budget;
	float LP;
	float Aufbau;
	float Zwischen;
	int32 FrontenStart;
	int32 NeueFrontAb;
	int32 EliteAb;
	float MinAbstand;
	float MaxAbstand;
	float KernLP;
	/** Wellenart, Elite-Anzahl und Tipp ansagen (Hart/Albtraum: nur Richtung und Anzahl) */
	bool bZusammensetzung;
};

/** Feste Front eines Laufs: Spawnpunkt mit Weg zum Kern (Level-Konzept Abschnitt 3) */
struct FTDFront
{
	float Winkel = 0.f;
	FVector Ort = FVector::ZeroVector;
	/** Weglaenge zum Kern in cm; 0 = nicht ueber das Navigationsnetz geprueft */
	float Weg = 0.f;
	/** Wegpunkte fuer die Vorschau (nur bei geprueften Fronten) */
	TArray<FVector> Punkte;
	/** 18.11. (feste Fronten je Kernplatz): Richtung 0-15 (je 22,5 Grad) und Abstand 0 nah / 1 mittel / 2 fern; -1 = unbekannt */
	int32 Richtung = -1;
	int32 Abstand = -1;
};

/** T3 Wellenvorschau: eine Gegnerart der geplanten Welle */
struct FTDPlanGruppe
{
	/** Objektpfad der Kreaturklasse */
	FString Pfad;
	int32 Anzahl = 0;
	/** davon Elite (gross, LP x Wellen.EliteLP) */
	int32 Elite = 0;
	/** erste Welle dieser Art (AbWelle == Welle): eigene kleine Gruppe, in der Vorschau als NEU */
	bool bNeu = false;
	bool bBoss = false;
	/** Boss: LP-Faktor obendrauf und Name fuer Ansage/Vorschau */
	float BossLP = 1.f;
	FString BossName;
};

/** T3: Steckbrief einer Gegnerart fuer Vorschau und Ansagen (deutscher Name, Bild, Beschreibung, Werte) */
struct FTDKreaturSteckbrief
{
	FString Name;
	/** Klasse des Kreatur-Deskriptors (Bild ueber UFGItemDescriptor::GetBigIcon, beachtet Arachnophobie) */
	FString Deskriptor;
	FString Text;
	/** Rolle 0 Ansturm, 1 Panzer, 2 Fern, 3 Schwarm, 4 Boss */
	int32 Rolle = 0;
	int32 Punkte = 0;
	float Tempo = 0.f;
	float Reichweite = 0.f;
	float Schaden = 0.f;
	/** Schadensart -> Faktor (aus den Resistenzen), z. B. "Bolzen" 1.0, "Gewehr" 0.6 */
	TArray<TPair<FString, float>> Wirkung;
};

/** T3: geplante Welle fuer die Vorschau */
struct FTDWellenVorschau
{
	bool bGueltig = false;
	int32 Welle = 0;
	int32 WellenGesamt = 0;
	FString Art;
	FString Tipp;
	FString Fronten;
	int32 Gegner = 0;
	int32 Elite = 0;
	int32 Punkte = 0;
	/** Hart/Albtraum: nur Arten zeigen, keine Anzahl */
	bool bZusammensetzung = true;
	/** Welle laeuft schon (sonst Aufbau/Countdown) */
	bool bLaeuft = false;
	/** die naechste bzw. laufende Welle (sonst Vorschau einer spaeteren/frueheren aus der Zeitleiste) */
	bool bAktuell = true;
	float Countdown = -1.f;
	TArray<FTDPlanGruppe> Gruppen;
};

/** T3: eine Welle in der Zeitleiste der Vorschau */
struct FTDZeitleistenWelle
{
	int32 Welle = 0;
	/** 0 geschafft, 1 naechste (Aufbau), 2 laeuft, 3 kommt noch */
	int32 Zustand = 3;
	bool bBoss = false;
	bool bNeueFront = false;
	/** Klassenpfade der Arten, die in dieser Welle neu dazukommen */
	TArray<FString> NeueArten;
};

/** Seitenleiste: Stand der laufenden Welle je Gegnerart */
struct FTDArtStatus
{
	FString Pfad;
	int32 Lebend = 0;
	int32 Offen = 0;
	int32 Gesamt = 0;
	bool bBoss = false;
};

/** Seitenleiste: Stand der laufenden Welle */
struct FTDWellenStatus
{
	bool bGueltig = false;
	int32 Welle = 0;
	int32 WellenGesamt = 0;
	FString Art;
	int32 Groesse = 0;
	int32 Lebend = 0;
	int32 Offen = 0;
	int32 Besiegt = 0;
	TArray<FTDArtStatus> Arten;
};

/** Ein Wellen-Gegner mit seinem eigenen Ziel. */
struct FTDWellenGegner
{
	TWeakObjectPtr<AFGCreature> Kreatur;
	/** Festes Ziel (Spieler/Gebaeude); ungenutzt im Mauer-Modus */
	TWeakObjectPtr<AActor> Ziel;
	/** Mauer-Modus: Ziel ist jeweils der naechste aktive Koeder (Test- oder Verteidigungsmauer) */
	bool bMauer = false;
	/** Kern-Modus: Gegner mit eigenem Gehirn (UTDGegnerHirn) - das Subsystem zaehlt ihn nur, gesteuert wird er dort */
	bool bKern = false;
	/** Nachzuegler-Pruefung: bester Abstand zum Kern (cm), Zeit ohne Annaeherung, Versuche */
	float BesterAbstand = -1.f;
	float Still = 0.f;
	int32 Rettungen = 0;
};

/**
 * Tower Defense: plant und spawnt Gegnerwellen (Kern-Verteidigung: Gegner mit UTDGegnerHirn) und Testmauern.
 * Nur die Testwellen von /welle werden hier jede Sekunde ueber die Spiel-KI gelenkt.
 * Schaltet ausserdem das Rezept der Verteidigungsmauer frei.
 * Laeuft nur auf dem Server bzw. im Einzelspieler (Chat-Befehle werden dort ausgefuehrt).
 */
UCLASS()
class FICSITTD_API UTDWaveSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Deinitialize() override;
	/** T2i Phasenmessung: Zeitstempel einer Bildphase (0..7), von Delegates und Tick-Sonden gesetzt */
	void Stempel(int32 Phase);

	/** Alle Kreatur-Blueprints des Spiels (Objektpfad der Klasse), gefunden ueber die Asset Registry. */
	static TArray<FSoftClassPath> FindCreatureClasses();

	/** Spawnt Anzahl Kreaturen im Halbkreis um Zentrum. Target == nullptr bedeutet Mauer-Modus. */
	int32 SpawnWave(TSubclassOf<AFGCreature> CreatureClass, int32 Count, float DistanceCm, const FVector& Center, AActor* Target);

	/** Testmauer am Boden bei GroundLocation, Rotation = Ausrichtung der Wandlaenge. */
	ATDTestWall* SpawnWall(const FVector& GroundLocation, const FRotator& Rotation, float MaxHealth);

	/** Naechstes noch stehendes Verteidigungsziel (Test- oder Verteidigungsmauer) zu Location. */
	AActor* FindNearestDefense(const FVector& Location) const;

	/** Alle lebenden Wellen-Gegner (Ziele fuer Tuerme). */
	TArray<AFGCreature*> GetLivingWaveCreatures() const;

	int32 StopAll();

	// ---- Modus Kern-Verteidigung ----
	/** Startet einen Lauf (Kern aufladen, Countdown zur ersten Welle). false, wenn schon einer laeuft. */
	/** Zeiten in Sekunden. Lange Aufbauphasen, weil die Munitions-Produktion aufgebaut werden muss (Spielkonzept: Munitionslogistik). */
	/** Countdown < 0 = Wert der Stufe. Stufe 0 Erkundung, 1 Normal, 2 Hart, 3 Albtraum. */
	bool StarteKernVerteidigung(ATDDefenseCore* Kern, int32 Wellen = 30, float ErsterCountdown = -1.f, float Countdown = -1.f, int32 Stufe = 1);
	/** Stufe mit den Werten aus der Balance-Datei (Stufen.<Name>.*) */
	static FTDStufe GetStufe(int32 Stufe);
	int32 GetStufeIndex() const { return mStufe; }
	/** "erkundung", "leicht", "normal", "hart", "albtraum" -> 0..3, sonst -1 */
	static int32 StufeAusText(const FString& Text);
	/** Wellen-Kreaturen lassen keine Teile fallen (Marius 02.11.: sonst liegt am Ende alles voll) */
	static void KeinLoot(class AFGCreature* Kreatur);
	/** Kreaturteile (Hog/Spitter/Stinger/Hatcher) im Umkreis wegraeumen, die doch gefallen sind; liefert die Anzahl */
	static int32 RaeumeLoot(UWorld* World, const FVector& Mitte, float Radius);
	/** Test 0.45.1: Wellengegner aus alten Spielstaenden (ohne Tag und ohne Spawner, Art aus dem Katalog) im Umkreis entfernen.
	 *  Sie kamen nach dem Laden als "Wildtiere" mit Spiel-KI zurueck und kaempften an den Fronten untereinander. */
	static int32 RaeumeWaisen(UWorld* World, const FVector& Mitte, float Radius, FString& Bericht);
	/** Diagnose Navigationsnetz um Mitte: Invoker-Modus, Netz am Kern, Reichweite je Abstand (8 Richtungen) */
	static FString NavDiagnose(UWorld* World, const FVector& Mitte);
	/** Pfad fuehrt zum Ziel: voll, oder teilweise mit Ende hoechstens 8 m vor dem Ziel (der Kern selbst ist nicht begehbar) */
	static bool PfadAmZiel(const class UNavigationPath* Pfad, const FVector& Ziel);
	/** Fronten beim naechsten Abruf neu suchen (/td kern fronten neu) */
	void VerwerfeFronten() { mFronten.Reset(); }
	/** 0.67 (Kernplaetze S1): Fronten an einem anderen Ort in einer Stufe suchen, ohne die Fronten des Einsatzes zu aendern */
	TArray<FTDFront> PruefeFrontenAn(const FVector& Mitte, int32 Stufe, int32& OutGueltig);
	/** Fronten pruefen und auflisten (auch ohne Lauf, dann mit Stufe Normal) */
	TArray<FString> GetFrontenText(ATDDefenseCore* Kern);
	/** Wegvorschau: alle Fronten (aktive zuerst) */
	const TArray<FTDFront>& GetFronten() const { return mFronten; }
	int32 AktiveFrontenFuer(int32 Welle) const;
	/** Wege der aktiven und der naechsten neuen Front zum Kern neu berechnen (beruecksichtigt neu gebaute Mauern) */
	TArray<TArray<FVector>> BerechneWege(ATDDefenseCore* Kern, FString& Bericht);
	/** Countdown ueberspringen: naechste Welle sofort */
	bool NaechsteWelleJetzt();
	/** Test: im Aufbau zur Welle N springen (naechste Welle ist dann N); Fronten-Marker und Ansagen wie nach Welle N-1 */
	bool SpringeZuWelle(int32 N);
	void StoppeLauf(const FString& Grund);
	void KernGefallen(ATDDefenseCore* Kern);
	/** Nach dem Laden: Durchgang aus dem Kern-Speicherstand fortsetzen (laufende Welle wird wiederholt) */
	void SetzeLaufFort(ATDDefenseCore* Kern);
	FString GetLaufText() const;
	bool LaeuftKernVerteidigung() const { return mLauf != ETDLauf::Aus; }
	/** Gerade unterwegs ist eine Welle (nicht Aufbau) */
	bool LaeuftWelle() const { return mLauf == ETDLauf::Welle; }
	/** Zuletzt gestartete Welle (0 = noch keine) */
	int32 GetWelle() const { return mWelle; }
	/** 17.11. (P16/A13): Welle, fuer die "ab Welle" freigibt - in der Bauphase die naechste, waehrend der Welle die laufende */
	int32 GetFreigabeWelle() const { return LaeuftWelle() ? mWelle : mWelle + 1; }
	/** 0.9.6: Stufe des laufenden bzw. letzten Durchgangs (Neustart nach Sieg/Niederlage) */
	int32 GetLaufStufe() const { return mStufe; }
	/** 0.9.6 P1: LP-Zuwachs der laufenden Welle ohne Stufe (Welle 1 = 1,0); der Spielerschaden waechst anteilig mit */
	float GetWellenWachstum() const { return mWellenWachstum; }
	float mWellenWachstum = 1.f;
	/** T3: Vorschau der naechsten (im Aufbau) bzw. laufenden Welle; plant sie bei Bedarf */
	FTDWellenVorschau GetVorschau(int32 Welle = -1);
	/** T3: alle Wellen des Laufs fuer die Zeitleiste (Zustand, Boss, neue Front, neue Arten) */
	TArray<FTDZeitleistenWelle> GetZeitleiste() const;
	/** Seitenleiste: lebende, noch kommende und besiegte Gegner der laufenden Welle, je Art */
	FTDWellenStatus GetWellenStatus() const;
	/** T4: Familie einer Kreaturklasse fuer die Optik-Tabelle (Hog, AlphaHog, ..., StingerElite, Johnny) */
	static FString OptikFamilie(const UClass* Klasse);
	/** T4: Optik.<Familie>.<Elite|Boss>.<Wert> aus der Balance */
	static float OptikWert(const UClass* Klasse, bool bBoss, const TCHAR* Wert, float Ersatz);
	/** T3: Steckbrief zu einem Kreatur-Klassenpfad (Name, Bild, Text, Werte) */
	static FTDKreaturSteckbrief Steckbrief(const FString& Pfad);
	/** Einsatz: kein Countdown, die naechste Welle kommt nur mit /td kern weiter */
	void SetManuell(bool bManuell) { mManuell = bManuell; }
	bool IstManuell() const { return mManuell; }
	/** 0.11 Endlos-Wellen: Abstand ab Wellenbeginn, Gegnerstrom, eine Front, Boss-Pause (vor StarteKernVerteidigung setzen) */
	void SetEndlos(bool bEndlos) { mEndlos = bEndlos; }
	bool IstEndlos() const { return mEndlos; }
	/** Endlos: Abstand von Wellenbeginn zu Wellenbeginn nach Welle W (schrumpft bis zum Minimum) */
	float EndlosAbstand(int32 Welle) const;
	/** Zeile fuer HUD, Kern-Menue und To-do zwischen den Wellen: Countdown (Endlos) oder "x / y bereit" (Einsatz) */
	FString GetBereitText(bool bKurz = false) const;
	/** Restzeit bis zur naechsten Welle (nur Aufbau, sonst -1) */
	float GetCountdown() const { return mLauf == ETDLauf::Aufbau ? mCountdown : -1.f; }
	// ---- 0.12-dev Freies Spiel mit Abwehr: Bohrung am Kern-Ableger (TDBohrung.cpp) ----
	/** Bohrung der Stufe 0 Leicht, 1 Mittel, 2 Schwer starten; false mit Grund in OutMeldung (Platzpruefung im Klartext) */
	bool StarteBohrung(ATDDefenseCore* Kern, int32 Stufe, FString& OutMeldung);
	/** Abbruch ohne Strafe: Gegner weg, kein Lohn, Kern voll */
	FString BrecheBohrungAb();
	bool IstBohrung() const { return mBohrung && mLauf != ETDLauf::Aus; }
	/** Leer = Platz taugt; sonst was fehlt (kein Knoten, kein Weg, laeuft schon ...) */
	FString BohrPruefung(ATDDefenseCore* Kern, class AFGResourceNode** OutKnoten = nullptr);
	static FString BohrStufenName(int32 Stufe);
	/** Mittel nach einem Erfolg auf Leicht, Schwer nach Mittel (irgendein Knoten) */
	static bool BohrStufeFrei(const ATDDefenseCore* Kern, int32 Stufe);
	/** Zeilen fuer das Kern-Menue (Knoten, Stufen, Erstlohn) */
	TArray<FString> BohrInfo(ATDDefenseCore* Kern);
	int32 GetBohrStufe() const { return mBohrStufe; }
	// ---- 0.13 K1 Kern-Zugang (TDKernZugang.cpp, Fahrplan-0.13 Abschnitt 3) ----
	/** Kern erreichbar? Aktive Fronten der naechsten Welle, ohne Fronten eine Ringprobe. Zwischenspeicher KernZugang.Haltezeit s */
	const FTDKernZugang& PruefeKernZugang(bool bErzwingen = false);
	/** Pruefung von beliebigen Startpunkten; leer = Ringprobe (8 Richtungen, KernZugang.RingAbstaende). Aendert mKernZugang nicht. */
	FTDKernZugang PruefeKernZugangVon(const ATDDefenseCore* Kern, const TArray<FVector>& Starts, bool bRingprobe) const;
	const FTDKernZugang& GetKernZugang() const { return mKernZugang; }
	static const TCHAR* KernZugangName(ETDKernZugang Zustand);
	/** 23.11.: Kasten der sichtbaren Modellteile eines Hologramms (ohne Reichweitenringe und Pruefkaesten) - Wegpruefung und Ring */
	static FBox HologrammKasten(const class AFGHologram* Holo);
	/** Zeile fuer Kern-Menue und Chat in der Spielsprache, z. B. "Kern-Zugang: abgeriegelt durch Konstruktor, 24 m Nordosten" */
	FString KernZugangText(const FTDKernZugang& Z, const ATDDefenseCore* FuerKern = nullptr) const;
	/** Wellenstart wartet gerade, weil der Kern abgeriegelt ist */
	bool WartetAufKernZugang() const { return mZugangWarte > 0.f; }
	// ---- 0.13 N1 Bau-Sperren (TDBauSperren.cpp): Maschinen und Tuerme als Sperrflaeche im Gegnernetz ----
	/** Alle passenden Bauten im Umkreis Gegner.BauSperrenRadius um Mitte sperren; liefert die Zahl neuer Sperrflaechen */
	int32 SperreBautenUm(const FVector& Mitte);
	/** Sperrflaechen abgerissener Bauten entfernen (bAlle: alle, beim Laufende) */
	void RaeumeBauSperren(bool bAlle);
	int32 GetBauSperrenZahl() const { return mBauSperren.Num(); }
	/** Bau-Ereignis des Spiels (AFGBuildableSubsystem::BuildableConstructedGlobalDelegate) */
	UFUNCTION()
	void BauGebaut(AFGBuildable* Bau);
	// ---- 0.13 K4 Wegpruefung beim Bauen (TDLebensadern.cpp) ----
	/** Hologramm, das den letzten Weg zum Kern schliessen wuerde, bekommt den Ablehnungsgrund UTDCDKernAbriegelung. true = abgelehnt */
	bool PruefeHologrammZugang(class AFGHologram* Holo);
	/** Schneidet der Kasten (Welt) alle gueltigen Lebensadern? (mindestens 3 gueltige) */
	bool WuerdeAbriegeln(const FBox& Kasten, int32* OutGeschnitten = nullptr, int32* OutGueltig = nullptr) const;
	int32 GetLebensadernZahl() const;
	// ---- 0.13 Ring- und Lueckensignale (TDRingSignale.cpp, Marius 22.11.) ----
	bool RingSignaleSichtbar() const;
	int32 RingSignaleZahl() const;
	/** gruene Signale entlang der letzten Lebensader am roten Hologramm (0,6 s sichtbar) */
	void ZeigeLetzteLuecke(const FBox& Kasten, const TArray<FVector>& Ader);
	/** Alle TD-Rezepte freischalten (freier Modus; im Einsatz kommen sie ueber das MAM) */
	void SchalteSandkastenFrei();
	int32 mWellenGesamt = 30;
	int32 RemoveWalls();

	TArray<FString> GetStatusLines() const;
	TArray<FString> GetWallStatusLines() const;

	ETDSteuerModus Modus = ETDSteuerModus::Aggro;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	/** 0.65: Takt fuer die MAM-Statuskarten */
	float mMamTakt = 0.f;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SteuereKreaturen();
	void LaufTick(float DeltaTime);
	void SpawneKernWelle(int32 Nummer);
	int32 SpawneGruppe(TSubclassOf<AFGCreature> Klasse, int32 Anzahl, const FVector& Mitte, const FVector& Ort, float LPFaktor, int32 Elite = 0, bool bBoss = false,
		const TArray<FVector>* VorWeg = nullptr);
	/** T4: leuchtende Huelle (Overlay-Material M_TD_Huelle) je Gegnerart und Rolle (Optik.<Familie>.Elite/Boss.* in der Balance) */
	class UMaterialInstanceDynamic* Huelle(const UClass* Klasse, bool bBoss);
	/** T4c: Haut toenen (mitlaufende Kopie des Modells mit M_TD_Toenung, Leader-Pose) und Fell faerben (HairColor) */
	void Faerbe(AFGCreature* Kreatur, const UClass* Klasse, bool bBoss);
	UPROPERTY()
	TMap<FString, TObjectPtr<class UMaterialInstanceDynamic>> mHuellen;
	/** T4: Bosse der laufenden Welle fuer den Lebensbalken */
	TArray<TWeakObjectPtr<AFGCreature>> mBosse;
	FString mBossName;
	UPROPERTY()
	TObjectPtr<class UUserWidget> mBossBalken;
	float mBossPuls = 0.f;
	void EntferneBossBalken();
	/** Seitenleiste rechts mit dem Stand der Welle, solange sie laeuft */
	UPROPERTY()
	TObjectPtr<class UUserWidget> mWellenLeiste;
	void ZeigeWellenLeiste();
	void EntferneWellenLeiste();
	/** T3: Zusammensetzung einer Welle festlegen (reproduzierbar je Kern, Stufe und Welle) - in der Bauphase fuer die Vorschau */
	void PlaneWelle(int32 Nummer);
	/** 24.11.2026: LP-Faktor der Kern-Welle Nummer (Stufe, Wachstum, Endlos) - Gegner-LP = Grund-LP der Art x Faktor */
	float LPFaktorFuer(int32 Nummer) const;
	/** Plan einer beliebigen Welle berechnen, ohne den gespeicherten Plan zu aendern */
	TArray<FTDPlanGruppe> PlanFuer(int32 Nummer, int32& OutArt) const;
	int32 mPlanWelle = -1;
	/** T5 Stroeme: Gegner der laufenden Welle, die noch kommen (gemischt, neue Arten frueh, Boss in der Mitte) */
	struct FTDNachschub
	{
		TSubclassOf<AFGCreature> Klasse;
		int32 Front = 0;
		float LP = 1.f;
		bool bElite = false;
		bool bBoss = false;
	};
	TArray<FTDNachschub> mNachschub;
	int32 mNachschubIndex = 0;
	float mNachschubTakt = 0.f;
	FVector mNachschubMitte = FVector::ZeroVector;
	TArray<FVector> mNachschubOrte;
	/** Weg je Front einmal je Welle (statt je Gegner) */
	TArray<TArray<FVector>> mNachschubWege;
	/** Gegner, der sich zu lange nicht dem Kern naehert: auf seinen Frontweg 30 m naeher setzen, zuletzt entfernen */
	void PruefeNachzuegler(FTDWellenGegner& G, float Abstand, float DeltaTime);
	bool NachschubHatBoss() const;
	void ZeigeBossBalken();
	int32 mPlanArt = 0;
	TArray<FTDPlanGruppe> mPlan;
	/** Fronten um den Kern suchen: 16 Richtungen x 3 Abstaende der Stufe, gueltig = Boden, kein Wasser, NavMesh, voller Weg, Umweg <= 1,8, Hoehe < 40 m */
	void SucheFronten(const FVector& Mitte);
	/** Fronten nur neu suchen, wenn keine da sind oder Kern/Stufe gewechselt haben - sonst bleiben sie stehen */
	void FrontenFuer(ATDDefenseCore* Kern);
	FString FrontName(int32 Index) const;
	/** Fronten auf Karte und Kompass: aktive rot, die naechste (oeffnet in hoechstens 3 Wellen) grau; ohne Lauf alle entfernen */
	void MarkiereFronten();
public:
	/** Marius 02.11.: Logistik (Baender, Masten, Leitungen, Rohre, Splitter) ist fuer Wellengegner kein Hindernis -
	 *  sonst liessen sich Wege mit billigen Baendern lenken. Sammelt sie im Umkreis von 500 m und nimmt sie aus dem Navigationsnetz. */
	void EntschaerfeLogistik(const FVector& Mitte);
	/** 0.9.4: Kernort, fuer den Erzvorkommen, Steinknoten, Nester und Buesche schon ins Navigationsnetz genommen sind */
	FVector mHindernisMitte = FVector(TNumericLimits<float>::Max());
	const TArray<TWeakObjectPtr<AActor>>& GetLogistik() const { return mLogistik; }
	/** Zaehlt mit jeder Neusammlung hoch; das Gegnerhirn uebernimmt die Liste nur bei neuer Version */
	int32 GetLogistikVersion() const { return mLogistikVersion; }
	TArray<TWeakObjectPtr<AActor>> mLogistik;
	int32 mLogistikVersion = 0;
	/** 18.11. (P12-E2 R3: 4x festgefahren an Nussbusch und Krabbennest, die schon als Sperrflaeche im Netz stehen - der Weg fuehrt
	 *  zu knapp vorbei, die Kapsel streift die Kante): die ins Netz genommenen Weltobjekte ignorieren Wellengegner beim Laufen.
	 *  Das Gegnerhirn uebernimmt sie zusammen mit der Logistik (gleiche Version). Gegner.HindernisseDurchlaufen 0 = aus */
	const TArray<TWeakObjectPtr<AActor>>& GetWeltHindernisse() const { return mWeltHindernisse; }
	TArray<TWeakObjectPtr<AActor>> mWeltHindernisse;
	/** 0.9.6 (Marius' Partie 11.11.: ab Welle 6 hingen fast alle Gegner an einem Klippenpfeiler, den das Netz fuer begehbar hielt):
	 *  genau diese Klippe als Sperrflaeche ins Netz nehmen. Nach dem Netzumbau pruefen, ob alle aktiven Frontwege noch voll sind -
	 *  sonst die Sperre zuruecknehmen. Jede Klippe wird nur einmal versucht. */
	/** 16.11. (W06): mit Stelle = kleiner Sperrkasten an der Probenstelle statt des ganzen Felsumrisses */
	bool SperreKlippe(AActor* Klippe, const FVector* Stelle = nullptr);
	/** 0.11 (Felspfeiler-Rest in G2, 15.11.: jeder Lauf 1x "Festgefahren" am selben CliffPillar_02_Top): vor der Welle den Frontweg
	 *  mit einer Gegner-Kapsel abfahren; Klippen mit steiler Flaeche, an denen sie haengt, vorab sperren. Liefert die Zahl der Sperren. */
	int32 ProbeWegKlippen(const TArray<FVector>& Weg, int32 Front);
private:
	void PruefeKlippenSperre(TWeakObjectPtr<AActor> Klippe, int32 Versuch);
	TSet<TWeakObjectPtr<AActor>> mKlippenGeprueft;
	int32 mKlippenGesperrt = 0;
	int32 mKlippenZurueck = 0;
	/** 16.11. (W06: Sperre und Wellenstart im selben Frame): Klippensperren, deren Netzumbau noch nicht bestaetigt ist */
	int32 mKlippenOffen = 0;
	/** Mitten der Sperrkaesten (kein zweiter Kasten naeher als Gegner.KlippenProbe.SperrAbstand) */
	TArray<FVector> mKlippenStellen;
	/** Welle, fuer die die Frontpruefung vor dem Start schon lief */
	int32 mFrontCheckWelle = -1;
	/** 23.11. (Wegvorschau vor dem Start): Sekunden seit Beginn der Bauphase und Welle, deren Fronten schon frueh geprueft sind.
	 *  Gegner.FrontPruefungAufbau > 0 zieht Frontpruefung und Klippenprobe in die Bauphase vor (Standard 5 s, Marius 23.11. abends). */
	float mAufbauUhr = 0.f;
	int32 mFrontCheckFruehWelle = -1;
	/** so lange wartet der Wellenstart schon auf den Netzumbau; wie oft gewartet wurde (Log) */
	float mKlippenWarte = 0.f;
	int32 mStartGewartet = 0;
	/** 18.11. (P12-E2 R3: Welle 1 startete 12 s nach dem Einsatzstart mit 0 von 16 gepruefen Richtungen): Start halten, bis die
	 *  Fronten geprueft sind (hoechstens Gegner.FrontenWarteMax s), dabei alle 2 s neu suchen */
	float mFrontenWarte = 0.f;
	float mFrontenSuchUhr = 0.f;
	/** Weg der aktiven Fronten pruefen (Klippenprobe, Teilweg -> Fronten neu); vor dem Start und beim Spawnen */
	void PruefeFrontenVorWelle(int32 Nummer);
	/** Beim Start war das Navigationsnetz noch nicht fertig (0 Richtungen geprueft): alle 15 s neu suchen, bis es klappt */
	void PruefeFrontenNach();
	FTimerHandle mFrontenNachTimer;
	void Ansage(const FString& Text) const;
	/** Versorgungs-Check vor der Welle: Tuerme ohne Rezept, Munition oder Strom melden */
	void PruefeVersorgung() const;
	/** Lauf-Stand in den Kern schreiben (wird mit dem Spielstand gespeichert) */
	void SpeichereLauf() const;
	void WelleAusgewertet(float Anteil, bool bVerloren = false);
	/** Messwerte (Recherche 3.3/3.8): zu Wellenbeginn Druckquote Q = Gegner-EHP / (K * Referenzzeit) mit K = Summe Nenn-DPS
	 *  der Tuerme, am Ende Auslastung, Kernverlust, knappste Annaeherung ... als Logzeile und CSV (Saved\FicsitTD\Metriken.csv) */
	void MesseWellenStart();
	void MeldeMetrik(float Anteil);

	/** T1 Bildmessung: Bildzeiten (ms) und Zaehler-Stand des eigenen Codes seit StarteBildMessung */
	struct FTDBildWerte
	{
		int32 Bilder = 0;
		float Mittel = 0.f, P95 = 0.f, Max = 0.f;
		/** Bilder ueber 50 ms (unter 20 FPS) */
		int32 Ruckler = 0;
		/** eigener Code je Bild in ms */
		float HirnMs = 0.f, TurmMs = 0.f;
		/** davon bzw. dazu: Schadensverarbeitung des Spiels je Bild, Geschosse erzeugen je Bild; erzeugte Geschosse */
		float SchadenMs = 0.f, GeschossMs = 0.f;
		int32 Geschosse = 0;
		int32 MaxLebend = 0;
		/** Engine-Zeiten je Bild (ms, Mittel/Max): Spiel-Thread, Render-Thread, Grafikkarte - 0, wenn das Spiel sie nicht liefert */
		float SpielMs = 0.f, SpielMax = 0.f, RenderMs = 0.f, RenderMax = 0.f, GpuMs = 0.f, GpuMax = 0.f;
		/** Gegner gestorben waehrend der Messung */
		int32 Tote = 0;
	};
	void StarteBildMessung();
	void MesseBild();
	FTDBildWerte BildWerte() const;
	/** Kreaturen in der Welt (lebend/tot, FicsitTD/Spiel) und das Spiel-Limit mMaxNumCreatures ins Log */
	void MeldeKreaturen(const FString& Anlass) const;
	void StressTick();
	void RaeumeStress();
	bool mBildMessung = false;
	TArray<float> mBildZeiten;
	double mLetztesBild = 0.0;
	uint64 mHirnStart = 0;
	uint64 mTurmStart = 0;
	int32 mMaxLebend = 0;
	double mSpielSumme = 0.0, mRenderSumme = 0.0, mGpuSumme = 0.0;
	float mSpielMax = 0.f, mRenderMax = 0.f, mGpuMax = 0.f;
	int32 mToteStart = 0;
	uint64 mSchadenStart = 0;
	uint64 mGeschossStart = 0;
	int32 mGeschosseStart = 0;
	/** T2d Ruckler-Zeitleiste: je 5 s Ruckler, schlechtestes Bild, naechster Gegner zum Kern, Gegner im Anlauf/Angriff */
	double mAbschnittEnde = 0.0;
	double mMessBeginn = 0.0;
	int32 mAbschnittRuckler = 0;
	float mAbschnittMax = 0.f;
	/** T2g Sichtbarkeit je Abschnitt: Gegner auf dem Bildschirm (Summe/Max je Bild), bei Rucklern, Blickwinkel zur Gegnermasse */
	int32 mAbschnittBilder = 0;
	double mSichtSumme = 0.0;
	int32 mSichtMax = 0;
	double mRucklerSichtSumme = 0.0;
	int32 mRucklerBilder = 0;
	/** T2h: Engine-Zeiten in den Ruckel-Bildern, Ruckel-Bilder waehrend Nachladen, Speicherbereinigungen im Abschnitt */
	double mRSpiel = 0.0;
	double mRRender = 0.0;
	double mRGpu = 0.0;
	int32 mRLaden = 0;
	int32 mGcStart = 0;
	double mBlickSumme = 0.0;
	int32 mBlickBilder = 0;
	void SetzeAbschnittZurueck();
	/** T2i: Bildphasen - Stempel des laufenden Bilds, Summen je Phase fuer normale und Ruckel-Bilder (Messung und Abschnitt) */
	static constexpr int32 PhasenAnzahl = 8;
	double mStempel[PhasenAnzahl] = {};
	bool mStempelGesetzt[PhasenAnzahl] = {};
	double mPhaseNormal[PhasenAnzahl] = {};
	double mPhaseRuckel[PhasenAnzahl] = {};
	int32 mPhaseNormalBilder = 0;
	int32 mPhaseRuckelBilder = 0;
	double mAbschnittPhaseRuckel[PhasenAnzahl] = {};
	int32 mAbschnittPhaseRuckelBilder = 0;
	bool mPhasenAngemeldet = false;
	/** T2j: Tick-Teile der Wellengegner je Bild (Bewegung, Controller, Kreatur, Skelett in ms; Bewegungs-Ticks, Durchdringung, Feststecken als Anzahl) */
	static constexpr int32 TeileAnzahl = 7;
	double mTeilVorher[TeileAnzahl] = {};
	double mTeilNormal[TeileAnzahl] = {};
	double mTeilRuckel[TeileAnzahl] = {};
	double mAbschnittTeilRuckel[TeileAnzahl] = {};
	void LeseTeile(double* Ziel) const;
	/** T2k: Tick-Zeit je Klasse (Zyklen), summiert ueber normale bzw. Ruckel-Bilder der Messung */
	TMap<const UClass*, uint64> mTickNormal;
	TMap<const UClass*, uint64> mTickRuckel;
	FString TickKlassenText() const;
	FString TeileText(const double* Summen, int32 Bilder) const;
	TArray<struct FTDPhasenSonde*> mSonden;
	FDelegateHandle mBeginnHandle, mEndeHandle, mWeltStartHandle, mWeltEndeHandle;
	void MeldePhasen(bool bSonden);
	void PhasenBildBeginn();
	FString PhasenText(const double* Summen, int32 Bilder) const;
	TArray<FString> mZeitleiste;
	void SchliesseAbschnitt(double Jetzt);
	void MeldeZeitleiste(const FString& Anlass);
	/** Stresstest: Gegner je Sekunde per Befehl toeten (Tode ohne Schuesse messen) */
	float mStressSterben = 0.f;
	float mStressSterbenRest = 0.f;
	double mStressLetzterTick = 0.0;
	/** Gegnerklassen fuer die naechste Welle vorgeladen (gegen Nachlade-Ruckler beim ersten Spawn) */
	int32 mVorgeladenFuer = -1;
	UPROPERTY()
	TArray<TObjectPtr<UClass>> mVorgeladen;
public:
	/** T4 Boss-Lebensbalken: Name, LP aller Bosse der Welle, lebend/gesamt; false ohne Boss */
	bool GetBossStatus(FString& Name, float& LP, float& MaxLP, int32& Lebend, int32& Gesamt) const;
	/** 0.9.8 P3 Jaeger 4: erster lebender Boss der Welle (Resistenzen im Boss-Balken) */
	AFGCreature* GetErsterBoss() const;
private:
	/** Stresstest: Stufen (Gegnerzahlen), aktuelle Stufe, Phase 0 aus / 1 messen / 2 Pause */
	TArray<int32> mStressStufen;
	int32 mStressIndex = 0;
	int32 mStressPhase = 0;
	double mStressEnde = 0.0;
	float mStressSekunden = 20.f;
	float mStressLP = 20.f;
	FString mStressArt;
	TArray<TWeakObjectPtr<class AFGCreature>> mStressGegner;
	TArray<FString> mStressErgebnis;
public:
	/** T1 Stresstest (/td kern stress): je Stufe N Gegner an Front 1, Bildzeit und Kosten ins Log, danach wieder entfernt */
	bool StarteStress(class ATDDefenseCore* Kern, const TArray<int32>& Stufen, float LPFaktor, float Sekunden, const FString& Art, float SterbenJeSekunde, FString& Meldung);
	void StoppeStress(const FString& Grund);
	bool IstStress() const { return mStressPhase != 0; }
	/** Waehrend des Stresstests richten Gegner keinen Schaden an und Abschuesse zaehlen nicht */
	static bool bStressSchutz;
	/** Stresstest: Tuerme suchen kein Ziel (Schuss-Kosten ausschliessen) */
	static bool bStressTuermeAus;
	/** Zaehler aller Tode von Wellengegnern (Gegnerhirn) */
	static int32 GegnerToteGesamt;
	/** Stresstest: Bolzentuerme ohne sichtbares Spiel-Geschoss (nur Schaden) */
	static bool bStressOhneGeschoss;
	/** T2e: Tuerme zielen ohne Drehteil/Kopf zu bewegen; Drehteil/Kopf ohne Schatten; ohne Animieren (Feuer, Flamme, Blitz) */
	static bool bStressDrehenAus;
	static bool bStressSchattenAus;
	static bool bStressEffekteAus;
	/** T2f: Drehteil/Kopf ohne Distanzfeld-Beleuchtung; Drehungen je Sekunde (0 = jeder Zieltakt, 30/s) */
	static bool bStressDfAus;
	static float StressDrehtakt;
	/** Zaehler erzeugter Bolzen-Geschosse */
	static int32 GeschosseGesamt;
private:
	AActor* KernKoeder(const FVector& Location) const;
	void SchalteRezepteFrei();
	AActor* NaechsterKoeder(const FVector& Location) const;
	AActor* ZielFuer(const FTDWellenGegner& Gegner) const;

	TArray<FTDWellenGegner> mGegner;

	ETDLauf mLauf = ETDLauf::Aus;
	TWeakObjectPtr<ATDDefenseCore> mKern;
	int32 mWelle = 0;
	float mCountdown = 0.f;
	float mZwischenCountdown = 600.f;
	float mWellenZeit = 0.f;
	int32 mWellenGroesse = 0;
	int32 mElites = 0;
	FString mRichtungen;
	int32 mStufe = 1;
	/** 16.11. (Nachpruefung N4): die gerade ausgewertete Welle lief ueber (Endlos) - andere Chatzeile */
	bool mLetzterUeberlauf = false;
public:
	/** 17.11. (Marius: Chatflut reduzieren): Bilanz der letzten Welle (beste Tuerme, Spieler) fuer die Sammelzeile des Einsatzes */
	const FString& GetLetzteWellenZeile() const { return mLetzteWellenZeile; }
	/** Sammelzeile statt Einzelzeilen am Wellenende (Einsatz und Endlos, nicht in der Bohrung) */
	bool Sammelzeile() const;
private:
	FString mLetzteWellenZeile;
	bool mManuell = false;
	bool mEndlos = false;
	bool mBohrung = false;
	int32 mBohrStufe = 0;
	int32 mBohrEndWelle = 0;
	TWeakObjectPtr<class AFGResourceNode> mBohrKnoten;
	FString mBohrRohstoff;
	float mBohrReinheit = 1.f;
	int32 mBohrErz = 0;
	void BohrWelleBeendet(float KernAnteil);
	void BohrungEnde(bool bErfolg, const FString& Grund);
	TArray<FTDFront> mFronten;
	TWeakObjectPtr<ATDDefenseCore> mFrontenKern;
	int32 mFrontenStufe = -1;
	/** Wie viele der 16 Richtungen beim Suchen geprueft gueltig waren */
	int32 mGueltigeRichtungen = 0;
	/** 0.13 K1: letzte Pruefung "Kern erreichbar", Warten des Wellenstarts, Uhren fuer Neupruefung und Ansage */
	FTDKernZugang mKernZugang;
	float mZugangWarte = 0.f;
	float mZugangPruefUhr = 0.f;
	float mZugangAnsageUhr = 0.f;
	/** Abriegelung gemeldet (Ansage, Kartenmarke, Zaehler) - erst nach einer zweiten Pruefung */
	bool mZugangGemeldet = false;
	/** so lange haelt der Start schon wegen eines laufenden Netzumbaus (KernZugang.WarteNetz) */
	float mZugangNetzWarte = 0.f;
	/** 23.11. (Gegencheck 0.13 F4): einmal nach Laden/Start pruefen, ob Bestand den Kern abriegelt (-1 = nicht geplant) */
	float mZugangLadeUhr = -1.f;
	int32 mZugangLadeVersuche = 0;
	bool mZugangLadeGeprueft = false;
	/** 0.13 N1: Bau -> Sperrkasten (eigener Actor, nicht gespeichert) */
	TMap<TWeakObjectPtr<AFGBuildable>, TWeakObjectPtr<AActor>> mBauSperren;
	bool mBauVerbunden = false;
	/** > 0: nach dem Bau-Ereignis in so vielen s die Bauten im Umkreis neu sperren */
	float mBauSperrenUhr = 0.f;
	float mBauRaeumUhr = 0.f;
	/** 0.13 K4: volle Wege aus 16 Richtungen (leer = kein voller Weg), reihum eine je Bild */
	TArray<TArray<FVector>> mLebensadern;
	/** 23.11. (Marius: Flieger bauen, Spezifikation E5): fliegende Krabben der laufenden Welle. Bodentuerme suchen nur
	 *  AFGCreature und treffen sie deshalb nicht, der Himmelsturm (ATDFlakTurret) sucht genau diese Klasse. */
	TArray<TWeakObjectPtr<AActor>> mFlieger;
	bool mFliegerAngesagt = false;
	bool mMantaAngesagt = false;
	void SpawneFlieger(int32 Nummer);
	/** Flieger am Kern: Schaden, Explosion; Rueckgabe: Zahl der lebenden */
	int32 PflegeFlieger();
	void EntferneFlieger();
	/** 23.11. (Stromzweig): Takt der Kraftwerkspflege (TDStrom.cpp) */
	float mStromUhr = 0.f;
	/** 23.11. (Gegencheck Konzept R4-E): Modus Aus erkennen, ohne in jedem Bild nachzufragen */
	float mModusUhr = 0.f;
	bool mModusAktiv = true;
	int32 mLebensaderIndex = 0;
	void PflegeLebensadern(float DeltaTime);
	/** 22.11. (H4): Takt und Kosten der Lebensadern (Logzeile je Wellenstart) */
	float mLebensaderUhr = 0.f;
	int32 mLebensaderZahl = 0;
	double mLebensaderMs = 0.0;
	double mLebensaderMaxMs = 0.0;
	void MeldeLebensaderKosten();
	/** Ringsignale anlegen und im Baumodus zeigen, Lueckensignale ausblenden */
	void PflegeRingSignale(float DeltaTime);
	TWeakObjectPtr<AActor> mRingSignale;
	TWeakObjectPtr<const ATDDefenseCore> mRingSignaleKern;
	TWeakObjectPtr<AActor> mLueckeSignale;
	double mLueckeBis = 0.0;
	float mRingSignalUhr = 0.f;
	bool BrauchtBauSperre(const AFGBuildable* Bau) const;
	bool SperreBau(AFGBuildable* Bau);
	void VerbindeBauSperren();
	/** Wellenstart halten, solange ein Bau den Kern abriegelt; true = Start diesmal halten (LaufTick kehrt zurueck) */
	bool HalteStartFuerKernZugang(float DeltaTime);
	/** Eine Front: Weg zum Kern, bei Teilweg die Sperrstelle (Kapselfahrten, Brueckentest) */
	FTDKernZugang PruefeZugangEinzeln(const ATDDefenseCore* Kern, const FVector& Start, int32 Front) const;
	/** Kartenmarke an der Sperrstelle setzen (bZeigen) oder entfernen */
	void MarkiereSperrstelle(bool bZeigen);
	/** 18.11. (P12-E5: Neusuche nach 0 von 16 fand 13 Richtungen, waehlte aber Norden 357 m statt Nordwesten 343 m - welche
	 *  Wege gueltig sind, haengt am Stand des Netzes): vor Welle 1 erst starten, wenn zwei Suchen nacheinander dieselben Fronten liefern */
	bool mFrontenStabil = true;
	FString FrontenSignatur() const;
	/** Abschuesse je Turm zu Wellenbeginn (fuer die Auswertung) */
	TMap<TWeakObjectPtr<class ATDTurretBase>, int32> mKillsBeiStart;
	/** Messwerte der laufenden Welle (nicht gespeichert: nach dem Laden mitten in der Welle fehlen sie) */
	bool mMessGueltig = false;
	float mMessQ = 0.f;
	/** 17.11. (P12): Spielzeit seit dem Ende der vorigen Welle (Pause des Spielers bzw. Pruefstands), Ende-Zeitpunkt */
	float mMessPause = 0.f;
	float mLetztesWellenEnde = -1.f;
public:
	/** 17.11. (P12): Name und laufende Nummer des Selbsttest-Plans fuer die Metriken-CSV (leer im normalen Spiel) */
	static FString GTestPlan;
	static int32 GTestLauf;
private:
	float mMessEHP = 0.f;
	float mMessLP = 0.f;
	float mMessK = 0.f;
	int32 mMessTuerme = 0;
	float mMessKernStart = 0.f;
	int32 mMessEP = 0;
	/** Knappster 2D-Abstand eines Gegners zum Kern in cm */
	float mMessNaechste = 0.f;

	UPROPERTY()
	TArray<TWeakObjectPtr<ATDTestWall>> mTestMauern;

	bool mRezepteFreigeschaltet = false;
	/** 0.11: einmal "Modus Aus" ins Log */
	bool mModusAusGemeldet = false;
	float mZeitBisSteuerung = 0.f;
	float mZeitBisLog = 0.f;
	float mZeitBisRezeptVersuch = 0.f;
};
