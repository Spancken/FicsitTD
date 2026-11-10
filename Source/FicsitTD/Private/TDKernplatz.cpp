#include "TDKernplatz.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "FGChatManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGItemDescriptor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "NavigationSystem.h"
#include "Engine/TargetPoint.h"
#include "Algo/Reverse.h"
#include "FGCharacterBase.h"
#include "FGHealthComponent.h"
#include "FGBuildableSubsystem.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGFactoryColoringTypes.h"
#include "Buildables/FGBuildable.h"
#include "FGRecipe.h"
#include "InstanceData.h"
#include "TDEinsatz.h"
#include "TDGameInstanceModule.h"
#include "TDGegnerHirn.h"
#include "AI/Navigation/NavigationTypes.h"
#include "FGHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "AkInitBank.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "AkAudioEvent.h"
#include "Containers/Ticker.h"
#include "Wwise/CookedData/WwiseInitBankCookedData.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "UI/FGGameUI.h"
#include "Narrative/FGMessage.h"
#include "SessionSettings/SessionSettingsManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "FGFoliageRemovalSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// Kandidaten je Biom (Konzept-Kernplaetze.md, Abschnitt 2): Eisen, Kupfer, Kalk in 400 m, Kohle in 800 m, mindestens 60 m bis zum naechsten Knoten
static const TArray<FTDPlatzKandidat> TDKandidaten = {
	{ TEXT("G1"), TEXT("Grasfelder"),      -70000.f,  240000.f, -3578.f, 0 },
	{ TEXT("G2"), TEXT("Grasfelder"),      -65000.f,  185000.f, -1877.f, 0 },
	{ TEXT("G3"), TEXT("Grasfelder"),      -75000.f,  290000.f, -3933.f, 0 },
	{ TEXT("F1"), TEXT("Felsenwueste"),   -185000.f,  -35000.f,   281.f, 1 },
	{ TEXT("F2"), TEXT("Felsenwueste"),   -175000.f,   -5000.f,  6395.f, 1 },
	{ TEXT("F3"), TEXT("Felsenwueste"),   -270000.f,  -90000.f,   972.f, 1 },
	{ TEXT("N1"), TEXT("Noerdlicher Wald"), 75000.f,  -90000.f, 10570.f, 2 },
	{ TEXT("N2"), TEXT("Noerdlicher Wald"), 80000.f,  -55000.f, 10715.f, 2 },
	{ TEXT("N3"), TEXT("Noerdlicher Wald"), 50000.f,  -60000.f, 10297.f, 2 },
	{ TEXT("D1"), TEXT("Duenenwueste"),    240000.f, -195000.f,  2680.f, 3 },
	{ TEXT("D2"), TEXT("Duenenwueste"),    235000.f, -230000.f,  2626.f, 3 },
	{ TEXT("D3"), TEXT("Duenenwueste"),    255000.f, -255000.f,  2089.f, 3 },
};
static const TCHAR* TDStufenNamen[] = { TEXT("Erkundung"), TEXT("Normal"), TEXT("Hart"), TEXT("Albtraum") };
/** 0.85 (L1): Stufe und Biom fuer Spielermeldungen in der Spielsprache (die deutschen Namen oben bleiben fuer Scan, Log und Dateien) */
static FString TDStufeAnzeige(int32 Stufe)
{
	switch (FMath::Clamp(Stufe, 0, 3))
	{
	case 0: return TDT(TEXT("Erkundung"), TEXT("Exploration"));
	case 1: return TDT(TEXT("Normal"), TEXT("Normal"));
	case 2: return TDT(TEXT("Hart"), TEXT("Hard"));
	default: return TDT(TEXT("Albtraum"), TEXT("Nightmare"));
	}
}
static FString TDBiomAnzeige(int32 Stufe)
{
	switch (FMath::Clamp(Stufe, 0, 3))
	{
	case 0: return TDT(TEXT("Grasfelder"), TEXT("Grass Fields"));
	case 1: return TDT(TEXT("Felsenwueste"), TEXT("Rocky Desert"));
	case 2: return TDT(TEXT("Noerdlicher Wald"), TEXT("Northern Forest"));
	default: return TDT(TEXT("Duenenwueste"), TEXT("Dune Desert"));
	}
}
/** Plattform 6 x 6 Fundamente zu 8 m: Raster 7 x 7 Messpunkte im Abstand von 8 m */
static constexpr int32 TDPlatzHalbe = 3;
static constexpr float TDPlatzRaster = 800.f;
/** Weltrand-Pruefung, unten beim Scan definiert */
static bool TDInnerhalbWeltrand(const FVector2D& P, float Abstand);

UTDKernplatz* UTDKernplatz::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDKernplatz>() : nullptr;
}

const TArray<FTDPlatzKandidat>& UTDKernplatz::Kandidaten()
{
	return TDKandidaten;
}

const FTDPlatzKandidat* UTDKernplatz::Finde(const FString& Id)
{
	return TDKandidaten.FindByPredicate([&Id](const FTDPlatzKandidat& K) { return Id.Equals(K.Id, ESearchCase::IgnoreCase); });
}

int32 UTDKernplatz::BiomAus(const FString& Eingabe, FString& OutName)
{
	const FString E = Eingabe.ToLower();
	if (E.StartsWith(TEXT("gras")))                                   { OutName = TEXT("Grasfelder"); return 0; }
	if (E.StartsWith(TEXT("fels")))                                   { OutName = TEXT("Felsenwueste"); return 1; }
	if (E.Contains(TEXT("wald")) || E.StartsWith(TEXT("nord")) || E.StartsWith(TEXT("nörd"))) { OutName = TEXT("Noerdlicher Wald"); return 2; }
	if (E.StartsWith(TEXT("duen")) || E.StartsWith(TEXT("dün")))      { OutName = TEXT("Duenenwueste"); return 3; }
	OutName = FString();
	return -1;
}

FString UTDKernplatz::AuswahlDatei()
{
	return FPaths::ProjectSavedDir() / TEXT("FicsitTD/Kernplaetze-Auswahl.json");
}

void UTDKernplatz::Melde(const FString& Text) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz: %s"), *Text);
	if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct N;
		N.MessageText = FText::FromString(Text);
		N.MessageType = EFGChatMessageType::CMT_CustomMessage;
		N.MessageSender = TDTX(TEXT("Kernplatz"), TEXT("Core site"));
		N.MessageSenderColor = FLinearColor(1.f, 0.55f, 0.1f);
		Chat->BroadcastChatMessage(N);
	}
}

FString UTDKernplatz::Pruefe(const FString& Id, APawn* Spieler, const FString& Biom)
{
	if (!Spieler)
	{
		return TEXT("Nur als Spieler.");
	}
	GetWorld()->GetTimerManager().ClearTimer(mTimer);
	mSpieler = Spieler;
	if (Id.IsEmpty() || Id.Equals(TEXT("hier"), ESearchCase::IgnoreCase))
	{
		mId = TEXT("hier");
		FString Name;
		const int32 Stufe = BiomAus(Biom, Name);
		mBiom = Stufe >= 0 ? Name : FString(TEXT("-"));
		mStufe = Stufe >= 0 ? Stufe : 1;
		mZiel = Spieler->GetActorLocation();
		mVersuche = 0;
		mHatErgebnis = false;
		SucheBoden();
		return FString::Printf(TEXT("Pruefe den Platz hier (%s), %s, Stufe %s. Du stehst auf der Mitte (dort kaeme der Kern hin) - bitte nicht weglaufen, bis das Ergebnis da ist."),
			*mZiel.ToCompactString(), *mBiom, TDStufenNamen[mStufe]);
	}
	if ((Id.StartsWith(TEXT("S")) || Id.StartsWith(TEXT("s"))) && Id.Len() <= 3 && FCString::IsNumeric(*Id.Mid(1)))
	{
		const int32 Nr = FCString::Atoi(*Id.Mid(1)) - 1;
		if (!mScanTreffer.IsValidIndex(Nr))
		{
			return FString::Printf(TEXT("Kein Scan-Treffer %s (letzter Scan hatte %d)."), *Id, mScanTreffer.Num());
		}
		if (!mHatRueckweg)
		{
			mHatRueckweg = true;
			mRueckweg = Spieler->GetActorLocation();
		}
		mId = Id.ToUpper();
		mBiom = mScanBiom;
		mStufe = mScanStufe;
		mZiel = mScanTreffer[Nr].Ort;
		mVersuche = 0;
		mHatErgebnis = false;
		Spieler->TeleportTo(mZiel + FVector(0.f, 0.f, 5000.f), Spieler->GetActorRotation(), false, true);
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->StopMovementImmediately();
			Figur->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		}
		GetWorld()->GetTimerManager().SetTimer(mTimer, this, &UTDKernplatz::SucheBoden, 1.f, true, 1.f);
		return FString::Printf(TEXT("Fliege zu Scan-Treffer %s (%s) ..."), *mId, *mBiom);
	}
	const FTDPlatzKandidat* K = Finde(Id);
	if (!K)
	{
		TArray<FString> Ids;
		for (const FTDPlatzKandidat& C : TDKandidaten)
		{
			Ids.Add(FString::Printf(TEXT("%s (%s)"), C.Id, C.Biom));
		}
		return TEXT("Unbekannter Platz. Kandidaten: ") + FString::Join(Ids, TEXT(", ")) + TEXT(" oder hier");
	}
	if (!mHatRueckweg)
	{
		mHatRueckweg = true;
		mRueckweg = Spieler->GetActorLocation();
	}
	if (!TDInnerhalbWeltrand(FVector2D(K->X, K->Y), 0.f))
	{
		return FString::Printf(TEXT("%s liegt ausserhalb der Spielwelt."), K->Id);
	}
	mId = K->Id;
	mBiom = K->Biom;
	mStufe = K->Stufe;
	mZiel = FVector(K->X, K->Y, K->ZHinweis);
	mVersuche = 0;
	mHatErgebnis = false;
	// hoch ueber den Platz und schweben, bis das Gelaende geladen ist (World Partition)
	Spieler->TeleportTo(mZiel + FVector(0.f, 0.f, 5000.f), Spieler->GetActorRotation(), false, true);
	if (ACharacter* Figur = Cast<ACharacter>(Spieler))
	{
		Figur->GetCharacterMovement()->StopMovementImmediately();
		Figur->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	GetWorld()->GetTimerManager().SetTimer(mTimer, this, &UTDKernplatz::SucheBoden, 1.f, true, 1.f);
	return FString::Printf(TEXT("Fliege zu %s (%s, Stufe %s) - warte auf das Gelaende ..."), K->Id, K->Biom, TDStufenNamen[K->Stufe]);
}

