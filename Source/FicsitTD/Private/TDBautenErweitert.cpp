#include "TDBautenErweitert.h"
#include "FGActorRepresentationManager.h"
#include "FGActorRepresentation.h"
#include "Resources/FGResourceNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TDDamageable.h"
#include "TDRepairStation.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDGegnerHirn.h"
#include "TDDefenseCore.h"
#include "TDWaveSubsystem.h"
#include "Creature/FGCreature.h"
#include "Creature/Enemy/FGFlyingBabyCrab.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "TDSprache.h"

// Lage der beweglichen Teile aus den Blender-Skripten in Satisfactory\Modelle (m -> cm). Namen mit Praefix wegen Unity-Build.

namespace TDErw
{
	static UStaticMeshComponent* Teil(AActor* Besitzer, USceneComponent* Eltern, const TCHAR* Name, const FVector& Ort, const FRotator& Lage = FRotator::ZeroRotator)
	{
		UStaticMeshComponent* T = Besitzer->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		T->SetupAttachment(Eltern);
		T->SetRelativeLocation(Ort);
		T->SetRelativeRotation(Lage);
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->SetCanEverAffectNavigation(false);
		return T;
	}

	static FString Modell(const TCHAR* Bau, const TCHAR* Teil)
	{
		return FString::Printf(TEXT("/FicsitTD/Buildings/%s/Mesh/SM_TD_%s_%s.SM_TD_%s_%s"), Bau, Bau, Teil, Bau, Teil);
	}

	static FString Rezept(const TCHAR* Bau, const TCHAR* Name)
	{
		return FString::Printf(TEXT("/FicsitTD/Buildings/%s/Recipe_TD_%s.Recipe_TD_%s_C"), Bau, Name, Name);
	}

	static double Jetzt(const UObject* O)
	{
		const UWorld* W = O ? O->GetWorld() : nullptr;
		return W ? W->GetTimeSeconds() : 0.0;
	}
}

// =====================================================================================================================
// Elektrozaun (td_elektrozaun.py: 8 x 0,6 x 4 m, zwei Felder, Schaltkasten-Tueren bei x = -2,5 / 1,5)

static const FVector ZaunTuer0(-250.f, -41.f, 25.f);
static const FVector ZaunTuer1(150.f, -41.f, 25.f);

ATDConductorWall::ATDConductorWall()
{
	mTurmName = TEXT("Elektrozaun");
	mSchadensart = ETDSchadensart::Strom;
	mDreht = false;
	mRange = 1500.f;                       // zieht nur Strom, wenn Gegner in 15 m sind
	mDrehpunkt = ZaunTuer0;
	mMuendung = FVector::ZeroVector;
	mHalbmasse = FVector(400.f, 30.f, 200.f);
	mLampenOrt = FVector(-60.f, -75.f, 340.f);   // 0.9.3: Mauer weg, Original-Mast hinten
	// 0.9.3 Original-Anschluesse (Modelle\Original_Anschluesse.json, Gebaeude-Review)
	OriginalTeil(TDOriginal::Strom, FVector(0.f, -30.f, 80.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-60.f, -75.f, 0.f), 0.f);

	mKlarraumHoehe = 420.f;
	mKlarraumHalb = FVector2D(400.f, 30.f);
	mBaseMaxHealth = 4000.f;
	mYawModell = TDErw::Modell(TEXT("ConductorWall"), TEXT("Door0"));
	mStandardRezept = TDErw::Rezept(TEXT("ConductorWall"), TEXT("Zaun_Strom"));
	mTuer2 = TDErw::Teil(this, RootComponent, TEXT("ZaunTuer1"), ZaunTuer1);
}

void ATDConductorWall::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mTuer2, TDErw::Modell(TEXT("ConductorWall"), TEXT("Door1")));
}

void ATDConductorWall::Schlag(AActor* Zaun, AFGCreature* Angreifer)
{
	ATDConductorWall* Z = Cast<ATDConductorWall>(Zaun);
	if (!Z || !Angreifer || !Angreifer->IsAliveAndWell() || !Z->IsTDIntact() || !Z->HasPower())
	{
		return;
	}
	const float Schaden = TDBalance::Wert(TEXT("Tuerme.Elektrozaun.Schaden"), 15.f) * Z->SchadenBonus();
	const float Betaeubung = TDBalance::Wert(TEXT("Tuerme.Elektrozaun.Betaeubung"), 1.f);
	const FVector Richtung = (Angreifer->GetActorLocation() - Z->GetActorLocation()).GetSafeNormal2D();
	Z->Treffer(Angreifer, Schaden, Richtung, Betaeubung);
	++Z->mSchlaege;
	++Z->mSchuesse;
	// Funke vom naechsten Punkt des Zauns zum Angreifer
	const FVector Lokal = Z->GetActorTransform().InverseTransformPosition(Angreifer->GetActorLocation());
	const FVector Von = Z->GetActorTransform().TransformPosition(FVector(FMath::Clamp(Lokal.X, -380.f, 380.f), 0.f, 220.f));
	Z->ZeigeSpur(Von, Angreifer->GetActorLocation() + FVector(0.f, 0.f, 60.f), FLinearColor(0.45f, 0.7f, 1.f), 0.12f, 1.f);
}

FString ATDConductorWall::ZusatzInfo() const
{
	if (!HasPower())
	{
		return TDT(TEXT("ohne Strom: nur Mauer"), TEXT("no power: wall only"));
	}
	return IsProducing() ? TDT(TEXT("unter Spannung"), TEXT("live")) : TDT(TEXT("bereit, Spannung bei Gegnern in der Nähe"), TEXT("ready, powers up when enemies are near"));
}

void ATDConductorWall::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Schlag"), TEXT("Shock")), TDF(TEXT("%.0f Strom-Schaden"), TEXT("%.0f electric damage"), TDBalance::Wert(TEXT("Tuerme.Elektrozaun.Schaden"), 15.f) * SchadenBonus()), true });
	Out.Add({ TDT(TEXT("Betäubung"), TEXT("Stun")), FString::Printf(TEXT("%.1f s"), TDBalance::Wert(TEXT("Tuerme.Elektrozaun.Betaeubung"), 1.f)), true });
	Out.Add({ TDT(TEXT("Schläge"), TEXT("Shocks")), FString::Printf(TEXT("%d"), mSchlaege), true });
}

// =====================================================================================================================
// Funkmast (td_funkmast.py: grosse Schuessel oben, Richtantenne darunter, kleine Schuessel seitlich)

TArray<TWeakObjectPtr<ATDRadioMast>> ATDRadioMast::sAlle;
static const FVector FunkAntenne(0.f, 0.f, 940.f);
static const FVector FunkKleineSchuessel(104.f, 0.f, 760.f);

ATDRadioMast::ATDRadioMast()
{
	mTurmName = TEXT("Funkmast");
	mDreht = false;
	mRange = 6000.f;                      // die Richtantenne verfolgt Gegner in 60 m
	mDrehpunkt = FVector(0.f, 0.f, 1064.f);
	mMuendung = FVector::ZeroVector;
	mHalbmasse = FVector(200.f, 200.f, 60.f);
	mLampenOrt = FVector(-60.f, 140.f, 352.f);
	// 0.9.3 Original-Anschluesse (Modelle\Original_Anschluesse.json, Gebaeude-Review)
	OriginalTeil(TDOriginal::Strom, FVector(100.f, 30.f, 75.f), 0.f);
	OriginalTeil(TDOriginal::Mast, FVector(-60.f, 140.f, 12.f), 180.f);

	mKlarraumHoehe = 1250.f;
	mKlarraumHalb = FVector2D(205.f, 205.f);
	mBaseMaxHealth = 2500.f;
	mYawModell = TDErw::Modell(TEXT("RadioMast"), TEXT("Dish"));
	mStandardRezept = TDErw::Rezept(TEXT("RadioMast"), TEXT("Funk_Strom"));
	mAntenne = TDErw::Teil(this, RootComponent, TEXT("FunkAntenne"), FunkAntenne);
	mKleineSchuessel = TDErw::Teil(this, RootComponent, TEXT("FunkKleineSchuessel"), FunkKleineSchuessel);
}

