#include "TDAutotest.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDTurretBase.h"
#include "TDForschung.h"
#include "TDWaveSubsystem.h"
#include "Misc/App.h"
#include "FGDismantleInterface.h"
#include "FGConstructDisqualifier.h"
#include "TDWellenStatistik.h"
#include "TDBalance.h"
#include "Command/ChatCommandLibrary.h"
#include "FGPlayerController.h"
#include "FGSaveSystem.h"
#include "FGSaveSession.h"
#include "FGGameMode.h"
#include "FGGameState.h"
#include "FGBuildableSubsystem.h"
#include "TDGate.h"
#include "FGPowerConnectionComponent.h"
#include "FGClearanceInterface.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "TDEinsatz.h"
#include "TDGegnerHirn.h"
#include "TDSicherheitsnetz.h"
#include "TDBandUpgrade.h"
#include "TDKernplatz.h"
#include "TDFabrikPruefstand.h"
#include "Creature/FGCreature.h"
#include "FGHealthComponent.h"
#include "FGSaveManagerInterface.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "TDPionier.h"
#include "TDKernTerminal.h"
#include "UObject/UObjectIterator.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "FGSchematic.h"
#include "FGResearchManager.h"
#include "FGResearchTree.h"
#include "Camera/PlayerCameraManager.h"
#include "FGSchematicManager.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGHUD.h"
#include "FGInventoryComponent.h"
#include "FGInventoryComponentEquipment.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Equipment/FGEquipment.h"
#include "FGCharacterPlayer.h"
#include "Buildables/FGBuildableMAM.h"
#include "UI/FGGameUI.h"
#include "Resources/FGItemDescriptor.h"

/** erster Turm eines Typs (Anzeigename, z. B. Gewehrturm) */
static ATDTurretBase* TDTestTurm(UWorld* World, const FString& Typ)
{
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		if (Typ == TEXT("*") || It->GetTurmName() == Typ)   // 0.9.7: "*" = irgendein Turm
		{
			return *It;
		}
	}
	return nullptr;
}

static const TCHAR* TDZustandName(ETDForschungZustand Z)
{
	switch (Z)
	{
	case ETDForschungZustand::Erforscht: return TEXT("Erforscht");
	case ETDForschungZustand::Moeglich: return TEXT("Moeglich");
	case ETDForschungZustand::ZuFrueh: return TEXT("ZuFrueh");
	case ETDForschungZustand::Gesperrt: return TEXT("Gesperrt");
	default: return TEXT("Ausgeschlossen");
	}
}

/** Sammelt Warnungen und Fehler des Mods waehrend des Selbsttests (Pruefung KeineWarnungen) */
class FTDAutotestLog : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (!Category.ToString().Contains(TEXT("FicsitTD")) || Verbosity > ELogVerbosity::Log)
		{
			return;
		}
		FScopeLock Sperre(&mSperre);
		// 0.10.1: alle Zeilen des Plans fuer die Pruefung LogEnthaelt (hoechstens 5000)
		if (mAlle.Num() < 5000)
		{
			mAlle.Add(FString(V));
		}
		if (Verbosity <= ELogVerbosity::Warning && mZeilen.Num() < 200)
		{
			mZeilen.Add(FString(V));
		}
	}
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	void Leeren()
	{
		FScopeLock Sperre(&mSperre);
		mZeilen.Reset();
		mAlle.Reset();
	}
	TArray<FString> Zeilen()
	{
		FScopeLock Sperre(&mSperre);
		return mZeilen;
	}
	/** Anzahl der Plan-Zeilen mit diesem Text, dazu die erste als Beleg */
	int32 Zaehle(const FString& Text, FString& OutErste)
	{
		FScopeLock Sperre(&mSperre);
		int32 N = 0;
		for (const FString& Z : mAlle)
		{
			// 16.11. (Bohrung: "kein zweiter Erstlohn" zaehlte die eigene OK-Zeile mit, die den Fund wiederholt): Zeilen des
			// Autotests selbst zaehlen nicht
			if (!Z.StartsWith(TEXT("Autotest")) && Z.Contains(Text))
			{
				OutErste = N++ == 0 ? Z : OutErste;
			}
		}
		return N;
	}
private:
	FCriticalSection mSperre;
	TArray<FString> mZeilen;
	TArray<FString> mAlle;
};

AFGPlayerController* UTDAutotestSender::GetPlayer() const
{
	const UWorld* World = GetWorld() ? GetWorld() : (GetOuter() ? GetOuter()->GetWorld() : nullptr);
	return World ? Cast<AFGPlayerController>(World->GetFirstPlayerController()) : nullptr;
}

FString UTDAutotest::Ordner()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FicsitTD"));
}

void UTDAutotest::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString Datei = FPaths::Combine(Ordner(), TEXT("Autotest.json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Datei))
	{
		return;
	}
	// sofort umbenennen: stuerzt der Test ab, startet das naechste normale Spiel nicht wieder im Testmodus
	IFileManager::Get().Move(*FPaths::Combine(Ordner(), TEXT("Autotest.laeuft.json")), *Datei, true);
	mSender = NewObject<UTDAutotestSender>(this);
	mLog = new FTDAutotestLog();
	GLog->AddOutputDevice(mLog);
	mTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTDAutotest::Tick), 0.5f);
	Starte(Text);
}

void UTDAutotest::Starte(const FString& Text)
{
	TSharedPtr<FJsonObject> Wurzel;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Wurzel) || !Wurzel.IsValid())
	{
		UE_LOG(LogFicsitTD, Error, TEXT("Autotest: Autotest.json ist kein gueltiges JSON"));
		mPhase = EPhase::Fertig;
		return;
	}
	// 0.76 Testpaket: mehrere Plaene in einem Spielstart; ein einzelner Plan ist ein Paket mit einem Plan
	mPaket = Wurzel;
	mPlaene = Wurzel->HasField(TEXT("Plaene")) ? Wurzel->GetArrayField(TEXT("Plaene")) : TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(Wurzel) };
	mPlanErgebnisse.Reset();
	mPlanNr = 0;
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest: Paket %s mit %d Plaenen%s"), *Wurzel->GetStringField(TEXT("Name")), mPlaene.Num(),
		Wurzel->HasField(TEXT("Bereitschaft")) && Wurzel->GetBoolField(TEXT("Bereitschaft")) ? TEXT(", danach Bereitschaft") : TEXT(""));
	StartePlan();
}

void UTDAutotest::StartePlan()
{
	mPlan = mPlaene[mPlanNr]->AsObject();
	mSchritte = mPlan->GetArrayField(TEXT("Schritte"));
	mErgebnisse.Reset();
	mFehler = 0;
	mIndex = 0;
	mSchrittBegonnen = false;
	mKlickUnten = false;
	mBauplatz = 0;
	mBauplatzInnen = 0;
	mBauplatzWeg = 0;
	mKernGefallen = false;
	TDNetz::Zuruecksetzen();
	mNachzueglerUhr = 0.f;
	mNachzueglerMeldungen = 0;
	mNachzueglerWelle = 0;
	mVersorgung.Reset();
	mVersorgtRest.Reset();
	// 17.11. (Test Fenster1117): Pruefstand je Plan neu - Bauten, Netzueberwachung, Zaehler und Kostenmodus
	mFabrik.Reset();
	mStromAktiv = false;
	UTDWaveSubsystem::GTestPlan = mPlan.IsValid() && mPlan->HasField(TEXT("Name")) ? mPlan->GetStringField(TEXT("Name")) : FString();
	++UTDWaveSubsystem::GTestLauf;
	mPauseSpiel = 0.f;
	TDFabrik::NeuerPlan();
	mPlanZeit = 0.f;
	mUhr = 0.f;
	if (mLog)
	{
		mLog->Leeren();
	}
	// 16.11.: jeder Plan beginnt mit Entwicklermodus (ein abgebrochener Plan Spielersicht darf die folgenden nicht verfaelschen)
	if (UTDKernplatz::IstSpielersicht())
	{
		UTDKernplatz::SetzeSpielersicht(false);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest: Plan %d/%d %s mit %d Schritten, Spielstand %s"), mPlanNr + 1, mPlaene.Num(), *mPlan->GetStringField(TEXT("Name")),
		mSchritte.Num(), *mPlan->GetStringField(TEXT("Spielstand")));
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World->IsGameWorld() && World->GetMapName().Contains(TEXT("Persistent_Level")))
	{
		if (mPlan->HasField(TEXT("NeuesSpiel")))
		{
			StarteNeuesSpiel(World);
		}
		else
		{
			LadeSpielstand(World);   // schon im Spiel: naechsten Spielstand direkt laden
		}
	}
	else
	{
		mPhase = EPhase::Menue;
	}
}

void UTDAutotest::Deinitialize()
{
	if (mTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(mTicker);
	}
	if (mLog)
	{
		GLog->RemoveOutputDevice(mLog);
		delete mLog;
		mLog = nullptr;
	}
	Super::Deinitialize();
}

static bool TDIstSpielwelt(const UWorld* World)
{
	return World && World->IsGameWorld() && World->GetMapName().Contains(TEXT("Persistent_Level"));
}

void UTDAutotest::SetzeZeitraffer(UWorld* World, float Wunsch)
{
	mZeitrafferWunsch = FMath::Max(0.1f, Wunsch);
	mZeitrafferIst = -1.f;
	PasseZeitrafferAn(World);
}

void UTDAutotest::PasseZeitrafferAn(UWorld* World)
{
	// 17.11. (B20, Messung Paket B20): das Spiel gibt dem Fabrik-Takt je Bild hoechstens ~1/30 s. Ist ein Rechenschritt
	// (echte Bildzeit x Zeitraffer) laenger, laufen Baender, Maschinen und das Nachladen der Tuerme langsamer als die Gegner -
	// bei x4 und 32 ms je Bild nur 52 % (Welle 5: Kern 36 % statt 91 %). Deshalb den Zeitraffer so deckeln, dass ein Schritt
	// Test.MaxSchritt nicht uebersteigt. Unter 1 wird nie gebremst.
	if (!World)
	{
		return;
	}
	mBildMittel = 0.8f * mBildMittel + 0.2f * FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.001f, 0.5f);
	const float MaxSchritt = TDBalance::Wert(TEXT("Test.MaxSchritt"), 1.f / 30.f);
	const float Erlaubt = mZeitrafferWunsch <= 1.f ? mZeitrafferWunsch : FMath::Clamp(MaxSchritt / mBildMittel, 1.f, mZeitrafferWunsch);
	if (mZeitrafferIst < 0.f || FMath::Abs(Erlaubt - mZeitrafferIst) > 0.05f * mZeitrafferIst)
	{
		if (Erlaubt < mZeitrafferWunsch - 0.01f && (mZeitrafferIst < 0.f || mZeitrafferIst >= mZeitrafferWunsch - 0.01f))
		{
			++mRafferGedeckelt;
			UE_LOG(LogFicsitTD, Log, TEXT("Autotest: Zeitraffer x%.1f gedeckelt auf x%.2f (Bild %.0f ms, Schritt hoechstens %.0f ms)"),
				mZeitrafferWunsch, Erlaubt, 1000.f * mBildMittel, 1000.f * MaxSchritt);
		}
		mZeitrafferIst = Erlaubt;
		UGameplayStatics::SetGlobalTimeDilation(World, Erlaubt);
	}
}

bool UTDAutotest::Tick(float /*BildDt*/)
{
	// Test 0.74.0: der Ticker uebergibt nur die Zeit eines Bildes, nicht die seit dem letzten Aufruf (alles lief ~30x zu langsam) -> selbst messen
	const double Jetzt = FPlatformTime::Seconds();
	const float Dt = mLetzte > 0.0 ? static_cast<float>(Jetzt - mLetzte) : 0.f;
	mLetzte = Jetzt;
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	mGesamt += Dt;
	mUhr += Dt;
	mPlanZeit += Dt;
	if (mZeitrafferWunsch > 1.f)
	{
		PasseZeitrafferAn(World);   // 17.11. (B20)
	}
	if (mPhase == EPhase::Bereit)
	{
		// 0.76 Bereitschaft: auf die naechste Autotest.json warten; Balance neu lesen (Aenderungen ohne Build)
		const FString Datei = FPaths::Combine(Ordner(), TEXT("Autotest.json"));
		FString Text;
		if (mUhr > 2.f && FFileHelper::LoadFileToString(Text, *Datei))
		{
			IFileManager::Get().Move(*FPaths::Combine(Ordner(), TEXT("Autotest.laeuft.json")), *Datei, true);
			UE_LOG(LogFicsitTD, Log, TEXT("Autotest: neuer Auftrag in Bereitschaft - %s"), *TDBalance::Lade());
			Starte(Text);
		}
		else if (mUhr > 2.f)
		{
			mUhr = 0.f;
		}
		return true;
	}
	// 17.11. (P11): Pruefstand-Netz ueberwachen - Sicherung nach Test.SicherungZurueck zuruecksetzen, Spitze merken
	if (mStromAktiv && World)
	{
		for (const FString& Meldung : TDFabrik::Ueberwache(World, mFabrik, Dt))
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Autotest Strom: %s"), *Meldung);
		}
	}
	// 0.77 (Test Neustart: Spielfigur starb in den Duenen, der Test wartete endlos): im Selbsttest ist die Figur unverwundbar
	if (World && World->GetFirstPlayerController())
	{
		if (APawn* Figur = World->GetFirstPlayerController()->GetPawn())
		{
			Figur->SetCanBeDamaged(false);
		}
	}
	const float MaxSekunden = 60.f * (mPlan.IsValid() && mPlan->HasField(TEXT("MaxMinuten")) ? mPlan->GetNumberField(TEXT("MaxMinuten")) : 12.f);
	if (mPhase != EPhase::Fertig && mPlanZeit > MaxSekunden)
	{
		Ergebnis(TEXT("Abbruch"), FString::Printf(TEXT("Zeitlimit %.0f min erreicht (Phase %d, Schritt %d)"), MaxSekunden / 60.f, static_cast<int32>(mPhase), mIndex + 1), false);
		Beende(TEXT("Zeitlimit"));
		return mPhase != EPhase::Fertig;
	}
	switch (mPhase)
	{
	case EPhase::Menue:
		// Hauptmenue: kurz warten, bis es steht, dann den Spielstand laden
		if (World && !TDIstSpielwelt(World) && World->GetFirstPlayerController() && mUhr > 8.f)
		{
			if (mPlan->HasField(TEXT("NeuesSpiel")))
			{
				StarteNeuesSpiel(World);
			}
			else
			{
				LadeSpielstand(World);
			}
		}
		else if (TDIstSpielwelt(World))
		{
			// 0.11 (Paket EndlosTest 15.11.: nach einem Plan, der am Ende einen Spielstand laedt, war die Welt beim Planwechsel
			// noch im Wechsel; der naechste Plan mit "NeuesSpiel" lief dann in der alten Welt weiter): neues Spiel nachholen
			if (mPlan->HasField(TEXT("NeuesSpiel")) && World->GetFirstPlayerController() && mUhr > 3.f)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Autotest: Plan verlangt ein neues Spiel - starte es aus der laufenden Welt"));
				StarteNeuesSpiel(World);
			}
			else if (!mPlan->HasField(TEXT("NeuesSpiel")))
			{
				mPhase = EPhase::Laedt;   // schon im Spiel (z. B. Startparameter)
				mUhr = 0.f;
			}
		}
		break;
	case EPhase::Laedt:
		// 0.76: beim Laden aus dem laufenden Spiel erst die neue Welt abwarten
		if (TDIstSpielwelt(World) && World != mAlteWelt.Get() && World->GetFirstPlayerController() && World->GetFirstPlayerController()->GetPawn())
		{
			if (mUhr > (mPlan->HasField(TEXT("WarteNachLaden")) ? mPlan->GetNumberField(TEXT("WarteNachLaden")) : 20.f))
			{
				Ergebnis(TEXT("Laden"), mPlan->HasField(TEXT("NeuesSpiel")) && !mLadenImPlan ? FString(TEXT("neues Spiel gestartet"))
					: FString(TEXT("Spielstand geladen")), true);
				mPhase = EPhase::Spiel;
				mIndex = mLadenImPlan ? mIndex + 1 : 0;
				mLadenImPlan = false;
				mSchrittBegonnen = false;
			}
		}
		else
		{
			mUhr = 0.f;
		}
		break;
	case EPhase::Spiel:
		if (!TDIstSpielwelt(World))
		{
			break;
		}
		Versorge(World, Dt);
		if (mIndex >= mSchritte.Num())
		{
			Beende(TEXT("alle Schritte"));
			return mPhase != EPhase::Fertig;
		}
		if (!mSchrittBegonnen)
		{
			mSchrittBegonnen = true;
			mSchrittUhr = 0.f;
		}
		mSchrittUhr += Dt;
		if (Schritt(World, mSchritte[mIndex]->AsObject(), Dt))
		{
			++mIndex;
			mSchrittBegonnen = false;
		}
		break;
	default:
		break;
	}
	return mPhase != EPhase::Fertig;
}

