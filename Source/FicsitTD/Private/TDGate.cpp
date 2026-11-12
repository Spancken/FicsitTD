#include "TDGate.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDDecoy.h"
#include "TDEinsatz.h"
#include "TDForschung.h"
#include "TDGegnerHirn.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creature/FGCreature.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "NavAreas/NavArea_Null.h"
#include "TimerManager.h"
#include "FGPowerInfoComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDTurretBase.h"
#include "TDBautenErweitert.h"
#include "TDWaveSubsystem.h"
#include "WheeledVehicles/FGWheeledVehicle.h"
#include "TDSprache.h"

// Masse aus Modelle\td_tor.py (Blender m -> cm): Oeffnung 4 m, Fluegel je 2 m, Saeulen 2 m, Hoehe 5 m
static constexpr float TorOeffnung = 400.f;
static constexpr float TorFluegelZ = 11.f;
static constexpr float TorLeuchteZ = 549.f;
static constexpr float TorAnimIntervall = 1.f / 30.f;

ATDGate::ATDGate()
{
	SetCanBeDamaged(true);
	auto Teil = [this](const TCHAR* Name, const FVector& Ort, bool bKollision)
	{
		UStaticMeshComponent* T = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		T->SetupAttachment(RootComponent);
		T->SetRelativeLocation(Ort);
		T->SetCanEverAffectNavigation(false);   // die Oeffnung sperrt mNavSperre, fahrende Fluegel sollen kein Netz neu bauen
		T->SetCollisionEnabled(bKollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bKollision)
		{
			T->SetCollisionResponseToAllChannels(ECR_Block);
		}
		return T;
	};
	mFluegelL = Teil(TEXT("TorFluegelL"), FVector(-TorOeffnung / 4.f, 0.f, TorFluegelZ), true);
	mFluegelR = Teil(TEXT("TorFluegelR"), FVector(TorOeffnung / 4.f, 0.f, TorFluegelZ), true);
	mLeuchteL = Teil(TEXT("TorLeuchteL"), FVector(-300.f, 0.f, TorLeuchteZ), false);
	mLeuchteR = Teil(TEXT("TorLeuchteR"), FVector(300.f, 0.f, TorLeuchteZ), false);

	mNavSperre = CreateDefaultSubobject<UBoxComponent>(TEXT("TorNavSperre"));
	mNavSperre->SetupAttachment(RootComponent);
	mNavSperre->SetBoxExtent(FVector(TorOeffnung / 2.f, 140.f, 200.f));
	mNavSperre->SetRelativeLocation(FVector(0.f, 0.f, 200.f));
	mNavSperre->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	mNavSperre->SetCollisionResponseToAllChannels(ECR_Ignore);
	mNavSperre->SetCanEverAffectNavigation(true);
	mNavSperre->bDynamicObstacle = true;
	mNavSperre->SetAreaClassOverride(UNavArea_Null::StaticClass());   // setzt auch bUseSystemDefaultObstacleAreaClass = false
	mNavSperre->SetHiddenInGame(true);
	// 0.9.3 Strom: Anschluss an der Original-Dose hinten an der linken Saeule (Modelle\Original_Anschluesse.json, Bau_Tor)
	mStrom = CreateDefaultSubobject<UTDTorStromAnschluss>(TEXT("TorStrom"));
	mStrom->SetupAttachment(RootComponent);
	mStrom->SetRelativeLocation(FVector(-300.f, -180.f, 160.f));
	mStromInfo = CreateDefaultSubobject<UFGPowerInfoComponent>(TEXT("TorStromInfo"));
}

void ATDGate::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (mStrom && mStromInfo)
	{
		mStrom->SetPowerInfo(mStromInfo);
		mStromInfo->SetTargetConsumption(TDBalance::Wert(TEXT("Bauten.Tor.Strom"), 2.f));
	}
}