void ATDRadioMast::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mAntenne, TDErw::Modell(TEXT("RadioMast"), TEXT("Antenna")));
	SetzeModell(mKleineSchuessel, TDErw::Modell(TEXT("RadioMast"), TEXT("SmallDish")));
	SperreTakt();   // 0.9.6 (Gesamtreview 12.11., A1)
	sAlle.RemoveAll([](const TWeakObjectPtr<ATDRadioMast>& M) { return !M.IsValid(); });
	sAlle.AddUnique(this);
	GetWorldTimerManager().SetTimer(mRadarTimer, this, &ATDRadioMast::RadarTakt, 2.f, true, 2.f);
}

void ATDRadioMast::RaeumeWellenMarker()
{
	if (AFGActorRepresentationManager* Rep = AFGActorRepresentationManager::Get(GetWorld()))
	{
		for (UFGActorRepresentation* M : mWellenMarker)
		{
			if (M)
			{
				Rep->RemoveRepresentation(M);
			}
		}
	}
	mWellenMarker.Reset();
	mMarkerWelle = -1;
}

void ATDRadioMast::RadarTakt()
{
	// 0.9.3 Radar: nur mit Strom. Ressourcen lokal (jeder Spieler sieht sie), Wellen-Markierungen vom Server (repliziert)
	const bool bStrom = !GetPowerInfo() || HasPower();
	if (!bStrom || !IsTDIntact())
	{
		if (HasAuthority() && mWellenMarker.Num() > 0)
		{
			RaeumeWellenMarker();
		}
		return;
	}
	// Rohstoffvorkommen in 150 m: wie der Ressourcenscanner, alle 30 s erneuert (Scan haelt 35 s)
	mRessourcenRest -= 2.f;
	if (mRessourcenRest <= 0.f && !IsRunningDedicatedServer())
	{
		mRessourcenRest = 30.f;
		const float R = TDBalance::Wert(TEXT("Tuerme.Funkmast.RessourcenRadius"), 150.f) * 100.f;
		mGescannt = 0;
		for (TActorIterator<AFGResourceNodeBase> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) < R)
			{
				It->ScanResourceNode_Local(35.f);
				++mGescannt;
			}
		}
	}
	if (!HasAuthority())
	{
		return;
	}
	// Wellen-Markierung: nur ein Radar (das erste gueltige) setzt sie, 30 s vor dem Start bis zum Ende der Welle
	for (const TWeakObjectPtr<ATDRadioMast>& M : sAlle)
	{
		if (M.IsValid() && M.Get() != this && M->IsTDIntact() && (!M->GetPowerInfo() || M->HasPower()))
		{
			if (sAlle.IndexOfByKey(M) < sAlle.IndexOfByKey(this))
			{
				return;
			}
		}
	}
	UTDWaveSubsystem* Wellen = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!Wellen)
	{
		return;
	}
	const FTDWellenVorschau V = Wellen->GetVorschau();
	const bool bZeigen = V.bGueltig && (V.bLaeuft || (V.Countdown >= 0.f && V.Countdown <= TDBalance::Wert(TEXT("Tuerme.Funkmast.Vorwarnung"), 30.f)));
	if (!bZeigen)
	{
		if (mWellenMarker.Num() > 0)
		{
			RaeumeWellenMarker();
		}
		return;
	}
	if (mMarkerWelle == V.Welle)
	{
		return;
	}
	RaeumeWellenMarker();
	AFGActorRepresentationManager* Rep = AFGActorRepresentationManager::Get(GetWorld());
	if (!Rep)
	{
		return;
	}
	// Text: Welle, Arten mit Anzahl, Flieger-Warnung
	FString Text = TDF(TEXT("Welle %d:"), TEXT("Wave %d:"), V.Welle);
	bool bFlieger = false;
	for (const FTDPlanGruppe& G : V.Gruppen)
	{
		const FTDKreaturSteckbrief S = UTDWaveSubsystem::Steckbrief(G.Pfad);
		Text += V.bZusammensetzung ? FString::Printf(TEXT(" %dx %s"), G.Anzahl, *S.Name) : FString::Printf(TEXT(" %s"), *S.Name);
		bFlieger |= G.Pfad.Contains(TEXT("Flying"));
	}
	if (bFlieger)
	{
		Text += TDT(TEXT(" - FLIEGER!"), TEXT(" - FLYERS!"));
	}
	UTexture2D* Symbol = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FactoryGame/Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Aggro.TXUI_MIcon_Aggro"));
	const FLinearColor Farbe = bFlieger ? FLinearColor(0.3f, 0.6f, 1.f) : FLinearColor(1.f, 0.15f, 0.05f);
	const TArray<FTDFront>& Fronten = Wellen->GetFronten();
	const int32 Aktiv = FMath::Min(Wellen->AktiveFrontenFuer(V.Welle), Fronten.Num());
	FTextProperty* TextFeld = FindFProperty<FTextProperty>(UFGActorRepresentation::StaticClass(), TEXT("mRepresentationText"));
	for (int32 i = 0; i < Aktiv; ++i)
	{
		UFGActorRepresentation* M = Rep->CreateAndAddNewRepresentationNoActor(Fronten[i].Ort + FVector(0.f, 0.f, 300.f), Symbol, Farbe, 900.f, true, true, ERepresentationType::RT_Default);
		if (M)
		{
			if (TextFeld)
			{
				TextFeld->SetPropertyValue_InContainer(M, FText::FromString(Text));
				Rep->UpdateRepresentation(M);
			}
			mWellenMarker.Add(M);
		}
	}
	mMarkerWelle = V.Welle;
	UE_LOG(LogFicsitTD, Log, TEXT("Radar %s: %d Front-Markierungen fuer Welle %d (%s), %d Rohstoffe gescannt"), *GetName(), mWellenMarker.Num(), V.Welle, *Text, mGescannt);
}

void ATDRadioMast::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	sAlle.Remove(this);
	GetWorldTimerManager().ClearTimer(mRadarTimer);
	if (HasAuthority())
	{
		RaeumeWellenMarker();
	}
	Super::EndPlay(EndPlayReason);
}

float ATDRadioMast::ReichweitenFaktor(const AActor* Turm)
{
	if (!Turm || Turm->IsA<ATDRadioMast>())
	{
		return 1.f;
	}
	const float Radius = TDBalance::Wert(TEXT("Tuerme.Funkmast.Radius"), 30.f) * 100.f;
	for (const TWeakObjectPtr<ATDRadioMast>& M : sAlle)
	{
		const ATDRadioMast* Mast = M.Get();
		if (Mast && Mast->GetWorld() == Turm->GetWorld() && Mast->IsTDIntact() && Mast->IsProducing()
			&& FVector::DistSquared2D(Mast->GetActorLocation(), Turm->GetActorLocation()) < Radius * Radius)
		{
			// mehrere Masten wirken nicht zusammen
			return 1.f + TDBalance::Wert(TEXT("Tuerme.Funkmast.Bonus"), 0.1f);
		}
	}
	return 1.f;
}

void ATDRadioMast::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	if (!IsProducing())
	{
		return;
	}
	mZeit += DeltaSeconds;
	if (mYawMesh)
	{
		mYawMesh->SetRelativeRotation(FRotator(0.f, FMath::Fmod(mZeit * 20.f, 360.f), 0.f));
	}
	if (mKleineSchuessel)
	{
		mKleineSchuessel->SetRelativeRotation(FRotator(0.f, 35.f * FMath::Sin(mZeit * 0.7f), 0.f));
	}
	if (mAntenne)
	{
		float Soll = mAntenne->GetRelativeRotation().Yaw;
		if (Ziel)
		{
			const FVector Lokal = GetActorTransform().InverseTransformPosition(Ziel->GetActorLocation());
			Soll = FMath::RadiansToDegrees(FMath::Atan2(Lokal.Y, Lokal.X));
		}
		mAntenne->SetRelativeRotation(FRotator(0.f, FMath::FixedTurn(mAntenne->GetRelativeRotation().Yaw, Soll, 90.f * DeltaSeconds), 0.f));
	}
}