FString UTDKernplatz::Zurueck(APawn* Spieler)
{
	if (!Spieler || !mHatRueckweg)
	{
		return TEXT("Kein Rueckweg gemerkt.");
	}
	GetWorld()->GetTimerManager().ClearTimer(mTimer);
	Spieler->TeleportTo(mRueckweg + FVector(0.f, 0.f, 200.f), Spieler->GetActorRotation(), false, true);
	if (ACharacter* Figur = Cast<ACharacter>(Spieler))
	{
		Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	mHatRueckweg = false;
	return TEXT("Zurueck am Ausgangsort.");
}

bool UTDKernplatz::Boden(float X, float Y, float ZMitte, FVector& Out, bool& bHindernis, FString& Hindernis) const
{
	bHindernis = false;
	TArray<FHitResult> Treffer;
	FCollisionQueryParams Params(TEXT("TDKernplatz"), true);
	if (mSpieler.IsValid())
	{
		Params.AddIgnoredActor(mSpieler.Get());
	}
	FCollisionObjectQueryParams Arten;
	Arten.AddObjectTypesToQuery(ECC_WorldStatic);
	Arten.AddObjectTypesToQuery(ECC_WorldDynamic);
	GetWorld()->LineTraceMultiByObjectType(Treffer, FVector(X, Y, ZMitte + 60000.f), FVector(X, Y, ZMitte - 60000.f), Arten, Params);
	// Test 0.67.0: in der Duenenwueste ist der Boden kein Landschafts-Objekt -> alle 49 Punkte als Hindernis.
	// Boden = Landschaft, sonst die tiefste ebene Flaeche, die kein Bewuchs ist
	const FHitResult* Grund = nullptr;
	for (const FHitResult& T : Treffer)
	{
		const UPrimitiveComponent* C = T.GetComponent();
		if (C && C->GetClass()->GetName().Contains(TEXT("Landscape")))
		{
			Grund = &T;
			break;
		}
	}
	auto IstBewuchs = [](const FHitResult& T)
	{
		const UPrimitiveComponent* C = T.GetComponent();
		const AActor* A = C ? C->GetOwner() : nullptr;
		const FString Wer = A ? A->GetClass()->GetName() : FString();
		const FString Was = C ? C->GetClass()->GetName() : FString();
		return Wer.Contains(TEXT("Foliage")) || Was.Contains(TEXT("Foliage")) || Was.Contains(TEXT("HierarchicalInstanced"));
	};
	if (!Grund)
	{
		for (int32 i = Treffer.Num() - 1; i >= 0; --i)
		{
			if (Treffer[i].ImpactNormal.Z > 0.7f && !IstBewuchs(Treffer[i]))
			{
				Grund = &Treffer[i];
				break;
			}
		}
	}
	if (!Grund)
	{
		return false;
	}
	Out = Grund->ImpactPoint;
	for (const FHitResult& T : Treffer)
	{
		if (&T == Grund)
		{
			break;
		}
		// darueber und mehr als 1,5 m hoch: Baum, Fels, Gebaeude
		if (T.ImpactPoint.Z > Out.Z + 150.f)
		{
			bHindernis = true;
			const UPrimitiveComponent* C = T.GetComponent();
			const UObject* Wer = C && C->GetOwner() ? static_cast<const UObject*>(C->GetOwner()) : static_cast<const UObject*>(C);
			Hindernis = Wer ? Wer->GetClass()->GetName() : TEXT("?");
			break;
		}
	}
	return true;
}

void UTDKernplatz::SetzeInvoker(const FVector& Ort)
{
	EntferneInvoker();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav || !Nav->IsActiveTilesGenerationEnabled())
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATargetPoint* Punkt = GetWorld()->SpawnActor<ATargetPoint>(ATargetPoint::StaticClass(), Ort, FRotator::ZeroRotator, P);
	if (Punkt)
	{
		Nav->RegisterNavigationInvoker(Punkt, 32000.f, 36000.f);
		mInvoker = Punkt;
	}
}

void UTDKernplatz::EntferneInvoker()
{
	if (AActor* Punkt = mInvoker.Get())
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			Nav->UnregisterNavigationInvoker(Punkt);
		}
		Punkt->Destroy();
	}
	mInvoker.Reset();
}

void UTDKernplatz::Schuetze(bool bAn)
{
	APawn* Spieler = mSpieler.Get();
	if (!Spieler)
	{
		return;
	}
	Spieler->SetCanBeDamaged(!bAn);
	if (bAn)
	{
		if (const AFGCharacterBase* Figur = Cast<AFGCharacterBase>(Spieler))
		{
			if (UFGHealthComponent* Leben = Figur->GetHealthComponent())
			{
				if (!Leben->IsDead() && Leben->GetCurrentHealth() < Leben->GetMaxHealth())
				{
					Leben->ResetHealth();
				}
			}
		}
	}
}

int32 UTDKernplatz::NetzPunkte() const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		return 0;
	}
	int32 Auf = 0;
	for (int32 r = 0; r < 16; ++r)
	{
		const FVector P = mBodenMitte + FRotator(0.f, 22.5f * r, 0.f).Vector() * 25000.f;
		FNavLocation Ort;
		Auf += Nav->ProjectPointToNavigation(P, Ort, FVector(800.f, 800.f, 6000.f)) ? 1 : 0;
	}
	return Auf;
}

void UTDKernplatz::WarteAufNetz()
{
	// Test 0.67.0: nach 5 s lag in 150-300 m fast nie Navigationsnetz ("ohne Netz") -> warten, bis es sich nicht mehr aendert (hoechstens 60 s)
	Schuetze(true);
	const int32 Auf = NetzPunkte();
	++mNetzVersuche;
	mNetzGleich = Auf == mNetzLetzte ? mNetzGleich + 1 : 0;
	mNetzLetzte = Auf;
	if (Auf >= 14 || (mNetzVersuche >= 5 && Auf > 0 && mNetzGleich >= 5) || mNetzVersuche >= 45)
	{
		GetWorld()->GetTimerManager().ClearTimer(mTimer);
		UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz: Netz nach %d s an %d von 16 Punkten in 250 m"), mNetzVersuche * 2, Auf);
		Auswerten();
	}
}