void ATDGate::BeginPlay()
{
	Super::BeginPlay();
	auto Modell = [](UStaticMeshComponent* T, const TCHAR* Teil)
	{
		if (T && !T->GetStaticMesh())
		{
			T->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/Gate/Mesh/SM_TD_Gate_%s.SM_TD_Gate_%s"), Teil, Teil)));
		}
	};
	Modell(mFluegelL, TEXT("LeafL"));
	Modell(mFluegelR, TEXT("LeafR"));
	Modell(mLeuchteL, TEXT("BeaconL"));
	Modell(mLeuchteR, TEXT("BeaconR"));
	// 0.9.3: Original-Stromdose und Statuslampe (gruen = Strom, rot = kein Strom, in der Welle offen)
	if (!IsRunningDedicatedServer())
	{
		mStromDose = NewObject<UStaticMeshComponent>(this);
		mStromDose->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TDOriginal::Strom, nullptr, LOAD_NoWarn | LOAD_Quiet));
		mStromDose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		mStromDose->SetupAttachment(RootComponent);
		mStromDose->SetRelativeLocation(FVector(-300.f, -90.f, 160.f));
		mStromDose->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		mStromDose->RegisterComponent();
		mStatusLampe = NewObject<UStaticMeshComponent>(this);
		mStatusLampe->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_StatusLight.SM_TD_StatusLight"), nullptr, LOAD_NoWarn | LOAD_Quiet));
		mStatusLampe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		mStatusLampe->SetupAttachment(RootComponent);
		mStatusLampe->SetRelativeLocation(FVector(0.f, 0.f, 525.f));
		mStatusLampe->RegisterComponent();
		if (mStatusLampe->GetStaticMesh() && mStatusLampe->GetNumMaterials() > 1)
		{
			mLampenMaterial = mStatusLampe->CreateDynamicMaterialInstance(1);
		}
	}
	if (mStrom && mStromInfo)
	{
		mStrom->SetPowerInfo(mStromInfo);
		mStromInfo->SetTargetConsumption(TDBalance::Wert(TEXT("Bauten.Tor.Strom"), 2.f));
	}
	GetWorldTimerManager().SetTimer(mAnimTimer, this, &ATDGate::Animieren, TorAnimIntervall, true);
	if (!HasAuthority())
	{
		return;
	}
	if (mHealth < 0.f)
	{
		mHealth = GetTDMaxHealth();
	}
	// Fuer die Gegner eine Mauer: je Seite zwei Koeder vor der Oeffnung und den Saeulen
	ATDDecoy::SpawnForBox(this, FVector(0.f, 0.f, 120.f), FVector(380.f, 90.f, 120.f), 2, mDecoys);
	GetWorldTimerManager().SetTimer(mSchrittTimer, this, &ATDGate::Schritt, 0.2f, true, 0.2f);
}

void ATDGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ATDDecoy::DestroyAll(mDecoys);
	Super::EndPlay(EndPlayReason);
}

void ATDGate::Schritt()
{
	const FVector Mitte = GetActorLocation();
	const float Oeffnen = TDBalance::Wert(TEXT("Bauten.Tor.Oeffnen"), 6.f) * 100.f;
	const float Sperre = TDBalance::Wert(TEXT("Bauten.Tor.GegnerSperre"), 15.f) * 100.f;
	bool bJemand = false;
	for (TActorIterator<AFGCharacterPlayer> It(GetWorld()); It && !bJemand; ++It)
	{
		bJemand = It->IsAliveAndWell() && FVector::Dist(It->GetActorLocation(), Mitte + FVector(0.f, 0.f, 150.f)) < Oeffnen;
	}
	for (TActorIterator<AFGWheeledVehicle> It(GetWorld()); It && !bJemand; ++It)
	{
		bJemand = FVector::Dist(It->GetActorLocation(), Mitte) < Oeffnen + 300.f;
	}
	mGegnerNah = false;
	for (TActorIterator<AFGCreature> It(GetWorld()); It && !mGegnerNah; ++It)
	{
		mGegnerNah = It->IsAliveAndWell() && UTDGegnerHirn::Von(*It) && FVector::Dist2D(It->GetActorLocation(), Mitte) < Sperre;
	}
	// 0.9.3: ohne Strom bleibt das Tor in der Welle offen, und die Sperre im Navigationsnetz faellt weg (Kreaturen kommen durch)
	const UTDWaveSubsystem* Wellen = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	const bool bStrom = mStromInfo && mStromInfo->HasPower();
	const bool bOffenOhneStrom = !bStrom && Wellen && Wellen->LaeuftWelle() && TDBalance::Wert(TEXT("Bauten.Tor.OhneStromOffen"), 1.f) > 0.f;
	if (bOffenOhneStrom != mOhneStromOffen)
	{
		mOhneStromOffen = bOffenOhneStrom;
		if (mNavSperre)
		{
			mNavSperre->SetCanEverAffectNavigation(!bOffenOhneStrom);
		}
		UE_LOG(LogFicsitTD, Log, TEXT("Tor %s: %s"), *GetName(), bOffenOhneStrom ? TEXT("kein Strom in der Welle - bleibt offen, Gegner kommen durch") : TEXT("Strom da oder Bauphase - normal"));
	}
	mSoll = bOffenOhneStrom ? 1.f : (bJemand && !mGegnerNah && IsTDIntact()) ? 1.f : 0.f;
	const int32 Lampe = !IsTDIntact() ? 0 : bOffenOhneStrom ? 1 : bStrom ? 3 : 2;
	if (mLampenMaterial && Lampe != mLampe)
	{
		mLampe = Lampe;
		static const FLinearColor Farben[] = { FLinearColor(0.02f, 0.02f, 0.02f), FLinearColor(1.f, 0.03f, 0.01f), FLinearColor(1.f, 0.55f, 0.02f), FLinearColor(0.1f, 1.f, 0.1f) };
		mLampenMaterial->SetVectorParameterValue(TEXT("Farbe"), Farben[Lampe]);
		mLampenMaterial->SetScalarParameterValue(TEXT("Leuchtkraft"), Lampe == 0 ? 0.f : 20.f);
	}
}