FString ATDRadioMast::ZusatzInfo() const
{
	return IsProducing() ? TDF(TEXT("sendet: Türme in %.0f m +%.0f %% Reichweite"), TEXT("transmitting: turrets within %.0f m +%.0f %% range"), TDBalance::Wert(TEXT("Tuerme.Funkmast.Radius"), 30.f),
		TDBalance::Wert(TEXT("Tuerme.Funkmast.Bonus"), 0.1f) * 100.f) : TDT(TEXT("sendet nicht (Strom?)"), TEXT("not transmitting (power?)"));
}

void ATDRadioMast::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	int32 Anzahl = 0;
	const float Radius = TDBalance::Wert(TEXT("Tuerme.Funkmast.Radius"), 30.f) * 100.f;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		Anzahl += !It->IsA<ATDRadioMast>() && FVector::DistSquared2D(It->GetActorLocation(), GetActorLocation()) < Radius * Radius ? 1 : 0;
	}
	Out.Add({ TDT(TEXT("Reichweite"), TEXT("Range")), FString::Printf(TEXT("+%.0f %%"), TDBalance::Wert(TEXT("Tuerme.Funkmast.Bonus"), 0.1f) * 100.f), true });
	Out.Add({ TDT(TEXT("Sendekreis"), TEXT("Broadcast radius")), FString::Printf(TEXT("%.0f m"), Radius / 100.f), true });
	Out.Add({ TDT(TEXT("Türme im Kreis"), TEXT("Turrets in radius")), FString::Printf(TEXT("%d"), Anzahl), true });
}

// =====================================================================================================================
// Schildgenerator (td_schildgenerator.py: drei Ringspulen bei 2,9 / 4,2 / 5,4 m, vier Kondensator-Deckel bei 1,8 m / 2,0 m)

TArray<TWeakObjectPtr<ATDShieldGenerator>> ATDShieldGenerator::sAlle;

ATDShieldGenerator::ATDShieldGenerator()
{
	mTurmName = TEXT("Schildgenerator");
	mDreht = false;
	mRange = 2000.f;
	mDrehpunkt = FVector(0.f, 0.f, 290.f);
	mMuendung = FVector::ZeroVector;
	mHalbmasse = FVector(280.f, 280.f, 60.f);
	mLampenOrt = FVector(-260.f, 260.f, 400.f);
	// 0.9.3 Original-Anschluesse (Modelle\Original_Anschluesse.json, Gebaeude-Review)
	OriginalTeil(TDOriginal::Strom, FVector(-200.f, -295.f, 110.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-260.f, 260.f, 60.f), 180.f);

	mKlarraumHoehe = 620.f;
	mKlarraumHalb = FVector2D(300.f, 300.f);
	mBaseMaxHealth = 3500.f;
	mYawModell = TDErw::Modell(TEXT("ShieldGenerator"), TEXT("Ring0"));
	mStandardRezept = TDErw::Rezept(TEXT("ShieldGenerator"), TEXT("Schild_Strom"));
	mRing1 = TDErw::Teil(this, RootComponent, TEXT("SchildRing1"), FVector(0.f, 0.f, 420.f));
	mRing2 = TDErw::Teil(this, RootComponent, TEXT("SchildRing2"), FVector(0.f, 0.f, 540.f));
	TObjectPtr<UStaticMeshComponent>* Deckel[] = { &mDeckel0, &mDeckel1, &mDeckel2, &mDeckel3 };
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = 90.f * i;
		*Deckel[i] = TDErw::Teil(this, RootComponent, *FString::Printf(TEXT("SchildDeckel%d"), i),
			FVector(180.f * FMath::Cos(FMath::DegreesToRadians(A)), 180.f * FMath::Sin(FMath::DegreesToRadians(A)), 200.f), FRotator(0.f, A, 0.f));
	}
}

void ATDShieldGenerator::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mRing1, TDErw::Modell(TEXT("ShieldGenerator"), TEXT("Ring1")));
	SetzeModell(mRing2, TDErw::Modell(TEXT("ShieldGenerator"), TEXT("Ring2")));
	UStaticMeshComponent* Deckel[] = { mDeckel0, mDeckel1, mDeckel2, mDeckel3 };
	for (int32 i = 0; i < 4; ++i)
	{
		SetzeModell(Deckel[i], FString::Printf(TEXT("/FicsitTD/Buildings/ShieldGenerator/Mesh/SM_TD_ShieldGenerator_Cover%d.SM_TD_ShieldGenerator_Cover%d"), i, i));
	}
	// 0.9.6 (Gesamtreview 12.11., D): geladener Ladestand und Rest der Ladezeit; neu gebaut (-1) = voll
	if (mSchild < 0.f)
	{
		mSchild = SchildMax();
	}
	else
	{
		mSchild = FMath::Min(mSchild, SchildMax());
		mLadenBis = mSchild <= 0.f ? Jetzt() + FMath::Max(mLadenRest, 0.f) : 0.0;
		UE_LOG(LogFicsitTD, Log, TEXT("Schildgenerator %s: geladen mit Schild %.0f / %.0f, Ladezeit noch %.0f s"), *GetName(), mSchild, SchildMax(),
			mSchild <= 0.f ? FMath::Max(mLadenRest, 0.f) : 0.f);
	}
	SperreTakt();   // 0.9.6 (Gesamtreview 12.11., A1)
	sAlle.RemoveAll([](const TWeakObjectPtr<ATDShieldGenerator>& S) { return !S.IsValid(); });
	sAlle.AddUnique(this);
}

void ATDShieldGenerator::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	sAlle.Remove(this);
	Super::EndPlay(EndPlayReason);
}

void ATDShieldGenerator::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	Super::PreSaveGame_Implementation(saveVersion, gameVersion);
	// 0.9.6: Weltzeit beginnt nach dem Laden bei 0, deshalb die Restdauer speichern
	mLadenRest = FMath::Max(0.f, float(mLadenBis - Jetzt()));
}

double ATDShieldGenerator::Jetzt() const
{
	return TDErw::Jetzt(this);
}

float ATDShieldGenerator::SchildMax() const
{
	return TDBalance::Wert(TEXT("Tuerme.Schildgenerator.Schild"), 3000.f) * SchadenBonus();
}

bool ATDShieldGenerator::IstAktiv() const
{
	return IsTDIntact() && IsProducing() && Jetzt() >= mLadenBis && mSchild > 0.f;
}

float ATDShieldGenerator::Schlucke(const UWorld* Welt, const AActor* Ziel, float Schaden)
{
	if (!Ziel || Schaden <= 0.f || Ziel->IsA<ATDDefenseCore>())
	{
		return Schaden;
	}
	const float Radius = TDBalance::Wert(TEXT("Tuerme.Schildgenerator.Radius"), 20.f) * 100.f;
	for (const TWeakObjectPtr<ATDShieldGenerator>& S : sAlle)
	{
		ATDShieldGenerator* Gen = S.Get();
		if (!Gen || Gen->GetWorld() != Welt || !Gen->IstAktiv()
			|| FVector::DistSquared2D(Gen->GetActorLocation(), Ziel->GetActorLocation()) > Radius * Radius)
		{
			continue;
		}
		const float Weg = FMath::Min(Gen->mSchild, Schaden);
		Gen->mSchild -= Weg;
		Gen->mGeschluckt += Weg;
		Gen->mLetzterTreffer = Gen->Jetzt();
		Schaden -= Weg;
		Gen->ZeigeLeuchten(Ziel->GetActorLocation() + FVector(0.f, 0.f, 150.f), FLinearColor(0.3f, 0.6f, 1.f), 2.f, 0.25f, 0.f, false, 20.f);
		if (Gen->mSchild <= 0.f)
		{
			Gen->mSchild = 0.f;
			Gen->mLadenBis = Gen->Jetzt() + TDBalance::Wert(TEXT("Tuerme.Schildgenerator.Ladezeit"), 20.f);
			UE_LOG(LogFicsitTD, Log, TEXT("Schildgenerator %s: Schild gebrochen, laedt %.0f s"), *Gen->GetName(),
				TDBalance::Wert(TEXT("Tuerme.Schildgenerator.Ladezeit"), 20.f));
		}
		if (Schaden <= 0.f)
		{
			break;
		}
	}
	return Schaden;
}

