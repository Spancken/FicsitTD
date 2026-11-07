#include "TDAutotest.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDTurretBase.h"
#include "TDForschung.h"
#include "TDWaveSubsystem.h"
#include "TDWellenStatistik.h"
#include "TDBalance.h"
#include "Command/ChatCommandLibrary.h"
#include "FGPlayerController.h"
#include "FGSaveSystem.h"
#include "FGSaveSession.h"
#include "FGGameMode.h"
#include "FGGameState.h"
#include "FGBuildableSubsystem.h"
#include "TDEinsatz.h"
#include "TDGegnerHirn.h"
#include "TDSicherheitsnetz.h"
#include "TDBandUpgrade.h"
#include "TDKernplatz.h"
#include "Creature/FGCreature.h"
#include "FGHealthComponent.h"
#include "FGSaveManagerInterface.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
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
#include "FGSchematicManager.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGHUD.h"
#include "FGInventoryComponent.h"
#include "FGCharacterPlayer.h"
#include "Buildables/FGBuildableMAM.h"
#include "UI/FGGameUI.h"
#include "Resources/FGItemDescriptor.h"

/** erster Turm eines Typs (Anzeigename, z. B. Gewehrturm) */
static ATDTurretBase* TDTestTurm(UWorld* World, const FString& Typ)
{
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		if (It->GetTurmName() == Typ)
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
		if (Verbosity > ELogVerbosity::Warning || !Category.ToString().Contains(TEXT("FicsitTD")))
		{
			return;
		}
		FScopeLock Sperre(&mSperre);
		if (mZeilen.Num() < 200)
		{
			mZeilen.Add(FString(V));
		}
	}
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	void Leeren()
	{
		FScopeLock Sperre(&mSperre);
		mZeilen.Reset();
	}
	TArray<FString> Zeilen()
	{
		FScopeLock Sperre(&mSperre);
		return mZeilen;
	}