void ATDGate::Animieren()
{
	const float Alt = mOffen;
	// 2 m Weg in etwa 1,6 s, schliessen etwas schneller
	mOffen = FMath::FInterpConstantTo(mOffen, mSoll, TorAnimIntervall, mSoll > mOffen ? 0.6f : 0.8f);
	if (mFluegelL && mFluegelR && !FMath::IsNearlyEqual(Alt, mOffen))
	{
		const float Weg = mOffen * TorOeffnung / 2.f;
		mFluegelL->SetRelativeLocation(FVector(-TorOeffnung / 4.f - Weg, 0.f, TorFluegelZ));
		mFluegelR->SetRelativeLocation(FVector(TorOeffnung / 4.f + Weg, 0.f, TorFluegelZ));
	}
	// Warnleuchten drehen, solange sich das Tor bewegt oder Gegner nah sind
	if (!FMath::IsNearlyEqual(Alt, mOffen) || mGegnerNah)
	{
		mLeuchteWinkel = FMath::Fmod(mLeuchteWinkel + 240.f * TorAnimIntervall, 360.f);
		if (mLeuchteL) mLeuchteL->SetRelativeRotation(FRotator(0.f, -mLeuchteWinkel, 0.f));
		if (mLeuchteR) mLeuchteR->SetRelativeRotation(FRotator(0.f, mLeuchteWinkel, 0.f));
	}
}

void ATDGate::GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const
{
	FFGClearanceData Data;
	Data.ClearanceBox = FBox(FVector(-400.f, -80.f, 0.f), FVector(400.f, 80.f, 500.f));
	out_data.Add(Data);
}

FText ATDGate::GetLookAtDecription_Implementation(AFGCharacterPlayer* byCharacter, const FUseState& state) const
{
	return FText::FromString(TDF(TEXT("Tor (%s)\nLebenspunkte %.0f / %.0f%s"), TEXT("Gate (%s)\nHealth %.0f / %.0f%s"),
		*(mOffen > 0.95f ? TDT(TEXT("offen"), TEXT("open")) : mOffen < 0.05f ? TDT(TEXT("zu"), TEXT("closed")) : TDT(TEXT("bewegt sich"), TEXT("moving"))),
		FMath::Max(mHealth, 0.f), GetTDMaxHealth(), *(mGegnerNah ? TDT(TEXT("\nGegner in der Naehe: bleibt zu"), TEXT("\nEnemies nearby: stays closed")) : FString())));
}

float ATDGate::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		return 0.f;
	}
	// 0.9.6 (Gesamtreview 12.11., A3): direkte Treffer gehen zuerst auf die Kuppel eines Schildgenerators
	const float Durch = ATDShieldGenerator::Schlucke(GetWorld(), this, DamageAmount);
	if (Durch > 0.f)
	{
		ApplyTDDamage(Durch, DamageCauser);
	}
	return Durch;
}

void ATDGate::ApplyTDDamage(float Amount, const AActor* Causer)
{
	if (!HasAuthority() || !IsTDIntact() || Amount <= 0.f)
	{
		return;
	}
	mHealth = FMath::Max(0.f, mHealth - Amount);
	if (mHealth <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Tor %s zerstoert"), *GetName());
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->GebaeudeZerstoert(GetClass());
		}
		ATDDecoy::DestroyAll(mDecoys);
		TurnOffAndDestroy();
	}
}

float ATDGate::GetTDMaxHealth() const
{
	return TDBalance::Wert(TEXT("Bauten.Tor.LP"), 3000.f)
		* (TDForschung::Hat(this, TEXT("MauerMk2")) ? TDBalance::Wert(TEXT("Forschung.MauerMk2.Faktor"), 2.f) : 1.f);
}
