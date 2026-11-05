#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Command/CommandSender.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "TDAutotest.generated.h"

/** Absender fuer /td-Befehle im Selbsttest: gilt als Spieler (erster lokaler Controller) und schreibt die Antworten mit */
UCLASS()
class FICSITTD_API UTDAutotestSender : public UCommandSender
{
	GENERATED_BODY()
public:
	virtual FString GetSenderName() const override { return TEXT("Autotest"); }
	virtual bool IsPlayerSender() const override { return true; }
	virtual class AFGPlayerController* GetPlayer() const override;
	virtual void SendChatMessage(const FString& Message, const FLinearColor PrefixColor) override { Antworten.Add(Message); }
	TArray<FString> Antworten;
};

/**
 * Selbsttest im Spiel (Konzept-Testautomatik, Stufe 2 / A2). Liegt beim Start Saved/FicsitTD/Autotest.json vor (Teste-Mod.ps1),
 * laedt der Mod den Spielstand selbst, arbeitet die Schritte des Plans ab (Befehle, Warten, Pruefungen, Bildschirmfotos),
 * schreibt Saved/FicsitTD/Autotest-Ergebnis.json und beendet das Spiel. Ohne die Datei tut das Subsystem nichts.
 *
 * 0.76 Testpaket: Autotest.json darf {"Name", "Plaene": [Plan, ...], "Bereitschaft": bool} sein. Die Plaene laufen nacheinander in
 * einem Spielstart (Spielstand je Plan neu geladen). Mit Bereitschaft bleibt das Spiel danach offen und wartet auf die naechste
 * Autotest.json (Balance wird dann neu gelesen) - fuer Aenderungen ohne neuen Build.
 *
 * Schritte (Feld = Art):
 *   {"warte": s}                         Sekunden Spielzeit-unabhaengig warten
 *   {"befehl": "td kern status"}          Chat-Befehl ohne Schraegstrich; Antworten landen im Ergebnis
 *   {"zeitraffer": 4}                     globale Zeitdehnung (1 = normal)
 *   {"warteBis": "WelleLaeuft"|"WelleVorbei", "max": s}
 *   {"pruefe": "<Name>", ...}             siehe TDAutotest.cpp (Pruefe)
 *   {"foto": "Name"}                      Bildschirmfoto nach Saved/FicsitTD/Autotest/Bilder/Name.png
 *   {"erforsche": "Knoten"}               Forschung direkt geben (0.75)
 *   {"uebertakte": "Gewehrturm", "splitter": 2}  Energiesplitter in den ersten Turm des Typs (0.75)
 *   {"oeffne": "MAM"|"<Turmtyp>"}, {"schliesse": true}  Fenster fuer Fotos (0.75)
 *   {"speichere": "Name"}, {"lade": "Name"}  Spielstand speichern bzw. mitten im Plan laden (0.77)
 *   Plan-Feld "NeuesSpiel": {"Startort": "..."} statt "Spielstand": neues Spiel mit Tower-Defense-Schalter (0.77)
 *   Pruefstand (0.78, Roadmap M1):
 *   {"stromfrei": true}                    Cheat ohne Strom (Fabrik und Netz sind nicht Teil des Pruefstands)
 *   {"baue": "Gewehrturm", "anzahl": 2, "level": 2, "rezept": "Recipe_TD_Gun_Standard"}  Tuerme auf freie Plaetze der Plattform
 *        optional "ring": "innen" -> 8 m um den Kern (kurze Reichweite: Flamme, Tesla), sonst 16-m-Ring
 *   {"versorgung": {"Bolzenturm": 30, "Gewehrturm": 8}}  Munition je Turm und Minute in den Eingang (ersetzt die Fabrik)
 *   {"wellenlauf": 10, "zeitraffer": 4, "max": 400}  Wellen nacheinander starten, bis Welle 10 vorbei ist (max s je Welle)
 *   {"balance": {"Kern.Ladequelle": 0}}  0.78.2: Balance-Werte fuer einen A/B-Versuch setzen (vor dem Kernbau wirksam)
 *   {"pruefe": "Sicherheitsnetze"}  0.78.2: kein Sicherheitsnetz (Versetzen, Absturz, 0 LP) hat seit Planbeginn ausgeloest
 *   {"klick": [x, y]}                     Mausklick im Spielfenster, Anteil 0..1 von links oben (0.75)
 */
UCLASS()
class FICSITTD_API UTDAutotest : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	static FString Ordner();

private:
	enum class EPhase : uint8 { Aus, Menue, Laedt, Spiel, Bereit, Fertig };
	/** Testpaket (oder einzelnen Plan) aus Text starten */
	void Starte(const FString& Text);
	void StartePlan();
	void PlanFertig(const FString& Grund);
	void PaketFertig();
	bool Tick(float Dt);
	void LadeSpielstand(UWorld* World, const FString& Name = FString());
	void StarteNeuesSpiel(UWorld* World);
	/** einen Schritt ausfuehren; true = fertig, weiter zum naechsten */
	bool Schritt(UWorld* World, const TSharedPtr<FJsonObject>& S, float Dt);
	bool Pruefe(UWorld* World, const TSharedPtr<FJsonObject>& S, FString& OutIst);
	void Ergebnis(const FString& Art, const FString& Text, bool bOk, const FString& Ist = FString(), const TArray<FString>& Antworten = {});
	void Beende(const FString& Grund);

	EPhase mPhase = EPhase::Aus;
	TSharedPtr<FJsonObject> mPlan;
	TSharedPtr<FJsonObject> mPaket;
	TArray<TSharedPtr<FJsonValue>> mPlaene;
	TArray<TSharedPtr<FJsonValue>> mPlanErgebnisse;
	int32 mPlanNr = 0;
	float mPlanZeit = 0.f;
	TWeakObjectPtr<UWorld> mAlteWelt;
	TArray<TSharedPtr<FJsonValue>> mSchritte;
	int32 mIndex = 0;
	float mUhr = 0.f;
	float mSchrittUhr = 0.f;
	float mGesamt = 0.f;
	double mLetzte = 0.0;
	bool mKlickUnten = false;
	/** 0.77: Laden mitten im Plan (Schritt "lade"): danach mit dem naechsten Schritt weiter statt von vorn */
	bool mLadenImPlan = false;
	/** Pruefstand: Munition je Turmtyp und Minute, Rest je Turm, belegte Bauplaetze */
	TMap<FString, float> mVersorgung;
	TMap<TWeakObjectPtr<class ATDTurretBase>, float> mVersorgtRest;
	int32 mBauplatz = 0;
	int32 mBauplatzInnen = 0;
	/** 0.78.1: Kern im Wellenlauf gefallen -> weitere Wellenlaeufe des Plans ueberspringen (keine Folgefehler) */
	bool mKernGefallen = false;
	/** 0.78.1: Nachzuegler-Diagnose (hoechstens 3 Gegner leben lange): Uhr, Meldungen je Welle, Welle der letzten Meldung */
	float mNachzueglerUhr = 0.f;
	int32 mNachzueglerMeldungen = 0;
	int32 mNachzueglerWelle = 0;
	void MeldeNachzuegler(UWorld* World, const TArray<class AFGCreature*>& Lebende, int32 Welle);
	void Versorge(UWorld* World, float Dt);
	bool mSchrittBegonnen = false;
	FTSTicker::FDelegateHandle mTicker;
	TArray<TSharedPtr<FJsonValue>> mErgebnisse;
	int32 mFehler = 0;
	UPROPERTY()
	TObjectPtr<UTDAutotestSender> mSender;
	class FTDAutotestLog* mLog = nullptr;
};