void ATDShieldGenerator::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	const double T = Jetzt();
	if (mSchild < 0.f)
	{
		mSchild = SchildMax();
	}
	// Nach dem Bruch: Ladezeit vorbei -> voll. Sonst nach 5 s ohne Treffer langsam nachladen (5 % je Sekunde)
	if (mSchild <= 0.f && T >= mLadenBis && IsProducing())
	{
		mSchild = SchildMax();
		UE_LOG(LogFicsitTD, Log, TEXT("Schildgenerator %s: Schild wieder voll"), *GetName());
	}
	else if (mSchild > 0.f && IsProducing() && T - mLetzterTreffer > 5.0)
	{
		mSchild = FMath::Min(SchildMax(), mSchild + SchildMax() * 0.05f * DeltaSeconds);
	}
	const bool bAktiv = IstAktiv();
	mDeckelWinkel = FMath::FInterpConstantTo(mDeckelWinkel, bAktiv ? 0.f : 60.f, DeltaSeconds, 60.f);   // offen = laedt
	UStaticMeshComponent* Deckel[] = { mDeckel0, mDeckel1, mDeckel2, mDeckel3 };
	for (int32 i = 0; i < 4; ++i)
	{
		if (Deckel[i])
		{
			Deckel[i]->SetRelativeRotation(FRotator(mDeckelWinkel, 90.f * i, 0.f));
		}
	}
	if (IsProducing())
	{
		const float Tempo = bAktiv ? 1.f : 3.f;
		if (mYawMesh) mYawMesh->AddLocalRotation(FRotator(0.f, 25.f * Tempo * DeltaSeconds, 0.f));
		if (mRing1) mRing1->AddLocalRotation(FRotator(0.f, -45.f * Tempo * DeltaSeconds, 0.f));
		if (mRing2) mRing2->AddLocalRotation(FRotator(0.f, 80.f * Tempo * DeltaSeconds, 0.f));
	}
}

FString ATDShieldGenerator::ZusatzInfo() const
{
	if (!IsProducing())
	{
		return TDT(TEXT("aus (Strom?)"), TEXT("off (power?)"));
	}
	const double Rest = mLadenBis - Jetzt();
	if (Rest > 0.0)
	{
		return TDF(TEXT("Schild gebrochen, lädt noch %.0f s"), TEXT("Shield broken, recharging for %.0f s"), Rest);
	}
	return TDF(TEXT("Schild %.0f / %.0f"), TEXT("Shield %.0f / %.0f"), FMath::Max(mSchild, 0.f), SchildMax());
}

void ATDShieldGenerator::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Schild"), TEXT("Shield")), FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(mSchild, 0.f), SchildMax()), true });
	Out.Add({ TDT(TEXT("Kuppel"), TEXT("Dome")), FString::Printf(TEXT("%.0f m"), TDBalance::Wert(TEXT("Tuerme.Schildgenerator.Radius"), 20.f)), true });
	Out.Add({ TDT(TEXT("Ladezeit"), TEXT("Recharge")), TDF(TEXT("%.0f s nach Bruch"), TEXT("%.0f s after breaking"), TDBalance::Wert(TEXT("Tuerme.Schildgenerator.Ladezeit"), 20.f)), true });
	Out.Add({ TDT(TEXT("Geschluckt"), TEXT("Absorbed")), FString::Printf(TEXT("%.0f"), mGeschluckt), true });
}

// =====================================================================================================================
// Himmelsturm / Flak (td_flak.py, Bolzenturm-Sockel, Drehteil um 1,3 skaliert: Deck 2,25 m, Kanzel 2,08 m, Radar an der Kanzel)

ATDFlakTurret::ATDFlakTurret()
{
	mTurmName = TEXT("Himmelsturm");
	mSchadensart = ETDSchadensart::Explosion;
	mDreht = true;
	mRange = 6000.f;
	mDrehpunkt = FVector(0.f, 0.f, 290.f);   // 0.9.3: Bolzensockel 65 cm hoeher
	mNeigeachse = FVector(0.f, 0.f, 208.f);
	mLampenOrt = FVector(-90.f, 205.f, 340.f);
	OriginalTeil(TDOriginal::Eingang, FVector(-210.f, 0.f, 0.f), 90.f);
	OriginalTeil(TDOriginal::Strom, FVector(0.f, -160.f, 130.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-90.f, 205.f, 0.f), 180.f);
	mMuendung = FVector(172.f, 0.f, 0.f);
	mNeigungMin = -5.f;
	mNeigungMax = 80.f;
	mTurnSpeed = 140.f;
	mYawModell = TDErw::Modell(TEXT("FlakTurret"), TEXT("Yaw"));
	mHeadModell = TDErw::Modell(TEXT("FlakTurret"), TEXT("Pitch"));
	mStandardRezept = TDErw::Rezept(TEXT("FlakTurret"), TEXT("Flak_Shoot"));
	mRadar = TDErw::Teil(this, mHeadMesh ? static_cast<USceneComponent*>(mHeadMesh) : RootComponent.Get(), TEXT("FlakRadar"), FVector(-26.f, 71.5f, 156.f));
}

// Plaetze der 6 Kanister im Werferkasten (relativ zum Neigepunkt, mit Kopf-Skalierung 1,3; td_umbau_093.py himmel())
static const FVector FlakKanister[6] = { FVector(130.f, 80.6f, 54.6f), FVector(130.f, 0.f, 54.6f), FVector(130.f, -80.6f, 54.6f),
	FVector(130.f, 80.6f, -26.f), FVector(130.f, 0.f, -26.f), FVector(130.f, -80.6f, -26.f) };