void UTDAutotest::StarteNeuesSpiel(UWorld* World)
{
	// 0.77 Neustart-Rauchtest: neues Spiel mit dem Sitzungsschalter "FICSIT Tower Defense" (Wert wie im Log einer echten Sitzung)
	const TSharedPtr<FJsonObject> Neu = mPlan->GetObjectField(TEXT("NeuesSpiel"));
	// Optionsnamen wie im Log einer echten Sitzung ("?sessionName=...", AFGGameMode::SessionNameOption ist nicht linkbar)
	// 0.11: "Modus" (Standard 1 = Einsatz, 0 = Aus fuer den Plan ModusAus). Wert ist ein FVariant als Base64:
	// Typ 14 (Int32), Laenge 4, Wert - also DgAAAAQAAAA + Base64 der letzten 4 Byte
	const int32 Modus = Neu->HasField(TEXT("Modus")) ? static_cast<int32>(Neu->GetNumberField(TEXT("Modus"))) : 1;
	// 0.11 (Gegencheck Einsatz, B12): optional "Stufe" (0 nach Startort, 1..4 Erkundung..Albtraum) als zweite Einstellung,
	// SML trennt die Einstellungen mit Komma (USessionSettingsManager::SerializeSettingsToString)
	static const TCHAR* IntWert[] = { TEXT("DgAAAAQAAAAAAAAA"), TEXT("DgAAAAQAAAABAAAA"), TEXT("DgAAAAQAAAACAAAA"), TEXT("DgAAAAQAAAADAAAA"), TEXT("DgAAAAQAAAAEAAAA") };
	FString Einstellungen = FString::Printf(TEXT("FicsitTD.ModusInt=%s"), IntWert[FMath::Clamp(Modus, 0, 3)]);
	if (Neu->HasField(TEXT("Stufe")))
	{
		Einstellungen += FString::Printf(TEXT(",FicsitTD.StufeInt=%s"), IntWert[FMath::Clamp(static_cast<int32>(Neu->GetNumberField(TEXT("Stufe"))), 0, 4)]);
	}
	FString Optionen = FString::Printf(TEXT("?sessionName=Autotest_Neu?SessionSettings=%s?skiponboarding"), *Einstellungen);
	if (Neu->HasField(TEXT("Startort")))
	{
		Optionen += FString::Printf(TEXT("?%s=%s"), AFGGameMode::StartLocationOption, *Neu->GetStringField(TEXT("Startort")));
	}
	// 0.83.1 (Analyse 07.11.: der zufaellige Kernplatz streute die Laeufe staerker als jede Code-Aenderung): fester Platz,
	// Standard 2 (Fels-S8, die meisten Messungen der Kalibrierung); "Kernplatz": "zufall" im Plan erlaubt die Zufallswahl
	const FString Platz = Neu->HasField(TEXT("Kernplatz")) ? Neu->GetStringField(TEXT("Kernplatz")) : FString(TEXT("2"));
	UTDKernplatz::sVorgabePlatz = Platz.Equals(TEXT("zufall"), ESearchCase::IgnoreCase) ? FString() : Platz;
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest: neues Spiel %s, Kernplatz %s"), *Optionen, *Platz);
	mAlteWelt = TDIstSpielwelt(World) ? World : nullptr;
	SetzeZeitraffer(World, 1.f);
	UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/FactoryGame/Map/GameLevel01/Persistent_Level")), true, Optionen);
	mPhase = EPhase::Laedt;
	mUhr = 0.f;
}

void UTDAutotest::LadeSpielstand(UWorld* World, const FString& NameVorgabe)
{
	const FString Name = NameVorgabe.IsEmpty() ? mPlan->GetStringField(TEXT("Spielstand")) : NameVorgabe;
	UFGSaveSystem* Speicher = UFGSaveSystem::Get(World);
	FSaveHeader Kopf;
	if (!Speicher || !Speicher->LoadSaveGameHeaderSync(Name, Kopf))
	{
		Ergebnis(TEXT("Laden"), FString::Printf(TEXT("Spielstand %s nicht gefunden"), *Name), false);
		Beende(TEXT("Spielstand fehlt"));
		return;
	}
	FLoadSaveFileParameters Parameter;
	Parameter.bEnableAdvancedGameSettings = !mPlan->HasField(TEXT("ErweiterteEinstellungen")) || mPlan->GetBoolField(TEXT("ErweiterteEinstellungen"));
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest: lade Spielstand %s"), *Name);
	mAlteWelt = TDIstSpielwelt(World) ? World : nullptr;
	SetzeZeitraffer(World, 1.f);
	Speicher->LoadSaveFile(Kopf, Parameter, World->GetFirstPlayerController());
	mPhase = EPhase::Laedt;
	mUhr = 0.f;
}

