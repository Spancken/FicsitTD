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
	UE_LOG(LogFicsitTD, Log, TEXT("Autotest: neues Spiel %s"), *Optionen);
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
	if (S->TryGetNumberField(TEXT("zeitraffer"), Zahl))
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