void ATDFlakTurret::BeginPlay()
{
	Super::BeginPlay();
	if (mRadar)
	{
		mRadar->SetVisibility(false);   // 0.9.3: Radar entfernt
	}
	UStaticMesh* Kanister = LoadObject<UStaticMesh>(nullptr, *TDErw::Modell(TEXT("FlakTurret"), TEXT("Rakete")), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Kanister && mHeadMesh && !IsRunningDedicatedServer())
	{
		for (int32 i = 0; i < 6; ++i)
		{
			UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(this);
			K->SetStaticMesh(Kanister);
			K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			K->SetCanEverAffectNavigation(false);
			K->SetupAttachment(mHeadMesh);
			K->SetRelativeLocation(FlakKanister[i]);
			K->RegisterComponent();
			mKanister.Add(K);
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Himmelsturm %s: %d sichtbare Kanister"), *GetName(), mKanister.Num());
}

AActor* ATDFlakTurret::SucheFlieger() const
{
	const float R = GetReichweite();
	AActor* Best = nullptr;
	float BestD = R * R;
	for (TActorIterator<AFGFlyingBabyCrab> It(GetWorld()); It; ++It)
	{
		if (!IsValid(*It) || It->IsActorBeingDestroyed())
		{
			continue;
		}
		const float D = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
		if (D < BestD)
		{
			BestD = D;
			Best = *It;
		}
	}
	return Best;
}

bool ATDFlakTurret::BereitZumProduzieren(AFGCreature* Ziel) const
{
	const AActor* F = mFlieger.Get();
	return F && IsValid(F) && IstAusgerichtet(F);
}

void ATDFlakTurret::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	mSuchTakt -= DeltaSeconds;
	if (mSuchTakt <= 0.f || !mFlieger.IsValid())
	{
		mSuchTakt = 0.4f;
		mFlieger = IsTDIntact() && GetCurrentRecipe() ? SucheFlieger() : nullptr;
	}
	// Kanister: leerer Platz wartet die Nachladezeit, in den letzten 0,4 s gleitet ein neuer von hinten (60 cm) ins Rohr
	const float Gleiten = 0.4f;
	for (int32 i = 0; i < mKanister.Num(); ++i)
	{
		if (mKanisterRest[i] <= 0.f || !mKanister[i])
		{
			continue;
		}
		mKanisterRest[i] = FMath::Max(0.f, mKanisterRest[i] - DeltaSeconds);
		const bool bSichtbar = mKanisterRest[i] < Gleiten;
		mKanister[i]->SetVisibility(bSichtbar);
		mKanister[i]->SetRelativeLocation(FlakKanister[i] - FVector(60.f * FMath::Clamp(mKanisterRest[i] / Gleiten, 0.f, 1.f), 0.f, 0.f));
	}
	const AActor* F = mFlieger.Get();
	if (!F || !mYawMesh || !mHeadMesh)
	{
		return;
	}
	// Die Basis zielt nur auf Bodengegner: Drehteil und Kanzel hier selbst nachfuehren (wie ATDTurretBase::Zielen)
	const float Schritt = mTurnSpeed * DeltaSeconds;
	const FVector Lokal = GetActorTransform().InverseTransformPosition(F->GetActorLocation());
	FRotator Gier = mYawMesh->GetRelativeRotation();
	Gier.Yaw = FMath::FixedTurn(Gier.Yaw, FMath::RadiansToDegrees(FMath::Atan2(Lokal.Y, Lokal.X)), Schritt);
	mYawMesh->SetRelativeRotation(Gier);
	const FVector ImDrehteil = mYawMesh->GetComponentTransform().InverseTransformPosition(F->GetActorLocation()) - mNeigeachse;
	const float Soll = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ImDrehteil.Z, FVector2D(ImDrehteil.X, ImDrehteil.Y).Size())), mNeigungMin, mNeigungMax);
	FRotator Neigung = mHeadMesh->GetRelativeRotation();
	Neigung.Pitch = FMath::FixedTurn(Neigung.Pitch, Soll, Schritt);
	mHeadMesh->SetRelativeRotation(Neigung);
}

void ATDFlakTurret::Feuern(AFGCreature* Ziel)
{
	AActor* F = mFlieger.Get();
	if (!F)
	{
		return;
	}
	++mSchuesse;
	const FVector Ort = F->GetActorLocation();
	FVector Muendung = GetMuzzleLocation();
	if (mKanister.Num() == 6)
	{
		// naechster volle Platz feuert: Kanister weg, Schuss kommt aus diesem Rohr
		const int32 i = mKanisterNaechster;
		mKanisterNaechster = (mKanisterNaechster + 1) % 6;
		mKanister[i]->SetVisibility(false);
		mKanisterRest[i] = TDBalance::Wert(TEXT("Tuerme.Himmelsturm.Nachladen"), 1.5f);
		Muendung = mHeadMesh->GetComponentTransform().TransformPosition(FlakKanister[i] + FVector(60.f, 0.f, 0.f));
	}
	const float Flugzeit = FVector::Dist(Muendung, Ort) / 6000.f;
	ZeigeSpur(Muendung, Ort, FLinearColor(1.f, 0.75f, 0.3f), 0.08f, 2.f);
	ZeigeLeuchten(Ort, FLinearColor(1.f, 0.55f, 0.15f), 5.f, 0.4f, Flugzeit, false, 30.f);
	const float Schaden = TDBalance::Wert(TEXT("Tuerme.Himmelsturm.Schaden"), 200.f) * SchadenBonus();
	const float Radius = TDBalance::Wert(TEXT("Tuerme.Himmelsturm.Radius"), 6.f) * 100.f;
	TWeakObjectPtr<ATDFlakTurret> Selbst = this;
	auto Wirkung = [Selbst, Ort, Schaden, Radius]()
	{
		ATDFlakTurret* T = Selbst.Get();
		if (!T)
		{
			return;
		}
		TArray<AActor*> Opfer;
		for (TActorIterator<AFGFlyingBabyCrab> It(T->GetWorld()); It; ++It)
		{
			if (IsValid(*It) && FVector::DistSquared(It->GetActorLocation(), Ort) < Radius * Radius)
			{
				Opfer.Add(*It);
			}
		}
		for (AActor* O : Opfer)
		{
			UGameplayStatics::ApplyDamage(O, Schaden, nullptr, T, UDamageType::StaticClass());
			UTDGegnerHirn::RestToeten(O);
			++T->mTreffer;
			T->mBilanz.Schaden += Schaden;
			++T->mBilanz.Treffer;
		}
	};
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda(Wirkung), FMath::Max(Flugzeit, 0.01f), false);
}

FString ATDFlakTurret::ZusatzInfo() const
{
	return mFlieger.IsValid() ? TDT(TEXT("Flieger erfasst"), TEXT("flyer locked")) : TDT(TEXT("sucht Flieger (keine Bodenziele)"), TEXT("searching for flyers (no ground targets)"));
}

void ATDFlakTurret::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Schaden"), TEXT("Damage")), TDF(TEXT("%.0f in %.0f m"), TEXT("%.0f within %.0f m"), TDBalance::Wert(TEXT("Tuerme.Himmelsturm.Schaden"), 200.f) * SchadenBonus(),
		TDBalance::Wert(TEXT("Tuerme.Himmelsturm.Radius"), 6.f)), true });
	Out.Add({ TDT(TEXT("Feuerrate"), TEXT("Fire rate")), FString::Printf(TEXT("%.1f s"), Zykluszeit()), true });
	Out.Add({ TDT(TEXT("Ziele"), TEXT("Targets")), TDT(TEXT("nur Flieger"), TEXT("flyers only")), true });
	Out.Add({ TDT(TEXT("Flieger getroffen"), TEXT("Flyers hit")), FString::Printf(TEXT("%d"), mTreffer), true });
}

// =====================================================================================================================
// Stoerfeld-Projektor (td_stoerfeld.py: Kristall bei 4,6 m, drei Ringe um ihn in drei Achsen)

ATDFieldProjector::ATDFieldProjector()
{
	mTurmName = TEXT("Stoerfeld");
	mDreht = false;
	mRange = 1500.f;
	mDrehpunkt = FVector(0.f, 0.f, 525.f);   // 0.9.3: Bolzensockel 65 cm hoeher
	mLampenOrt = FVector(-90.f, 205.f, 340.f);
	// 0.9.3 Original-Anschluesse (Modelle\Original_Anschluesse.json, Gebaeude-Review)
	OriginalTeil(TDOriginal::Eingang, FVector(-210.f, 0.f, 0.f), 90.f);
	OriginalTeil(TDOriginal::Strom, FVector(0.f, -160.f, 130.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-90.f, 205.f, 0.f), 180.f);

	mMuendung = FVector::ZeroVector;
	mBaseMaxHealth = 3500.f;
	mKlarraumHoehe = 640.f;
	mYawModell = TDErw::Modell(TEXT("FieldProjector"), TEXT("Core"));
	mStandardRezept = TDErw::Rezept(TEXT("FieldProjector"), TEXT("Feld_SAM"));
	mRing0 = TDErw::Teil(this, RootComponent, TEXT("FeldRing0"), FVector(0.f, 0.f, 525.f), FRotator(0.f, 0.f, 0.f));
	mRing1 = TDErw::Teil(this, RootComponent, TEXT("FeldRing1"), FVector(0.f, 0.f, 525.f), FRotator(0.f, 0.f, 90.f));
	mRing2 = TDErw::Teil(this, RootComponent, TEXT("FeldRing2"), FVector(0.f, 0.f, 525.f), FRotator(90.f, 0.f, 0.f));
}