bool UTDAutotest::Schritt(UWorld* World, const TSharedPtr<FJsonObject>& S, float Dt)
{
	if (!S.IsValid())
	{
		return true;
	}
	double Zahl = 0.0;
	FString Wort;
	if (S->TryGetNumberField(TEXT("warte"), Zahl))
	{
		return mSchrittUhr >= Zahl;
	}
	if (!S->HasField(TEXT("wellenlauf")) && S->TryGetNumberField(TEXT("zeitraffer"), Zahl))   // 0.78: "zeitraffer" ist auch Feld des Wellenlaufs
	{
		SetzeZeitraffer(World, FMath::Clamp(static_cast<float>(Zahl), 0.25f, 8.f));
		Ergebnis(TEXT("Zeitraffer"), FString::Printf(TEXT("x%.1f"), Zahl), true);
		return true;
	}
	if (S->TryGetStringField(TEXT("fabrik"), Wort))
	{
		// 16.11. Fabrik-Pruefstand F1: echte Munitionslinie mit Spielgebaeuden und Baendern (TDFabrikPruefstand.cpp)
		ATDDefenseCore* FabrikKern = nullptr;
		for (TActorIterator<ATDDefenseCore> It(World); It && !FabrikKern; ++It)
		{
			FabrikKern = *It;
		}
		TArray<FString> Bericht;
		const bool bOk = TDFabrik::BaueLinie(World, FabrikKern, Wort, S->HasField(TEXT("tuerme")) ? int32(S->GetNumberField(TEXT("tuerme"))) : 4,
			S->HasField(TEXT("level")) ? int32(S->GetNumberField(TEXT("level"))) : 2, S->HasField(TEXT("band")) ? int32(S->GetNumberField(TEXT("band"))) : 1, mFabrik, Bericht,
			S->HasField(TEXT("ketten")) ? int32(S->GetNumberField(TEXT("ketten"))) : 1,
			S->HasField(TEXT("ausbau")) ? int32(S->GetNumberField(TEXT("ausbau"))) : 1);
		Ergebnis(TEXT("Fabrik"), Wort, bOk, FString::Join(Bericht, TEXT(" | ")));
		return true;
	}
	if (S->TryGetStringField(TEXT("kosten"), Wort))
	{
		TDFabrik::SetzeKosten(Wort);   // 17.11. (P11/B6): "frei", "kontingent" oder "echt"
		Ergebnis(TEXT("Fabrik"), TEXT("Kosten ") + Wort, true);
		return true;
	}
	if (S->TryGetStringField(TEXT("strom"), Wort))
	{
		ATDDefenseCore* StromKern = nullptr;
		for (TActorIterator<ATDDefenseCore> It(World); It && !StromKern; ++It)
		{
			StromKern = *It;
		}
		TArray<FString> Bericht;
		const bool bOk = TDFabrik::BaueStrom(World, StromKern, Wort, S->HasField(TEXT("anzahl")) ? int32(S->GetNumberField(TEXT("anzahl"))) : 2, mFabrik, Bericht);
		// 17.11. (Test Fenster1117): ein fehlgeschlagener zweiter Stromschritt (Kohle ohne Forschung) schaltete die Ueberwachung
		// des laufenden Biomasse-Netzes ab - die Sicherung blieb dann aus. Ueberwacht wird, sobald irgendein Kraftwerk steht.
		mStromAktiv = mStromAktiv || bOk;
		Ergebnis(TEXT("Fabrik"), TEXT("Strom ") + Wort, bOk, FString::Join(Bericht, TEXT(" | ")));
		return true;
	}
	if (S->HasField(TEXT("netz")))
	{
		TArray<FString> Bericht;
		const int32 Masten = TDFabrik::VerbindeNetz(World, mFabrik, Bericht);
		Ergebnis(TEXT("Fabrik"), TEXT("Netz verbinden"), true, FString::Printf(TEXT("%d neue Masten"), Masten));
		return true;
	}
	if (S->HasField(TEXT("fabrikbericht")))
	{
		TArray<FString> Zeilen = TDFabrik::Bericht(mFabrik);
		Zeilen.Append(TDFabrik::StromBericht());
		Ergebnis(TEXT("Fabrik-Bericht"), TEXT("Stand der Linie"), mFabrik.Num() > 0, FString::Join(Zeilen, TEXT(" | ")));
		return true;
	}
	if (S->HasField(TEXT("spielersicht")))
	{
		// 16.11. (Nachpruefung B14): Entwicklermodus zur Laufzeit ab- bzw. wieder anschalten
		UTDKernplatz::SetzeSpielersicht(S->GetBoolField(TEXT("spielersicht")));
		Ergebnis(TEXT("Spielersicht"), UTDKernplatz::IstSpielersicht() ? TEXT("an") : TEXT("aus"), true,
			UTDKernplatz::Entwicklermodus() ? TEXT("Entwicklermodus an") : TEXT("Entwicklermodus aus"));
		return true;
	}
	if (S->TryGetStringField(TEXT("befehl"), Wort))
	{
		mSender->Antworten.Reset();
		AChatCommandSubsystem* Befehle = AChatCommandSubsystem::Get(World);
		const EExecutionStatus Status = Befehle ? Befehle->RunChatCommand(Wort, mSender) : EExecutionStatus::UNCOMPLETED;
		bool bOk = Status == EExecutionStatus::COMPLETED;
		// 16.11.: optional "antwortEnthaelt" - die Antwort muss diesen Text enthalten (z. B. Ablehnung ohne Entwicklermodus)
		FString Erwartet;
		if (S->TryGetStringField(TEXT("antwortEnthaelt"), Erwartet))
		{
			bOk = bOk && mSender->Antworten.ContainsByPredicate([&Erwartet](const FString& A) { return A.Contains(Erwartet); });
		}
		Ergebnis(TEXT("Befehl"), TEXT("/") + Wort, bOk, bOk ? TEXT("ausgefuehrt") : TEXT("nicht ausgefuehrt"), mSender->Antworten);
		return true;
	}
	if (S->TryGetStringField(TEXT("warteBis"), Wort))
	{
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		const bool bLaeuft = Waves && Waves->LaeuftWelle();
		// 0.11: "EinsatzAktiv" fuer den Plan Endlos (der Kernplatz startet den Einsatz erst nach dem Netz)
		const UTDEinsatzSubsystem* EinsatzWarte = World->GetSubsystem<UTDEinsatzSubsystem>();
		const bool bErreicht = Wort == TEXT("WelleLaeuft") ? bLaeuft
			: Wort == TEXT("EinsatzAktiv") ? (EinsatzWarte && EinsatzWarte->IstAktiv())
			: Wort == TEXT("BohrungVorbei") ? (Waves && !Waves->IstBohrung())   // 0.12-dev Freies Spiel mit Abwehr
			: !bLaeuft;
		const double Max = S->HasField(TEXT("max")) ? S->GetNumberField(TEXT("max")) : 300.0;
		if (bErreicht || mSchrittUhr >= Max)
		{
			Ergebnis(TEXT("Warten"), Wort, bErreicht, FString::Printf(TEXT("%.0f s%s"), mSchrittUhr, bErreicht ? TEXT("") : TEXT(", nicht erreicht")));
			return true;
		}
		return false;
	}
	if (S->TryGetStringField(TEXT("pruefe"), Wort))
	{
		FString Ist;
		const bool bOk = Pruefe(World, S, Ist);
		Ergebnis(TEXT("Pruefung"), S->HasField(TEXT("text")) ? S->GetStringField(TEXT("text")) : Wort, bOk, Ist);
		return true;
	}
	if (S->HasField(TEXT("ueberblick")))
	{
		// 0.78: fuer Fotos die Figur schraeg ueber den Kern stellen und auf ihn blicken (Test 0.78: Turm stand im Bild)
		ATDDefenseCore* Kern = nullptr;
		for (TActorIterator<ATDDefenseCore> It(World); It && !Kern; ++It)
		{
			Kern = *It;
		}
		APlayerController* PC = World->GetFirstPlayerController();
		APawn* Figur = PC ? PC->GetPawn() : nullptr;
		if (Kern && Figur)
		{
			const FVector Ort = Kern->GetActorLocation() + FVector(-3500.f, -1500.f, 2500.f);
			Figur->SetActorLocation(Ort, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation((Kern->GetActorLocation() - Ort).Rotation());
		}
		Ergebnis(TEXT("Ueberblick"), TEXT("Figur über dem Kern"), Kern && Figur);
		return true;
	}
	if (S->TryGetStringField(TEXT("stelle"), Wort))
	{
		// 0.10.2 (Screenshots): Figur neben einen Turm ("*" = irgendeiner) auf den Boden stellen, Blick zum Turm.
		// "abstand" in m (Standard 9), "winkel" in Grad um den Turm (Standard 0)
		// "gegner": waehrend der Welle den Turm, der den Wellengegnern am naechsten steht; die Figur steht hinter ihm und blickt zu den Gegnern
		// Optik-Durchgang 16.11.2026: auch "Tor" (ATDGate); Standplatz wird gesucht (siehe unten)
		AActor* Turm = nullptr;
		bool bGegner = false;
		FVector GegnerMitte = FVector::ZeroVector;
		if (Wort == TEXT("gegner"))
		{
			int32 Anzahl = 0;
			for (TActorIterator<AFGCreature> It(World); It; ++It)
			{
				if (It->Tags.Contains(TEXT("FicsitTD")) && !It->IsPendingKillPending())
				{
					GegnerMitte += It->GetActorLocation();
					++Anzahl;
				}
			}
			if (Anzahl > 0)
			{
				GegnerMitte /= Anzahl;
				bGegner = true;
				float Beste = TNumericLimits<float>::Max();
				for (TActorIterator<ATDTurretBase> It(World); It; ++It)
				{
					const float D = FVector::DistSquared(It->GetActorLocation(), GegnerMitte);
					if (D < Beste)
					{
						Beste = D;
						Turm = *It;
					}
				}
			}
		}
		else if (Wort == TEXT("Tor"))
		{
			for (TActorIterator<ATDGate> It(World); It && !Turm; ++It)
			{
				Turm = *It;
			}
		}
		else if (Wort.StartsWith(TEXT("Build_")))
		{
			// Optik-Durchgang 16.11.2026 (P4): Spielgebaeude nach Klassenname (z. B. Build_ConstructorMk1)
			for (TActorIterator<AFGBuildable> It(World); It && !Turm; ++It)
			{
				if (It->GetClass()->GetName().StartsWith(Wort))
				{
					Turm = *It;
				}
			}
		}
		else
		{
			Turm = TDTestTurm(World, Wort);
		}
		APlayerController* PC = World->GetFirstPlayerController();
		ACharacter* Figur = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		bool bBoden = false;
		if (Turm && Figur)
		{
			const float Abstand = (S->HasField(TEXT("abstand")) ? static_cast<float>(S->GetNumberField(TEXT("abstand"))) : 9.f) * 100.f;
			float Winkel = S->HasField(TEXT("winkel")) ? static_cast<float>(S->GetNumberField(TEXT("winkel"))) : 0.f;
			const FVector Mitte = Turm->GetActorLocation();
			if (bGegner)
			{
				// hinter den Turm (von den Gegnern aus gesehen), seitlich versetzt um "winkel"
				Winkel += (Mitte - GegnerMitte).GetSafeNormal2D().Rotation().Yaw;
			}
			// Optik-Durchgang 16.11.2026 (Drohnenplatz-Foto zeigte nur Himmel): mit "suchen": true den Standplatz reihum in
			// 30-Grad-Schritten suchen, bis der Boden frei von Gebaeuden ist und hoechstens 4 m ueber/unter dem Fuss des Baus liegt;
			// sonst der erste Bodentreffer. Ohne "suchen" wie bisher genau ein Versuch unter "winkel".
			FVector Ort = Mitte + FRotator(0.f, Winkel, 0.f).RotateVector(FVector(Abstand, 0.f, 0.f));
			FCollisionQueryParams Abfrage(TEXT("TDStelle"), false, Figur);
			const int32 Versuche = S->HasField(TEXT("suchen")) && S->GetBoolField(TEXT("suchen")) ? 12 : 1;
			bool bErster = true;
			for (int32 Versuch = 0; Versuch < Versuche; ++Versuch)
			{
				const FVector Probe = Mitte + FRotator(0.f, Winkel + Versuch * 30.f, 0.f).RotateVector(FVector(Abstand, 0.f, 0.f));
				FHitResult Treffer;
				if (!World->LineTraceSingleByChannel(Treffer, Probe + FVector(0.f, 0.f, 3000.f), Probe - FVector(0.f, 0.f, 5000.f), ECC_Visibility, Abfrage))
				{
					continue;
				}
				const FVector Stand = Treffer.ImpactPoint + FVector(0.f, 0.f, Figur->GetSimpleCollisionHalfHeight() + 5.f);
				const bool bFrei = !Cast<AFGBuildable>(Treffer.GetActor()) && FMath::Abs(Treffer.ImpactPoint.Z - Mitte.Z) <= 400.f;
				if (bErster || bFrei)
				{
					Ort = Stand;
					bBoden = true;
					bErster = false;
				}
				if (bFrei)
				{
					break;
				}
			}
			if (Figur->GetCharacterMovement())
			{
				Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			}
			Figur->SetActorLocation(Ort, false, nullptr, ETeleportType::TeleportPhysics);
			const FVector Ziel = bGegner ? GegnerMitte + FVector(0.f, 0.f, 100.f) : Mitte + FVector(0.f, 0.f, 250.f);
			const FRotator Blick = (Ziel - Ort).Rotation();
			PC->SetControlRotation(Blick);
			Figur->SetActorRotation(FRotator(0.f, Blick.Yaw, 0.f));
		}
		Ergebnis(TEXT("Spieler"), FString::Printf(TEXT("stelle %s"), *Wort), Turm && Figur && bBoden,
			!Turm ? FString(bGegner || Wort != TEXT("gegner") ? TEXT("kein Turm") : TEXT("keine Wellengegner")) : bBoden
			? FString::Printf(TEXT("neben %s%s"), *Turm->GetName(), bGegner ? *FString::Printf(TEXT(", Gegner in %.0f m"), FVector::Dist(Turm->GetActorLocation(), GegnerMitte) / 100.f) : TEXT(""))
			: FString(TEXT("kein Boden gefunden")));
		return true;
	}
	const TSharedPtr<FJsonObject>* BalanceWerte = nullptr;
	if (S->TryGetObjectField(TEXT("balance"), BalanceWerte))
	{
		// 0.78.2: Balance-Werte fuer den A/B-Versuch setzen (gelten bis zum naechsten Laden der Datei)
		TArray<FString> Teile;
		bool bAlle = true;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*BalanceWerte)->Values)
		{
			const bool bOk = TDBalance::SetzeTest(V.Key, static_cast<float>(V.Value->AsNumber()));
			bAlle &= bOk;
			Teile.Add(FString::Printf(TEXT("%s=%g%s"), *V.Key, V.Value->AsNumber(), bOk ? TEXT("") : TEXT(" (unbekannt)")));
		}
		Ergebnis(TEXT("Pruefstand"), TEXT("Balance ") + FString::Join(Teile, TEXT(", ")), bAlle);
		return true;
	}
	if (S->HasField(TEXT("stromfrei")))
	{
		AFGGameState* Zustand = World->GetGameState<AFGGameState>();
		if (Zustand)
		{
			Zustand->SetCheatNoPower(S->GetBoolField(TEXT("stromfrei")));
		}
		Ergebnis(TEXT("Pruefstand"), TEXT("Strom frei"), Zustand != nullptr);
		return true;
	}
	if (S->TryGetStringField(TEXT("baue"), Wort))
	{
		// Pruefstand: Tuerme direkt auf freie Plaetze der Plattform setzen (8-m-Raster um den Kern, ohne Kontingent)
		ATDDefenseCore* Kern = nullptr;
		for (TActorIterator<ATDDefenseCore> It(World); It && !Kern; ++It)
		{
			Kern = *It;
		}
		static const TMap<FString, FString> Ordner = { { TEXT("Bolzenturm"), TEXT("BoltTurret") }, { TEXT("Gewehrturm"), TEXT("GunTurret") },
			{ TEXT("Teslaspule"), TEXT("TeslaCoil") }, { TEXT("Flammenwerfer"), TEXT("FlameTurret") }, { TEXT("Nobelisk-Werfer"), TEXT("NobeliskLauncher") },
			{ TEXT("Tor"), TEXT("Gate") }, { TEXT("Lockfeuer"), TEXT("LureBeacon") }, { TEXT("Oelschlick"), TEXT("SlickPlate") },   // 0.81 passive Bauten P1
			{ TEXT("Elektrozaun"), TEXT("ConductorWall") }, { TEXT("Funkmast"), TEXT("RadioMast") }, { TEXT("Schildgenerator"), TEXT("ShieldGenerator") }, { TEXT("Himmelsturm"), TEXT("FlakTurret") }, { TEXT("Stoerfeld"), TEXT("FieldProjector") }, { TEXT("Singularitaet"), TEXT("SingularityLauncher") }, { TEXT("Dimensionslager"), TEXT("DimensionalDepot") }, { TEXT("Drohnenplatz"), TEXT("DronePort") } };   // 0.83 Bauten P2/P3, 0.9.3 Drohnenplatz
		const FString* O = Ordner.Find(Wort);
		UClass* Klasse = O ? LoadClass<AFGBuildable>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/%s/Build_TD_%s.Build_TD_%s_C"), **O, **O, **O)) : nullptr;
		// Optik-Durchgang 16.11.2026 (P4): Spielgebaeude als Vergleich im selben Licht, "klasse": voller Klassenpfad
		// (z. B. /Game/FactoryGame/Buildable/Factory/ConstructorMk1/Build_ConstructorMk1.Build_ConstructorMk1_C)
		if (!Klasse && S->HasField(TEXT("klasse")))
		{
			Klasse = LoadClass<AFGBuildable>(nullptr, *S->GetStringField(TEXT("klasse")));
		}
		UClass* BauRezept = O ? LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/%s/Recipe_TD_%s.Recipe_TD_%s_C"), **O, **O, **O)) : nullptr;
		UClass* Munition = O && S->HasField(TEXT("rezept")) ? LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/%s/%s.%s_C"), **O,
			*S->GetStringField(TEXT("rezept")), *S->GetStringField(TEXT("rezept")))) : nullptr;
		AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
		const int32 Anzahl = S->HasField(TEXT("anzahl")) ? static_cast<int32>(S->GetNumberField(TEXT("anzahl"))) : 1;
		const int32 Level = S->HasField(TEXT("level")) ? static_cast<int32>(S->GetNumberField(TEXT("level"))) : 1;
		// Plaetze: Ring um den Kern im 8-m-Raster, abwechselnd die Seiten, damit sich die Tuerme verteilen.
		// 0.78.1 (Pruefstand: Flammenwerfer auf 16 m hatte nur 32 % Zeit mit Ziel): "ring": "innen" nimmt den 8-m-Ring
		// 0.9.3a: Kern 16 x 16 m (Kante 8,1 m) -> innerer Ring von 8 auf 12 m, aeusserer von 16 auf 20 m (Plattform 48 m, Kante 24 m)
		static const FVector2D Aussen[] = { {20, 0}, {-20, 0}, {0, 20}, {0, -20}, {20, 20}, {-20, -20}, {20, -20}, {-20, 20}, {10, 20}, {-10, -20}, {20, 10}, {-20, -10},
			{-10, 20}, {10, -20}, {-20, 10}, {20, -10} };
		static const FVector2D Innen[] = { {0, 12}, {0, -12}, {12, 0}, {-12, 0}, {12, 12}, {-12, -12}, {12, -12}, {-12, 12} };
		const bool bInnen = S->HasField(TEXT("ring")) && S->GetStringField(TEXT("ring")) == TEXT("innen");
		const FVector2D* Raster = bInnen ? Innen : Aussen;
		const int32 Plaetze = bInnen ? UE_ARRAY_COUNT(Innen) : UE_ARRAY_COUNT(Aussen);
		int32 WegZaehler = 0;
		int32& Platz = S->HasField(TEXT("ring")) && S->GetStringField(TEXT("ring")) == TEXT("weg") ? WegZaehler : bInnen ? mBauplatzInnen : mBauplatz;
		// 0.9.4 (Nachtest094: Stoerfeld und Zaun 0 gebaut): direkt nach dem Spielstart ist das Netz noch nicht fertig, die Fronten sind
		// ungeprueft und haben keinen Weg. Das Spiel sucht sie alle 15 s neu -> bis 90 s auf einen Frontweg warten.
		if (S->HasField(TEXT("ring")) && S->GetStringField(TEXT("ring")) == TEXT("weg") && mSchrittUhr < 90.f)
		{
			bool bWegDa = false;
			if (UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>())
			{
				for (const FTDFront& Fr : Waves->GetFronten())
				{
					bWegDa |= Fr.Punkte.Num() >= 2;
				}
			}
			if (!bWegDa)
			{
				return false;
			}
		}
		int32 Gebaut = 0;
		// 0.9.1 (Pruefung091: Oelschlick lag auf dem Ring neben dem Weg, nie ein Gegner darauf): "ring": "weg" setzt den Bau auf den
		// Weg der ersten aktiven Front, 18 m vom Kern und dann je 10 m weiter nach aussen
		const bool bWeg = S->HasField(TEXT("ring")) && S->GetStringField(TEXT("ring")) == TEXT("weg");
		const TArray<FVector>* WegPunkte = nullptr;
		if (bWeg)
		{
			if (UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>())
			{
				for (const FTDFront& Fr : Waves->GetFronten())
				{
					if (Fr.Punkte.Num() >= 2) { WegPunkte = &Fr.Punkte; break; }
				}
			}
		}
		for (int32 i = 0; i < Anzahl && Kern && Klasse && Bau && (bWeg || Platz < Plaetze); ++i, ++Platz)
		{
			FVector Ort = Kern->GetActorLocation() + FVector(Raster[FMath::Min(Platz, Plaetze - 1)].X * 100.f, Raster[FMath::Min(Platz, Plaetze - 1)].Y * 100.f, 0.f);
			FRotator Dreh = FRotator::ZeroRotator;
			if (bWeg)
			{
				if (!WegPunkte)
				{
					break;
				}
				// 0.9.4 (Nachtest094: Oelschlick 0 Takte mit Ziel): die Wegpunkte liegen bis zu 50 m auseinander, der naechste Punkt war
				// das Wegende am Kern. Jetzt wird jedes Teilstueck in 1-m-Schritten abgetastet, so liegt der Bau genau auf dem Weg im Sollabstand.
				// 0.9.4: Kern 16 x 16 m -> erster Platz 22 m (vorher 18 m bei 8 x 8 m)
				// 0.9.4 (Pruefung091b: Leiter 0-mal, Teslas am Kern, Zaun 22-32 m draussen): "abstand" setzt den Sollabstand fest
				// (ohne den Zaehler weiterzuschieben), "seite" stellt den Bau so viele Meter neben den Weg (abwechselnd links/rechts)
				const float Soll = S->HasField(TEXT("abstand")) ? static_cast<float>(S->GetNumberField(TEXT("abstand"))) * 100.f : (22.f + 10.f * mBauplatzWeg++) * 100.f;
				float Best = TNumericLimits<float>::Max();
				for (int32 p = 0; p + 1 < WegPunkte->Num(); ++p)
				{
					const FVector A = (*WegPunkte)[p], B = (*WegPunkte)[p + 1];
					const int32 Schritte = FMath::Max(1, FMath::CeilToInt(FVector::Dist(A, B) / 100.f));
					for (int32 k = 0; k <= Schritte; ++k)
					{
						const FVector P = FMath::Lerp(A, B, float(k) / Schritte);
						const float D = FMath::Abs(FVector::Dist2D(P, Kern->GetActorLocation()) - Soll);
						if (D < Best)
						{
							Best = D;
							Ort = P;
							Dreh = FRotator(0.f, (B - A).Rotation().Yaw, 0.f);
						}
					}
				}
				// 0.9.4: "quer" stellt den Bau quer zum Weg (Elektrozaun als Sperre, wie man ihn baut)
				if (S->HasField(TEXT("quer")) && S->GetBoolField(TEXT("quer")))
				{
					Dreh.Yaw += 90.f;
				}
				if (S->HasField(TEXT("seite")))
				{
					const float Seite = static_cast<float>(S->GetNumberField(TEXT("seite"))) * 100.f * ((i % 2) ? -1.f : 1.f);
					Ort += FRotator(0.f, Dreh.Yaw + 90.f, 0.f).Vector() * Seite;
				}
				UE_LOG(LogFicsitTD, Log, TEXT("Autotest: %s auf dem Weg, %.0f m vom Kern (Soll %.0f m)"), *Wort, FVector::Dist2D(Ort, Kern->GetActorLocation()) / 100.f, Soll / 100.f);
			}
			const FTransform T(Dreh, Ort);
			if (AFGBuildable* Neu = Bau->BeginSpawnBuildable(Klasse, T))
			{
				Neu->SetBuiltWithRecipe(BauRezept);
				Neu->FinishSpawning(T);
				++Gebaut;   // 0.81: auch Bauten ohne Turmlogik (Tor) zaehlen
				if (ATDTurretBase* Turm = Cast<ATDTurretBase>(Neu))
				{
					if (Munition)
					{
						Turm->SetRecipe(Munition);
					}
					Turm->SetzeLevelTest(Level);
				}
			}
		}
		Ergebnis(TEXT("Pruefstand"), FString::Printf(TEXT("baue %d %s (Level %d)"), Anzahl, *Wort, Level), Gebaut == Anzahl,
			FString::Printf(TEXT("%d gebaut%s"), Gebaut, Munition || !S->HasField(TEXT("rezept")) ? TEXT("") : TEXT(", Rezept nicht gefunden")));
		return true;
	}
	if (S->TryGetStringField(TEXT("aufwerten"), Wort))
	{
		// 0.84 (Tagesauftrag 08.11.: W1-W50 wie ein Spieler): vorhandene Tuerme eines Typs auf ein hoeheres Level und optional eine
		// andere Munition bringen. Vorher konnten Spielplaene nur neu bauen; spaete Wellen liefen mit Level-2-Tuermen und ungenutzten EP.
		const int32 Level = S->HasField(TEXT("level")) ? static_cast<int32>(S->GetNumberField(TEXT("level"))) : 1;
		const FString RezeptName = S->HasField(TEXT("rezept")) ? S->GetStringField(TEXT("rezept")) : FString();
		int32 Gehoben = 0, Umgestellt = 0, Gesamt = 0;
		for (TActorIterator<ATDTurretBase> It(World); It; ++It)
		{
			if (It->GetTurmName() != Wort)
			{
				continue;
			}
			++Gesamt;
			if (It->GetLevel() < Level)
			{
				It->SetzeLevelTest(Level);
				++Gehoben;
			}
			if (!RezeptName.IsEmpty())
			{
				const FString Ordner = FPaths::GetPath(It->GetClass()->GetPathName());
				if (UClass* R = LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("%s/%s.%s_C"), *Ordner, *RezeptName, *RezeptName)))
				{
					if (It->GetCurrentRecipe() != R)
					{
						It->SetRecipe(R);
						++Umgestellt;
					}
				}
			}
		}
		Ergebnis(TEXT("Pruefstand"), FString::Printf(TEXT("aufwerten %s auf Level %d%s"), *Wort, Level, RezeptName.IsEmpty() ? TEXT("") : *(TEXT(", ") + RezeptName)),
			Gesamt > 0, FString::Printf(TEXT("%d von %d gehoben, %d umgestellt"), Gehoben, Gesamt, Umgestellt));
		return true;
	}
	const TSharedPtr<FJsonObject>* Liefern = nullptr;
	if (S->TryGetObjectField(TEXT("versorgung"), Liefern))
	{
		mVersorgung.Reset();
		TArray<FString> Teile;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*Liefern)->Values)
		{
			mVersorgung.Add(V.Key, static_cast<float>(V.Value->AsNumber()));
			Teile.Add(FString::Printf(TEXT("%s %.0f/min"), *V.Key, V.Value->AsNumber()));
		}
		Ergebnis(TEXT("Pruefstand"), TEXT("Versorgung je Turm: ") + FString::Join(Teile, TEXT(", ")), true);
		return true;
	}
	if (S->HasField(TEXT("wellenlauf")))
	{
		// Pruefstand: Wellen nacheinander starten, bis die Zielwelle vorbei ist; Kern tot oder Welle haengt -> Abbruch
		UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		const int32 Ziel = static_cast<int32>(S->GetNumberField(TEXT("wellenlauf")));
		if (mKernGefallen)
		{
			// 0.78.1: nach dem Kernverlust keine weiteren Wellen; der erste Abbruch steht schon im Bericht
			UE_LOG(LogFicsitTD, Log, TEXT("Autotest: Wellenlauf bis %d uebersprungen (Kern schon gefallen)"), Ziel);
			// Gegencheck 15.11.: im Bericht sichtbar machen, sonst liest sich der Plan wie ein Lauf bis zur letzten Welle
			Ergebnis(TEXT("Wellenlauf"), FString::Printf(TEXT("bis Welle %d"), Ziel), true, TEXT("uebersprungen: Kern schon gefallen"));
			return true;
		}
		if (mSchrittUhr <= Dt + 0.01f)
		{
			SetzeZeitraffer(World, S->HasField(TEXT("zeitraffer")) ? FMath::Clamp(static_cast<float>(S->GetNumberField(TEXT("zeitraffer"))), 1.f, 8.f) : 4.f);
			mUhr = 0.f;
		}
		bool bKern = false;
		for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
		{
			bKern |= It->IsTDIntact();
		}
		const double Max = S->HasField(TEXT("max")) ? S->GetNumberField(TEXT("max")) : 400.0;
		if (!Waves || !bKern || mUhr > Max)
		{
			SetzeZeitraffer(World, 1.f);
			// 0.78 (Pruefstand: Mischbasis fiel in W21, Ursache unsichtbar): Schaden und Munitionsmangel je Turmart, lebende Gegner
			TMap<FString, FVector2D> JeArt;
			for (TActorIterator<ATDTurretBase> It(World); It; ++It)
			{
				FVector2D& W = JeArt.FindOrAdd(It->GetTurmName());
				W.X += It->GetBilanz().Schaden;
				W.Y += It->GetBilanz().ZeitHungrig;
			}
			TMap<FString, int32> Lebend;
			if (Waves)
			{
				for (const AFGCreature* C : Waves->GetLivingWaveCreatures())
				{
					FString Name = C->GetClass()->GetName();
					Name.RemoveFromStart(TEXT("Char_"));
					Name.RemoveFromEnd(TEXT("_C"));
					++Lebend.FindOrAdd(Name);
				}
			}
			TArray<FString> Teile;
			for (const TPair<FString, FVector2D>& A : JeArt)
			{
				Teile.Add(FString::Printf(TEXT("%s %.0f Schaden, %.0f s ohne Munition"), *A.Key, A.Value.X, A.Value.Y));
			}
			for (const TPair<FString, int32>& L : Lebend)
			{
				Teile.Add(FString::Printf(TEXT("lebend %s x%d"), *L.Key, L.Value));
			}
			UE_LOG(LogFicsitTD, Log, TEXT("Autotest Niederlage-Diagnose: %s"), *FString::Join(Teile, TEXT("; ")));
			mKernGefallen = !bKern;
			Ergebnis(TEXT("Diagnose"), TEXT("Welle bei Abbruch"), true, FString::Join(Teile, TEXT("; ")));
			Ergebnis(TEXT("Wellenlauf"), FString::Printf(TEXT("bis Welle %d"), Ziel), false,
				!bKern ? FString::Printf(TEXT("Kern zerstört in Welle %d"), Waves ? Waves->GetWelle() : 0) : FString::Printf(TEXT("Welle %d hängt (%.0f s)"), Waves ? Waves->GetWelle() : 0, mUhr));
			return true;
		}
		if (Waves->LaeuftWelle())
		{
			mPauseSpiel = 0.f;
			// 17.11. (P12-K: K1 Lauf 2 brach mit "Welle 8 startet seit 900 s nicht" ab): die Start-Warteuhr unten wurde nur ausserhalb
			// der Welle hochgezaehlt und nie zurueckgesetzt - sie summierte alle Pausen des Laufs. Hier, waehrend die Welle laeuft, nullen.
			mStartWarteUhr = 0.f;
			// 0.78.1 (Pruefstand: ein einzelner Gegner zerstoerte vor 14 feuernden Tuermen den Kern, Art unbekannt):
			// leben 20 s lang hoechstens 3 Gegner, wird jeder mit LP, Abstand, Hirn und Zielstand der Tuerme gemeldet
			const TArray<AFGCreature*> Lebende = Waves->GetLivingWaveCreatures();
			mNachzueglerUhr = Lebende.Num() > 0 && Lebende.Num() <= 3 ? mNachzueglerUhr + Dt : 0.f;
			if (mNachzueglerUhr > 20.f)
			{
				mNachzueglerUhr = 0.f;
				MeldeNachzuegler(World, Lebende, Waves->GetWelle());
			}
			return false;
		}
		if (Waves->GetWelle() >= Ziel)
		{
			SetzeZeitraffer(World, 1.f);
			Ergebnis(TEXT("Wellenlauf"), FString::Printf(TEXT("bis Welle %d"), Ziel), true, FString::Printf(TEXT("%.0f s"), mSchrittUhr));
			return true;
		}
		// 0.11 (Endlos-Wellen): "aufZeit" wartet auf den Countdown statt die naechste Welle vorzuziehen; die Haenger-Uhr
		// misst dann nur die Wellen selbst
		if (S->HasField(TEXT("aufZeit")) && S->GetBoolField(TEXT("aufZeit")))
		{
			// Ohne Countdown (manueller Einsatz) kaeme nie eine Welle: sofort als Fehler melden statt endlos zu warten
			if (Waves->IstManuell())
			{
				SetzeZeitraffer(World, 1.f);
				Ergebnis(TEXT("Wellenlauf"), FString::Printf(TEXT("bis Welle %d"), Ziel), false,
					TEXT("aufZeit, aber der Lauf ist manuell (kein Countdown) - Spielmodus nicht Endlos?"));
				return true;
			}
			mUhr = 0.f;
			return false;
		}
		// 17.11. (P12, Marius: Referenzpause 195 s wie im Handspiel): "pause" (Spielsekunden, Standard Test.Pause = 0) zwischen
		// den Wellen - in der Zeit baut die Fabrik Munition auf wie bei einem Spieler, der zwischen den Wellen baut
		const float Pause = S->HasField(TEXT("pause")) ? static_cast<float>(S->GetNumberField(TEXT("pause"))) : TDBalance::Wert(TEXT("Test.Pause"), 0.f);
		mPauseSpiel += Dt * FMath::Max(1.f, mZeitrafferIst);
		if (mUhr > 2.f && mPauseSpiel >= Pause && Waves->NaechsteWelleJetzt())
		{
			mUhr = 0.f;
		}
		// 16.11. (B19: Fabrik-F1-3K4T stand 12 min vor Welle 1): die Uhr oben wird bei jedem Startversuch zurueckgesetzt -
		// startet die Welle gar nicht, hier nach "max" abbrechen statt bis zum Stillstand des Testskripts zu warten
		mStartWarteUhr = Waves->LaeuftWelle() ? 0.f : mStartWarteUhr + Dt;
		if (mStartWarteUhr > Max + Pause)
		{
			mStartWarteUhr = 0.f;
			SetzeZeitraffer(World, 1.f);
			UE_LOG(LogFicsitTD, Warning, TEXT("Autotest: Welle %d startet seit %.0f s nicht (Lauf %d, Countdown %.0f s)"), Waves->GetWelle() + 1, Max,
				static_cast<int32>(Waves->LaeuftWelle()), Waves->GetCountdown());
			Ergebnis(TEXT("Wellenlauf"), FString::Printf(TEXT("bis Welle %d"), Ziel), false, FString::Printf(TEXT("Welle %d startet nicht"), Waves->GetWelle() + 1));
			return true;
		}
		return false;
	}
	if (S->TryGetStringField(TEXT("speichere"), Wort))
	{
		// 0.77: speichern wie der Mod je Welle (UFGSaveSession), danach kurz warten, bis die Datei geschrieben ist
		if (mSchrittUhr <= Dt + 0.01f)
		{
			UFGSaveSession* Speicher = UFGSaveSession::Get(World);
			if (Speicher)
			{
				Speicher->SaveGame(Wort, FOnSaveGameComplete(), nullptr);
			}
			Ergebnis(TEXT("Speichern"), Wort, Speicher != nullptr);
		}
		return mSchrittUhr >= 5.f;
	}
	if (S->TryGetStringField(TEXT("lade"), Wort))
	{
		mLadenImPlan = true;
		LadeSpielstand(World, Wort);
		return false;   // weiter geht es nach dem Laden (Phase Laedt setzt den naechsten Schritt)
	}
	if (S->HasField(TEXT("kernansicht")))
	{
		// 0.9.6: Ansicht im offenen Kern-Menue umschalten (5 = Fehler melden / Idee), fuer Fotos
		const int32 Ansicht = static_cast<int32>(S->GetNumberField(TEXT("kernansicht")));
		bool bOffen = false;
		for (TObjectIterator<UTDKernTerminal> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
			{
				It->ZeigeAnsicht(Ansicht);
				bOffen = true;
			}
		}
		Ergebnis(TEXT("Kern-Menue"), FString::Printf(TEXT("Ansicht %d"), Ansicht), bOffen, bOffen ? TEXT("umgeschaltet") : TEXT("Kern-Menue nicht offen"));
		return true;
	}
	if (S->HasField(TEXT("spielertreffer")))
	{
		// 0.9.6 P0: Spielertreffer auf den ersten lebenden Wellengegner (Ausloeser = Spieler) - prueft die Zaehlung des Spielerschadens
		const float Menge = S->GetNumberField(TEXT("spielertreffer"));
		APlayerController* PC = World->GetFirstPlayerController();
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		const TArray<AFGCreature*> Lebend = Waves ? Waves->GetLivingWaveCreatures() : TArray<AFGCreature*>();
		if (PC && PC->GetPawn() && Lebend.Num() > 0)
		{
			UGameplayStatics::ApplyDamage(Lebend[0], Menge, PC, PC->GetPawn(), UDamageType::StaticClass());
		}
		const UTDPionier* Pionier = UTDPionier::Get(World);
		Ergebnis(TEXT("Spieler"), FString::Printf(TEXT("Treffer %.0f"), Menge), Pionier && Pionier->GetWellenBilanz().Treffer > 0,
			Pionier ? FString::Printf(TEXT("Wellenbilanz %.0f Schaden, %d Treffer"), Pionier->GetWellenBilanz().Schaden, Pionier->GetWellenBilanz().Treffer) : TEXT("kein Pionier"));
		return true;
	}
	const TSharedPtr<FJsonObject>* TalentWerte = nullptr;
	if (S->TryGetObjectField(TEXT("talente"), TalentWerte))
	{
		// 0.9.8 P3: Talentstufen direkt setzen (z. B. { "Kommandant": 4, "Techniker": 5 }), ohne Punkte
		UTDPionier* Pionier = UTDPionier::Get(World);
		TArray<FString> Gesetzt;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& V : (*TalentWerte)->Values)
		{
			if (Pionier)
			{
				Pionier->SetzeStufe(V.Key, static_cast<int32>(V.Value->AsNumber()));
				Gesetzt.Add(FString::Printf(TEXT("%s %d"), *V.Key, static_cast<int32>(V.Value->AsNumber())));
			}
		}
		Ergebnis(TEXT("Spieler"), TEXT("Talente"), Pionier && Gesetzt.Num() > 0, FString::Join(Gesetzt, TEXT(", ")));
		return true;
	}
	if (S->TryGetStringField(TEXT("gib"), Wort))
	{
		// 0.9.8: Gegenstand ins Spielerinventar (Klassenpfad, "anzahl"), z. B. Eisenplatten fuer die Feldreparatur
		UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, *Wort);
		APlayerController* PC = World->GetFirstPlayerController();
		AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		const int32 Anzahl = S->HasField(TEXT("anzahl")) ? static_cast<int32>(S->GetNumberField(TEXT("anzahl"))) : 1;
		const int32 Rein = Klasse && Spieler && Spieler->GetInventory() ? Spieler->GetInventory()->AddStack(FInventoryStack(Anzahl, Klasse), true) : 0;
		Ergebnis(TEXT("Spieler"), FString::Printf(TEXT("gib %d"), Anzahl), Rein > 0, FString::Printf(TEXT("%d x %s"), Rein, *FPaths::GetBaseFilename(Wort)));
		return true;
	}
	if (S->TryGetStringField(TEXT("schaue"), Wort))
	{
		// 0.10.1: Blick auf einen Turm ("*" = irgendeiner), damit Fotos die Faehigkeits-Effekte zeigen
		ATDTurretBase* Turm = TDTestTurm(World, Wort);
		APlayerController* PC = World->GetFirstPlayerController();
		const FVector Auge = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
		if (Turm && PC)
		{
			PC->SetControlRotation((Turm->GetActorLocation() + FVector(0.f, 0.f, 300.f) - Auge).Rotation());
		}
		Ergebnis(TEXT("Kamera"), FString::Printf(TEXT("schaue %s"), *Wort), Turm && PC,
			Turm ? FString::Printf(TEXT("%s in %.0f m"), *Turm->GetName(), FVector::Dist(Auge, Turm->GetActorLocation()) / 100.f) : FString(TEXT("kein Turm")));
		return true;
	}
	if (S->HasField(TEXT("warteGegner")))
	{
		// 18.11. (Fotos): warten, bis ein Wellengegner hoechstens "warteGegner" m vom Kern entfernt ist (hoechstens "max" s, Standard 120)
		const float Grenze = static_cast<float>(S->GetNumberField(TEXT("warteGegner"))) * 100.f;
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		ATDDefenseCore* Kern = nullptr;
		for (TActorIterator<ATDDefenseCore> It(World); It && !Kern; ++It)
		{
			Kern = *It;
		}
		float Naechster = 1e9f;
		if (Waves && Kern)
		{
			for (const AFGCreature* G : Waves->GetLivingWaveCreatures())
			{
				Naechster = G ? FMath::Min(Naechster, FVector::Dist2D(G->GetActorLocation(), Kern->GetActorLocation())) : Naechster;
			}
		}
		const float Max = S->HasField(TEXT("max")) ? static_cast<float>(S->GetNumberField(TEXT("max"))) : 120.f;
		if (Naechster <= Grenze || mSchrittUhr > Max)
		{
			Ergebnis(TEXT("Warten"), FString::Printf(TEXT("Gegner naeher als %.0f m"), Grenze / 100.f), Naechster <= Grenze,
				FString::Printf(TEXT("naechster %.0f m nach %.0f s"), Naechster / 100.f, mSchrittUhr));
			return true;
		}
		return false;
	}
	if (S->TryGetStringField(TEXT("kamera"), Wort))
	{
		// 0.10.2 (Screenshots fuer Handbuch/Mod-Seite): Kamera des Spiels in dritter Person ("dritte") oder zurueck ("erste").
		// "drehen" (Grad) kreist die Kamera um die Figur, die Figur behaelt ihre Richtung - 180 = Pionier schaut in die Kamera.
		// (Test 15.11.: die Figur zu drehen half nicht, die Kamera der dritten Person folgt der Steuerrichtung.)
		APlayerController* PC = World->GetFirstPlayerController();
		AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		static TWeakObjectPtr<ACameraActor> FrontKamera;
		if (Spieler && Wort == TEXT("front"))
		{
			// 0.10.2 (Test 15.11.): in dritter Person dreht das Spiel die Figur immer mit dem Ruecken zur Kamera. Fuer ein Bild von
			// vorn eine eigene Kamera vor die Figur stellen. "abstand" m (Standard 3.5), "hoehe" m ueber der Huefte (0.4), "seite" Grad (20)
			Spieler->SetCameraMode(ECameraMode::ECM_ThirdPerson);
			const float Abstand = (S->HasField(TEXT("abstand")) ? static_cast<float>(S->GetNumberField(TEXT("abstand"))) : 3.5f) * 100.f;
			const float Hoehe = (S->HasField(TEXT("hoehe")) ? static_cast<float>(S->GetNumberField(TEXT("hoehe"))) : 0.4f) * 100.f;
			const float Seite = S->HasField(TEXT("seite")) ? static_cast<float>(S->GetNumberField(TEXT("seite"))) : 20.f;
			// "drehen": Figur vorher um so viele Grad drehen (180 = Turm hinter der Figur, also im Bildhintergrund)
			const FRotator Figur(0.f, Spieler->GetActorRotation().Yaw + (S->HasField(TEXT("drehen")) ? static_cast<float>(S->GetNumberField(TEXT("drehen"))) : 0.f), 0.f);
			Spieler->SetActorRotation(Figur);
			PC->SetControlRotation(Figur);   // sonst dreht die dritte Person die Figur zur alten Blickrichtung zurueck
			const FVector Brust = Spieler->GetActorLocation() + FVector(0.f, 0.f, 40.f);
			const FVector Ort = Brust + FRotator(0.f, Figur.Yaw + Seite, 0.f).RotateVector(FVector(Abstand, 0.f, 0.f)) + FVector(0.f, 0.f, Hoehe);
			const FRotator Blick = (Brust - Ort).Rotation();
			if (!FrontKamera.IsValid())
			{
				FActorSpawnParameters Param;
				Param.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				FrontKamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Ort, Blick, Param);
			}
			if (FrontKamera.IsValid())
			{
				FrontKamera->SetActorLocationAndRotation(Ort, Blick);
				if (UCameraComponent* Linse = FrontKamera->GetCameraComponent())
				{
					Linse->SetConstraintAspectRatio(false);
					Linse->SetFieldOfView(S->HasField(TEXT("fov")) ? static_cast<float>(S->GetNumberField(TEXT("fov"))) : 60.f);
				}
				PC->SetViewTarget(FrontKamera.Get());
			}
			Ergebnis(TEXT("Kamera"), TEXT("kamera front"), FrontKamera.IsValid(), FrontKamera.IsValid() ? TEXT("eigene Kamera vor der Figur") : TEXT("Kamera nicht erzeugt"));
			return true;
		}
		if (PC && Wort == TEXT("frei"))
		{
			// 18.11. (Marius: Bilder fuer Posts, Basis und anrollende Wellen): freie Kamera wie ein Fotomodus, ohne Figur im Bild.
			// Punkte: "kern", "front1".."front4", "gegner" (Mitte der lebenden Wellengegner), "spieler".
			// "ort" (Standard kern) + "richtung" (Grad, Welt) oder "richtungZu" (Punkt) * "abstand" m + "seite" m quer + "hoehe" m;
			// Blick auf "blick" (Standard ort) + "blickhoehe" m (3), "fov" (70). Bodenschutz: Kamera mind. 2 m ueber dem Gelaende.
			const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
			ATDDefenseCore* Kern = nullptr;
			for (TActorIterator<ATDDefenseCore> It(World); It && !Kern; ++It)
			{
				Kern = *It;
			}
			// Mitte der lebenden Wellengegner (fuer "gegner" und "zwischen")
			FVector GegnerMitte = FVector::ZeroVector;
			int32 GegnerZahl = 0;
			if (Waves)
			{
				for (const AFGCreature* G : Waves->GetLivingWaveCreatures())
				{
					if (G)
					{
						GegnerMitte += G->GetActorLocation();
						++GegnerZahl;
					}
				}
				GegnerMitte = GegnerZahl > 0 ? GegnerMitte / GegnerZahl : GegnerMitte;
			}
			auto Punkt = [&](const FString& Name, FVector& Out) -> bool
			{
				if (Name == TEXT("zwischen") && Kern && GegnerZahl > 0)
				{
					// Punkt auf der Linie Kern -> Gegnermitte, "anteil" 0..1 (Standard 0.5); Test 18.11.: nach 20 s ist die Welle 150 m weit
					const float Anteil = S->HasField(TEXT("anteil")) ? static_cast<float>(S->GetNumberField(TEXT("anteil"))) : 0.5f;
					Out = FMath::Lerp(Kern->GetActorLocation(), GegnerMitte, Anteil);
					return true;
				}
				if (Name == TEXT("kern") && Kern)
				{
					Out = Kern->GetActorLocation();
					return true;
				}
				if (Name == TEXT("spieler") && PC->GetPawn())
				{
					Out = PC->GetPawn()->GetActorLocation();
					return true;
				}
				if (Name.StartsWith(TEXT("front")) && Waves)
				{
					const int32 i = FCString::Atoi(*Name.Mid(5)) - 1;
					if (Waves->GetFronten().IsValidIndex(i))
					{
						Out = Waves->GetFronten()[i].Ort;
						return true;
					}
					return false;
				}
				if (Name == TEXT("gegner") && GegnerZahl > 0)
				{
					Out = GegnerMitte;
					return true;
				}
				return false;
			};
			auto Text = [&](const TCHAR* Feld, const TCHAR* Standard) { FString W; return S->TryGetStringField(Feld, W) ? W : FString(Standard); };
			auto Wert = [&](const TCHAR* Feld, float Standard) { return S->HasField(Feld) ? static_cast<float>(S->GetNumberField(Feld)) : Standard; };
			FVector Ort, Ziel;
			const FString OrtName = Text(TEXT("ort"), TEXT("kern"));
			const FString BlickName = Text(TEXT("blick"), *OrtName);
			bool bOk = Punkt(OrtName, Ort) && Punkt(BlickName, Ziel);
			float Yaw = Wert(TEXT("richtung"), 0.f);
			FVector ZuPunkt;
			const FString ZuName = Text(TEXT("richtungZu"), TEXT(""));
			if (bOk && !ZuName.IsEmpty())
			{
				bOk = Punkt(ZuName, ZuPunkt);
				Yaw = bOk ? (ZuPunkt - Ort).GetSafeNormal2D().Rotation().Yaw + Wert(TEXT("richtung"), 0.f) : Yaw;
			}
			FString Info = TEXT("Punkt fehlt");
			if (bOk)
			{
				const FRotator Dir(0.f, Yaw, 0.f);
				FVector Kamera = Ort + Dir.RotateVector(FVector(Wert(TEXT("abstand"), 30.f) * 100.f, Wert(TEXT("seite"), 0.f) * 100.f, 0.f))
					+ FVector(0.f, 0.f, Wert(TEXT("hoehe"), 15.f) * 100.f);
				FHitResult Boden;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(TDFotoBoden), false);
				if (World->LineTraceSingleByChannel(Boden, Kamera + FVector(0.f, 0.f, 20000.f), Kamera - FVector(0.f, 0.f, 20000.f), ECC_Visibility, Q)
					&& Kamera.Z < Boden.ImpactPoint.Z + 200.f)
				{
					Kamera.Z = Boden.ImpactPoint.Z + 200.f;
				}
				// Sichtschutz (Test 18.11.: Fels und Kernturm verdeckten das Bild): verdeckt etwas die Sicht zum Ziel (Treffer mehr als
				// 15 m vor dem Ziel), die Kamera in 5-m-Schritten anheben, hoechstens um "anheben" m (Standard 40)
				const FVector ZielPunkt = Ziel + FVector(0.f, 0.f, Wert(TEXT("blickhoehe"), 3.f) * 100.f);
				FCollisionQueryParams Sicht(SCENE_QUERY_STAT(TDFotoSicht), true);
				Sicht.AddIgnoredActor(PC->GetPawn());
				int32 Angehoben = 0;
				for (; Angehoben < FMath::RoundToInt(Wert(TEXT("anheben"), 40.f) / 5.f); ++Angehoben)
				{
					FHitResult Treffer;
					if (!World->LineTraceSingleByChannel(Treffer, Kamera, ZielPunkt, ECC_Visibility, Sicht)
						|| FVector::Dist(Treffer.ImpactPoint, ZielPunkt) < 1500.f)
					{
						break;
					}
					Kamera.Z += 500.f;
				}
				const FRotator Blick = (Ziel + FVector(0.f, 0.f, Wert(TEXT("blickhoehe"), 3.f) * 100.f) - Kamera).Rotation();
				static TWeakObjectPtr<ACameraActor> FreiKamera;
				if (!FreiKamera.IsValid())
				{
					FActorSpawnParameters Param;
					Param.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					FreiKamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Kamera, Blick, Param);
				}
				if (FreiKamera.IsValid())
				{
					FreiKamera->SetActorLocationAndRotation(Kamera, Blick);
					if (UCameraComponent* Linse = FreiKamera->GetCameraComponent())
					{
						// 16:9-Bildfeld (Test 18.11.: auf 32:9 gab der 16:9-Zuschnitt nur das halbe Sichtfeld); Seitenbalken schneidet der Zuschnitt weg
						Linse->SetConstraintAspectRatio(true);
						Linse->SetAspectRatio(16.f / 9.f);
						Linse->SetFieldOfView(Wert(TEXT("fov"), 70.f));
					}
					PC->SetViewTarget(FreiKamera.Get());
				}
				bOk = FreiKamera.IsValid();
				Info = FString::Printf(TEXT("%s -> %s, Kamera %.0f m vom Ziel, %.0f m hoch, %d x angehoben, %d Gegner"), *OrtName, *BlickName,
					FVector::Dist(Kamera, Ziel) / 100.f, (Kamera.Z - Ziel.Z) / 100.f, Angehoben, GegnerZahl);
			}
			Ergebnis(TEXT("Kamera"), TEXT("kamera frei"), bOk, Info);
			return true;
		}
		if (Spieler)
		{
			if (PC->GetViewTarget() != Spieler)
			{
				PC->SetViewTarget(Spieler);
			}
			Spieler->SetCameraMode(Wort == TEXT("dritte") ? ECameraMode::ECM_ThirdPerson : ECameraMode::ECM_FirstPerson);
			if (S->HasField(TEXT("drehen")))
			{
				const FRotator Figur(0.f, Spieler->GetActorRotation().Yaw, 0.f);
				FRotator Blick = PC->GetControlRotation();
				Blick.Yaw = Figur.Yaw + static_cast<float>(S->GetNumberField(TEXT("drehen")));
				PC->SetControlRotation(Blick);
				Spieler->SetActorRotation(Figur);
			}
			if (S->HasField(TEXT("neigung")))
			{
				FRotator Blick = PC->GetControlRotation();
				Blick.Pitch = static_cast<float>(S->GetNumberField(TEXT("neigung")));
				PC->SetControlRotation(Blick);
			}
		}
		Ergebnis(TEXT("Kamera"), FString::Printf(TEXT("kamera %s"), *Wort), Spieler != nullptr,
			Spieler ? FString::Printf(TEXT("Modus %d"), static_cast<int32>(Spieler->GetCameraMode())) : FString(TEXT("kein Spieler")));
		return true;
	}
	if (S->TryGetStringField(TEXT("ausruesten"), Wort))
	{
		// 0.10.2: Gegenstand (Klassenpfad) in den Handplatz legen und aktiv schalten, z. B. Gewehr fuer das Schuetzen-Foto
		UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, *Wort);
		APlayerController* PC = World->GetFirstPlayerController();
		AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		UFGInventoryComponentEquipment* Hand = Spieler ? Spieler->GetEquipmentSlot(EEquipmentSlot::ES_ARMS) : nullptr;
		bool bOk = false;
		bool bErsetzt = false;
		if (Klasse && Hand && Hand->GetSizeLinear() > 0)
		{
			// freier Platz, sonst der aktive Platz (Test 15.11.: im Spielstand waren alle Handplaetze belegt; nur Testkopie)
			int32 Platz = INDEX_NONE;
			for (int32 i = 0; i < Hand->GetSizeLinear() && Platz == INDEX_NONE; ++i)
			{
				if (Hand->IsIndexEmpty(i))
				{
					Platz = i;
				}
			}
			if (Platz == INDEX_NONE)
			{
				Platz = FMath::Clamp(Hand->GetActiveIndex(), 0, Hand->GetSizeLinear() - 1);
				Hand->RemoveAllFromIndex(Platz);
				bErsetzt = true;
			}
			bOk = Hand->AddStackToIndex(Platz, FInventoryStack(1, Klasse), true) > 0;
			Hand->SetActiveIndex(Platz);
		}
		Ergebnis(TEXT("Spieler"), FString::Printf(TEXT("ausruesten %s"), *FPaths::GetBaseFilename(Wort)), bOk,
			!Klasse ? FString(TEXT("Klasse fehlt")) : !Hand ? FString(TEXT("kein Handplatz")) : !bOk ? FString(TEXT("nicht eingelegt"))
			: bErsetzt ? FString(TEXT("in der Hand (aktiven Platz ersetzt)")) : FString(TEXT("in der Hand")));
		return true;
	}
	if (S->HasField(TEXT("zahle")))
	{
		// 0.10.0: Coupons ausgeben wie Turmlevel oder Talente (ZahleEP) - prueft, dass Coupon-Items abgebucht werden
		UTDEinsatzSubsystem* Einsatz = World->GetSubsystem<UTDEinsatzSubsystem>();
		const int32 Preis = static_cast<int32>(S->GetNumberField(TEXT("zahle")));
		const int32 Vorher = Einsatz ? Einsatz->Konto() : 0;
		const FString Fehler = Einsatz ? Einsatz->ZahleEP(Preis, TEXT("Autotest"), true) : FString(TEXT("kein Einsatz"));
		const int32 Nachher = Einsatz ? Einsatz->Konto() : 0;
		Ergebnis(TEXT("Coupons"), FString::Printf(TEXT("zahle %d"), Preis), Fehler.IsEmpty() && Vorher - Nachher == Preis,
			FString::Printf(TEXT("Konto %d -> %d%s"), Vorher, Nachher, Fehler.IsEmpty() ? TEXT("") : *(TEXT(", ") + Fehler)));
		return true;
	}
	if (S->TryGetStringField(TEXT("uebersteuere"), Wort))
	{
		// 0.9.8 P4: Uebersteuern wie der Knopf im Turmfenster ("*" = irgendein Turm)
		ATDTurretBase* Turm = TDTestTurm(World, Wort);
		UTDPionier* Pionier = UTDPionier::Get(World);
		const FString Antwort = Pionier && Turm ? Pionier->Uebersteuere(Turm) : FString(TEXT("kein Turm"));
		Ergebnis(TEXT("Spieler"), TEXT("Uebersteuern"), Turm && Turm->UebersteuernRest() > 0.f, Antwort);
		return true;
	}
	if (S->HasField(TEXT("schadebau")))
	{
		// 0.9.8 P4: Turm beschaedigen (Anteil der Max-LP), fuer Feldreparatur und Bollwerk
		const float Anteil = S->GetNumberField(TEXT("schadebau"));
		ATDTurretBase* Turm = TDTestTurm(World, TEXT("*"));
		if (Turm)
		{
			Turm->ApplyTDDamage(Anteil * Turm->GetTDMaxHealth(), nullptr);
		}
		Ergebnis(TEXT("Bau"), FString::Printf(TEXT("Schaden %.0f %%"), Anteil * 100.f), Turm && Turm->IsTDIntact(),
			Turm ? FString::Printf(TEXT("%s %.0f / %.0f LP"), *Turm->GetName(), Turm->GetTDHealth(), Turm->GetTDMaxHealth()) : TEXT("kein Turm"));
		return true;
	}
	if (S->HasField(TEXT("markiere")))
	{
		// 0.9.7 P2: lebende Wellengegner markieren wie ein Spielertreffer (0 = alle) - prueft Turm-Zielwahl, Bonus und Assists
		const int32 Anzahl = static_cast<int32>(S->GetNumberField(TEXT("markiere")));
		UTDPionier* Pionier = UTDPionier::Get(World);
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		int32 Markiert = 0;
		for (AFGCreature* C : Waves ? Waves->GetLivingWaveCreatures() : TArray<AFGCreature*>())
		{
			if (Pionier && (Anzahl <= 0 || Markiert < Anzahl))
			{
				Pionier->Markiere(C);
				++Markiert;
			}
		}
		Ergebnis(TEXT("Spieler"), FString::Printf(TEXT("markiere %d"), Anzahl), Markiert > 0, FString::Printf(TEXT("%d Gegner markiert"), Markiert));
		return true;
	}
	if (S->TryGetStringField(TEXT("pionier"), Wort))
	{
		// 0.9.7 P2: Wert der laufenden Wellenbilanz > 0 (Markiert, MarkBonus, Assists, Treffer, Schaden)
		const UTDPionier* Pionier = UTDPionier::Get(World);
		const UTDPionier::FSpielerBilanz B = Pionier ? Pionier->GetWellenBilanz() : UTDPionier::FSpielerBilanz();
		const TMap<FString, float> Werte = { { TEXT("Markiert"), float(B.Markiert) }, { TEXT("MarkBonus"), B.MarkBonus }, { TEXT("Assists"), float(B.Assists) },
			{ TEXT("Treffer"), float(B.Treffer) }, { TEXT("Repariert"), B.Repariert }, { TEXT("Uebersteuert"), float(B.Uebersteuert) }, { TEXT("Gerettet"), float(B.Gerettet) },
			{ TEXT("Nachschub"), float(B.Nachschub) }, { TEXT("JagdSchaden"), B.JagdSchaden }, { TEXT("Sammelbefehle"), float(B.Sammelbefehle) } };
		const float* Gefunden = Werte.Find(Wort);
		const float Wert = Gefunden ? *Gefunden : B.Schaden;
		Ergebnis(TEXT("Spieler"), FString::Printf(TEXT("Bilanz %s > 0"), *Wort), Wert > 0.f,
			FString::Printf(TEXT("%s = %.0f (Markiert %d, Markierungsbonus %.0f, Assists %d, Treffer %d, Schaden %.0f, repariert %.0f, gerettet %d)"), *Wort, Wert, B.Markiert, B.MarkBonus, B.Assists, B.Treffer, B.Schaden, B.Repariert, B.Gerettet));
		return true;
	}
	if (S->HasField(TEXT("kernfaellt")))
	{
		// 0.9.6: Niederlage nachstellen (Kern-Menue nach dem Ende pruefen)
		bool bGefallen = false;
		for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
		{
			It->ApplyTDDamage(It->GetTDHealth() + 1.f, nullptr);
			bGefallen = !It->IsTDIntact();
		}
		Ergebnis(TEXT("Kern"), TEXT("faellt"), bGefallen);
		return true;
	}
	if (S->HasField(TEXT("endeknopf")))
	{
		// 0.9.6: "neustart" oder "beenden" wie die Knoepfe im Kern-Menue nach Sieg oder Niederlage
		UTDEinsatzSubsystem* Einsatz = World->GetSubsystem<UTDEinsatzSubsystem>();
		const FString Knopf = S->GetStringField(TEXT("endeknopf"));
		const int32 Vorher = Einsatz ? Einsatz->LaufEnde() : -1;
		APlayerController* PC = World->GetFirstPlayerController();
		const FString Antwort = !Einsatz ? FString(TEXT("kein Einsatz")) : Knopf == TEXT("neustart") ? Einsatz->StarteNeu(PC ? PC->GetPawn() : nullptr) : Einsatz->BeendeNachEnde();
		Ergebnis(TEXT("Ende"), FString::Printf(TEXT("%s (Ende vorher %d)"), *Knopf, Vorher), Vorher != 0, Antwort);
		return true;
	}
	if (S->TryGetStringField(TEXT("erforsche"), Wort))
	{
		// 0.75: Forschung direkt geben wie das MAM (Spiel-Delegate laeuft, stille Turm-Rezepte kommen mit); MAM-Sperren gelten hier nicht
		const FTDForschungKnoten* K = TDForschung::Finde(Wort);
		UClass* Klasse = K ? LoadClass<UFGSchematic>(nullptr, *K->Pfad) : nullptr;
		AFGSchematicManager* Schemata = AFGSchematicManager::Get(World);
		APlayerController* PC = World->GetFirstPlayerController();
		// 17.11. (P11, Marius: Forschung im Pruefstand mit echten Festplatten): "echt": true bezahlt wie das MAM aus dem Inventar
		// und beachtet Welle, Voraussetzungen und Gabelungen; ohne "echt" wie bisher direkt geschenkt
		if (K && Klasse && Schemata && S->HasField(TEXT("echt")) && S->GetBoolField(TEXT("echt")))
		{
			FString Status;
			const ETDForschungZustand Zustand = TDForschung::Zustand(World, *K);
			TDForschung::StatusTitel(World, *K, Status);
			AFGCharacterPlayer* Figur = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
			UFGInventoryComponent* Inventar = Figur ? Figur->GetInventory() : nullptr;
			UClass* Platte = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Environment/CrashSites/Desc_HardDrive.Desc_HardDrive_C"));
			const int32 Hat = (Inventar && Platte) ? Inventar->GetNumItems(Platte) : 0;
			if (Zustand != ETDForschungZustand::Moeglich || Hat < K->FP)
			{
				Ergebnis(TEXT("Forschung"), Wort + TEXT(" (echt)"), false, FString::Printf(TEXT("%s, Festplatten %d / %d"), *Status, Hat, K->FP));
				return true;
			}
			if (K->FP > 0)
			{
				Inventar->Remove(Platte, K->FP);
			}
			Schemata->GiveAccessToSchematic(Klasse, Figur);
			Ergebnis(TEXT("Forschung"), Wort + TEXT(" (echt)"), Schemata->IsSchematicPurchased(Klasse), FString::Printf(TEXT("%d Festplatten bezahlt, %d uebrig"), K->FP, Hat - K->FP));
			return true;
		}
		if (Klasse && Schemata)
		{
			Schemata->GiveAccessToSchematic(Klasse, PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr);
		}
		Ergebnis(TEXT("Forschung"), Wort, Klasse && Schemata && Schemata->IsSchematicPurchased(Klasse), Klasse ? TEXT("gegeben") : TEXT("Knoten unbekannt"));
		return true;
	}
	if (S->TryGetStringField(TEXT("uebertakte"), Wort))
	{
		// 0.75: Energiesplitter in den ersten Turm des Typs legen und den Takt hochsetzen (wie im Maschinenfenster)
		ATDTurretBase* Turm = TDTestTurm(World, Wort);
		const int32 Anzahl = S->HasField(TEXT("splitter")) ? static_cast<int32>(S->GetNumberField(TEXT("splitter"))) : 1;
		UClass* Splitter = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Environment/Crystal/Desc_CrystalShard.Desc_CrystalShard_C"));
		UFGInventoryComponent* Platz = Turm ? Turm->GetPotentialInventory() : nullptr;
		if (!Turm || !Platz || !Splitter)
		{
			Ergebnis(TEXT("Uebertakten"), Wort, false, !Turm ? TEXT("kein Turm") : !Platz ? TEXT("keine Splitter-Plaetze") : TEXT("Splitter-Item fehlt"));
			return true;
		}
		const int32 Gelegt = Platz->AddStack(FInventoryStack(Anzahl, Splitter), true);
		const float Ziel = FMath::Min(Turm->GetCurrentMaxPotential(), 1.f + 0.5f * Gelegt);
		Turm->SetPendingPotential(Ziel);
		// Test 0.75: 0 Splitter angenommen -> Groesse, Erlaubnis und Filter des Inventars mitschreiben
		Ergebnis(TEXT("Uebertakten"), Wort, Gelegt == Anzahl, FString::Printf(TEXT("%d Splitter, Ziel %.0f %%, hoechstens %.0f %% | Inventar %d Plaetze, Splitter erlaubt %d, an Platz 0 %d"),
			Gelegt, Ziel * 100.f, Turm->GetCurrentMaxPotential() * 100.f, Platz->GetSizeLinear(), Platz->IsItemAllowed(Splitter) ? 1 : 0, Platz->IsItemAllowed(Splitter, 0) ? 1 : 0));
		return true;
	}
	if (S->TryGetStringField(TEXT("oeffne"), Wort))
	{
		// 0.75 (A4): Fenster fuer Fotos oeffnen - "MAM" oder ein Turmtyp (Maschinenfenster mit Turmwerten)
		APlayerController* PC = World->GetFirstPlayerController();
		AFGHUD* HUD = PC ? Cast<AFGHUD>(PC->GetHUD()) : nullptr;
		AFGBuildable* Ziel = nullptr;
		if (Wort == TEXT("MAM"))
		{
			for (TActorIterator<AFGBuildableMAM> It(World); It && !Ziel; ++It)
			{
				Ziel = *It;
			}
		}
		else if (Wort == TEXT("Kern"))
		{
			// 0.9.6: Kern-Menue (Laden mit Kategorien) fotografieren
			for (TActorIterator<ATDDefenseCore> It(World); It && !Ziel; ++It)
			{
				Ziel = *It;
			}
		}
		else
		{
			Ziel = TDTestTurm(World, Wort);
		}
		// GetInteractWidgetClass ist protected -> Feld mInteractWidgetSoftClass ueber die Reflexion lesen
		TSubclassOf<UFGInteractWidget> Fenster = nullptr;
		if (Ziel)
		{
			if (const FSoftClassProperty* Feld = FindFProperty<FSoftClassProperty>(AFGBuildable::StaticClass(), TEXT("mInteractWidgetSoftClass")))
			{
				Fenster = Cast<UClass>(Feld->GetPropertyValue_InContainer(Ziel).LoadSynchronous());
			}
		}
		if (HUD && Ziel && Fenster)
		{
			HUD->OpenInteractUI(Fenster, Ziel);
		}
		Ergebnis(TEXT("Fenster"), Wort, HUD && Ziel && Fenster, !Ziel ? TEXT("nicht gebaut") : !Fenster ? TEXT("kein Fenster") : TEXT("geoeffnet"));
		return true;
	}
	const TArray<TSharedPtr<FJsonValue>>* Ort = nullptr;
	if (S->TryGetArrayField(TEXT("klick"), Ort) && Ort->Num() == 2)
	{
		// 0.75 (A4): Mausklick an einer Stelle des Spielfensters (Anteil 0..1 von links oben), z. B. einen MAM-Baum waehlen.
		// Bewegen, druecken und erst nach 0,3 s loslassen (Test 0.75: alles im selben Bild hob den Eintrag nur hervor)
		TSharedPtr<SWindow> Fenster = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
		if (!Fenster.IsValid() || !FSlateApplication::IsInitialized())
		{
			Ergebnis(TEXT("Klick"), TEXT("kein Spielfenster"), false);
			return true;
		}
		const FVector2D Pos = Fenster->GetPositionInScreen() + Fenster->GetSizeInScreen() * FVector2D((*Ort)[0]->AsNumber(), (*Ort)[1]->AsNumber());
		FSlateApplication& App = FSlateApplication::Get();
		if (mSchrittUhr <= Dt + 0.01f)
		{
			App.SetCursorPos(Pos);
			App.ProcessMouseMoveEvent(FPointerEvent(0, Pos, Pos - FVector2D(1.f, 1.f), TSet<FKey>(), EKeys::Invalid, 0, FModifierKeysState()));
			return false;
		}
		if (!mKlickUnten)
		{
			App.ProcessMouseButtonDownEvent(nullptr, FPointerEvent(0, Pos, Pos, TSet<FKey>({ EKeys::LeftMouseButton }), EKeys::LeftMouseButton, 0, FModifierKeysState()));
			mKlickUnten = true;
			return false;
		}
		if (mSchrittUhr < 1.2f)
		{
			return false;
		}
		App.ProcessMouseButtonUpEvent(FPointerEvent(0, Pos, Pos, TSet<FKey>(), EKeys::LeftMouseButton, 0, FModifierKeysState()));
		mKlickUnten = false;
		Ergebnis(TEXT("Klick"), FString::Printf(TEXT("%.3f / %.3f"), (*Ort)[0]->AsNumber(), (*Ort)[1]->AsNumber()), true);
		return true;
	}
	if (S->HasField(TEXT("schliesse")))
	{
		APlayerController* PC = World->GetFirstPlayerController();
		AFGHUD* HUD = PC ? Cast<AFGHUD>(PC->GetHUD()) : nullptr;
		if (HUD && HUD->GetGameUI())
		{
			HUD->GetGameUI()->PopAllWidgets();
		}
		return true;
	}
	if (S->TryGetStringField(TEXT("foto"), Wort))
	{
		// ein Bild je Schritt; die Aufnahme kommt mit dem naechsten gezeichneten Bild, darum kurz warten
		if (mSchrittUhr <= Dt + 0.01f)
		{
			const FString Pfad = FPaths::Combine(Ordner(), TEXT("Autotest"), TEXT("Bilder"), Wort + TEXT(".png"));
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Pfad), true);
			// mit Oberflaeche (Test 0.75: ohne fehlten die Fenster); 0.10.2: "ohneHUD": true fuer Screenshots fuers Handbuch
			const bool bOhneHUD = S->HasField(TEXT("ohneHUD")) && S->GetBoolField(TEXT("ohneHUD"));
			FScreenshotRequest::RequestScreenshot(Pfad, !bOhneHUD, false);
			Ergebnis(TEXT("Foto"), Wort, true, Pfad);
		}
		return mSchrittUhr >= 2.f;
	}
	Ergebnis(TEXT("Schritt"), TEXT("unbekannter Schritt"), false);
	return true;
}