private:
	FCriticalSection mSperre;
	TArray<FString> mZeilen;
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
	mKernGefallen = false;
	TDNetz::Zuruecksetzen();
	mNachzueglerUhr = 0.f;
	mNachzueglerMeldungen = 0;
	mNachzueglerWelle = 0;
	mVersorgung.Reset();
	mVersorgtRest.Reset();
	mPlanZeit = 0.f;
	mUhr = 0.f;
	if (mLog)
	{
		mLog->Leeren();
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
			mPhase = EPhase::Laedt;   // schon im Spiel (z. B. Startparameter)
			mUhr = 0.f;
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
	FString Optionen = TEXT("?sessionName=Autotest_Neu?SessionSettings=FicsitTD.TowerDefense=AgAAAAQAAAABAAAA?skiponboarding");
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
	UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
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
	UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
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
		UGameplayStatics::SetGlobalTimeDilation(World, FMath::Clamp(static_cast<float>(Zahl), 0.25f, 8.f));
		Ergebnis(TEXT("Zeitraffer"), FString::Printf(TEXT("x%.1f"), Zahl), true);
		return true;
	}
	if (S->TryGetStringField(TEXT("befehl"), Wort))
	{
		mSender->Antworten.Reset();
		AChatCommandSubsystem* Befehle = AChatCommandSubsystem::Get(World);
		const EExecutionStatus Status = Befehle ? Befehle->RunChatCommand(Wort, mSender) : EExecutionStatus::UNCOMPLETED;
		const bool bOk = Status == EExecutionStatus::COMPLETED;
		Ergebnis(TEXT("Befehl"), TEXT("/") + Wort, bOk, bOk ? TEXT("ausgefuehrt") : TEXT("nicht ausgefuehrt"), mSender->Antworten);
		return true;
	}
	if (S->TryGetStringField(TEXT("warteBis"), Wort))
	{
		const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();
		const bool bLaeuft = Waves && Waves->LaeuftWelle();
		const bool bErreicht = Wort == TEXT("WelleLaeuft") ? bLaeuft : !bLaeuft;
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
			{ TEXT("Elektrozaun"), TEXT("ConductorWall") }, { TEXT("Funkmast"), TEXT("RadioMast") }, { TEXT("Schildgenerator"), TEXT("ShieldGenerator") }, { TEXT("Himmelsturm"), TEXT("FlakTurret") }, { TEXT("Stoerfeld"), TEXT("FieldProjector") }, { TEXT("Singularitaet"), TEXT("SingularityLauncher") }, { TEXT("Dimensionslager"), TEXT("DimensionalDepot") } };   // 0.83 Bauten P2/P3
		const FString* O = Ordner.Find(Wort);
		UClass* Klasse = O ? LoadClass<AFGBuildable>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/%s/Build_TD_%s.Build_TD_%s_C"), **O, **O, **O)) : nullptr;
		UClass* BauRezept = O ? LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/%s/Recipe_TD_%s.Recipe_TD_%s_C"), **O, **O, **O)) : nullptr;
		UClass* Munition = O && S->HasField(TEXT("rezept")) ? LoadClass<UFGRecipe>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/%s/%s.%s_C"), **O,
			*S->GetStringField(TEXT("rezept")), *S->GetStringField(TEXT("rezept")))) : nullptr;
		AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
		const int32 Anzahl = S->HasField(TEXT("anzahl")) ? static_cast<int32>(S->GetNumberField(TEXT("anzahl"))) : 1;
		const int32 Level = S->HasField(TEXT("level")) ? static_cast<int32>(S->GetNumberField(TEXT("level"))) : 1;
		// Plaetze: Ring um den Kern im 8-m-Raster, abwechselnd die Seiten, damit sich die Tuerme verteilen.
		// 0.78.1 (Pruefstand: Flammenwerfer auf 16 m hatte nur 32 % Zeit mit Ziel): "ring": "innen" nimmt den 8-m-Ring
		static const FVector2D Aussen[] = { {16, 0}, {-16, 0}, {0, 16}, {0, -16}, {16, 16}, {-16, -16}, {16, -16}, {-16, 16}, {8, 16}, {-8, -16}, {16, 8}, {-16, -8},
			{-8, 16}, {8, -16}, {-16, 8}, {16, -8} };
		static const FVector2D Innen[] = { {0, 8}, {0, -8}, {8, 0}, {-8, 0}, {8, 8}, {-8, -8}, {8, -8}, {-8, 8} };
		const bool bInnen = S->HasField(TEXT("ring")) && S->GetStringField(TEXT("ring")) == TEXT("innen");
		const FVector2D* Raster = bInnen ? Innen : Aussen;
		const int32 Plaetze = bInnen ? UE_ARRAY_COUNT(Innen) : UE_ARRAY_COUNT(Aussen);
		int32& Platz = bInnen ? mBauplatzInnen : mBauplatz;
		int32 Gebaut = 0;
		for (int32 i = 0; i < Anzahl && Kern && Klasse && Bau && Platz < Plaetze; ++i, ++Platz)
		{
			const FVector Ort = Kern->GetActorLocation() + FVector(Raster[Platz].X * 100.f, Raster[Platz].Y * 100.f, 0.f);
			const FTransform T(FRotator::ZeroRotator, Ort);
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
			return true;
		}
		if (mSchrittUhr <= Dt + 0.01f)
		{
			UGameplayStatics::SetGlobalTimeDilation(World, S->HasField(TEXT("zeitraffer")) ? FMath::Clamp(static_cast<float>(S->GetNumberField(TEXT("zeitraffer"))), 1.f, 8.f) : 4.f);
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
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
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
			UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
			Ergebnis(TEXT("Wellenlauf"), FString::Printf(TEXT("bis Welle %d"), Ziel), true, FString::Printf(TEXT("%.0f s"), mSchrittUhr));
			return true;
		}
		if (mUhr > 2.f && Waves->NaechsteWelleJetzt())
		{
			mUhr = 0.f;
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
	if (S->TryGetStringField(TEXT("erforsche"), Wort))
	{
		// 0.75: Forschung direkt geben wie das MAM (Spiel-Delegate laeuft, stille Turm-Rezepte kommen mit); MAM-Sperren gelten hier nicht
		const FTDForschungKnoten* K = TDForschung::Finde(Wort);
		UClass* Klasse = K ? LoadClass<UFGSchematic>(nullptr, *K->Pfad) : nullptr;
		AFGSchematicManager* Schemata = AFGSchematicManager::Get(World);
		APlayerController* PC = World->GetFirstPlayerController();
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
			FScreenshotRequest::RequestScreenshot(Pfad, true, false);   // mit Oberflaeche (Test 0.75: ohne fehlten die Fenster)
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
	if (Name == TEXT("Version"))
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FicsitTD"));
		OutIst = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : TEXT("nicht geladen");
		return !S->HasField(TEXT("soll")) || OutIst == S->GetStringField(TEXT("soll"));
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