void ATDFieldProjector::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mRing0, TDErw::Modell(TEXT("FieldProjector"), TEXT("Ring0")));
	SetzeModell(mRing1, TDErw::Modell(TEXT("FieldProjector"), TEXT("Ring1")));
	SetzeModell(mRing2, TDErw::Modell(TEXT("FieldProjector"), TEXT("Ring2")));
	SperreTakt();   // 0.9.6 (Gesamtreview 12.11., A1): ein Barren hielt untertaktet 6000 s statt 60 s
}

void ATDFieldProjector::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	const bool bAktiv = IsTDIntact() && IsProducing();
	if (!bAktiv)
	{
		return;
	}
	mZeit += DeltaSeconds;
	if (mYawMesh)
	{
		mYawMesh->SetRelativeLocation(mDrehpunkt + FVector(0.f, 0.f, 12.f * FMath::Sin(mZeit * 1.5f)));
		mYawMesh->AddLocalRotation(FRotator(0.f, 30.f * DeltaSeconds, 0.f));
	}
	if (mRing0) mRing0->AddLocalRotation(FRotator(0.f, 70.f * DeltaSeconds, 0.f));
	if (mRing1) mRing1->AddLocalRotation(FRotator(0.f, -55.f * DeltaSeconds, 0.f));
	if (mRing2) mRing2->AddLocalRotation(FRotator(0.f, 40.f * DeltaSeconds, 0.f));
	mWirkTakt -= DeltaSeconds;
	if (mWirkTakt > 0.f)
	{
		return;
	}
	mWirkTakt = 0.5f;
	const float Bremse = TDBalance::Wert(TEXT("Tuerme.Stoerfeld.Bremse"), 0.6f);
	const float R = GetReichweite();
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	for (AFGCreature* F : Feinde)
	{
		if (F && F->IsAliveAndWell() && FVector::DistSquared(F->GetActorLocation(), GetActorLocation()) < R * R)
		{
			if (UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(F))
			{
				Hirn->Bremse(F, Bremse, 0.8f);
				++mGebremst;
			}
		}
	}
	ZeigeLeuchten(GetActorLocation() + mDrehpunkt, FLinearColor(0.7f, 0.25f, 1.f), 3.f, 0.6f, 0.f, true, 25.f);
}

FString ATDFieldProjector::ZusatzInfo() const
{
	return IsProducing() ? TDT(TEXT("Feld aktiv, Gegner gebremst"), TEXT("field active, enemies slowed")) : TDT(TEXT("bereit, wartet auf Gegner (braucht SAM-Barren)"), TEXT("ready, waiting for enemies (needs Reanimated SAM)"));
}

void ATDFieldProjector::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Bremse"), TEXT("Slow")), FString::Printf(TEXT("%.0f %%"), (1.f - TDBalance::Wert(TEXT("Tuerme.Stoerfeld.Bremse"), 0.6f)) * 100.f), true });
	Out.Add({ TDT(TEXT("Feld"), TEXT("Field")), FString::Printf(TEXT("%.0f m"), GetReichweite() / 100.f), true });
	Out.Add({ TDT(TEXT("Verbrauch"), TEXT("Consumption")), TDF(TEXT("1 Barren je %.0f s"), TEXT("1 Reanimated SAM per %.0f s"), Zykluszeit()), true });
	Out.Add({ TDT(TEXT("Bremsungen"), TEXT("Enemies slowed")), FString::Printf(TEXT("%d"), mGebremst), true });
}

// =====================================================================================================================
// Singularitaetswerfer (td_singularitaet.py: Drehteil auf 1,86 m, Werfer-Kanzel 2,3 m darueber, Muendung 2,3 m vorn)

ATDSingularityLauncher::ATDSingularityLauncher()
{
	mTurmName = TEXT("Singularitaet");
	mSchadensart = ETDSchadensart::Explosion;
	mDreht = true;
	mRange = 5000.f;
	mDrehpunkt = FVector(0.f, 0.f, 186.f);
	mNeigeachse = FVector(0.f, 0.f, 230.f);
	mMuendung = FVector(230.f, 0.f, 0.f);
	mHalbmasse = FVector(300.f, 300.f, 55.f);
	mLampenOrt = FVector(-150.f, 305.f, 340.f);   // 0.9.3: Lampe auf dem Original-Statusmast
	OriginalTeil(TDOriginal::Eingang, FVector(-350.f, 0.f, 0.f), 90.f);
	OriginalTeil(TDOriginal::Strom, FVector(-240.f, -300.f, 75.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-150.f, 305.f, 0.f), 180.f);
	mKlarraumHoehe = 620.f;
	mKlarraumHalb = FVector2D(305.f, 305.f);
	mBaseMaxHealth = 5000.f;
	mNeigungMax = 55.f;
	mTurnSpeed = 60.f;
	mYawModell = TDErw::Modell(TEXT("SingularityLauncher"), TEXT("Yaw"));
	mHeadModell = TDErw::Modell(TEXT("SingularityLauncher"), TEXT("Pitch"));
	mStandardRezept = TDErw::Rezept(TEXT("SingularityLauncher"), TEXT("Singularitaet_Ficsonium"));
}

void ATDSingularityLauncher::BeginPlay()
{
	Super::BeginPlay();
}

void ATDSingularityLauncher::Feuern(AFGCreature* Ziel)
{
	if (!Ziel)
	{
		return;
	}
	// 0.9.3: Lauf waechst mit den Stufen (L3 Verlaengerung bis 3,4 m, L5 Feldemitter bis 4,2 m)
	mMuendung = FVector(GetLevel() >= 5 ? 420.f : GetLevel() >= 3 ? 340.f : 230.f, 0.f, 0.f);
	++mSchuesse;
	const FVector Ort = Ziel->GetActorLocation();
	const FVector Muendung = GetMuzzleLocation();
	const float Flugzeit = FVector::Dist(Muendung, Ort) / 2500.f;
	const float Sog = TDBalance::Wert(TEXT("Tuerme.Singularitaet.Sogzeit"), 1.5f);
	const float Radius = TDBalance::Wert(TEXT("Tuerme.Singularitaet.Radius"), 12.f) * 100.f;
	const float Schaden = TDBalance::Wert(TEXT("Tuerme.Singularitaet.Schaden"), 1500.f) * SchadenBonus();
	ZeigeSpur(Muendung, Ort, FLinearColor(0.6f, 0.2f, 1.f), Flugzeit, 2.5f);
	ZeigeLeuchten(Ort + FVector(0.f, 0.f, 120.f), FLinearColor(0.5f, 0.1f, 1.f), 6.f, Sog, Flugzeit, true, 40.f);
	ZeigeLeuchten(Ort + FVector(0.f, 0.f, 120.f), FLinearColor(1.f, 0.85f, 1.f), 10.f, 0.35f, Flugzeit + Sog, false, 60.f);
	TWeakObjectPtr<ATDSingularityLauncher> Selbst = this;
	// Einschlag: alle in Reichweite festhalten
	auto Einschlag = [Selbst, Ort, Radius, Sog]()
	{
		ATDSingularityLauncher* T = Selbst.Get();
		if (!T)
		{
			return;
		}
		TArray<AFGCreature*> Feinde;
		T->SammleFeinde(Feinde);
		for (AFGCreature* F : Feinde)
		{
			if (F && F->IsAliveAndWell() && FVector::DistSquared(F->GetActorLocation(), Ort) < Radius * Radius)
			{
				if (UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(F))
				{
					Hirn->Bremse(F, TDBalance::Wert(TEXT("Tuerme.Singularitaet.Bremse"), 0.05f), Sog);
				}
			}
		}
	};
	// Nach der Sogzeit: Sprengung
	auto Sprengung = [Selbst, Ort, Radius, Schaden]()
	{
		ATDSingularityLauncher* T = Selbst.Get();
		if (!T)
		{
			return;
		}
		TArray<AFGCreature*> Feinde;
		T->SammleFeinde(Feinde);
		int32 Anzahl = 0;
		for (AFGCreature* F : Feinde)
		{
			if (F && F->IsAliveAndWell() && FVector::DistSquared(F->GetActorLocation(), Ort) < Radius * Radius)
			{
				T->TrefferArt(F, Schaden, (F->GetActorLocation() - Ort).GetSafeNormal2D(), 0.f, 0.f, ETDSchadensart::Explosion);
				++Anzahl;
			}
		}
		T->mGetroffen += Anzahl;
		UE_LOG(LogFicsitTD, Log, TEXT("Singularitaet %s: Sprengung, %d Gegner"), *T->GetName(), Anzahl);
	};
	FTimerHandle H1, H2;
	GetWorldTimerManager().SetTimer(H1, FTimerDelegate::CreateLambda(Einschlag), FMath::Max(Flugzeit, 0.01f), false);
	GetWorldTimerManager().SetTimer(H2, FTimerDelegate::CreateLambda(Sprengung), Flugzeit + Sog, false);
}