bool UTDAutotest::Pruefe(UWorld* World, const TSharedPtr<FJsonObject>& S, FString& OutIst)
{
	const FString Name = S->GetStringField(TEXT("pruefe"));
	if (Name == TEXT("TestKnoepfe"))
	{
		// 16.11. (Spielersicht): sichtbare Entwickler-Knoepfe im offenen Kern-Menue; "soll" = erwartete Liste (Standard: keine)
		const FString Soll = S->HasField(TEXT("soll")) ? S->GetStringField(TEXT("soll")) : FString();
		bool bOffen = false;
		TArray<FString> Ist;
		for (TObjectIterator<UTDKernTerminal> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetCachedWidget().IsValid())
			{
				Ist = It->SichtbareTestKnoepfe();
				bOffen = true;
			}
		}
		OutIst = !bOffen ? FString(TEXT("Kern-Menue nicht offen")) : Ist.Num() ? FString::Join(Ist, TEXT(", ")) : FString(TEXT("keine"));
		return bOffen && FString::Join(Ist, TEXT(", ")) == Soll;
	}
	// 17.11. (P16/A12): darf der erste Turm des Typs abgerissen werden? "turm" (Standard Bolzenturm), "soll" true/false
	if (Name == TEXT("Abriss"))
	{
		ATDTurretBase* Turm = TDTestTurm(World, S->HasField(TEXT("turm")) ? S->GetStringField(TEXT("turm")) : TEXT("Bolzenturm"));
		if (!Turm)
		{
			OutIst = TEXT("kein Turm");
			return false;
		}
		const bool bDarf = IFGDismantleInterface::Execute_CanDismantle(Turm);
		TArray<TSubclassOf<UFGConstructDisqualifier>> Gruende;
		IFGDismantleInterface::Execute_GetDismantleDisqualifiers(Turm, Gruende, TArray<AActor*>());
		OutIst = FString::Printf(TEXT("%s, %d Gruende"), bDarf ? TEXT("abreissbar") : TEXT("gesperrt"), Gruende.Num());
		return !S->HasField(TEXT("soll")) || S->GetBoolField(TEXT("soll")) == bDarf;
	}
	// 17.11. (P16/A13): Welle, fuer die "ab Welle" freigibt; "soll" = erwartete Zahl
	if (Name == TEXT("FreigabeWelle"))
	{
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		const int32 Ist = Waves ? Waves->GetFreigabeWelle() : -1;
		OutIst = FString::Printf(TEXT("%d (Welle %d, laeuft %d)"), Ist, Waves ? Waves->GetWelle() : -1, Waves && Waves->LaeuftWelle() ? 1 : 0);
		return !S->HasField(TEXT("soll")) || int32(S->GetNumberField(TEXT("soll"))) == Ist;
	}
	if (Name == TEXT("Version"))
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FicsitTD"));
		OutIst = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : TEXT("nicht geladen");
		return !S->HasField(TEXT("soll")) || OutIst == S->GetStringField(TEXT("soll"));
	}
	if (Name == TEXT("Optik"))
	{
		// Optik-Durchgang 16.11.2026 (P3): je Turm/Tor kein Original-Strommast oder -Statusmast als Teil, keine Nebelflaeche des
		// Spiel-Vorbilds im Umkreis von 6 m, Stromanschluss im eigenen Klarraum (sonst haengt das Kabel ausserhalb des Sockels)
		TArray<FString> Fehler;
		int32 Bauten = 0;
		TArray<const UInstancedStaticMeshComponent*> Nebel;
		for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetStaticMesh() && It->GetStaticMesh()->GetName().Contains(TEXT("FogPlane")))
			{
				Nebel.Add(*It);
			}
		}
		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			if (!Cast<ATDTurretBase>(*It) && !Cast<ATDGate>(*It))
			{
				continue;
			}
			++Bauten;
			const FString Wer = It->GetClass()->GetName().Replace(TEXT("Build_TD_"), TEXT("")).Replace(TEXT("_C"), TEXT(""));
			TInlineComponentArray<UStaticMeshComponent*> Teile(*It);
			for (const UStaticMeshComponent* T : Teile)
			{
				const FString M = T->GetStaticMesh() ? T->GetStaticMesh()->GetName() : FString();
				if (M == TEXT("SM_PowerPoleWall_MK1") || M == TEXT("SM_ProductionLight_01"))
				{
					Fehler.Add(FString::Printf(TEXT("%s: %s"), *Wer, *M));
				}
			}
			TArray<FFGClearanceData> Klar;
			IFGClearanceInterface::Execute_GetClearanceData(*It, Klar);
			const FBox Box = Klar.Num() ? Klar[0].ClearanceBox.ExpandBy(FVector(15.f, 15.f, 0.f)) : FBox(ForceInit);
			TInlineComponentArray<UFGPowerConnectionComponent*> Strom(*It);
			for (const UFGPowerConnectionComponent* P : Strom)
			{
				const FVector L = It->GetActorTransform().InverseTransformPosition(P->GetComponentLocation());
				if (Box.IsValid && (L.X < Box.Min.X || L.X > Box.Max.X || L.Y < Box.Min.Y || L.Y > Box.Max.Y))
				{
					Fehler.Add(FString::Printf(TEXT("%s: Stromanschluss %s ausserhalb des Klarraums"), *Wer, *L.ToCompactString()));
				}
			}
			for (const UInstancedStaticMeshComponent* N : Nebel)
			{
				for (int32 i = 0; i < N->GetInstanceCount(); ++i)
				{
					FTransform T;
					if (N->GetInstanceTransform(i, T, true) && FVector::Dist(T.GetLocation(), It->GetActorLocation()) < 600.f)
					{
						Fehler.Add(FString::Printf(TEXT("%s: Nebelflaeche"), *Wer));
						break;
					}
				}
			}
		}
		OutIst = FString::Printf(TEXT("%d Bauten, %d Befunde%s%s"), Bauten, Fehler.Num(), Fehler.Num() ? TEXT(": ") : TEXT(""),
			*FString::Join(TArray<FString>(Fehler.GetData(), FMath::Min(Fehler.Num(), 6)), TEXT("; ")));
		return Bauten > 0 && Fehler.Num() == 0;
	}
	if (Name == TEXT("KernLebt"))
	{
		for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
		{
			const float Prozent = 100.f * It->GetTDHealth() / FMath::Max(1.f, It->GetTDMaxHealth());
			OutIst = FString::Printf(TEXT("%.0f %%"), Prozent);
			return Prozent >= (S->HasField(TEXT("min")) ? S->GetNumberField(TEXT("min")) : 1.0);
		}
		OutIst = TEXT("kein Kern");
		return false;
	}
	if (Name == TEXT("WelleAbgewehrt") || Name == TEXT("SchadenJeTurmart"))
	{
		const UTDWellenStatistik* Statistik = UTDWellenStatistik::Get(World);
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		if (!Statistik || Statistik->Verlauf().Num() == 0)
		{
			OutIst = TEXT("keine Wellenstatistik");
			return false;
		}
		const FTDWellenStat& W = Statistik->Verlauf().Last();
		if (Name == TEXT("WelleAbgewehrt"))
		{
			OutIst = FString::Printf(TEXT("Welle %d, %d Gegner, %.0f s, Kern %.0f -> %.0f %%"), W.Welle, W.Gegner, W.Dauer, W.KernStart, W.KernEnde);
			return W.KernEnde > 0.f && (!Waves || !Waves->LaeuftWelle());
		}
		TArray<FString> Teile;
		bool bAlle = W.Tuerme.Num() > 0;
		for (const FTDTurmStat& T : W.Tuerme)
		{
			Teile.Add(FString::Printf(TEXT("%s x%d: %.0f"), *T.Art, T.Anzahl, T.Schaden));
			bAlle &= T.Anzahl == 0 || T.Schaden > 0.f;
		}
		OutIst = Teile.Num() ? FString::Join(Teile, TEXT(", ")) : TEXT("keine Tuerme");
		return bAlle;
	}
	if (Name == TEXT("Forschung"))
	{
		const FString Knoten = S->GetStringField(TEXT("knoten"));
		const bool bHat = TDForschung::Hat(World, *Knoten);
		const bool bSoll = !S->HasField(TEXT("erwartet")) || S->GetBoolField(TEXT("erwartet"));
		OutIst = FString::Printf(TEXT("%s %s"), *Knoten, bHat ? TEXT("erforscht") : TEXT("nicht erforscht"));
		return bHat == bSoll;
	}
	if (Name == TEXT("TurmWert"))
	{
		// 0.75: Messwert am ersten Turm eines Typs gegen Soll (Toleranz absolut, Standard 0,01)
		const FString Typ = S->GetStringField(TEXT("turm"));
		const FString Wert = S->GetStringField(TEXT("wert"));
		const ATDTurretBase* Turm = TDTestTurm(World, Typ);
		if (!Turm)
		{
			OutIst = FString::Printf(TEXT("kein %s gebaut"), *Typ);
			return false;
		}
		const float Ist = Turm->TestWert(Wert);
		const double Soll = S->GetNumberField(TEXT("soll"));
		const double Tol = S->HasField(TEXT("toleranz")) ? S->GetNumberField(TEXT("toleranz")) : 0.01;
		if (S->HasField(TEXT("min")))
		{
			// 0.83: Wirkung der Bauten nachweisen (mindestens so viel)
			const double Min = S->GetNumberField(TEXT("min"));
			OutIst = FString::Printf(TEXT("%s %s = %.3f (mindestens %.3f)"), *Typ, *Wert, Ist, Min);
			return Ist >= Min;
		}
		OutIst = FString::Printf(TEXT("%s %s = %.3f (Soll %.3f)"), *Typ, *Wert, Ist, Soll);
		return FMath::Abs(Ist - Soll) <= Tol;
	}
	if (Name == TEXT("Kombo"))
	{
		// 0.9.1: Kombination hat mindestens "min"-mal ausgeloest (Zaehler der Sitzung)
		const FString K = S->GetStringField(TEXT("kombo"));
		const int32 Ist = TDKombo::Anzahl(*K);
		const int32 Min = S->HasField(TEXT("min")) ? int32(S->GetNumberField(TEXT("min"))) : 1;
		OutIst = FString::Printf(TEXT("%s %d-mal (mindestens %d)"), *K, Ist, Min);
		return Ist >= Min;
	}
	if (Name == TEXT("Bausperre"))
	{
		// 0.9.1 (Review 09.11.): Sperrentscheidung des Einsatzes fuer eine Bauklasse an einem Ort ("kern" = 20 m neben dem Kern,
		// "fern" = 2 km entfernt). soll: "" (erlaubt), "Welle", "Sink" oder "Kontingent". Prueft die Logik von PruefeHologramm.
		UClass* Klasse = LoadClass<UObject>(nullptr, *S->GetStringField(TEXT("klasse")));
		UTDEinsatzSubsystem* Einsatz = World->GetSubsystem<UTDEinsatzSubsystem>();
		const ATDDefenseCore* Kern = nullptr;
		for (TActorIterator<ATDDefenseCore> It(World); It; ++It) { Kern = *It; break; }
		if (!Klasse || !Einsatz || !Kern)
		{
			OutIst = !Klasse ? TEXT("Klasse nicht gefunden") : !Einsatz ? TEXT("kein Einsatz-Subsystem") : TEXT("kein Kern");
			return false;
		}
		const bool bFern = S->HasField(TEXT("ort")) && S->GetStringField(TEXT("ort")) == TEXT("fern");
		const FVector Ort = Kern->GetActorLocation() + FVector(bFern ? 200000.f : 2000.f, 0.f, 0.f);
		const int32 Anzahl = S->HasField(TEXT("anzahl")) ? int32(S->GetNumberField(TEXT("anzahl"))) : 1;
		const FString Ist = Einsatz->SperrGrund(Klasse, Ort, Anzahl);
		const FString Soll = S->HasField(TEXT("soll")) ? S->GetStringField(TEXT("soll")) : FString();
		OutIst = FString::Printf(TEXT("%s %s: %s"), *Klasse->GetName(), bFern ? TEXT("fern") : TEXT("am Kern"), Ist.IsEmpty() ? TEXT("erlaubt") : *Ist);
		return Ist == Soll;
	}
	if (Name == TEXT("Inventar"))
	{
		// 0.10.0: Anzahl eines Items im Spielerinventar gegen "min"/"max" (Klassenpfad in "item")
		UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, *S->GetStringField(TEXT("item")));
		const APlayerController* PC = World->GetFirstPlayerController();
		const AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		const int32 Anzahl = Klasse && Spieler && Spieler->GetInventory() ? Spieler->GetInventory()->GetNumItems(Klasse) : -1;
		OutIst = Klasse ? FString::Printf(TEXT("%d x %s"), Anzahl, *Klasse->GetName()) : FString(TEXT("Item nicht gefunden"));
		return Klasse && Anzahl >= (S->HasField(TEXT("min")) ? int32(S->GetNumberField(TEXT("min"))) : 0)
			&& Anzahl <= (S->HasField(TEXT("max")) ? int32(S->GetNumberField(TEXT("max"))) : MAX_int32);
	}
	if (Name == TEXT("Konto"))
	{
		// 0.10.0: Coupon-Konto = Items im Inventar + am Kern wartende; "min" und optional "nurItems" (alles im Inventar)
		const UTDEinsatzSubsystem* Einsatz = World->GetSubsystem<UTDEinsatzSubsystem>();
		const ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
		const int32 Konto = Einsatz ? Einsatz->Konto() : -1;
		const int32 Wartend = Kern ? Kern->mEinsatzPunkte : -1;
		OutIst = FString::Printf(TEXT("Konto %d, davon %d am Kern wartend"), Konto, Wartend);
		const bool bNurItems = S->HasField(TEXT("nurItems")) && S->GetBoolField(TEXT("nurItems"));
		return Konto >= (S->HasField(TEXT("min")) ? int32(S->GetNumberField(TEXT("min"))) : 0) && (!bNurItems || Wartend == 0);
	}
	if (Name == TEXT("MAMKosten"))
	{
		// 0.10.0: Kosten eines FicsitTD-Knotens bestehen aus Festplatten (keine FICSIT-Kampfdaten mehr)
		const FTDForschungKnoten* K = TDForschung::Finde(S->GetStringField(TEXT("knoten")));
		UClass* Klasse = K ? LoadClass<UFGSchematic>(nullptr, *K->Pfad) : nullptr;
		if (!Klasse)
		{
			OutIst = TEXT("Knoten unbekannt");
			return false;
		}
		TArray<FString> Teile;
		bool bFestplatte = false, bAlt = false;
		for (const FItemAmount& Kosten : UFGSchematic::GetCost(Klasse))
		{
			Teile.Add(FString::Printf(TEXT("%d x %s"), Kosten.Amount, Kosten.ItemClass ? *Kosten.ItemClass->GetName() : TEXT("?")));
			bFestplatte |= Kosten.ItemClass == UTDEinsatzSubsystem::FPKlasse();
			bAlt |= Kosten.ItemClass && Kosten.ItemClass->GetName().Contains(TEXT("Kampfdaten"));
		}
		OutIst = FString::Join(Teile, TEXT(", "));
		// 0.12-dev: "coupons": true erwartet im Freien Spiel mit Abwehr Coupons statt Festplatten
		if (S->HasField(TEXT("coupons")) && S->GetBoolField(TEXT("coupons")))
		{
			bool bCoupon = false;
			for (const FItemAmount& Kosten : UFGSchematic::GetCost(Klasse))
			{
				bCoupon |= Kosten.ItemClass == UTDEinsatzSubsystem::CouponKlasse();
			}
			return bCoupon && !bFestplatte;
		}
		return bFestplatte && !bAlt;
	}
	if (Name == TEXT("ForschungStartbar"))
	{
		// 0.10.0: darf das MAM diese Forschung beginnen? (Hook CanResearchBeInitiated mit allen FicsitTD-Sperren)
		UClass* Klasse = LoadClass<UFGSchematic>(nullptr, *S->GetStringField(TEXT("schematic")));
		const AFGResearchManager* Forschung = AFGResearchManager::Get(World);
		const bool bSoll = !S->HasField(TEXT("erwartet")) || S->GetBoolField(TEXT("erwartet"));
		const bool bIst = Klasse && Forschung && Forschung->CanResearchBeInitiated(Klasse);
		OutIst = !Klasse ? FString(TEXT("Schematic nicht gefunden")) : FString::Printf(TEXT("%s %s"), *Klasse->GetName(), bIst ? TEXT("startbar") : TEXT("gesperrt"));
		return Klasse && bIst == bSoll;
	}
	if (Name == TEXT("BaumFrei"))
	{
		// 0.11 (Modus Aus): ist ein TD-Forschungsbaum freigeschaltet? "baum" = Industrie|Ballistik|Elementar|Festung, "erwartet"
		// Vorher UpdateUnlockedResearchTrees, damit die Modus-Bedingung frisch ausgewertet wird
		const FString Baum = S->GetStringField(TEXT("baum"));
		UClass* Klasse = LoadClass<UFGResearchTree>(nullptr, *FString::Printf(TEXT("/FicsitTD/Research/BPD_ResearchTree_TD_%s.BPD_ResearchTree_TD_%s_C"), *Baum, *Baum));
		AFGResearchManager* Forschung = AFGResearchManager::Get(World);
		if (Forschung)
		{
			Forschung->UpdateUnlockedResearchTrees();
		}
		const bool bSoll = !S->HasField(TEXT("erwartet")) || S->GetBoolField(TEXT("erwartet"));
		const bool bIst = Klasse && Forschung && Forschung->IsResesearchTreeUnlocked(Klasse);
		OutIst = !Klasse ? FString(TEXT("Baum nicht gefunden")) : FString::Printf(TEXT("%s %s"), *Baum, bIst ? TEXT("frei") : TEXT("gesperrt"));
		return Klasse && bIst == bSoll;
	}
	if (Name == TEXT("ForschungZustand"))
	{
		const FTDForschungKnoten* K = TDForschung::Finde(S->GetStringField(TEXT("knoten")));
		if (!K)
		{
			OutIst = TEXT("Knoten unbekannt");
			return false;
		}
		OutIst = FString::Printf(TEXT("%s: %s"), *K->Key, TDZustandName(TDForschung::Zustand(World, *K)));
		return OutIst.EndsWith(S->GetStringField(TEXT("soll")));
	}
	if (Name == TEXT("RezeptFrei"))
	{
		const FString Pfad = S->GetStringField(TEXT("rezept"));
		UClass* Rezept = LoadClass<UFGRecipe>(nullptr, *Pfad);
		AFGRecipeManager* Rezepte = AFGRecipeManager::Get(World);
		const bool bFrei = Rezept && Rezepte && Rezepte->IsRecipeAvailable(Rezept);
		const bool bSoll = !S->HasField(TEXT("erwartet")) || S->GetBoolField(TEXT("erwartet"));
		OutIst = !Rezept ? FString(TEXT("Rezept nicht gefunden")) : FString::Printf(TEXT("%s %s"), *Rezept->GetName(), bFrei ? TEXT("frei") : TEXT("nicht frei"));
		return Rezept && bFrei == bSoll;
	}
	if (Name == TEXT("Sichtlinie"))
	{
		// 0.77: von jedem Turm 16 Richtungen in 20 m auf Bodenhoehe (+1 m) pruefen; Anteil freier Linien gegen "min"
		int32 Frei = 0, Alle = 0;
		TMap<FString, int32> Hindernisse;
		const float Abstand = (S->HasField(TEXT("abstand")) ? S->GetNumberField(TEXT("abstand")) : 20.0) * 100.f;
		for (TActorIterator<ATDTurretBase> It(World); It; ++It)
		{
			for (int32 k = 0; k < 16; ++k)
			{
				const FVector Richtung = FRotator(0.f, k * 22.5f, 0.f).Vector();
				FVector Punkt = It->GetActorLocation() + Richtung * Abstand;
				FHitResult Boden;
				if (World->LineTraceSingleByChannel(Boden, Punkt + FVector(0.f, 0.f, 3000.f), Punkt - FVector(0.f, 0.f, 3000.f), ECC_Visibility))
				{
					Punkt = Boden.ImpactPoint + FVector(0.f, 0.f, 100.f);
				}
				FString Hindernis;
				++Alle;
				if (It->FreieSichtZuPunkt(Punkt, Hindernis))
				{
					++Frei;
				}
				else
				{
					++Hindernisse.FindOrAdd(Hindernis);
				}
			}
		}
		TArray<FString> Teile;
		for (const TPair<FString, int32>& H : Hindernisse)
		{
			Teile.Add(FString::Printf(TEXT("%s %d"), *H.Key, H.Value));
		}
		const float Anteil = Alle ? float(Frei) / Alle : 0.f;
		OutIst = FString::Printf(TEXT("%d von %d Linien frei (%.0f %%)%s"), Frei, Alle, Anteil * 100.f, Teile.Num() ? *(TEXT("; verdeckt durch ") + FString::Join(Teile, TEXT(", "))) : TEXT(""));
		return Alle > 0 && Anteil >= (S->HasField(TEXT("min")) ? S->GetNumberField(TEXT("min")) : 0.75);
	}
	if (Name == TEXT("TuermeMitStrom"))
	{
		// Test 0.74: im T2-Spielstand hingen die Gewehrtuerme an keinem Netz ("kein Netz mit Generator") und schossen nie
		TArray<FString> Ohne;
		int32 Alle = 0;
		for (TActorIterator<ATDTurretBase> It(World); It; ++It)
		{
			++Alle;
			if (It->GetPowerInfo() && !It->HasPower())
			{
				Ohne.Add(It->GetName());
			}
		}
		OutIst = Ohne.Num() ? FString::Printf(TEXT("%d von %d ohne Strom: %s"), Ohne.Num(), Alle, *FString::Join(Ohne, TEXT(", "))) : FString::Printf(TEXT("alle %d mit Strom"), Alle);
		return Ohne.Num() == 0;
	}
	if (Name == TEXT("BandVerbindungen"))
	{
		// 0.78.3: kein Band- oder Lift-Ende offen, an dem ein passendes Gegenstueck liegt (nach dem Aufruesten)
		const int32 Offen = UTDBandUpgrade::VerbindeOffeneEnden(World, true, &OutIst);
		return Offen == 0;
	}
	if (Name == TEXT("Sicherheitsnetze"))
	{
		// 0.78.2: jede Ausloesung eines Sicherheitsnetzes ist ein Fehler mit Ursache (Ziel 0)
		int32 Summe = 0;
		OutIst = TDNetz::Text(&Summe);
		return Summe == 0;
	}
	if (Name == TEXT("LogEnthaelt"))
	{
		// 0.10.1: kommt der Text im Log dieses Plans vor? "suche", "min" (Standard 1), optional "max"
		FString Erste;
		const int32 N = mLog ? mLog->Zaehle(S->GetStringField(TEXT("suche")), Erste) : 0;
		const int32 Min = S->HasField(TEXT("min")) ? int32(S->GetNumberField(TEXT("min"))) : 1;
		const int32 Max = S->HasField(TEXT("max")) ? int32(S->GetNumberField(TEXT("max"))) : MAX_int32;
		OutIst = FString::Printf(TEXT("%d Zeilen%s"), N, Erste.IsEmpty() ? TEXT("") : *(TEXT(": ") + Erste.Left(160)));
		return N >= Min && N <= Max;
	}
	if (Name == TEXT("KeineWarnungen"))
	{
		const TArray<FString> Zeilen = mLog ? mLog->Zeilen() : TArray<FString>();
		OutIst = Zeilen.Num() == 0 ? FString(TEXT("keine")) : FString::Printf(TEXT("%d: %s"), Zeilen.Num(), *FString::Join(TArray<FString>(Zeilen.GetData(), FMath::Min(5, Zeilen.Num())), TEXT(" | ")));
		return Zeilen.Num() == 0;
	}
	OutIst = TEXT("unbekannte Pruefung");
	return false;
}

