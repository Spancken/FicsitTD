#include "TDDefenseCore.h"
#include "TDGegnerHirn.h"
#include "FicsitTDModule.h"
#include "TDDecoy.h"
#include "TDEinsatz.h"
#include "TDBalance.h"
#include "TDForschung.h"
#include "Creature/FGCreature.h"
#include "Equipment/FGAmmoTypeProjectile.h"
#include "FGProjectile.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Resources/FGItemDescriptor.h"
#include "TDKernTerminal.h"
#include "TDWaveSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

static constexpr float KernAnimIntervall = 1.f / 30.f;

ATDDefenseCore::ATDDefenseCore()
{
	SetCanBeDamaged(true);
	// Modelle setzt die Blueprint (setup_turret_blueprint.py); BeginPlay laedt sie notfalls nach
	mAufbau = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KernAufbau"));
	mAufbau->SetupAttachment(RootComponent);
	mAufbau->SetRelativeLocation(FVector(0.f, 0.f, 225.f));

	mKristall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KernKristall"));
	mKristall->SetupAttachment(mAufbau);
	mKristall->SetRelativeLocation(FVector(0.f, 0.f, 215.f));
	mKristall->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Tiefbohrer (0.61): bewegliche Teile, ohne Kollision (der Sockel traegt die Kollision)
	auto Teil = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* T = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		T->SetupAttachment(RootComponent);
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->SetCanEverAffectNavigation(false);
		return T;
	};
	mBohrer = Teil(TEXT("KernBohrer"));
	mHub = Teil(TEXT("KernHub"));
	mZahnkranz = Teil(TEXT("KernZahnkranz"));
	mLeuchtring = Teil(TEXT("KernLeuchtring"));
}