FString ATDSingularityLauncher::ZusatzInfo() const
{
	return HatMunition() ? FString() : TDT(TEXT("braucht Ficsonium"), TEXT("needs Ficsonium"));
}

void ATDSingularityLauncher::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Schaden"), TEXT("Damage")), TDF(TEXT("%.0f in %.0f m"), TEXT("%.0f within %.0f m"), TDBalance::Wert(TEXT("Tuerme.Singularitaet.Schaden"), 1500.f) * SchadenBonus(),
		TDBalance::Wert(TEXT("Tuerme.Singularitaet.Radius"), 12.f)), true });
	Out.Add({ TDT(TEXT("Sog"), TEXT("Pull")), TDF(TEXT("%.1f s festgehalten"), TEXT("%.1f s held"), TDBalance::Wert(TEXT("Tuerme.Singularitaet.Sogzeit"), 1.5f)), true });
	Out.Add({ TDT(TEXT("Feuerrate"), TEXT("Fire rate")), FString::Printf(TEXT("%.1f s"), Zykluszeit()), true });
	Out.Add({ TDT(TEXT("Getroffen"), TEXT("Hit")), FString::Printf(TEXT("%d"), mGetroffen), true });
}


// =====================================================================================================================
// 0.9.3 Drohnenplatz (td_drohnenplatz.py: Plattform 6 x 6 m, vier Landefelder bei +-1,5 m auf 1,0 m Hoehe)

static const FVector DrohnenFelder[4] = { FVector(150.f, 150.f, 110.f), FVector(150.f, -150.f, 110.f), FVector(-150.f, 150.f, 110.f), FVector(-150.f, -150.f, 110.f) };

ATDDronePort::ATDDronePort()
{
	mTurmName = TEXT("Drohnenplatz");
	mDreht = false;
	mRange = 4000.f;
	mMuendung = FVector::ZeroVector;
	mHalbmasse = FVector(300.f, 300.f, 60.f);
	mKlarraumHoehe = 500.f;
	mKlarraumHalb = FVector2D(305.f, 305.f);
	mBaseMaxHealth = 4000.f;
	mLampenOrt = FVector(-150.f, 345.f, 340.f);
	OriginalTeil(TDOriginal::Eingang, FVector(-350.f, 0.f, 0.f), 90.f);
	OriginalTeil(TDOriginal::Strom, FVector(150.f, 300.f, 60.f), 90.f);   // Dose an der Unreal-+Y-Seite, ragt nach aussen
	OriginalTeil(TDOriginal::Mast, FVector(-150.f, 345.f, 0.f), 180.f);
	mYawModell = TDErw::Modell(TEXT("DronePort"), TEXT("Tower"));
	mStandardRezept = TDErw::Rezept(TEXT("DronePort"), TEXT("Drohne_Reparatur"));
}

void ATDDronePort::BeginPlay()
{
	Super::BeginPlay();
	BaueDrohnen();
	UE_LOG(LogFicsitTD, Log, TEXT("Drohnenplatz %s: Level %d von %d, Soll %d Drohnen"), *GetName(), GetLevel(), MaxLevel(), SollDrohnen());
}

bool ATDDronePort::IstKampf() const
{
	const UClass* R = GetCurrentRecipe();
	return R && R->GetName().Contains(TEXT("Kampf"));
}

// 0.9.6 (Gesamtreview 12.11., A2, Marius 12.11.): Drohnenzahl je Level steht in DrohnenFuer (L1 2, L2 3, L3 4).
// Vorher +1 bei Level 3 und 5, die der Drohnenplatz nie erreichte (keine Level-Upgrades).
int32 ATDDronePort::LevelPreis() const
{
	const int32 Naechstes = GetLevel() + 1;
	if (Naechstes > MaxLevel())
	{
		return 0;
	}
	return FMath::Max(0, TDBalance::Ganz(FString::Printf(TEXT("Tuerme.Drohnenplatz.LevelPreis%d"), Naechstes), Naechstes == 2 ? 8 : 15));
}

void ATDDronePort::LevelZusatzText(int32 Von, int32 Bis, TArray<FString>& Teile) const
{
	const int32 Mehr = DrohnenFuer(FMath::Min(Bis, MaxLevel())) - DrohnenFuer(FMath::Min(Von, MaxLevel()));
	if (Mehr > 0)
	{
		Teile.Add(Mehr == 1 ? TDT(TEXT("+1 Drohne"), TEXT("+1 drone")) : TDF(TEXT("+%d Drohnen"), TEXT("+%d drones"), Mehr));
	}
}