void UTDAutotest::Ergebnis(const FString& Art, const FString& Text, bool bOk, const FString& Ist, const TArray<FString>& Antworten)
{
	TSharedPtr<FJsonObject> E = MakeShared<FJsonObject>();
	E->SetStringField(TEXT("Art"), Art);
	E->SetStringField(TEXT("Text"), Text);
	E->SetBoolField(TEXT("Ok"), bOk);
	E->SetStringField(TEXT("Ist"), Ist);
	E->SetNumberField(TEXT("Sekunde"), FMath::RoundToInt(mGesamt));
	TArray<TSharedPtr<FJsonValue>> A;
	for (const FString& Z : Antworten)
	{
		A.Add(MakeShared<FJsonValueString>(Z));
	}
	E->SetArrayField(TEXT("Antworten"), A);
	mErgebnisse.Add(MakeShared<FJsonValueObject>(E));
	mFehler += bOk ? 0 : 1;
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest %s: %s %s%s"), bOk ? TEXT("OK") : TEXT("FEHLER"), *Art, *Text, Ist.IsEmpty() ? TEXT("") : *(TEXT(" = ") + Ist));
}

void UTDAutotest::MeldeNachzuegler(UWorld* World, const TArray<AFGCreature*>& Lebende, int32 Welle)
{
	if (Welle != mNachzueglerWelle)
	{
		mNachzueglerWelle = Welle;
		mNachzueglerMeldungen = 0;
	}
	if (++mNachzueglerMeldungen > 3)
	{
		return;
	}
	FVector Mitte = FVector::ZeroVector;
	for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
	{
		Mitte = It->GetActorLocation();
	}
	TArray<FString> Teile;
	for (AFGCreature* C : Lebende)
	{
		if (!C)
		{
			continue;
		}
		FString Name = C->GetClass()->GetName();
		Name.RemoveFromStart(TEXT("Char_"));
		Name.RemoveFromEnd(TEXT("_C"));
		const UFGHealthComponent* Leben = C->GetHealthComponent();
		int32 Zielen = 0;
		FString Grund;
		for (TActorIterator<ATDTurretBase> It(World); It; ++It)
		{
			if (It->GetAktuellesZiel() == C)
			{
				++Zielen;
			}
			else if (Grund.IsEmpty())
			{
				Grund = It->GetTurmName() + TEXT(": ") + It->KeinZielGrund();
			}
		}
		const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(C);
		Teile.Add(FString::Printf(TEXT("%s LP %.2f/%.0f, %.0f m vom Kern (Hoehe %+.1f m), %d Tuerme zielen darauf%s | %s"), *Name,
			Leben ? Leben->GetCurrentHealth() : -1.f, Leben ? Leben->GetMaxHealth() : -1.f, FVector::Dist2D(C->GetActorLocation(), Mitte) / 100.f,
			(C->GetActorLocation().Z - Mitte.Z) / 100.f, Zielen, Grund.IsEmpty() ? TEXT("") : *(TEXT(", sonst ") + Grund), Hirn ? *Hirn->Diagnose() : TEXT("-")));
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest Nachzuegler Welle %d: %s"), Welle, *FString::Join(Teile, TEXT(" || ")));
	Ergebnis(TEXT("Diagnose"), FString::Printf(TEXT("Nachzügler Welle %d"), Welle), true, FString::Join(Teile, TEXT(" || ")));
}

void UTDAutotest::Versorge(UWorld* World, float Dt)
{
	// Pruefstand: Munition je Turm und Minute in den Eingang legen (Zutat des gewaehlten Rezepts); Spielzeit mit Zeitraffer
	if (mVersorgung.Num() == 0)
	{
		return;
	}
	const float Spielzeit = Dt * UGameplayStatics::GetGlobalTimeDilation(World);
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		const float* Rate = mVersorgung.Find(It->GetTurmName());
		const TSubclassOf<UFGRecipe> Rezept = It->GetCurrentRecipe();
		UFGInventoryComponent* Eingang = It->GetInputInventory();
		if (!Rate || !Rezept || !Eingang)
		{
			continue;
		}
		const TArray<FItemAmount> Zutaten = UFGRecipe::GetIngredients(*It, Rezept);
		if (Zutaten.Num() == 0 || !Zutaten[0].ItemClass)
		{
			continue;
		}
		float& Rest = mVersorgtRest.FindOrAdd(*It);
		Rest += *Rate * Spielzeit / 60.f;
		const int32 Ganz = FMath::FloorToInt(Rest);
		if (Ganz > 0)
		{
			Rest -= Eingang->AddStack(FInventoryStack(Ganz, Zutaten[0].ItemClass), true);
			Rest = FMath::Min(Rest, 5.f);   // voller Eingang: nicht ansparen
		}
	}
}