void ATDDefenseCore::BeginPlay()
{
	Super::BeginPlay();
	// 0.61: Aufbau und Kristall des alten Kerns entfallen (die Blueprint setzt sie noch), dafuer die Teile des Tiefbohrers
	if (mAufbau)
	{
		mAufbau->SetStaticMesh(nullptr);
	}
	if (mKristall)
	{
		mKristall->SetStaticMesh(nullptr);
	}
	auto Modell = [](UStaticMeshComponent* T, const TCHAR* Name)
	{
		if (T)
		{
			T->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/FicsitTD/Buildings/DefenseCore/Mesh/%s.%s"), Name, Name)));
			T->SetRelativeLocation(FVector::ZeroVector);
		}
	};
	Modell(mBohrer, TEXT("SM_TD_DefenseCore_Drill"));
	Modell(mHub, TEXT("SM_TD_DefenseCore_Lift"));
	Modell(mZahnkranz, TEXT("SM_TD_DefenseCore_Gear"));
	Modell(mLeuchtring, TEXT("SM_TD_DefenseCore_Ring"));
	if (mLeuchtring && mLeuchtring->GetStaticMesh())
	{
		const int32 Slot = mLeuchtring->GetStaticMesh()->GetMaterialIndex(TEXT("M_TD_StatusLight"));
		if (Slot != INDEX_NONE)
		{
			mRingMaterial = mLeuchtring->CreateDynamicMaterialInstance(Slot);
			mGluehen.Add(mRingMaterial);
		}
	}
	// Leuchtteile (M_TD_TeslaGlow, Parameter Leuchtkraft) in Aufbau und Kristall
	for (UStaticMeshComponent* Teil : { mAufbau.Get(), mKristall.Get() })
	{
		const int32 Slot = Teil && Teil->GetStaticMesh() ? Teil->GetStaticMesh()->GetMaterialIndex(TEXT("M_TD_TeslaGlow")) : INDEX_NONE;
		if (Slot != INDEX_NONE)
		{
			mGluehen.Add(Teil->CreateDynamicMaterialInstance(Slot));
		}
	}
	GetWorldTimerManager().SetTimer(mAnimTimer, this, &ATDDefenseCore::Animieren, KernAnimIntervall, true);
	// Kern-Menue (E) statt Chat-Befehlen. Die Felder sind in AFGBuildable private -> ueber die Reflexion setzen
	if (FBoolProperty* Nutzbar = FindFProperty<FBoolProperty>(AFGBuildable::StaticClass(), TEXT("mIsUseable")))
	{
		Nutzbar->SetPropertyValue_InContainer(this, true);
	}
	if (FSoftClassProperty* Menue = FindFProperty<FSoftClassProperty>(AFGBuildable::StaticClass(), TEXT("mInteractWidgetSoftClass")))
	{
		Menue->SetPropertyValue_InContainer(this, FSoftObjectPtr(UTDKernTerminal::StaticClass()));
	}
	if (!HasAuthority())
	{
		return;
	}
	if (mHealth < 0.f)
	{
		mHealth = mMaxHealth;
	}
	GetWorldTimerManager().SetTimer(mGeschuetzTimer, this, &ATDDefenseCore::Geschuetz, 1.5f, true, 1.5f);
	// 8 Koeder rund um den Sockel (Modell +-204 cm). Test 0.16.2: die alten sassen 1,1 m von der Mitte im Sockel, Gegner kamen nicht heran
	// 0.61 Tiefbohrer: Sockel 8 x 8 m -> 12 Koeder knapp ausserhalb
	ATDDecoy::SpawnRing(this, 940.f, 95.f, 16, mDecoys);   // 0.9.3: Kern doppelt so gross (16 x 16 m), Koeder knapp ausserhalb
	// Spielt das Navigationsnetz nur um "Invoker" (Spieler) herum, erzeugt der Kern sein eigenes bis hinter die weitesten Fronten (300 m)
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		if (Nav->IsActiveTilesGenerationEnabled())
		{
			Nav->RegisterNavigationInvoker(this, 32000.f, 36000.f);
			UE_LOG(LogFicsitTD, Log, TEXT("Abwehrkern %s: Navigationsnetz-Invoker 320 m registriert"), *GetName());
		}
	}
	// Einsatz (Kontingente) aus dem Spielstand uebernehmen
	if (mEinsatzAktiv)
	{
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->Lade(this);
		}
	}
	// Gespeicherten Durchgang fortsetzen
	if (mLaufAktiv)
	{
		if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
		{
			Waves->SetzeLaufFort(this);
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Abwehrkern %s: %.0f/%.0f LP"), *GetName(), mHealth, mMaxHealth);
}

float ATDDefenseCore::GetTDMaxHealth() const
{
	return mMaxHealth * (TDForschung::Hat(this, TEXT("KernPanzer")) ? TDBalance::Wert(TEXT("Forschung.KernPanzer.Faktor"), 1.3f) : 1.f);
}

void ATDDefenseCore::LadeSchild()
{
	mSchild = TDForschung::Hat(this, TEXT("KernSchild")) ? TDBalance::Wert(TEXT("Forschung.KernSchild.Menge"), 2000.f) : 0.f;
}

void ATDDefenseCore::Geschuetz()
{
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!Waves || !Waves->LaeuftWelle() || !TDForschung::Hat(this, TEXT("KernGeschuetz")))
	{
		return;
	}
	AFGCreature* Ziel = nullptr;
	float Best = FMath::Square(TDBalance::Wert(TEXT("Forschung.KernGeschuetz.Reichweite"), 30.f) * 100.f);
	for (AFGCreature* C : Waves->GetLivingWaveCreatures())
	{
		const float D = FVector::DistSquared(C->GetActorLocation(), GetActorLocation());
		if (D < Best)
		{
			Best = D;
			Ziel = C;
		}
	}
	if (!Ziel)
	{
		return;
	}
	// Optik wie beim Bolzenturm: Geschoss des Armierungsgewehrs, rein sichtbar
	const FVector Von = GetActorLocation() + FVector(0.f, 0.f, 1300.f);
	const FVector Nach = Ziel->GetActorLocation();
	UClass* Bolzen = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/SpikedRebar/Desc_SpikedRebar.Desc_SpikedRebar_C"));
	const UFGAmmoTypeProjectile* Typ = Cast<UFGAmmoTypeProjectile>(Bolzen ? Bolzen->GetDefaultObject() : nullptr);
	if (Typ && Typ->GetProjectileClass())
	{
		const FVector Richtung = (Nach - Von).GetSafeNormal();
		const FTransform Start(Richtung.Rotation(), Von);
		if (AFGProjectile* Geschoss = GetWorld()->SpawnActorDeferred<AFGProjectile>(Typ->GetProjectileClass(), Start, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Geschoss->SetImpactDamageTypes({});
			Geschoss->SetEndOfLifeDamageTypes({});
			UGameplayStatics::FinishSpawningActor(Geschoss, Start);
			Geschoss->SetActorEnableCollision(false);
			Geschoss->SetActorScale3D(FVector(3.f));
			Geschoss->SetInitialVelocity(Richtung * 6000.f);
			Geschoss->SetLifeSpan(FMath::Max(FVector::Dist(Von, Nach) / 6000.f, 0.05f));
		}
	}
	UGameplayStatics::ApplyDamage(Ziel, TDBalance::Wert(TEXT("Forschung.KernGeschuetz.Schaden"), 60.f), nullptr, this, UDamageType::StaticClass());
	UTDGegnerHirn::RestToeten(Ziel);
}

void ATDDefenseCore::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(mGeschuetzTimer);
	GetWorldTimerManager().ClearTimer(mAnimTimer);
	ATDDecoy::DestroyAll(mDecoys);
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		Nav->UnregisterNavigationInvoker(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ATDDefenseCore::GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const
{
	FFGClearanceData Data;
	Data.ClearanceBox = FBox(FVector(-810.f, -810.f, 0.f), FVector(810.f, 810.f, 3100.f));
	out_data.Add(Data);
}

FText ATDDefenseCore::GetLookAtDecription_Implementation(AFGCharacterPlayer* byCharacter, const FUseState& state) const
{
	FString Lauf;
	if (const UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr)
	{
		Lauf = Waves->GetLaufText();
	}
	return FText::FromString(FString::Printf(TEXT("Abwehrkern\nKern %.0f / %.0f LP\n%s"), mHealth, mMaxHealth, *Lauf));
}

void ATDDefenseCore::Aufladen()
{
	mHealth = mMaxHealth;
}

void ATDDefenseCore::ZeigeWege(const TArray<TArray<FVector>>& Wege)
{
	VersteckeWege();
	UStaticMesh* Ring = LoadObject<UStaticMesh>(nullptr, TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_RangeRing.SM_TD_RangeRing"));
	if (!Ring)
	{
		return;
	}
	// alle 10 m ein Leuchtring (1,5 m Radius) knapp ueber dem Weg, hoechstens 400 insgesamt
	for (const TArray<FVector>& Weg : Wege)
	{
		float BisNaechster = 0.f;
		for (int32 i = 1; i < Weg.Num() && mWegMarker.Num() < 400; ++i)
		{
			const FVector A = Weg[i - 1];
			const FVector B = Weg[i];
			const float Laenge = FVector::Dist(A, B);
			float s = BisNaechster;
			for (; s < Laenge && mWegMarker.Num() < 400; s += 1000.f)
			{
				UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
				M->SetStaticMesh(Ring);
				M->SetMobility(EComponentMobility::Movable);
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M->SetCastShadow(false);
				M->SetCanEverAffectNavigation(false);
				M->RegisterComponent();
				M->SetWorldLocation(FMath::Lerp(A, B, s / FMath::Max(Laenge, 1.f)) + FVector(0.f, 0.f, 30.f));
				M->SetWorldScale3D(FVector(1.5f, 1.5f, 1.f));
				mWegMarker.Add(M);
			}
			BisNaechster = s - Laenge;
		}
	}
}

void ATDDefenseCore::VersteckeWege()
{
	for (UStaticMeshComponent* M : mWegMarker)
	{
		if (M)
		{
			M->DestroyComponent();
		}
	}
	mWegMarker.Reset();
}

float ATDDefenseCore::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		return 0.f;   // Giftwolken, Umgebung, eigene Waffen
	}
	ApplyTDDamage(DamageAmount, DamageCauser);
	return DamageAmount;
}

void ATDDefenseCore::ApplyTDDamage(float Amount, const AActor* Causer)
{
	if (!HasAuthority() || !IsTDIntact() || Amount <= 0.f)
	{
		return;
	}
	if (mSchild > 0.f)
	{
		const float Ab = FMath::Min(mSchild, Amount);
		mSchild -= Ab;
		Amount -= Ab;
		if (mSchild <= 0.f)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Abwehrkern %s: Kern-Schild erschoepft"), *GetName());
		}
		if (Amount <= 0.f)
		{
			mTrefferBlitz = 0.3f;
			return;
		}
	}
	mHealth = FMath::Max(0.f, mHealth - Amount);
	mTrefferBlitz = 0.3f;
	if (mHealth <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Abwehrkern %s gefallen"), *GetName());
		if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
		{
			Waves->KernGefallen(this);
		}
	}
}