void ATDDronePort::BaueDrohnen()
{
	// Original-Drohne des Spiels (Skelettmodell, per Pfad geladen); faellt sie aus, eigene Drohne aus dem Mod
	USkeletalMesh* Original = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/DroneStation/Mesh/SK_DroneTransport_01.SK_DroneTransport_01"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UStaticMesh* Eigene = Original ? nullptr : LoadObject<UStaticMesh>(nullptr, *TDErw::Modell(TEXT("DronePort"), TEXT("Drone")), nullptr, LOAD_NoWarn | LOAD_Quiet);
	const float Groesse = TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.DrohnenGroesse"), 0.2f);
	while (mDrohnen.Num() < SollDrohnen())
	{
		const int32 i = mDrohnen.Num();
		USceneComponent* Teil = nullptr;
		if (Original)
		{
			USkeletalMeshComponent* K = NewObject<USkeletalMeshComponent>(this);
			K->SetSkeletalMesh(Original);
			Teil = K;
		}
		else if (Eigene)
		{
			UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(this);
			K->SetStaticMesh(Eigene);
			Teil = K;
		}
		else
		{
			Teil = NewObject<USceneComponent>(this);
		}
		if (UPrimitiveComponent* P = Cast<UPrimitiveComponent>(Teil))
		{
			P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			P->SetCanEverAffectNavigation(false);
		}
		Teil->SetupAttachment(RootComponent);
		Teil->SetUsingAbsoluteLocation(true);
		Teil->SetWorldScale3D(FVector(Groesse));
		Teil->RegisterComponent();
		FDrohne D;
		D.Platz = GetActorTransform().TransformPosition(DrohnenFelder[i % 4]);
		D.Ort = D.Platz;
		Teil->SetWorldLocation(D.Ort);
		mDrohnenTeile.Add(Teil);
		mDrohnen.Add(D);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Drohnenplatz %s: %d Drohnen (%s)"), *GetName(), mDrohnen.Num(), Original ? TEXT("Original-Drohne") : Eigene ? TEXT("eigene Drohne") : TEXT("ohne Modell"));
}

int32 ATDDronePort::FreieDrohne() const
{
	for (int32 i = 0; i < mDrohnen.Num(); ++i)
	{
		if (mDrohnen[i].Zustand == 0)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

AActor* ATDDronePort::SucheBeschaedigt() const
{
	// Am staerksten beschaedigter Bau in Reichweite, der noch keine Drohne hat
	AActor* Best = nullptr;
	float BestAnteil = 0.995f;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const ITDDamageable* D = Cast<ITDDamageable>(*It);
		if (!D || !D->IsTDIntact() || FVector::Dist(It->GetActorLocation(), GetActorLocation()) > GetReichweite())
		{
			continue;
		}
		const float Anteil = D->GetTDHealth() / FMath::Max(D->GetTDMaxHealth(), 1.f);
		bool bVergeben = false;
		for (const FDrohne& X : mDrohnen)
		{
			bVergeben |= X.Zustand != 0 && X.Ziel.Get() == *It;
		}
		if (!bVergeben && Anteil < BestAnteil)
		{
			BestAnteil = Anteil;
			Best = *It;
		}
	}
	return Best;
}

bool ATDDronePort::BereitZumProduzieren(AFGCreature* Ziel) const
{
	if (FreieDrohne() == INDEX_NONE)
	{
		return false;
	}
	return IstKampf() ? Ziel != nullptr : mGibtArbeit;
}

void ATDDronePort::Feuern(AFGCreature* Ziel)
{
	// Ein Zyklus = eine Drohne startet zu ihrem Ziel
	const int32 i = FreieDrohne();
	if (i == INDEX_NONE)
	{
		return;
	}
	AActor* Auftrag = IstKampf() ? static_cast<AActor*>(Ziel) : SucheBeschaedigt();
	if (!Auftrag)
	{
		return;
	}
	FDrohne& D = mDrohnen[i];
	D.Ziel = Auftrag;
	D.Zustand = 1;
	D.Arbeit = IstKampf() ? TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.KampfDauer"), 3.f) : TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.ReparaturDauer"), 2.f);
	D.Takt = 0.f;
	D.Menge = IstKampf() ? TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.KampfSchaden"), 25.f) * SchadenBonus()
		: ATDRepairStation::ReparaturFuer(AktuelleMunition()) * SchadenBonus();
	++mEinsaetze;
	++mSchuesse;
}

void ATDDronePort::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	if (mDrohnen.Num() < SollDrohnen())
	{
		BaueDrohnen();
	}
	mSuchTakt -= DeltaSeconds;
	if (mSuchTakt <= 0.f)
	{
		mSuchTakt = 1.f;
		mGibtArbeit = !IstKampf() && SucheBeschaedigt() != nullptr;
	}
	const float Tempo = TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.Tempo"), 1500.f) * DeltaSeconds;
	const float Reise = TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.Flughoehe"), 15.f) * 100.f;
	const float Basis = GetActorLocation().Z;
	for (int32 i = 0; i < mDrohnen.Num(); ++i)
	{
		FDrohne& D = mDrohnen[i];
		USceneComponent* Teil = mDrohnenTeile.IsValidIndex(i) ? mDrohnenTeile[i].Get() : nullptr;
		AActor* Z = D.Ziel.Get();
		if (D.Zustand >= 1 && D.Zustand <= 3 && (!Z || (Cast<ITDDamageable>(Z) && !Cast<ITDDamageable>(Z)->IsTDIntact())))
		{
			D.Zustand = 4;   // Ziel weg: zurueck
		}
		FVector Soll = D.Ort;
		switch (D.Zustand)
		{
		case 1: Soll = FVector(D.Ort.X, D.Ort.Y, Basis + Reise); if (FMath::IsNearlyEqual(D.Ort.Z, Soll.Z, 5.f)) D.Zustand = 2; break;
		case 2:
		{
			const FVector ZOrt = Z->GetActorLocation();
			Soll = FVector(ZOrt.X, ZOrt.Y, Basis + Reise);
			if (FVector::Dist2D(D.Ort, ZOrt) < 150.f) D.Zustand = 3;
			break;
		}
		case 3:
		{
			const FVector ZOrt = Z->GetActorLocation();
			Soll = ZOrt + FVector(0.f, 0.f, IstKampf() ? 600.f : 450.f);
			D.Arbeit -= DeltaSeconds;
			D.Takt -= DeltaSeconds;
			if (D.Takt <= 0.f)
			{
				D.Takt = 0.35f;
				if (AFGCreature* F = Cast<AFGCreature>(Z))
				{
					Treffer(F, D.Menge, (ZOrt - D.Ort).GetSafeNormal(), 0.f);
					ZeigeSpur(D.Ort, ZOrt + FVector(0.f, 0.f, 40.f), FLinearColor(1.f, 0.75f, 0.3f), 0.06f, 0.6f);
				}
				else if (ITDDamageable* B = Cast<ITDDamageable>(Z))
				{
					const float Teil0 = D.Menge * 0.35f / FMath::Max(TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.ReparaturDauer"), 2.f), 0.35f);
					mGeheilt += B->HealTD(Teil0);
					ZeigeSpur(D.Ort, ZOrt + FVector(0.f, 0.f, 150.f), FLinearColor(0.2f, 1.f, 0.35f), 0.3f, 0.8f);
				}
			}
			if (D.Arbeit <= 0.f) D.Zustand = 4;
			break;
		}
		case 4: Soll = FVector(D.Platz.X, D.Platz.Y, Basis + Reise); if (FVector::Dist2D(D.Ort, D.Platz) < 30.f) D.Zustand = 5; break;
		case 5: Soll = D.Platz; if (FVector::Dist(D.Ort, D.Platz) < 5.f) { D.Zustand = 0; D.Ziel.Reset(); } break;
		default: Soll = D.Platz; break;
		}
		const FVector Weg = Soll - D.Ort;
		D.Ort += Weg.GetClampedToMaxSize(D.Zustand == 3 ? Tempo * 0.5f : Tempo);
		if (Teil)
		{
			Teil->SetWorldLocation(D.Ort);
			if (!Weg.IsNearlyZero(1.f) && D.Zustand != 0)
			{
				Teil->SetWorldRotation(FRotator(0.f, Weg.Rotation().Yaw, 0.f));
			}
		}
	}
}

FString ATDDronePort::ZusatzInfo() const
{
	int32 Unterwegs = 0;
	for (const FDrohne& D : mDrohnen)
	{
		Unterwegs += D.Zustand != 0 ? 1 : 0;
	}
	return TDF(TEXT("%s: %d von %d Drohnen unterwegs, %d Einsaetze"), TEXT("%s: %d of %d drones out, %d sorties"),
		*(IstKampf() ? TDT(TEXT("Kampfdrohnen"), TEXT("Combat drones")) : TDT(TEXT("Reparaturdrohnen"), TEXT("Repair drones"))), Unterwegs, mDrohnen.Num(), mEinsaetze);
}

void ATDDronePort::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Drohnen"), TEXT("Drones")), FString::Printf(TEXT("%d"), SollDrohnen()), true });
	Out.Add({ TDT(TEXT("Umkreis"), TEXT("Radius")), FString::Printf(TEXT("%.0f m"), GetReichweite() / 100.f), true });
	if (IstKampf())
	{
		Out.Add({ TDT(TEXT("Schaden je Treffer"), TEXT("Damage per hit")), FString::Printf(TEXT("%.0f"), TDBalance::Wert(TEXT("Tuerme.Drohnenplatz.KampfSchaden"), 25.f) * SchadenBonus()), true });
	}
	else
	{
		Out.Add({ TDT(TEXT("Reparatur je Einsatz"), TEXT("Repair per sortie")), TDF(TEXT("%.0f LP"), TEXT("%.0f HP"), ATDRepairStation::ReparaturFuer(AktuelleMunition()) * SchadenBonus()), true });
	}
}