void UTDKernplatz::SucheBoden()
{
	APawn* Spieler = mSpieler.Get();
	if (!Spieler)
	{
		GetWorld()->GetTimerManager().ClearTimer(mTimer);
		return;
	}
	Schuetze(true);
	FVector Grund;
	bool bHindernis = false;
	FString Was;
	const bool bGefunden = Boden(mZiel.X, mZiel.Y, mZiel.Z, Grund, bHindernis, Was);
	++mVersuche;
	if (!bGefunden && mVersuche < 30)
	{
		if (mId != TEXT("hier"))
		{
			Spieler->TeleportTo(mZiel + FVector(0.f, 0.f, 5000.f), Spieler->GetActorRotation(), false, true);
		}
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(mTimer);
	if (!bGefunden)
	{
		Melde(TDF(TEXT("%s: nach 30 s kein Boden gefunden - Gelaende nicht geladen oder Platz ueber Wasser."),
			TEXT("%s: no ground found after 30 s - terrain not loaded or site over water."), *mId));
		if (mScanLaeuft && mScanPhase == 2)
		{
			GetWorld()->GetTimerManager().SetTimer(mScanTimer, this, &UTDKernplatz::ScanNaechstePruefung, 1.f, false);
			return;
		}
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		return;
	}
	mBodenMitte = Grund;
	if (mBauen)
	{
		Melde(TDF(TEXT("%s: Boden gefunden, baue in 2 s ..."), TEXT("%s: ground found, building in 2 s ..."), *mId));
		GetWorld()->GetTimerManager().SetTimer(mTimer, this, &UTDKernplatz::BaueJetzt, 2.f, false);
		return;
	}
	if (mId != TEXT("hier"))
	{
		// neben die Mitte stellen (die Mitte bleibt fuer den Kern frei), Blick zur Mitte
		Spieler->TeleportTo(Grund + FVector(-1500.f, 0.f, 250.f), FRotator::ZeroRotator, false, true);
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
	Melde(FString::Printf(TEXT("%s: Boden bei %.0f m Hoehe gefunden, erzeuge das Navigationsnetz bis 320 m und warte darauf (bis 90 s) ..."), *mId, Grund.Z / 100.f));
	// Test 0.67.1: Netz nur um den Spieler (100 m 8/8, 250 m 0/8) -> eigener Invoker wie am Kern
	SetzeInvoker(Grund + FVector(0.f, 0.f, 100.f));
	mNetzVersuche = 0;
	mNetzLetzte = -1;
	mNetzGleich = 0;
	GetWorld()->GetTimerManager().SetTimer(mTimer, this, &UTDKernplatz::WarteAufNetz, 2.f, true, 2.f);
}

void UTDKernplatz::Auswerten()
{
	UWorld* World = GetWorld();
	// 1) Gelaende: 7 x 7 Punkte im Raster der Fundamente
	float ZMin = TNumericLimits<float>::Max(), ZMax = -TNumericLimits<float>::Max();
	int32 Punkte = 0, OhneBoden = 0, Hindernisse = 0;
	TMap<FString, int32> Arten;
	for (int32 ix = -TDPlatzHalbe; ix <= TDPlatzHalbe; ++ix)
	{
		for (int32 iy = -TDPlatzHalbe; iy <= TDPlatzHalbe; ++iy)
		{
			FVector P;
			bool bHindernis = false;
			FString Was;
			if (!Boden(mBodenMitte.X + ix * TDPlatzRaster, mBodenMitte.Y + iy * TDPlatzRaster, mBodenMitte.Z, P, bHindernis, Was))
			{
				++OhneBoden;
				continue;
			}
			++Punkte;
			ZMin = FMath::Min(ZMin, P.Z);
			ZMax = FMath::Max(ZMax, P.Z);
			if (bHindernis && !Was.Contains(TEXT("Character")))
			{
				++Hindernisse;
				Arten.FindOrAdd(Was.Replace(TEXT("_C"), TEXT("")))++;
			}
		}
	}
	const float Hub = Punkte > 0 ? (ZMax - ZMin) / 100.f : 0.f;
	TArray<FString> ArtText;
	for (const TPair<FString, int32>& A : Arten)
	{
		ArtText.Add(FString::Printf(TEXT("%s %d"), *A.Key, A.Value));
	}

	// 2) Fronten in der Stufe des Bioms (die Fronten des laufenden Einsatzes bleiben unveraendert)
	TArray<FString> FrontText;
	TArray<TArray<FVector>> Wege;
	int32 Gueltig = 0;
	float WegMin = 0.f, WegMax = 0.f;
	if (UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>())
	{
		const TArray<FTDFront> Fronten = Waves->PruefeFrontenAn(mBodenMitte + FVector(0.f, 0.f, 100.f), mStufe, Gueltig);
		static const TCHAR* Richtungen[] = { TEXT("N"), TEXT("NNO"), TEXT("NO"), TEXT("ONO"), TEXT("O"), TEXT("OSO"), TEXT("SO"), TEXT("SSO"),
			TEXT("S"), TEXT("SSW"), TEXT("SW"), TEXT("WSW"), TEXT("W"), TEXT("WNW"), TEXT("NW"), TEXT("NNW") };
		for (const FTDFront& F : Fronten)
		{
			// Karte: +X Osten, +Y Sueden; Winkel 0 = +X, 90 = +Y -> Kompass (im Uhrzeigersinn ab Norden) = 90 + Winkel
			const float Kompass = FMath::Fmod(90.f + F.Winkel + 360.f, 360.f);
			const int32 Index = FMath::RoundToInt(Kompass / 22.5f) % 16;
			FrontText.Add(F.Weg > 0.f ? FString::Printf(TEXT("%s %.0f m"), Richtungen[Index], F.Weg / 100.f)
				: FString::Printf(TEXT("%s (Ersatz, ungeprueft)"), Richtungen[Index]));
			if (F.Punkte.Num() >= 2)
			{
				Wege.Add(F.Punkte);
				WegMin = WegMin <= 0.f ? F.Weg : FMath::Min(WegMin, F.Weg);
				WegMax = FMath::Max(WegMax, F.Weg);
			}
		}
	}
	for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
	{
		It->ZeigeWege(Wege);
		break;
	}

	// 3) Rohstoffe im Umkreis (Knoten der Welt): Anzahl und naechster je Art
	struct FRoh { int32 Anzahl = 0; float Naechster = TNumericLimits<float>::Max(); int32 Rein = 0; };
	TMap<FString, FRoh> Roh;
	for (TActorIterator<AFGResourceNode> It(World); It; ++It)
	{
		const float D = FVector::Dist2D(It->GetActorLocation(), mBodenMitte);
		if (D > 60000.f || !It->GetResourceClass())
		{
			continue;
		}
		FRoh& R = Roh.FindOrAdd(UFGItemDescriptor::GetItemName(It->GetResourceClass()).ToString());
		++R.Anzahl;
		R.Naechster = FMath::Min(R.Naechster, D);
		R.Rein += It->GetResourcePurity() == RP_Pure ? 1 : 0;
	}
	TArray<FString> RohText;
	Roh.ValueSort([](const FRoh& A, const FRoh& B) { return A.Naechster < B.Naechster; });
	for (const TPair<FString, FRoh>& R : Roh)
	{
		RohText.Add(FString::Printf(TEXT("%s %d (ab %.0f m%s)"), *R.Key, R.Value.Anzahl, R.Value.Naechster / 100.f,
			R.Value.Rein > 0 ? *FString::Printf(TEXT(", %d rein"), R.Value.Rein) : TEXT("")));
	}

	// 4) Bericht
	const FString Gelaende = Punkte == 0 ? FString(TEXT("kein Boden gemessen"))
		: FString::Printf(TEXT("Hoehenunterschied %.1f m auf 48 x 48 m (%d Punkte%s), %d Hindernisse%s"), Hub, Punkte,
			OhneBoden > 0 ? *FString::Printf(TEXT(", %d ohne Boden"), OhneBoden) : TEXT(""), Hindernisse,
			ArtText.Num() ? *FString::Printf(TEXT(" (%s)"), *FString::Join(ArtText, TEXT(", "))) : TEXT(""));
	Melde(FString::Printf(TEXT("%s %s (Stufe %s): %s"), *mId, *mBiom, TDStufenNamen[FMath::Clamp(mStufe, 0, 3)], *Gelaende));
	const int32 Netz = NetzPunkte();
	Melde(FString::Printf(TEXT("%s Fronten: %d von 16 Richtungen begehbar (Netz an %d/16 Punkten in 250 m), gewaehlt %s. Leuchtringe zeigen die Wege."), *mId, Gueltig, Netz,
		FrontText.Num() ? *FString::Join(FrontText, TEXT(", ")) : TEXT("keine")));
	Melde(FString::Printf(TEXT("%s Rohstoffe in 600 m: %s"), *mId, RohText.Num() ? *FString::Join(RohText, TEXT("; ")) : TEXT("keine")));

	mHatErgebnis = true;
	mErgebnisOrt = mBodenMitte;
	mErgebnisHub = Hub;
	mErgebnisHindernisse = Hindernisse;
	mErgebnisRichtungen = Gueltig;
	mErgebnisNetz = Netz;
	mErgebnisFronten = FString::Join(FrontText, TEXT(", "));
	mErgebnisRohstoffe = FString::Join(RohText, TEXT("; "));
	Melde(TEXT("Gefaellt dir der Platz: /td platz merken <grasfelder|felsenwueste|wald|duenen> [Notiz]"));
	EntferneInvoker();
	if (!mScanLaeuft)
	{
		Schuetze(false);
	}
	if (mScanLaeuft && mScanPhase == 2)
	{
		if (mScanTreffer.IsValidIndex(mScanPruefIndex - 1))
		{
			mScanTreffer[mScanPruefIndex - 1].Richtungen = Gueltig;
			mScanTreffer[mScanPruefIndex - 1].Fronten = FString::Join(FrontText, TEXT(", "));
		}
		GetWorld()->GetTimerManager().SetTimer(mScanTimer, this, &UTDKernplatz::ScanNaechstePruefung, 2.f, false);
	}

	// CSV zum Vergleich aller Kandidaten
	const FString Datei = FPaths::ProjectSavedDir() / TEXT("FicsitTD/Kernplaetze.csv");
	FString Zeile;
	if (!IFileManager::Get().FileExists(*Datei))
	{
		Zeile = TEXT("Zeit;Id;Biom;Stufe;X;Y;Z;Hoehenunterschied_m;Punkte;Hindernisse;Richtungen_begehbar;Fronten;Weg_min_m;Weg_max_m;Rohstoffe\n");
	}
	Zeile += FString::Printf(TEXT("%s;%s;%s;%s;%.0f;%.0f;%.0f;%.1f;%d;%d;%d;%s;%.0f;%.0f;%s\n"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")),
		*mId, *mBiom, TDStufenNamen[FMath::Clamp(mStufe, 0, 3)], mBodenMitte.X, mBodenMitte.Y, mBodenMitte.Z, Hub, Punkte, Hindernisse, Gueltig,
		*FString::Join(FrontText, TEXT(" | ")), WegMin / 100.f, WegMax / 100.f, *FString::Join(RohText, TEXT(" | ")));
	FFileHelper::SaveStringToFile(Zeile, *Datei, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

FString UTDKernplatz::Merke(const FString& Biom, const FString& Notiz, APawn* Spieler)
{
	FString Name;
	const int32 Stufe = BiomAus(Biom, Name);
	if (Stufe < 0)
	{
		return TEXT("Biom angeben: grasfelder, felsenwueste, wald oder duenen.");
	}
	if (!mHatErgebnis)
	{
		return TEXT("Erst pruefen: /td platz pruefen hier <Biom>, dann merken.");
	}
	if (Spieler && FVector::Dist2D(Spieler->GetActorLocation(), mErgebnisOrt) > 10000.f)
	{
		return TEXT("Du bist mehr als 100 m von der letzten Pruefung entfernt - bitte dort erneut pruefen.");
	}
	TSharedPtr<FJsonObject> Wurzel;
	FString Text;
	if (FFileHelper::LoadFileToString(Text, *AuswahlDatei()))
	{
		const TSharedRef<TJsonReader<>> Leser = TJsonReaderFactory<>::Create(Text);
		FJsonSerializer::Deserialize(Leser, Wurzel);
	}
	if (!Wurzel.IsValid())
	{
		Wurzel = MakeShared<FJsonObject>();
	}
	TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
	P->SetStringField(TEXT("Biom"), Name);
	P->SetStringField(TEXT("Stufe"), TDStufenNamen[Stufe]);
	P->SetNumberField(TEXT("X"), FMath::RoundToFloat(mErgebnisOrt.X));
	P->SetNumberField(TEXT("Y"), FMath::RoundToFloat(mErgebnisOrt.Y));
	P->SetNumberField(TEXT("Z"), FMath::RoundToFloat(mErgebnisOrt.Z));
	P->SetNumberField(TEXT("Blick"), Spieler ? FMath::RoundToFloat(Spieler->GetActorRotation().Yaw) : 0.f);
	P->SetNumberField(TEXT("Hoehenunterschied_m"), FMath::RoundToFloat(mErgebnisHub * 10.f) / 10.f);
	P->SetNumberField(TEXT("Hindernisse"), mErgebnisHindernisse);
	P->SetNumberField(TEXT("Richtungen_begehbar"), mErgebnisRichtungen);
	P->SetNumberField(TEXT("Netz_Punkte"), mErgebnisNetz);
	P->SetStringField(TEXT("Fronten"), mErgebnisFronten);
	P->SetStringField(TEXT("Rohstoffe"), mErgebnisRohstoffe);
	P->SetStringField(TEXT("Notiz"), Notiz);
	P->SetStringField(TEXT("Zeit"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M")));
	// 0.68 (Marius: mehrere Plaetze je Biom, zufaellig gewaehlt): Liste je Biom; ein alter Einzel-Eintrag wird uebernommen
	TArray<TSharedPtr<FJsonValue>> Liste;
	if (const TSharedPtr<FJsonValue>* Alt = Wurzel->Values.Find(Name))
	{
		if ((*Alt)->Type == EJson::Array)
		{
			Liste = (*Alt)->AsArray();
		}
		else if ((*Alt)->Type == EJson::Object)
		{
			Liste.Add(*Alt);
		}
	}
	// derselbe Platz (unter 50 m) wird ersetzt statt doppelt gespeichert
	Liste.RemoveAll([this](const TSharedPtr<FJsonValue>& V)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		return O.IsValid() && FVector2D::Distance(FVector2D(O->GetNumberField(TEXT("X")), O->GetNumberField(TEXT("Y"))), FVector2D(mErgebnisOrt)) < 5000.f;
	});
	Liste.Add(MakeShared<FJsonValueObject>(P));
	Wurzel->SetArrayField(Name, Liste);
	FString Aus;
	const TSharedRef<TJsonWriter<>> Schreiber = TJsonWriterFactory<>::Create(&Aus);
	FJsonSerializer::Serialize(Wurzel.ToSharedRef(), Schreiber);
	FFileHelper::SaveStringToFile(Aus, *AuswahlDatei(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz gemerkt: %s bei %s (%s)"), *Name, *mErgebnisOrt.ToCompactString(), *Notiz);
	return FString::Printf(TEXT("Gemerkt als Kernplatz %d fuer %s (Stufe %s) bei x %.0f / y %.0f / z %.0f%s. Datei: Saved/FicsitTD/Kernplaetze-Auswahl.json"),
		Liste.Num(), *Name, TDStufenNamen[Stufe], mErgebnisOrt.X, mErgebnisOrt.Y, mErgebnisOrt.Z, Notiz.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(", Notiz: %s"), *Notiz));
}

TArray<FString> UTDKernplatz::Gemerkt() const
{
	TArray<FString> Zeilen;
	FString Text;
	TSharedPtr<FJsonObject> Wurzel;
	if (FFileHelper::LoadFileToString(Text, *AuswahlDatei()))
	{
		const TSharedRef<TJsonReader<>> Leser = TJsonReaderFactory<>::Create(Text);
		FJsonSerializer::Deserialize(Leser, Wurzel);
	}
	if (!Wurzel.IsValid() || Wurzel->Values.Num() == 0)
	{
		Zeilen.Add(TEXT("Noch kein Kernplatz gemerkt."));
		return Zeilen;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>>& E : Wurzel->Values)
	{
		TArray<TSharedPtr<FJsonValue>> Eintraege;
		if (E.Value->Type == EJson::Array)
		{
			Eintraege = E.Value->AsArray();
		}
		else
		{
			Eintraege.Add(E.Value);
		}
		for (const TSharedPtr<FJsonValue>& V : Eintraege)
		{
		const TSharedPtr<FJsonObject> P = V->AsObject();
		if (!P.IsValid())
		{
			continue;
		}
		Zeilen.Add(FString::Printf(TEXT("%s: x %.0f / y %.0f / z %.0f, Hoehenunterschied %.1f m, %d Hindernisse, %d Richtungen begehbar%s"), *E.Key,
			P->GetNumberField(TEXT("X")), P->GetNumberField(TEXT("Y")), P->GetNumberField(TEXT("Z")), P->GetNumberField(TEXT("Hoehenunterschied_m")),
			(int32)P->GetNumberField(TEXT("Hindernisse")), (int32)P->GetNumberField(TEXT("Richtungen_begehbar")),
			P->GetStringField(TEXT("Notiz")).IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" - %s"), *P->GetStringField(TEXT("Notiz")))));
		}
	}
	return Zeilen;
}

// ======================= Scan (0.68) =======================
// Marius 04.11.2026: selbst nach Plaetzen suchen statt Kandidaten aus der Rohstoffkarte. Das Biom wird in 300-m-Schritten abgeflogen,
// an jeder Station ein 16-m-Raster gemessen (Boden, Hindernisse, Wasser). Danach werden alle Rasterpunkte als Kernmitte bewertet:
// Plattform 64 x 64 m eben und frei, Umfeld 40-150 m begehbar, ohne Wasser und nicht zu steil. Die besten 8 (mind. 250 m auseinander)
// bekommen die volle Pruefung mit Navigationsnetz und Fronten.

static constexpr float TDScanRaster = 1600.f;      // 16 m
static constexpr float TDScanStation = 30000.f;    // 300 m zwischen den Stationen
static constexpr int32 TDScanHalb = 11;            // +-11 Rasterpunkte = +-176 m je Station

struct FTDScanGebiet { int32 Stufe; float X; float Y; float Halb; };
// Startgebiete des Spiels (Kartendaten satisfactory-calculator.com, Ebene "spawn"), etwas groesser als dort
static const FTDScanGebiet TDScanGebiete[] = {
	{ 0,  -50000.f,  240000.f, 100000.f },   // Grasfelder
	{ 1, -220000.f,  -35000.f, 100000.f },   // Felsenwueste
	{ 2,   50000.f,  -90000.f, 100000.f },   // Noerdlicher Wald
	{ 3,  300000.f, -175000.f, 140000.f },   // Duenenwueste
};

// Weltrand (Kartendaten satisfactory-calculator.com, Ebene "worldBorder"); ausserhalb nimmt der Spieler Schaden (Test 0.68.0)
static const FVector2D TDWeltrand[] = {
	{ -232400.f, -370000.f }, { 450000.f, -370000.f }, { 450000.f, -33000.f }, { 192000.f, 335000.f },
	{ -237000.f, 335000.f }, { -341500.f, 96000.f }, { -341500.f, -313000.f },
};

/** Liegt der Punkt innerhalb des Weltrands und mindestens Abstand cm davon entfernt? */
static bool TDInnerhalbWeltrand(const FVector2D& P, float Abstand)
{
	const int32 N = UE_ARRAY_COUNT(TDWeltrand);
	bool bInnen = false;
	float MinDist = TNumericLimits<float>::Max();
	for (int32 i = 0, j = N - 1; i < N; j = i++)
	{
		const FVector2D& A = TDWeltrand[i];
		const FVector2D& C = TDWeltrand[j];
		if (((A.Y > P.Y) != (C.Y > P.Y)) && (P.X < (C.X - A.X) * (P.Y - A.Y) / (C.Y - A.Y) + A.X))
		{
			bInnen = !bInnen;
		}
		// Abstand zur Kante A-C
		const FVector2D AC = C - A;
		const float t = FMath::Clamp(FVector2D::DotProduct(P - A, AC) / FMath::Max(AC.SizeSquared(), 1.f), 0.f, 1.f);
		MinDist = FMath::Min(MinDist, FVector2D::Distance(P, A + AC * t));
	}
	return bInnen && MinDist >= Abstand;
}

FString UTDKernplatz::StarteScan(const FString& Biom, APawn* Spieler)
{
	FString Name;
	const int32 Stufe = BiomAus(Biom, Name);
	if (Stufe < 0)
	{
		return TEXT("Biom angeben: /td platz scan <grasfelder|felsenwueste|wald|duenen>");
	}
	if (!Spieler)
	{
		return TEXT("Nur als Spieler.");
	}
	StoppeScan();
	const FTDScanGebiet& G = TDScanGebiete[Stufe];
	mScanStationen.Reset();
	// Schlangenlinie, damit die Fluege kurz bleiben
	int32 Reihe = 0;
	for (float Y = G.Y - G.Halb; Y <= G.Y + G.Halb + 1.f; Y += TDScanStation, ++Reihe)
	{
		TArray<FVector2D> Zeile;
		for (float X = G.X - G.Halb; X <= G.X + G.Halb + 1.f; X += TDScanStation)
		{
			// Test 0.68.0: Stationen ausserhalb der Spielwelt -> Schaden bis zum Tod; 200 m Abstand zum Rand
			if (TDInnerhalbWeltrand(FVector2D(X, Y), 20000.f))
			{
				Zeile.Add(FVector2D(X, Y));
			}
		}
		if (Reihe % 2)
		{
			Algo::Reverse(Zeile);
		}
		mScanStationen.Append(Zeile);
	}
	mSpieler = Spieler;
	if (!mHatRueckweg)
	{
		mHatRueckweg = true;
		mRueckweg = Spieler->GetActorLocation();
	}
	mScanLaeuft = true;
	mScanPhase = 1;
	mScanBiom = Name;
	mScanStufe = Stufe;
	mScanIndex = 0;
	mScanWarten = 0;
	mScanZ = Spieler->GetActorLocation().Z;
	mScanKarte.Reset();
	mScanTreffer.Reset();
	mScanStart = FPlatformTime::Seconds();
	if (ACharacter* Figur = Cast<ACharacter>(Spieler))
	{
		Figur->GetCharacterMovement()->StopMovementImmediately();
		Figur->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	GetWorld()->GetTimerManager().SetTimer(mScanTimer, this, &UTDKernplatz::ScanSchritt, 0.5f, true, 0.1f);
	return FString::Printf(TEXT("Scan %s: %d Stationen im Abstand von 300 m, etwa %d min. Du fliegst mit - nicht steuern. Abbrechen: /td platz scan stop"),
		*Name, mScanStationen.Num(), FMath::Max(1, FMath::RoundToInt(mScanStationen.Num() * 4.f / 60.f)) + 4);
}

FString UTDKernplatz::StoppeScan()
{
	const bool bLief = mScanLaeuft;
	mScanLaeuft = false;
	mScanPhase = 0;
	Schuetze(false);
	GetWorld()->GetTimerManager().ClearTimer(mScanTimer);
	if (APawn* Spieler = mSpieler.Get())
	{
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
	return bLief ? TEXT("Scan abgebrochen. /td platz zurueck bringt dich zum Ausgangsort.") : TEXT("Kein Scan aktiv.");
}

void UTDKernplatz::ScanSchritt()
{
	APawn* Spieler = mSpieler.Get();
	if (!mScanLaeuft || mScanPhase != 1 || !Spieler)
	{
		return;
	}
	Schuetze(true);
	if (mScanIndex >= mScanStationen.Num())
	{
		GetWorld()->GetTimerManager().ClearTimer(mScanTimer);
		ScanBewerten();
		return;
	}
	const FVector2D S = mScanStationen[mScanIndex];
	if (mScanWarten == 0)
	{
		// hoch ueber die Station, schweben
		Spieler->TeleportTo(FVector(S.X, S.Y, mScanZ + 8000.f), Spieler->GetActorRotation(), false, true);
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->StopMovementImmediately();
		}
	}
	++mScanWarten;
	FVector Grund;
	bool bHindernis = false;
	FString Was;
	const bool bBoden = Boden(S.X, S.Y, mScanZ, Grund, bHindernis, Was);
	// Boden da und 2 s Zeit fuer die Kollision der Umgebung, oder nach 12 s aufgeben (Wasser, nicht geladen)
	if ((bBoden && mScanWarten >= 4) || mScanWarten >= 24)
	{
		if (bBoden)
		{
			mScanZ = Grund.Z;
			ScanMessen();
		}
		mScanWarten = 0;
		++mScanIndex;
		if (mScanIndex % 10 == 0)
		{
			Melde(FString::Printf(TEXT("Scan %s: %d von %d Stationen, %d Messpunkte"), *mScanBiom, mScanIndex, mScanStationen.Num(), mScanKarte.Num()));
		}
	}
}

void UTDKernplatz::ScanMessen()
{
	const FVector2D S = mScanStationen[mScanIndex];
	const int32 MX = FMath::RoundToInt(S.X / TDScanRaster), MY = FMath::RoundToInt(S.Y / TDScanRaster);
	FCollisionQueryParams Wasserfrage(TEXT("TDKernplatzWasser"), false);
	for (int32 dx = -TDScanHalb; dx <= TDScanHalb; ++dx)
	{
		for (int32 dy = -TDScanHalb; dy <= TDScanHalb; ++dy)
		{
			const FIntPoint K(MX + dx, MY + dy);
			if (mScanKarte.Contains(K))
			{
				continue;
			}
			FVector P;
			bool bHindernis = false;
			FString Was;
			if (!Boden(K.X * TDScanRaster, K.Y * TDScanRaster, mScanZ, P, bHindernis, Was))
			{
				continue;
			}
			FScanPunkt Pkt;
			Pkt.Z = P.Z;
			Pkt.bHindernis = bHindernis && !Was.Contains(TEXT("Character"));
			// Wasser wie bei der Frontsuche: Sichtstrahl trifft einen Wasser-Actor
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0.f, 0.f, 3000.f), P - FVector(0.f, 0.f, 100.f), ECC_Visibility, Wasserfrage))
			{
				Pkt.bWasser = Hit.GetActor() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Water"));
			}
			mScanKarte.Add(K, Pkt);
		}
	}
}

void UTDKernplatz::ScanBewerten()
{
	mScanTreffer.Reset();
	TArray<FScanTreffer> Alle;
	for (const TPair<FIntPoint, FScanPunkt>& E : mScanKarte)
	{
		const FIntPoint M = E.Key;
		if ((M.X + M.Y) % 2 != 0)
		{
			continue;   // jeden zweiten Punkt als Mitte pruefen (32 m), reicht und halbiert die Zeit
		}
		// Plattform 64 x 64 m (+-2 Punkte): vollstaendig, eben, trocken, frei
		float ZMin = TNumericLimits<float>::Max(), ZMax = -TNumericLimits<float>::Max();
		int32 Hindernisse = 0;
		bool bOk = true;
		for (int32 dx = -2; dx <= 2 && bOk; ++dx)
		{
			for (int32 dy = -2; dy <= 2 && bOk; ++dy)
			{
				const FScanPunkt* P = mScanKarte.Find(FIntPoint(M.X + dx, M.Y + dy));
				if (!P || P->bWasser)
				{
					bOk = false;
					break;
				}
				ZMin = FMath::Min(ZMin, P->Z);
				ZMax = FMath::Max(ZMax, P->Z);
				Hindernisse += P->bHindernis ? 1 : 0;
			}
		}
		const float Hub = (ZMax - ZMin) / 100.f;
		if (!bOk || Hub > 6.f || Hindernisse > 3)
		{
			continue;
		}
		// Umfeld 40-150 m: frei (kein Hindernis, kein Wasser), eben zur Plattform (+-20 m) und wenig rau
		int32 Gesamt = 0, Frei = 0, Eben = 0;
		float Rau = 0.f;
		const float ZMitte = E.Value.Z;
		for (int32 dx = -9; dx <= 9; ++dx)
		{
			for (int32 dy = -9; dy <= 9; ++dy)
			{
				const float R = FMath::Sqrt(float(dx * dx + dy * dy)) * TDScanRaster;
				if (R < 4000.f || R > 15000.f)
				{
					continue;
				}
				++Gesamt;
				const FScanPunkt* P = mScanKarte.Find(FIntPoint(M.X + dx, M.Y + dy));
				if (!P)
				{
					continue;
				}
				Frei += !P->bHindernis && !P->bWasser ? 1 : 0;
				Eben += FMath::Abs(P->Z - ZMitte) < 2000.f ? 1 : 0;
				if (const FScanPunkt* N = mScanKarte.Find(FIntPoint(M.X + dx + 1, M.Y + dy)))
				{
					Rau += FMath::Abs(N->Z - P->Z) / 100.f;
				}
			}
		}
		if (Gesamt == 0)
		{
			continue;
		}
		FScanTreffer T;
		T.Ort = FVector(M.X * TDScanRaster, M.Y * TDScanRaster, ZMitte);
		T.Hub = Hub;
		T.Hindernisse = Hindernisse;
		T.Frei = float(Frei) / Gesamt;
		T.Eben = float(Eben) / Gesamt;
		T.Rau = Rau / Gesamt;
		// Wert: freies, ebenes Umfeld zaehlt am meisten; Plattform-Hub, Hindernisse und Rauheit ziehen ab
		T.Wert = 60.f * T.Frei + 40.f * T.Eben - 3.f * T.Hub - 4.f * T.Hindernisse - 2.f * T.Rau;
		Alle.Add(T);
	}
	Alle.Sort([](const FScanTreffer& A, const FScanTreffer& B) { return A.Wert > B.Wert; });
	for (const FScanTreffer& T : Alle)
	{
		if (mScanTreffer.Num() >= 8)
		{
			break;
		}
		if (mScanTreffer.ContainsByPredicate([&T](const FScanTreffer& O) { return FVector::Dist2D(O.Ort, T.Ort) < 25000.f; }))
		{
			continue;
		}
		mScanTreffer.Add(T);
	}
	const double Minuten = (FPlatformTime::Seconds() - mScanStart) / 60.0;
	Melde(FString::Printf(TEXT("Scan %s: %d Messpunkte in %.0f min, %d moegliche Mitten, die besten %d:"), *mScanBiom, mScanKarte.Num(), Minuten, Alle.Num(), mScanTreffer.Num()));
	const FString Datei = FPaths::ProjectSavedDir() / FString::Printf(TEXT("FicsitTD/Kernplatz-Scan-%s.csv"), *mScanBiom.Replace(TEXT(" "), TEXT("")));
	FString Csv = TEXT("Rang;X;Y;Z;Wert;Plattform_Hub_m;Plattform_Hindernisse;Umfeld_frei;Umfeld_eben;Rauheit_m\n");
	for (int32 i = 0; i < mScanTreffer.Num(); ++i)
	{
		const FScanTreffer& T = mScanTreffer[i];
		Melde(FString::Printf(TEXT("S%d: x %.0f / y %.0f, Wert %.0f - Plattform %.1f m Hub, %d Hindernisse; Umfeld %.0f %% frei, %.0f %% eben, Rauheit %.1f m"),
			i + 1, T.Ort.X, T.Ort.Y, T.Wert, T.Hub, T.Hindernisse, 100.f * T.Frei, 100.f * T.Eben, T.Rau));
		Csv += FString::Printf(TEXT("S%d;%.0f;%.0f;%.0f;%.1f;%.1f;%d;%.2f;%.2f;%.2f\n"), i + 1, T.Ort.X, T.Ort.Y, T.Ort.Z, T.Wert, T.Hub, T.Hindernisse, T.Frei, T.Eben, T.Rau);
	}
	FFileHelper::SaveStringToFile(Csv, *Datei, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (mScanTreffer.Num() == 0)
	{
		Melde(TEXT("Keine ebene freie Flaeche gefunden. /td platz zurueck"));
		StoppeScan();
		return;
	}
	// Phase 2: die Treffer nacheinander voll pruefen (Netz, Fronten, Rohstoffe)
	Melde(TEXT("Pruefe die Treffer jetzt einzeln mit Navigationsnetz und Fronten (je etwa 30-90 s) ..."));
	mScanPhase = 2;
	mScanPruefIndex = 0;
	ScanNaechstePruefung();
}

void UTDKernplatz::ScanNaechstePruefung()
{
	APawn* Spieler = mSpieler.Get();
	if (!mScanLaeuft || mScanPhase != 2 || !Spieler)
	{
		return;
	}
	if (mScanPruefIndex >= mScanTreffer.Num())
	{
		mScanLaeuft = false;
		mScanPhase = 0;
		TArray<int32> Reihenfolge;
		for (int32 i = 0; i < mScanTreffer.Num(); ++i)
		{
			Reihenfolge.Add(i);
		}
		auto Gesamt = [this](int32 i) { const FScanTreffer& T = mScanTreffer[i]; return T.Wert + 4.f * FMath::Max(0, T.Richtungen); };
		Reihenfolge.Sort([&Gesamt](int32 A, int32 B) { return Gesamt(A) > Gesamt(B); });
		TArray<FString> Gut, Schwach;
		for (const int32 i : Reihenfolge)
		{
			const FScanTreffer& T = mScanTreffer[i];
			const FString Eintrag = FString::Printf(TEXT("S%d (%d/16, %.1f m)"), i + 1, FMath::Max(0, T.Richtungen), T.Hub);
			(T.Richtungen >= 8 ? Gut : Schwach).Add(Eintrag);
		}
		Melde(FString::Printf(TEXT("Empfehlung %s nach Gelaende und Wegen: %s%s"), *mScanBiom, Gut.Num() ? *FString::Join(Gut, TEXT(", ")) : TEXT("keiner mit 8+ Richtungen"),
			Schwach.Num() ? *FString::Printf(TEXT(" | schwach: %s"), *FString::Join(Schwach, TEXT(", "))) : TEXT("")));
		Schuetze(false);
		Melde(FString::Printf(TEXT("Scan %s fertig. Ansehen: /td platz pruefen S1 .. S%d, gefaellt einer: /td platz merken %s [Notiz]. Ergebnisse auch in Saved/FicsitTD/Kernplaetze.csv"),
			*mScanBiom, mScanTreffer.Num(), *mScanBiom.Left(4).ToLower()));
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		return;
	}
	const FString Id = FString::Printf(TEXT("S%d"), ++mScanPruefIndex);
	Melde(Pruefe(Id, Spieler));
}

// ======================= Feste Plaetze und Bau (0.69, Konzept-Kernplaetze S3) =======================
// Auswahl aus den Scans vom 04.11.2026 (Kernplatz-Scan-<Biom>.csv): je Biom 3 Plaetze, moeglichst verteilt, Erz in der Naehe.
struct FTDFesterPlatz { int32 Stufe; const TCHAR* Id; float X; float Y; float Z; };
static const FTDFesterPlatz TDFestePlaetze[] = {
	{ 0, TEXT("Gras-S1"),   -84800.f,  244800.f, -2788.f },   // 15/16, 0,0 m
	{ 0, TEXT("Gras-S8"),   -54400.f,  236800.f, -3926.f },   // 15/16, 1,0 m
	{ 0, TEXT("Gras-S2"),   -59200.f,  264000.f, -3761.f },   // 10/16, 1,9 m
	{ 1, TEXT("Fels-S3"),  -166400.f, -105600.f,    42.f },   // 15/16, 0,9 m, Osten
	{ 1, TEXT("Fels-S8"),  -244800.f, -123200.f,  3872.f },   // 12/16, 1,6 m, Sueden
	{ 1, TEXT("Fels-S1"),  -243200.f,  -60800.f,  -322.f },   // 16/16, 0,1 m
	{ 2, TEXT("Wald-S2"),     6400.f, -102400.f, 11884.f },   // 15/16, 0,6 m
	{ 2, TEXT("Wald-S3"),    68800.f,  -91200.f,  9399.f },   // 15/16, 3,2 m
	{ 2, TEXT("Wald-S5"),   -54400.f,  -32000.f, 17481.f },   // 11/16, 3,1 m
	{ 3, TEXT("Duene-S1"),  336000.f, -198400.f,  4219.f },   // 15/16, 0,1 m
	{ 3, TEXT("Duene-S6"),  347200.f, -145600.f,  4105.f },   // 16/16, 3,1 m
	{ 3, TEXT("Duene-S8"),  294400.f, -144000.f,  4333.f },   // 16/16, 4,2 m
};
static const TCHAR* TDFundamentKlasse = TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_8x4_01.Build_Foundation_8x4_01_C");
static const TCHAR* TDFundamentRezept = TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x4_01.Recipe_Foundation_8x4_01_C");
static const TCHAR* TDKernKlasse = TEXT("/FicsitTD/Buildings/DefenseCore/Build_TD_DefenseCore.Build_TD_DefenseCore_C");
static const TCHAR* TDKernRezept = TEXT("/FicsitTD/Buildings/DefenseCore/Recipe_TD_DefenseCore.Recipe_TD_DefenseCore_C");
static constexpr int32 TDPlattformKanten = 6;      // 6 x 6 Fundamente
static constexpr float TDFundament = 800.f;        // 8 m
static constexpr float TDFundamentHoehe = 400.f;   // 4 m

/** Oberkante des Modells ueber dem Pivot (aus den Lightweight-Instanzdaten der Klasse) */
static float TDOberkante(UClass* Klasse, float Ersatz)
{
	const AFGBuildable* CDO = Klasse ? Cast<AFGBuildable>(Klasse->GetDefaultObject()) : nullptr;
	const UAbstractInstanceDataObject* Daten = CDO ? CDO->GetLightweightInstanceData() : nullptr;
	if (!Daten)
	{
		return Ersatz;
	}
	float ZMax = -TNumericLimits<float>::Max();
	for (const FInstanceData& I : Daten->GetInstanceData())
	{
		if (I.StaticMesh)
		{
			const FBox Box = I.StaticMesh->GetBoundingBox().TransformBy(I.RelativeTransform);
			ZMax = FMath::Max(ZMax, Box.Max.Z);
		}
	}
	return ZMax > -TNumericLimits<float>::Max() ? ZMax : Ersatz;
}

/** Ausdehnung des Modells um den Pivot (Instanzdaten der Klasse) */
static FBox TDModellBox(UClass* Klasse)
{
	FBox Box(ForceInit);
	const AFGBuildable* CDO = Klasse ? Cast<AFGBuildable>(Klasse->GetDefaultObject()) : nullptr;
	const UAbstractInstanceDataObject* Daten = CDO ? CDO->GetLightweightInstanceData() : nullptr;
	if (Daten)
	{
		for (const FInstanceData& I : Daten->GetInstanceData())
		{
			if (I.StaticMesh)
			{
				Box += I.StaticMesh->GetBoundingBox().TransformBy(I.RelativeTransform);
			}
		}
	}
	return Box;
}

/** Steigt die Rampe zur lokalen +X-Seite an? Aus der Kollisionsform (Konvex-Huelle): hoechster Punkt bei +X oder -X */
static bool TDRampeSteigtNachX(UClass* Klasse, FString& Wie)
{
	const AFGBuildable* CDO = Klasse ? Cast<AFGBuildable>(Klasse->GetDefaultObject()) : nullptr;
	const UAbstractInstanceDataObject* Daten = CDO ? CDO->GetLightweightInstanceData() : nullptr;
	float ZPlus = -TNumericLimits<float>::Max(), ZMinus = -TNumericLimits<float>::Max();
	if (Daten)
	{
		for (const FInstanceData& I : Daten->GetInstanceData())
		{
			const UBodySetup* Koerper = I.StaticMesh ? I.StaticMesh->GetBodySetup() : nullptr;
			if (!Koerper)
			{
				continue;
			}
			for (const FKConvexElem& K : Koerper->AggGeom.ConvexElems)
			{
				for (const FVector& V : K.VertexData)
				{
					const FVector P = I.RelativeTransform.TransformPosition(K.GetTransform().TransformPosition(V));
					if (P.X > 100.f) ZPlus = FMath::Max(ZPlus, P.Z);
					if (P.X < -100.f) ZMinus = FMath::Max(ZMinus, P.Z);
				}
			}
		}
	}
	if (ZPlus <= -TNumericLimits<float>::Max() || ZMinus <= -TNumericLimits<float>::Max())
	{
		Wie = TEXT("ohne Kollisionsdaten, angenommen +X");
		return true;
	}
	Wie = FString::Printf(TEXT("Kollision: +X %.0f, -X %.0f"), ZPlus, ZMinus);
	return ZPlus >= ZMinus;
}

/** Hoechster Punkt der Kollisionsform je Ecke (Quadranten +X+Y, +X-Y, -X+Y, -X-Y); false ohne Kollisionsdaten */
static bool TDEckHoehen(UClass* Klasse, float Out[4])
{
	for (int32 i = 0; i < 4; ++i)
	{
		Out[i] = -TNumericLimits<float>::Max();
	}
	const AFGBuildable* CDO = Klasse ? Cast<AFGBuildable>(Klasse->GetDefaultObject()) : nullptr;
	const UAbstractInstanceDataObject* Daten = CDO ? CDO->GetLightweightInstanceData() : nullptr;
	if (!Daten)
	{
		return false;
	}
	for (const FInstanceData& I : Daten->GetInstanceData())
	{
		const UBodySetup* Koerper = I.StaticMesh ? I.StaticMesh->GetBodySetup() : nullptr;
		if (!Koerper)
		{
			continue;
		}
		for (const FKConvexElem& K : Koerper->AggGeom.ConvexElems)
		{
			for (const FVector& V : K.VertexData)
			{
				const FVector P = I.RelativeTransform.TransformPosition(K.GetTransform().TransformPosition(V));
				if (FMath::Abs(P.X) < 250.f || FMath::Abs(P.Y) < 250.f)
				{
					continue;
				}
				const int32 Q = (P.X > 0.f ? 0 : 2) + (P.Y > 0.f ? 0 : 1);
				Out[Q] = FMath::Max(Out[Q], P.Z);
			}
		}
	}
	for (int32 i = 0; i < 4; ++i)
	{
		if (Out[i] <= -TNumericLimits<float>::Max())
		{
			return false;
		}
	}
	return true;
}

/** 0.69.2 (Marius: Baeume um den Kern weg): Bewuchs (Baeume, Bueschel, kleine Felsen) im Radius entfernen wie die Kettensaege */
int32 TDRaeumeBewuchs(UWorld* World, const FVector& Mitte, float Radius, bool bAuchKakteen, TSet<FString>* OutUebrig)
{
	AFGFoliageRemovalSubsystem* Foliage = AFGFoliageRemovalSubsystem::Get(World);
	if (!Foliage)
	{
		return 0;
	}
	TMap<UHierarchicalInstancedStaticMeshComponent*, TArray<int32>> Treffer;
	Foliage->GetFoliageWithinRadius(Mitte, Radius, Treffer);
	APlayerController* PC = World->GetFirstPlayerController();
	int32 Weg = 0;
	for (TPair<UHierarchicalInstancedStaticMeshComponent*, TArray<int32>>& T : Treffer)
	{
		if (!T.Key || T.Value.Num() == 0)
		{
			continue;
		}
		if (!AFGFoliageRemovalSubsystem::IsRemovable(Foliage->GetFoliageType(T.Key)))
		{
			const FString Modell = T.Key->GetStaticMesh() ? T.Key->GetStaticMesh()->GetName() : FString(TEXT("-"));
			if (!bAuchKakteen || !Modell.Contains(TEXT("Cactus")))
			{
				if (OutUebrig)
				{
					OutUebrig->Add(Modell);
				}
				continue;
			}
		}
		// hoechste Indizes zuerst, damit die anderen gueltig bleiben
		T.Value.Sort([](int32 A, int32 B) { return A > B; });
		if (Foliage->RemoveFoliageInstances(PC, T.Key, T.Value))
		{
			Weg += T.Value.Num();
		}
	}
	return Weg;
}

FString UTDKernplatz::sVorgabePlatz;

FString UTDKernplatz::BauePlatz(const FString& Biom, const FString& Nr, APawn* Spieler)
{
	FString Name;
	const int32 Stufe = BiomAus(Biom, Name);
	if (Stufe < 0 || !Spieler)
	{
		return TEXT("/td platz bauen <grasfelder|felsenwueste|wald|duenen> [1-3]");
	}
	for (TActorIterator<ATDDefenseCore> It(GetWorld()); It; ++It)
	{
		return TEXT("Es steht schon ein Abwehrkern. /td platz bauen ist fuer einen neuen Spielstand gedacht (ein Kern je Welt).");
	}
	TArray<const FTDFesterPlatz*> Auswahl;
	for (const FTDFesterPlatz& P : TDFestePlaetze)
	{
		if (P.Stufe == Stufe)
		{
			Auswahl.Add(&P);
		}
	}
	const int32 Index = Nr.IsEmpty() ? FMath::RandRange(0, Auswahl.Num() - 1) : FMath::Clamp(FCString::Atoi(*Nr) - 1, 0, Auswahl.Num() - 1);
	const FTDFesterPlatz* P = Auswahl[Index];
	GetWorld()->GetTimerManager().ClearTimer(mTimer);
	StoppeScan();
	mSpieler = Spieler;
	mId = P->Id;
	mBiom = Name;
	mStufe = Stufe;
	mZiel = FVector(P->X, P->Y, P->Z);
	mVersuche = 0;
	mBauen = true;
	Spieler->TeleportTo(mZiel + FVector(0.f, 0.f, 5000.f), Spieler->GetActorRotation(), false, true);
	if (ACharacter* Figur = Cast<ACharacter>(Spieler))
	{
		Figur->GetCharacterMovement()->StopMovementImmediately();
		Figur->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	}
	GetWorld()->GetTimerManager().SetTimer(mTimer, this, &UTDKernplatz::SucheBoden, 1.f, true, 1.f);
	return TDF(TEXT("Baue den Kernplatz %s (%s, Stufe %s, %d von %d Plaetzen): fliege hin und warte auf das Gelaende ..."),
		TEXT("Building core site %s (%s, difficulty %s, %d of %d sites): flying there and waiting for the terrain ..."),
		P->Id, *TDBiomAnzeige(Stufe), *TDStufeAnzeige(Stufe), Index + 1, Auswahl.Num());
}

void UTDKernplatz::BaueJetzt()
{
	mBauen = false;
	UWorld* World = GetWorld();
	APawn* Spieler = mSpieler.Get();
	AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
	UClass* Fundament = LoadClass<AFGBuildable>(nullptr, TDFundamentKlasse);
	UClass* FundamentRezept = LoadClass<UFGRecipe>(nullptr, TDFundamentRezept);
	UClass* KernKlasse = LoadClass<AFGBuildable>(nullptr, TDKernKlasse);
	UClass* KernRezept = LoadClass<UFGRecipe>(nullptr, TDKernRezept);
	if (!Bau || !Fundament || !KernKlasse)
	{
		Melde(TDT(TEXT("Bauen nicht moeglich: Fundament- oder Kernklasse nicht gefunden."), TEXT("Cannot build: foundation or core class not found.")));
		Schuetze(false);
		return;
	}
	// 1) Gelaende je Fundament: Mitte und vier Ecken
	const float Halb = TDPlattformKanten * TDFundament * 0.5f;
	TArray<float> Hoehen;
	TArray<float> ZellMin;
	ZellMin.Init(TNumericLimits<float>::Max(), TDPlattformKanten * TDPlattformKanten);
	for (int32 ix = 0; ix < TDPlattformKanten; ++ix)
	{
		for (int32 iy = 0; iy < TDPlattformKanten; ++iy)
		{
			const float CX = mBodenMitte.X - Halb + (ix + 0.5f) * TDFundament;
			const float CY = mBodenMitte.Y - Halb + (iy + 0.5f) * TDFundament;
			for (const FVector2D& O : { FVector2D(0.f, 0.f), FVector2D(-390.f, -390.f), FVector2D(390.f, -390.f), FVector2D(-390.f, 390.f), FVector2D(390.f, 390.f) })
			{
				FVector G;
				bool bHindernis = false;
				FString Was;
				if (Boden(CX + O.X, CY + O.Y, mBodenMitte.Z, G, bHindernis, Was))
				{
					Hoehen.Add(G.Z);
					ZellMin[ix * TDPlattformKanten + iy] = FMath::Min(ZellMin[ix * TDPlattformKanten + iy], G.Z);
				}
			}
		}
	}
	if (Hoehen.Num() == 0)
	{
		Melde(TDT(TEXT("Bauen nicht moeglich: kein Boden unter der Plattform."), TEXT("Cannot build: no ground under the platform.")));
		Schuetze(false);
		return;
	}
	Hoehen.Sort();
	// Oberkante: Mittelwert der oberen Haelfte + 30 cm. Hoehere Stellen verschwinden im Fundament, tiefere bekommen Stapel
	float Summe = 0.f;
	int32 Anzahl = 0;
	for (int32 i = Hoehen.Num() / 2; i < Hoehen.Num(); ++i)
	{
		Summe += Hoehen[i];
		++Anzahl;
	}
	// Test 0.69.1: Gras schien durch die Fundamente -> 1 m hoeher, ein Rampenring fuehrt hinunter
	const float Oben = Summe / FMath::Max(1, Anzahl) + 130.f;
	const float FundamentOben = TDOberkante(Fundament, TDFundamentHoehe * 0.5f);

	// 2) Fundamente wie beim Bauen im Spiel (BeginSpawnBuildable, danach Lightweight)
	int32 Gesetzt = 0, Stapel = 0;
	for (int32 ix = 0; ix < TDPlattformKanten; ++ix)
	{
		for (int32 iy = 0; iy < TDPlattformKanten; ++iy)
		{
			const float CX = mBodenMitte.X - Halb + (ix + 0.5f) * TDFundament;
			const float CY = mBodenMitte.Y - Halb + (iy + 0.5f) * TDFundament;
			const float Tiefe = Oben - ZellMin[ix * TDPlattformKanten + iy];
			const int32 Lagen = FMath::Clamp(FMath::CeilToInt(Tiefe / TDFundamentHoehe), 1, 8);
			Stapel = FMath::Max(Stapel, Lagen);
			for (int32 k = 0; k < Lagen; ++k)
			{
				const FVector Ort(CX, CY, Oben - k * TDFundamentHoehe - FundamentOben);
				if (AFGBuildable* F = Bau->BeginSpawnBuildable(Fundament, FTransform(FRotator::ZeroRotator, Ort)))
				{
					F->SetBuiltWithRecipe(FundamentRezept);
					F->FinishSpawning(FTransform(FRotator::ZeroRotator, Ort));
					++Gesetzt;
				}
			}
		}
	}

	// 2b) Rampenring eine Reihe weiter draussen (je Seite 6) plus Ecken, Gefaelle nach aussen; darunter Fundamente bis zum Boden.
	// Test 0.69.5: 1-m- und 2-m-Rampen nebeneinander ergaben Stufen -> ein Rampentyp fuer den ganzen Ring (der groesste noetige).
	int32 Rampen = 0, Ecken = 0;
	FString RampeWie, EckWie;
	{
		static const TCHAR* RampeKlassen[] = {
			TEXT("/Game/FactoryGame/Buildable/Building/Ramp/Build_Ramp_8x1_01.Build_Ramp_8x1_01_C"),
			TEXT("/Game/FactoryGame/Buildable/Building/Ramp/Build_Ramp_8x2_01.Build_Ramp_8x2_01_C"),
			TEXT("/Game/FactoryGame/Buildable/Building/Ramp/Build_Ramp_8x4_01.Build_Ramp_8x4_01_C") };
		static const TCHAR* RampeRezepte[] = {
			TEXT("/Game/FactoryGame/Recipes/Buildings/Ramps/Recipe_Ramp_8x1_01.Recipe_Ramp_8x1_01_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Ramps/Recipe_Ramp_8x2_01.Recipe_Ramp_8x2_01_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Ramps/Recipe_Ramp_8x4_01.Recipe_Ramp_8x4_01_C") };
		// Eck-Rampe mit einer hohen Ecke (Variante _02; _01 hat drei hohe Ecken, Test 0.69.5)
		static const TCHAR* EckKlassen[] = {
			TEXT("/Game/FactoryGame/Buildable/Building/Ramp/Build_Ramp_Diagonal_8x1_02.Build_Ramp_Diagonal_8x1_02_C"),
			TEXT("/Game/FactoryGame/Buildable/Building/Ramp/Build_Ramp_Diagonal_8x2_02.Build_Ramp_Diagonal_8x2_02_C"),
			TEXT("/Game/FactoryGame/Buildable/Building/Ramp/Build_Ramp_Diagonal_8x4_02.Build_Ramp_Diagonal_8x4_02_C") };
		static const TCHAR* EckRezepte[] = {
			TEXT("/Game/FactoryGame/Recipes/Buildings/Ramps/Recipe_Ramp_Diagonal_8x1_02.Recipe_Ramp_Diagonal_8x1_02_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Ramps/Recipe_Ramp_Diagonal_8x2_02.Recipe_Ramp_Diagonal_8x2_02_C"),
			TEXT("/Game/FactoryGame/Recipes/Buildings/Ramps/Recipe_Ramp_Diagonal_8x4_02.Recipe_Ramp_Diagonal_8x4_02_C") };

		// Zellen des Rings: 4 Seiten x 6 und 4 Ecken; je Zelle Mitte, Richtung nach aussen, Gefaelle am Aussenrand, tiefster Boden
		struct FZelle { FVector2D C; FVector2D Aussen; bool bEcke; float Rand; float Tief; };
		TArray<FZelle> Zellen;
		const FVector2D Seiten[] = { FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f) };
		for (const FVector2D& A : Seiten)
		{
			const FVector2D Quer(-A.Y, A.X);
			for (int32 i = 0; i < TDPlattformKanten; ++i)
			{
				Zellen.Add({ FVector2D(mBodenMitte) + A * (Halb + 0.5f * TDFundament) + Quer * (-Halb + (i + 0.5f) * TDFundament), A, false, 0.f, 0.f });
			}
		}
		for (const FVector2D& E : { FVector2D(1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, 1.f), FVector2D(-1.f, -1.f) })
		{
			Zellen.Add({ FVector2D(mBodenMitte) + E * (Halb + 0.5f * TDFundament), E, true, 0.f, 0.f });
		}
		float GefaelleMax = 0.f;
		for (FZelle& Z : Zellen)
		{
			FVector G;
			bool bH = false;
			FString W;
			Z.Rand = Oben - 100.f;
			Z.Tief = Oben;
			for (const FVector2D& O : { FVector2D(0.f, 0.f), FVector2D(-380.f, -380.f), FVector2D(380.f, -380.f), FVector2D(-380.f, 380.f), FVector2D(380.f, 380.f) })
			{
				if (Boden(Z.C.X + O.X, Z.C.Y + O.Y, Oben, G, bH, W))
				{
					Z.Tief = FMath::Min(Z.Tief, G.Z);
					// Aussenrand: Punkte auf der Seite nach aussen
					if (FVector2D::DotProduct(O, Z.Aussen) > 0.f)
					{
						Z.Rand = FMath::Min(Z.Rand, G.Z);
					}
				}
			}
			GefaelleMax = FMath::Max(GefaelleMax, Oben - Z.Rand);
		}
		const int32 r = GefaelleMax <= 130.f ? 0 : GefaelleMax <= 250.f ? 1 : 2;
		UClass* RKlasse = LoadClass<AFGBuildable>(nullptr, RampeKlassen[r]);
		UClass* RRezept = LoadClass<UFGRecipe>(nullptr, RampeRezepte[r]);
		UClass* EKlasse = LoadClass<AFGBuildable>(nullptr, EckKlassen[r]);
		UClass* ERezept = LoadClass<UFGRecipe>(nullptr, EckRezepte[r]);
		const FBox RBox = TDModellBox(RKlasse);
		const FBox EBox = TDModellBox(EKlasse);
		const bool bNachX = TDRampeSteigtNachX(RKlasse, RampeWie);
		RampeWie += FString::Printf(TEXT(", Typ 8x%d fuer den Ring (Gefaelle bis %.1f m)"), r == 0 ? 1 : r == 1 ? 2 : 4, GefaelleMax / 100.f);
		AFGLightweightBuildableSubsystem* Leicht = AFGLightweightBuildableSubsystem::Get(World);

		// Ecken: Drehung durch Ausprobieren finden (Variante _02 hat keine einfache Kollisionsform): Stueck setzen, Hoehen der vier
		// Ecken mit Strahlen messen; die hohe Ecke muss zur Plattform zeigen, sonst entfernen und 90 Grad weiterdrehen
		float EckDrehung = TNumericLimits<float>::Max();   // lokale Drehung, bei der die hohe Ecke nach -E zeigt (gilt fuer alle Ecken)
		auto SetzeFundamenteDarunter = [&](const FVector2D& C, float Unterkante, float Tief)
		{
			for (int32 k = 0; k < 8 && Unterkante > Tief; ++k)
			{
				const FVector FOrt(C.X, C.Y, Unterkante - FundamentOben);
				if (AFGBuildable* F = Bau->BeginSpawnBuildable(Fundament, FTransform(FRotator::ZeroRotator, FOrt)))
				{
					F->SetBuiltWithRecipe(FundamentRezept);
					F->FinishSpawning(FTransform(FRotator::ZeroRotator, FOrt));
					++Gesetzt;
				}
				Unterkante -= TDFundamentHoehe;
			}
		};
		for (const FZelle& Z : Zellen)
		{
			if (!Z.bEcke)
			{
				if (!RKlasse)
				{
					continue;
				}
				const FVector2D Innen = -Z.Aussen;
				float Gier = FMath::RadiansToDegrees(FMath::Atan2(Innen.Y, Innen.X)) + (bNachX ? 0.f : 180.f);
				const FVector Ort(Z.C.X, Z.C.Y, Oben - (RBox.IsValid ? RBox.Max.Z : 50.f));
				const FTransform T(FRotator(0.f, Gier, 0.f), Ort);
				if (AFGBuildable* R = Bau->BeginSpawnBuildable(RKlasse, T))
				{
					R->SetBuiltWithRecipe(RRezept);
					R->FinishSpawning(T);
					++Rampen;
				}
				SetzeFundamenteDarunter(Z.C, Ort.Z + (RBox.IsValid ? RBox.Min.Z : -50.f), Z.Tief);
				continue;
			}
			if (!EKlasse || !Leicht)
			{
				continue;
			}
			const FVector Ort(Z.C.X, Z.C.Y, Oben - (EBox.IsValid ? EBox.Max.Z : 50.f));
			const float Soll = FMath::RadiansToDegrees(FMath::Atan2(-Z.Aussen.Y, -Z.Aussen.X));   // Richtung zur Plattformmitte
			bool bGesetzt = false;
			for (int32 Versuch = 0; Versuch < 4 && !bGesetzt; ++Versuch)
			{
				const float Lokal = EckDrehung < TNumericLimits<float>::Max() ? EckDrehung : (45.f + 90.f * Versuch);
				const FTransform T(FRotator(0.f, Soll - Lokal, 0.f), Ort);
				FFactoryCustomizationData Farbe;
				FRuntimeBuildableInstanceData Daten(T, Farbe, ERezept, nullptr);
				const int32 Index = Leicht->AddFromBuildableInstanceData(EKlasse, Daten);
				// Hoehen innen (zur Plattform) und aussen messen
				float ZInnen = -TNumericLimits<float>::Max(), ZAussen = -TNumericLimits<float>::Max();
				for (const float Seite : { -1.f, 1.f })
				{
					const FVector2D P = Z.C + Z.Aussen * (Seite * 320.f);
					FHitResult Hit;
					FCollisionQueryParams Frage(TEXT("TDEcke"), true);
					if (World->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, Oben + 2000.f), FVector(P.X, P.Y, Oben - 2000.f), ECC_Visibility, Frage))
					{
						(Seite < 0.f ? ZInnen : ZAussen) = Hit.ImpactPoint.Z;
					}
				}
				// ohne Treffer (Kollision noch nicht da) laesst sich nichts pruefen -> erste Drehung behalten
				const bool bUngeprueft = ZInnen <= -TNumericLimits<float>::Max() && ZAussen <= -TNumericLimits<float>::Max();
				const bool bRichtig = bUngeprueft || ZInnen > ZAussen + 30.f;
				if (EckWie.Len() < 400)
				{
					EckWie += FString::Printf(TEXT("%s[%.0f: innen %.0f aussen %.0f]"), EckWie.IsEmpty() ? TEXT("") : TEXT(" "), Lokal, ZInnen - Oben, ZAussen - Oben);
				}
				if (bRichtig || EckDrehung < TNumericLimits<float>::Max())
				{
					EckDrehung = Lokal;
					bGesetzt = true;
					++Ecken;
				}
				else if (Index != INDEX_NONE)
				{
					Leicht->RemoveByInstanceIndex(EKlasse, Index);
				}
			}
			if (bGesetzt)
			{
				SetzeFundamenteDarunter(Z.C, Ort.Z + (EBox.IsValid ? EBox.Min.Z : -50.f), Z.Tief);
			}
		}
	}

	// 2c) Bewuchs im Umkreis von 60 m um den Kern raeumen
	const int32 Geraeumt = TDRaeumeBewuchs(World, FVector(mBodenMitte.X, mBodenMitte.Y, Oben), 6000.f);

	// 3) Kern in die Mitte
	const FVector KernOrt(mBodenMitte.X, mBodenMitte.Y, Oben);
	AFGBuildable* Kern = Bau->BeginSpawnBuildable(KernKlasse, FTransform(FRotator::ZeroRotator, KernOrt));
	if (Kern)
	{
		Kern->SetBuiltWithRecipe(KernRezept);
		Kern->FinishSpawning(FTransform(FRotator::ZeroRotator, KernOrt));
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz %s gebaut: %d Fundamente (bis %d uebereinander), %d Rampen (%s), %d Ecken (%s), %d Pflanzen geraeumt, Oberkante %.0f, Fundament-Oberkante +%.0f, Kern %s"),
		*mId, Gesetzt, Stapel, Rampen, *RampeWie, Ecken, *EckWie, Geraeumt, Oben, FundamentOben, Kern ? *Kern->GetName() : TEXT("FEHLT"));

	// 4) Spieler auf die Plattform, Einsatz in der Stufe des Bioms
	if (Spieler)
	{
		Spieler->TeleportTo(KernOrt + FVector(-1600.f, 0.f, 300.f), FRotator::ZeroRotator, false, true);
		if (ACharacter* Figur = Cast<ACharacter>(Spieler))
		{
			Figur->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
	Schuetze(false);
	Melde(TDF(TEXT("%s: Plattform (6 x 6, bis %d uebereinander, %d Fundamente) mit %d Rampen, %d Eckstuecken und Abwehrkern gebaut, %d Pflanzen im Umkreis von 60 m geraeumt."),
		TEXT("%s: platform (6 x 6, up to %d stacked, %d foundations) built with %d ramps, %d corner pieces and the Defense Core, %d plants cleared within 60 m."),
		*mId, Stapel, Gesetzt, Rampen, Ecken, Geraeumt));
	mNeuerKern = Cast<ATDDefenseCore>(Kern);
	mEinsatzWarten = 0;
	mNetzAufgefrischt = false;
	if (!mNeuerKern.IsValid())
	{
		Melde(TDT(TEXT("Einsatz nicht gestartet: Kern fehlt."), TEXT("Operation not started: Core missing.")));
		return;
	}
	Melde(TDT(TEXT("Warte auf das Navigationsnetz um den Kern, dann startet der Einsatz ..."), TEXT("Waiting for the navigation mesh around the Core, then the operation starts ...")));
	GetWorld()->GetTimerManager().SetTimer(mTimer, this, &UTDKernplatz::WarteNetzFuerEinsatz, 2.f, true, 2.f);
}

void UTDKernplatz::WarteNetzFuerEinsatz()
{
	ATDDefenseCore* Kern = mNeuerKern.Get();
	const int32 Auf = NetzPunkte();
	if (Kern && Auf < 14 && ++mEinsatzWarten < 30)
	{
		return;
	}
	// 0.9.4 (Selbsttest 09.11.: nach dem Start bis 4 min "0 von 16 Richtungen begehbar", alle Wege teilweise): das Netz liegt draussen,
	// ist aber noch nicht mit der neuen Plattform verbunden. Erst ein Bau oder der Wellenstart frischt es auf. Darum hier pruefen, ob aus
	// 100 m ein voller Weg zum Kern fuehrt, sonst das Gebiet um die Plattform einmal neu erzeugen lassen und weiter warten (hoechstens 60 s).
	if (Kern && mEinsatzWarten < 30)
	{
		bool bVerbunden = false;
		for (int32 r = 0; r < 8 && !bVerbunden; ++r)
		{
			bool bVoll = false;
			UTDGegnerHirn::BerechneWeg(GetWorld(), Kern->GetActorLocation() + FRotator(0.f, 45.f * r, 0.f).Vector() * 10000.f, Kern, &bVoll);
			bVerbunden = bVoll;
		}
		if (!bVerbunden)
		{
			if (!mNetzAufgefrischt)
			{
				mNetzAufgefrischt = true;
				if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
				{
					Nav->AddDirtyArea(FBox::BuildAABB(Kern->GetActorLocation(), FVector(6000.f, 6000.f, 3000.f)), ENavigationDirtyFlag::All);
				}
				UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz: kein voller Weg aus 100 m zum Kern - Netz um die Plattform wird neu erzeugt"));
			}
			++mEinsatzWarten;
			return;
		}
	}
	GetWorld()->GetTimerManager().ClearTimer(mTimer);
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (!Kern || !Einsatz)
	{
		Melde(TDT(TEXT("Einsatz nicht gestartet: Kern fehlt."), TEXT("Operation not started: Core missing.")));
		return;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz: Netz nach %d s an %d/16 Punkten, starte Einsatz"), mEinsatzWarten * 2, Auf);
	if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
	{
		Waves->VerwerfeFronten();   // Fronten mit dem fertigen Netz neu suchen
	}
	TArray<FString> Meldungen;
	Einsatz->Starte(Kern, mStufe, TDBalance::Ganz(TEXT("Einsatz.WellenStandard"), 50), mSpieler.Get(), Meldungen);   // 0.77 Marius: 50 Wellen Standard
	for (const FString& M : Meldungen)
	{
		Melde(M);
	}
	SpieleBegruessung(this, mSpieler.Get());
}

bool UTDKernplatz::SpieleBegruessung(const UObject* WorldContext, APawn* Spieler)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	// 0.69.5: Sprachausgabe je Sprache (Spielsprache Deutsch -> _de, sonst _en); das Spiel spielt Nachrichten nur mit Ton ab
	const bool bDeutsch = FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName() == TEXT("de");
	UFGMessage* Nachricht = LoadObject<UFGMessage>(nullptr, bDeutsch ? TEXT("/FicsitTD/Narrative/MSG_TD_Begruessung_de.MSG_TD_Begruessung_de")
		: TEXT("/FicsitTD/Narrative/MSG_TD_Begruessung_en.MSG_TD_Begruessung_en"));
	const APlayerController* PC = Spieler ? Cast<APlayerController>(Spieler->GetController()) : (World ? World->GetFirstPlayerController() : nullptr);
	const AFGHUD* Hud = PC ? Cast<AFGHUD>(PC->GetHUD()) : nullptr;
	UFGGameUI* Oberflaeche = Hud ? Hud->GetGameUI() : nullptr;
	if (!Nachricht || !Oberflaeche)
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("ADA-Begruessung nicht moeglich: Nachricht %d, Spiel-Oberflaeche %d"), Nachricht != nullptr, Oberflaeche != nullptr);
		return false;
	}
	// TryPlayMessages ist privat -> direkt in die Warteschlange der Spiel-Oberflaeche (wie Client_ForwardMessagesToGameUI)
	Oberflaeche->AddPendingMessages({ Nachricht });
	// 0.69.10: Ton ueber die Unreal-Audioengine (Mod-Soundbanks lehnt Wwise in 1.2.5 mit Fehler 92 ab, Unreal-Audio ist seit 1.2.4.1
	// wieder aktiv). Die Nachricht liefert Titel und Untertitel im Takt der Aufnahme.
	if (USoundBase* Ton = LoadObject<USoundBase>(nullptr, bDeutsch ? TEXT("/FicsitTD/Audio/SW_TD_Begruessung_de.SW_TD_Begruessung_de")
		: TEXT("/FicsitTD/Audio/SW_TD_Begruessung_en.SW_TD_Begruessung_en")))
	{
		UGameplayStatics::PlaySound2D(World ? World : Spieler->GetWorld(), Ton, 1.f, 1.f, 0.f, nullptr, nullptr, true);
		UE_LOG(LogFicsitTD, Log, TEXT("ADA-Begruessung: Ton %s (%.1f s) ueber Unreal-Audio"), *Ton->GetName(), Ton->GetDuration());
	}
	UE_LOG(LogFicsitTD, Log, TEXT("ADA-Begruessung abgespielt (%d Untertitel)"), Nachricht->GetSubtitles().Num());
	return true;
}

FString UTDKernplatz::PruefeWwiseInitBank()
{
	// Test 0.69.6: Bank mit Projekt-ID 3134 (FicsIt-Networks, CL 502094) weiter mit Fehler 92 abgelehnt -> echte ID des Spiels auslesen
	int32 Banken = 0;
	for (TObjectIterator<UAkInitBank> It; It; ++It)
	{
		if (It->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}
		++Banken;
		const FString Name = It->GetPathName();
		It->InitBankCookedData.PackagedFile.GetFullFileToPtr([Name, Bank = TWeakObjectPtr<UAkInitBank>(*It)](bool bOk, const uint8* Ptr, int64 Groesse)
		{
			if (bOk && Ptr && Groesse >= 28 && FMemory::Memcmp(Ptr, "BKHD", 4) == 0)
			{
				uint32 Version = 0, Projekt = 0;
				FMemory::Memcpy(&Version, Ptr + 8, 4);
				FMemory::Memcpy(&Projekt, Ptr + 24, 4);
				FString Hex;
				for (int32 i = 0; i < FMath::Min<int64>(72, Groesse); ++i)
				{
					Hex += FString::Printf(TEXT("%02X "), Ptr[i]);
				}
				UE_LOG(LogFicsitTD, Log, TEXT("Wwise-Init-Bank %s: Bankversion %u, Projekt-ID %u (%lld Bytes), Kopf %s"), *Name, Version, Projekt, Groesse, *Hex);
			}
			else
			{
				UE_LOG(LogFicsitTD, Warning, TEXT("Wwise-Init-Bank %s: nicht lesbar (ok %d, %lld Bytes)"), *Name, bOk, Groesse);
			}
			if (Ptr && Bank.IsValid())
			{
				Bank->InitBankCookedData.PackagedFile.DeallocateMemory(Ptr, Groesse, false, NAME_None, NAME_None);
			}
		}, false, NAME_None, NAME_None, NAME_None);
	}
	return FString::Printf(TEXT("%d Init-Bank(en) gefunden, Projekt-ID steht gleich im Log (Wwise-Init-Bank ...)."), Banken);
}

bool UTDKernplatz::TowerDefenseAn(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const USessionSettingsManager* Einstellungen = World ? World->GetSubsystem<USessionSettingsManager>() : nullptr;
	return Einstellungen && Einstellungen->GetBoolOptionValue(UTDGameInstanceModule::TowerDefenseId);
}

void UTDKernplatz::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(mStartTimer, this, &UTDKernplatz::PruefeTDStart, 2.f, true, 5.f);
}