void ATDDefenseCore::Animieren()
{
	mZeit += KernAnimIntervall;
	mTrefferBlitz = FMath::Max(0.f, mTrefferBlitz - KernAnimIntervall);
	const float Anteil = mMaxHealth > 0.f ? FMath::Max(mHealth, 0.f) / mMaxHealth : 0.f;
	// Tiefbohrer: in der Welle faehrt der Bohrer ins Loch und dreht schnell, in der Bauphase steht er oben und dreht langsam
	const UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const bool bBohrt = Waves && Waves->LaeuftWelle() && Anteil > 0.f;
	const float Soll = bBohrt ? 0.f : 300.f;   // 0.9.3: Hub mit dem Kern verdoppelt
	mBohrHoehe = FMath::FInterpConstantTo(mBohrHoehe, Soll, KernAnimIntervall, 120.f);
	const float Ruetteln = bBohrt && mBohrHoehe < 5.f ? 1.5f * FMath::Sin(mZeit * 45.f) : 0.f;
	if (mBohrer)
	{
		mBohrer->SetRelativeLocation(FVector(0.f, 0.f, mBohrHoehe + Ruetteln));
		mBohrer->AddLocalRotation(FRotator(0.f, (bBohrt ? 160.f : 18.f) * KernAnimIntervall, 0.f));
	}
	if (mHub)
	{
		mHub->SetRelativeLocation(FVector(0.f, 0.f, mBohrHoehe));
	}
	if (mZahnkranz)
	{
		mZahnkranz->AddLocalRotation(FRotator(0.f, -(bBohrt ? 70.f : 8.f) * KernAnimIntervall, 0.f));
	}
	// Leuchtring: gruen -> orange -> rot mit sinkenden Lebenspunkten
	if (mRingMaterial)
	{
		const FLinearColor Gut(0.15f, 1.f, 0.3f), Mittel(1.f, 0.55f, 0.05f), Schlecht(1.f, 0.05f, 0.03f);
		const FLinearColor Farbe = Anteil > 0.5f ? FLinearColor::LerpUsingHSV(Mittel, Gut, (Anteil - 0.5f) * 2.f) : FLinearColor::LerpUsingHSV(Schlecht, Mittel, Anteil * 2.f);
		mRingMaterial->SetVectorParameterValue(TEXT("Farbe"), Farbe);
	}
	// Pulsieren; je weniger Lebenspunkte, desto unruhiger; Treffer lassen ihn flackern; gefallen = fast dunkel
	const float Puls = Anteil > 0.f ? 6.f + 4.f * FMath::Sin(mZeit * (1.5f + 4.f * (1.f - Anteil))) : 0.5f;
	const float Wert = Puls + (mTrefferBlitz > 0.f ? FMath::FRandRange(0.f, 25.f) : 0.f);
	for (UMaterialInstanceDynamic* M : mGluehen)
	{
		if (M)
		{
			M->SetScalarParameterValue(TEXT("Leuchtkraft"), Wert);
		}
	}
}