void UTDAutotest::Beende(const FString& Grund)
{
	PlanFertig(Grund);
}

void UTDAutotest::PlanFertig(const FString& Grund)
{
	TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("Plan"), mPlan.IsValid() ? mPlan->GetStringField(TEXT("Name")) : FString());
	P->SetStringField(TEXT("Ende"), Grund);
	P->SetNumberField(TEXT("Sekunden"), FMath::RoundToInt(mPlanZeit));
	P->SetNumberField(TEXT("Fehler"), mFehler);
	P->SetBoolField(TEXT("Bestanden"), mFehler == 0);
	TArray<TSharedPtr<FJsonValue>> W;
	for (const FString& Z : mLog ? mLog->Zeilen() : TArray<FString>())
	{
		W.Add(MakeShared<FJsonValueString>(Z));
	}
	P->SetArrayField(TEXT("Warnungen"), W);
	P->SetArrayField(TEXT("Schritte"), mErgebnisse);
	mPlanErgebnisse.Add(MakeShared<FJsonValueObject>(P));
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest: Plan %s fertig (%s): %d Fehler"), *P->GetStringField(TEXT("Plan")), *Grund, mFehler);
	++mPlanNr;
	if (mPlanNr < mPlaene.Num())
	{
		StartePlan();
		return;
	}
	PaketFertig();
}