void UTDKernplatz::PruefeTDStart()
{
	UWorld* World = GetWorld();
	if (!TowerDefenseAn(this))
	{
		World->GetTimerManager().ClearTimer(mStartTimer);
		return;
	}
	for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
	{
		// Kern steht schon (geladener Spielstand oder bereits gebaut): nichts zu tun
		World->GetTimerManager().ClearTimer(mStartTimer);
		return;
	}
	if (mBauen || mNeuerKern.IsValid())
	{
		return;
	}
	const APlayerController* PC = World->GetFirstPlayerController();
	APawn* Spieler = PC ? PC->GetPawn() : nullptr;
	const ACharacter* Figur = Cast<ACharacter>(Spieler);
	// erst, wenn der Spieler 6 s lang zu Fuss unterwegs ist (nach Landung bzw. Intro)
	if (!Figur || Figur->GetCharacterMovement()->MovementMode != MOVE_Walking)
	{
		mStehtSeit = 0;
		return;
	}
	if (++mStehtSeit < 3)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(mStartTimer);
	// Startort = naechstes Startgebiet des Spiels
	static const TCHAR* Biome[] = { TEXT("grasfelder"), TEXT("felsenwueste"), TEXT("wald"), TEXT("duenen") };
	int32 Bestes = 0;
	float Abstand = TNumericLimits<float>::Max();
	for (int32 i = 0; i < 4; ++i)
	{
		const float D = FVector2D::Distance(FVector2D(Spieler->GetActorLocation()), FVector2D(TDScanGebiete[i].X, TDScanGebiete[i].Y));
		if (D < Abstand)
		{
			Abstand = D;
			Bestes = i;
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Kernplatz: Tower-Defense-Spielstand ohne Kern, Startort %s (%.0f m vom Startgebiet), baue den Kernplatz"), Biome[Bestes], Abstand / 100.f);
	Melde(TDT(TEXT("FICSIT Tower Defense: Dein Einsatzort wird vorbereitet ..."), TEXT("FICSIT Tower Defense: preparing your operation site ...")));
	// 0.83.1: der Selbsttest gibt den Platz vor (sonst zufaellig), gilt nur fuer diesen einen Bau
	const FString Nr = sVorgabePlatz;
	sVorgabePlatz.Reset();
	Melde(BauePlatz(Biome[Bestes], Nr, Spieler));
}
