#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TDPionier.generated.h"

class AFGCreature;
class ATDDefenseCore;
class ATDTurretBase;

/** Ein Zweig des Talentbaums "Pionier": 5 Stufen, jede Stufe ist eine Faehigkeit (0.9.8 P3, keine reinen Prozentstufen) */
struct FTDTalentZweig
{
	const TCHAR* Key;
	const TCHAR* Name;
	/** Rolle in einem Satz (Kopf der Spalte im Reiter) */
	const TCHAR* Rolle;
	const TCHAR* Stufen[5];
};

/**
 * Pionier-Rang und Talente. 0.9.8 P3/P4 (Analyse-Spieler-und-Pionier 3.3, Marius 12.11.2026): vier Rollen-Zweige
 * Schuetze, Kommandant, Techniker, Jaeger mit je 5 Stufen, hoechstens Pionier.MaxRang - 1 Punkte. Ueberleben und Mobilitaet
 * sind Rangboni ohne Punkte. XP fuer eigene Leistung (Schadensanteil, Assists, Reparatur) und, halbiert, fuer abgewehrte Wellen.
 * Gespeichert am Kern (mPionierXP, mTalente). Wirkt ueber die Schadens-Hooks im Modul, die Turmtreffer und den Takt der Tuerme.
 */
UCLASS()
class FICSITTD_API UTDPionier : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	static UTDPionier* Get(const UObject* WorldContext);
	static const TArray<FTDTalentZweig>& Zweige();
	static constexpr int32 MaxStufe = 5;
	/** Zweig-Indizes */
	static constexpr int32 Schuetze = 0;
	static constexpr int32 Kommandant = 1;
	static constexpr int32 Techniker = 2;
	static constexpr int32 Jaeger = 3;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	int32 GetXP() const;
	int32 GetRang() const;
	/** XP, ab der ein Rang erreicht ist (Rang 1 = 0) */
	static int32 XPFuerRang(int32 Rang);
	static int32 MaxRang();
	int32 GetStufe(int32 Zweig) const;
	/** Stufe eines Zweigs ueber irgendein Objekt der Welt (0 ohne Einsatz) */
	static int32 Stufe(const UObject* Kontext, int32 Zweig);
	int32 FreiePunkte() const;
	/** Rangbonus ohne Punkte: je 3 Raenge eine Stufe (+10 LP, +3 % Lauftempo) */
	int32 RangBoni() const;

	/** Talent lernen bzw. alles neu verteilen; Rueckgabe = Meldung */
	FString Lerne(int32 Zweig);
	FString NeuVerteilen();
	static int32 NeuVerteilenPreis();
	/** Selbsttest: Stufe direkt setzen (ohne Punkte) */
	void SetzeStufe(const FString& Key, int32 Stufe);

	/** Abgewehrte Welle (aus der EP-Abrechnung) und Wellenbeginn */
	void WelleBeendet(int32 Welle, float KernAnteil);
	void WellenStart();
	/** Tod eines Wellengegners: XP, wenn der Spieler ihn zuletzt getroffen hat; Assist, wenn er markiert war */
	void Abschuss(const AFGCreature* Kreatur);

	/** Aus den Schadens-Hooks: Faktor auf den Schaden. Knochen nur bei Punktschaden (Kopftreffer). */
	static float Schadensfaktor(AActor* Getroffen, AController* Ausloeser, AActor* Verursacher, float Menge, bool bWellenKreatur, bool bVonKreatur, FName Knochen = NAME_None);

	/** Was der Spieler in einer Welle bewirkt hat (P0, erweitert P2/P3) */
	struct FSpielerBilanz
	{
		float Schaden = 0.f;
		int32 Treffer = 0;
		int32 Abschuesse = 0;
		int32 Eliten = 0;
		int32 Bosse = 0;
		int32 XP = 0;
		int32 Assists = 0;
		float MarkBonus = 0.f;
		int32 Markiert = 0;
		// 0.9.8 P3/P4
		int32 Kopftreffer = 0;
		int32 MunitionGespart = 0;
		int32 Durchschlaege = 0;
		float Repariert = 0.f;
		int32 Uebersteuert = 0;
		int32 Nachschub = 0;
		int32 Gerettet = 0;
		float JagdSchaden = 0.f;
		int32 Hinrichtungen = 0;
		int32 JagdBeute = 0;
		int32 Sammelbefehle = 0;
		int32 LeistungsXP = 0;
		float Anteil = 0.f;
	};
	const FSpielerBilanz& GetWellenBilanz() const { return mBilanz; }
	/** Bilanz der zuletzt abgeschlossenen Welle (fuer den Reiter) */
	const FSpielerBilanz& GetLetzteBilanz() const { return mLetzte; }
	/** Zeile fuer die Wellenmeldung ("Du: 1240 Schaden (11 %), 9 Abschuesse ..."); merkt den Schadensanteil fuer die Leistungs-XP */
	FString WellenZeile(float TurmSchaden);
	/** Ist-Werte eines Zweigs aus der letzten Welle (eine Zeile, leer ohne Wirkung) */
	FString IstWerte(int32 Zweig) const;

	/** Kommandant 1: Markierung durch Spielertreffer; Tuerme mit "Markierte zuerst" / "Elite und Boss zuerst" nehmen sie vorrangig */
	static bool IstMarkiert(const AActor* Gegner);
	/** Turmtreffer: Faktor auf den Turmschaden (Kommandant 2 Markierung, Jaeger 3 Schwachstelle), zaehlt den Zusatz */
	static float MarkierungsFaktor(const AActor* Gegner, float Schaden);
	/** Hat der Spieler diesen Gegner in den letzten Sekunden getroffen? */
	static bool VomSpielerErlegt(const AActor* Gegner);
	/** Jaeger 2: sichere Beute bei eigenem Elite- oder Boss-Abschuss */
	static bool JagdBeute(const AActor* Gegner);
	/** Kommandant 4 Sammelbefehl: Ziel fuer einen Turm in Reichweite des Spielers (nullptr = keins) */
	static AFGCreature* Sammelziel(const ATDTurretBase* Turm);
	/** Techniker 5 Bollwerk: einmal je Welle faellt ein Bau nicht, sondern bleibt mit halben LP stehen */
	static bool Bollwerk(const AActor* Bau);
	/** Selbsttest und Kommandant 1: Markierung setzen */
	void Markiere(AActor* Gegner);

	/** Techniker 3 / Kommandant 5: Uebersteuern am Turm (Knopf im Turmfenster) */
	bool KannUebersteuern() const;
	FString Uebersteuere(ATDTurretBase* Turm);