void UTDAutotest::PaketFertig()
{
	int32 Fehler = 0;
	for (const TSharedPtr<FJsonValue>& P : mPlanErgebnisse)
	{
		Fehler += static_cast<int32>(P->AsObject()->GetNumberField(TEXT("Fehler")));
	}
	TSharedPtr<FJsonObject> Wurzel = MakeShared<FJsonObject>();
	Wurzel->SetStringField(TEXT("Paket"), mPaket.IsValid() ? mPaket->GetStringField(TEXT("Name")) : FString());
	Wurzel->SetNumberField(TEXT("Sekunden"), FMath::RoundToInt(mGesamt));
	Wurzel->SetNumberField(TEXT("Fehler"), Fehler);
	Wurzel->SetBoolField(TEXT("Bestanden"), Fehler == 0);
	Wurzel->SetArrayField(TEXT("Plaene"), mPlanErgebnisse);
	FString Text;
	FJsonSerializer::Serialize(Wurzel.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
	FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Ordner(), TEXT("Autotest-Ergebnis.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	IFileManager::Get().Delete(*FPaths::Combine(Ordner(), TEXT("Autotest.laeuft.json")));
	const bool bBereit = mPaket.IsValid() && mPaket->HasField(TEXT("Bereitschaft")) && mPaket->GetBoolField(TEXT("Bereitschaft"));
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest-Paket fertig: %d Plaene, %d Fehler%s"), mPlanErgebnisse.Num(), Fehler,
		bBereit ? TEXT(" - Bereitschaft, warte auf den naechsten Auftrag") : TEXT(", Spiel wird geschlossen"));
	if (bBereit)
	{
		mPhase = EPhase::Bereit;
		mUhr = 0.f;
		return;
	}
	mPhase = EPhase::Fertig;
	FPlatformMisc::RequestExit(false, TEXT("FicsitTD Autotest"));
}