private:
	FSpielerBilanz mBilanz;
	FSpielerBilanz mLetzte;
	TSet<FString> mGemeldeteTreffer;
	ATDDefenseCore* Kern() const;
	void GibXP(int32 XP, const FString& Grund);
	void Melde(const FString& Text) const;
	/** alle 0,5 s: Rangboni, Nachladen, Feldreparatur, Nachschub, Markierungen */
	void Wirke();
	/** Waffe des Verursachers: 0 Armierpistole, 1 Gewehr, 2 Nobelisk, -1 keine */
	int32 Waffe(const AActor* Verursacher);
	float SpielerTreffer(AActor* Getroffen, AActor* Verursacher, float Menge, FName Knochen);
	/** 0.9.6 P1: Handwaffen nutzen die Munitionswerte der Tuerme */
	float HandFaktor(int32 W, const AActor* Verursacher, const AActor* Getroffen, FString& OutMunition, float& OutSkalierung, float& OutResistenz) const;
	/** Talente aus dem alten Baum (0.58-0.9.7) entfernen, Punkte zurueckgeben */
	void PruefeAlteTalente();
	void Feldreparatur(class AFGCharacterPlayer* Spieler, float Sekunden);
	void Nachschub(class AFGCharacterPlayer* Spieler);
	void SpareMunition(AActor* Verursacher, class AFGCharacterPlayer* Spieler);
	void Zusatzschaden(AActor* Getroffen, AActor* Verursacher, int32 W, float Schaden);

	/** Markierung: Ablaufzeit (Weltzeit), Schwachstelle (Jaeger 3) und das Licht ueber dem Gegner */
	struct FMarke
	{
		float Bis = 0.f;
		float SchwachBis = 0.f;
		TWeakObjectPtr<class UPointLightComponent> Licht;
	};
	TMap<TWeakObjectPtr<AActor>, FMarke> mMarken;
	void RaeumeMarken(bool bAlle);
	void EntferneMarke(const AActor* Gegner);
	/** Kommandant 3: Markierung springt beim Tod auf den naechsten Gegner */
	void SpringeMarke(const AFGCreature* Tot);

	FTimerHandle mTimer;
	TMap<TWeakObjectPtr<const AActor>, double> mSpielerTreffer;
	TSet<FString> mGemeldeteKlassen;
	int32 mTrefferZaehler = 0;
	float mMunitionKonto = 0.f;
	float mPlattenKonto = 0.f;
	float mNachschubTakt = 0.f;
	bool mPlattenFehltGemeldet = false;
	bool mBollwerkGenutzt = false;
	bool mAlteGeprueft = false;
	TWeakObjectPtr<AFGCreature> mSammelZiel;
	float mSammelBis = 0.f;
	TWeakObjectPtr<AFGCreature> mLetztesZiel;
};
