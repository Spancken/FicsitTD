#include "TDFlieger.h"
#include "TDDefenseCore.h"
#include "TDBalance.h"
#include "FicsitTDModule.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Engine/CollisionProfile.h"

// 25.11.2026 (Marius: Krabben raus, Vogelschwarm und Manta, "was machen die Manta - schiessen?" -> Manta schiesst auf den Kern).
// Modelle und Animationen sind Spiel-Assets (Starter-Project-Platzhalter), zur Laufzeit per LoadObject - der Mod liefert nichts davon mit.

int32 ATDFlieger::sAbgeschossen = 0;
int32 ATDFlieger::sSaeureTreffer = 0;

namespace TDFliegerPfad
{
	static const TCHAR* Vogel = TEXT("/Game/FactoryGame/Character/Creature/Wildlife/Bird/BirdRed/Mesh/VAT/BirdFlying.BirdFlying");
	static const TCHAR* Manta = TEXT("/Game/FactoryGame/Character/Creature/Wildlife/Manta/Mesh/Manta_01.Manta_01");
	static const TCHAR* MantaFlug = TEXT("/Game/FactoryGame/Character/Creature/Wildlife/Manta/Animation/Flying_01.Flying_01");
	static const TCHAR* Saeure = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/Mesh/SpitterProjectile_01.SpitterProjectile_01");
}

ATDFlieger::ATDFlieger()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	mKoerper = CreateDefaultSubobject<USphereComponent>(TEXT("Koerper"));
	mKoerper->InitSphereRadius(80.f);
	// trifft Spielerwaffen und Explosionen (alle Kanaele blocken), aendert kein Netz
	mKoerper->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	mKoerper->SetCanEverAffectNavigation(false);
	mKoerper->SetGenerateOverlapEvents(false);
	RootComponent = mKoerper;
	Tags.Add(TEXT("FicsitTD"));
}

void ATDFlieger::BeginPlay()
{
	Super::BeginPlay();
	mPhase = FMath::FRandRange(0.f, 2.f * PI);
	mBodenZ = GetActorLocation().Z - 600.f;
}

void ATDFlieger::Starte(ATDDefenseCore* Kern, float LP, float KernSchaden)
{
	mKern = Kern;
	mLP = mMaxLP = FMath::Max(1.f, LP);
	mKernSchaden = KernSchaden;
}

float ATDFlieger::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IstTot() || DamageAmount <= 0.f || Cast<ATDFlieger>(DamageCauser))
	{
		return 0.f;
	}
	mLP -= DamageAmount;
	if (mLP <= 0.f)
	{
		mLP = 0.f;
		++sAbgeschossen;
		UE_LOG(LogFicsitTD, Log, TEXT("Flieger: %s abgeschossen (%.0f LP, von %s)"), *Art(), mMaxLP, DamageCauser ? *DamageCauser->GetClass()->GetName() : TEXT("?"));
		Sterben();
	}
	return DamageAmount;
}

void ATDFlieger::Sterben()
{
	Destroy();
}

void ATDFlieger::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	mZeit += DeltaSeconds;
	if (!mKern.IsValid())
	{
		Destroy();
		return;
	}
	Fliege(DeltaSeconds);
}

float ATDFlieger::BodenZ(const FVector& Ort)
{
	mBodenTakt -= GetWorld()->GetDeltaSeconds();
	if (mBodenTakt <= 0.f)
	{
		mBodenTakt = 0.3f;
		FHitResult Treffer;
		FCollisionQueryParams P(SCENE_QUERY_STAT(TDFliegerBoden), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Treffer, Ort + FVector(0.f, 0.f, 2000.f), Ort - FVector(0.f, 0.f, 20000.f), ECC_Visibility, P))
		{
			mBodenZ = Treffer.ImpactPoint.Z;
		}
	}
	return mBodenZ;
}

void ATDFlieger::Ausrichten(const FVector& Richtung, float DeltaSeconds)
{
	if (Richtung.IsNearlyZero())
	{
		return;
	}
	const FRotator Soll = Richtung.Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Soll, DeltaSeconds, 4.f));
}

// ---------------------------------------------------------------------------------------------------------------
// Vogel

ATDFliegerVogel::ATDFliegerVogel()
{
	mMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Modell"));
	mMesh->SetupAttachment(mKoerper);
	mMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mMesh->SetCanEverAffectNavigation(false);
	mMesh->SetCastShadow(false);
}

void ATDFliegerVogel::BeginPlay()
{
	Super::BeginPlay();
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TDFliegerPfad::Vogel))
	{
		mMesh->SetStaticMesh(M);
	}
	else
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Flieger: Vogelmodell nicht gefunden (%s)"), TDFliegerPfad::Vogel);
	}
	mMesh->SetRelativeScale3D(FVector(TDBalance::Wert(TEXT("Flieger.Vogel.Skala"), 1.5f)));
	// Lage des Spielmodells zur Flugrichtung (+X), per Balance einstellbar (Fotolauf Flieger-Fotos)
	mMesh->SetRelativeRotation(FRotator(TDBalance::Wert(TEXT("Flieger.Vogel.Neigen"), 0.f), TDBalance::Wert(TEXT("Flieger.Vogel.Drehung"), 0.f),
		TDBalance::Wert(TEXT("Flieger.Vogel.Rollen"), 0.f)));
}

void ATDFliegerVogel::Fliege(float DeltaSeconds)
{
	const FVector Ziel = mKern->GetActorLocation() + FVector(0.f, 0.f, TDBalance::Wert(TEXT("Flieger.ZielHoehe"), 4.f) * 100.f);
	FVector Ort = GetActorLocation();
	const FVector Weg = Ziel - Ort;
	const float Abstand = Weg.Size();
	if (Abstand < TDBalance::Wert(TEXT("Flieger.Trefferabstand"), 12.f) * 100.f)
	{
		mAmKern = true;
		return;
	}
	const FVector Richtung = Weg / Abstand;
	// Schwarmflug: seitliches und senkrechtes Schlingern, damit der Schwarm nicht wie an der Schnur zieht
	const FVector Seite = FVector::CrossProduct(Richtung, FVector::UpVector).GetSafeNormal();
	const float Schlingern = TDBalance::Wert(TEXT("Flieger.Vogel.Schlingern"), 3.f) * 100.f;
	const FVector Wackel = Seite * FMath::Cos(mZeit * 1.7f + mPhase) * Schlingern * 1.7f + FVector::UpVector * FMath::Sin(mZeit * 2.3f + mPhase) * Schlingern * 0.5f;
	const FVector Schritt = (Richtung * TDBalance::Wert(TEXT("Flieger.Vogel.Tempo"), 14.f) * 100.f + Wackel) * DeltaSeconds;
	Ort += Schritt;
	// nicht durch Huegel: Mindesthoehe ueber Grund, ausser im Anflug auf den Kern
	const float Min = BodenZ(Ort) + TDBalance::Wert(TEXT("Flieger.MinHoehe"), 5.f) * 100.f;
	if (Abstand > 3000.f && Ort.Z < Min)
	{
		Ort.Z = FMath::FInterpTo(Ort.Z, Min, DeltaSeconds, 3.f);
	}
	SetActorLocation(Ort);
	Ausrichten(Schritt, DeltaSeconds);
}

// ---------------------------------------------------------------------------------------------------------------
// Manta

ATDFliegerManta::ATDFliegerManta()
{
	mKoerper->InitSphereRadius(260.f);
	mMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Modell"));
	mMesh->SetupAttachment(mKoerper);
	mMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mMesh->SetCanEverAffectNavigation(false);
	mMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
}

void ATDFliegerManta::BeginPlay()
{
	Super::BeginPlay();
	if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, TDFliegerPfad::Manta))
	{
		mMesh->SetSkeletalMesh(M);
		if (UAnimSequence* Flug = LoadObject<UAnimSequence>(nullptr, TDFliegerPfad::MantaFlug))
		{
			mMesh->PlayAnimation(Flug, true);
		}
		else
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Flieger: Manta-Flug nicht gefunden (%s)"), TDFliegerPfad::MantaFlug);
		}
	}
	else
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Flieger: Mantamodell nicht gefunden (%s)"), TDFliegerPfad::Manta);
	}
	mMesh->SetRelativeScale3D(FVector(TDBalance::Wert(TEXT("Flieger.Manta.Skala"), 0.35f)));
	mMesh->SetRelativeRotation(FRotator(TDBalance::Wert(TEXT("Flieger.Manta.Neigen"), 0.f), TDBalance::Wert(TEXT("Flieger.Manta.Drehung"), 0.f),
		TDBalance::Wert(TEXT("Flieger.Manta.Rollen"), 0.f)));
	mSchuss = FMath::FRandRange(0.5f, 1.f) * TDBalance::Wert(TEXT("Flieger.Manta.Takt"), 3.f);
	mWinkel = FMath::FRandRange(0.f, 2.f * PI);
}

void ATDFliegerManta::Fliege(float DeltaSeconds)
{
	PflegeSaeure(DeltaSeconds);
	if (mFall >= 0.f)
	{
		// abgeschossen: trudelt zu Boden und verschwindet
		mFall += DeltaSeconds;
		mFallTempo.Z -= 980.f * DeltaSeconds;
		SetActorLocation(GetActorLocation() + mFallTempo * DeltaSeconds);
		AddActorLocalRotation(FRotator(0.f, 0.f, 90.f * DeltaSeconds));
		if (mFall > 3.f || GetActorLocation().Z < BodenZ(GetActorLocation()))
		{
			Destroy();
		}
		return;
	}
	const FVector KernOrt = mKern->GetActorLocation();
	const float Radius = TDBalance::Wert(TEXT("Flieger.Manta.Abstand"), 30.f) * 100.f;
	const float Tempo = TDBalance::Wert(TEXT("Flieger.Manta.Tempo"), 8.f) * 100.f;
	const float Hoehe = KernOrt.Z + TDBalance::Wert(TEXT("Flieger.Manta.Hoehe"), 18.f) * 100.f;
	FVector Ort = GetActorLocation();
	const FVector2D Flach(Ort.X - KernOrt.X, Ort.Y - KernOrt.Y);
	const float Abstand = Flach.Size();
	FVector Soll;
	if (!mKreist && Abstand > Radius)
	{
		// Anflug
		const FVector Richtung = FVector(-Flach.X, -Flach.Y, 0.f).GetSafeNormal();
		Soll = Ort + Richtung * Tempo * DeltaSeconds;
		const float Min = BodenZ(Ort) + TDBalance::Wert(TEXT("Flieger.MinHoehe"), 5.f) * 100.f;
		Soll.Z = FMath::FInterpTo(Ort.Z, FMath::Max(Hoehe, Min), DeltaSeconds, 0.8f);
		mWinkel = FMath::Atan2(Flach.Y, Flach.X);
	}
	else
	{
		// Kreisen in Abstand, leichtes Auf und Ab
		mKreist = true;
		mWinkel += Tempo / Radius * DeltaSeconds;
		Soll = KernOrt + FVector(FMath::Cos(mWinkel) * Radius, FMath::Sin(mWinkel) * Radius, 0.f);
		Soll.Z = Hoehe + FMath::Sin(mZeit * 0.6f + mPhase) * 150.f;
		Soll = FMath::VInterpTo(Ort, Soll, DeltaSeconds, 1.5f);
	}
	const FVector Schritt = Soll - Ort;
	SetActorLocation(Soll);
	Ausrichten(FVector(Schritt.X, Schritt.Y, 0.f), DeltaSeconds);
	if (Abstand < Radius * 1.3f)
	{
		mSchuss -= DeltaSeconds;
		if (mSchuss <= 0.f)
		{
			mSchuss = TDBalance::Wert(TEXT("Flieger.Manta.Takt"), 3.f);
			Schiesse();
		}
	}
}

void ATDFliegerManta::Schiesse()
{
	UStaticMeshComponent* Kugel = NewObject<UStaticMeshComponent>(this);
	if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, TDFliegerPfad::Saeure))
	{
		Kugel->SetStaticMesh(M);
	}
	Kugel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Kugel->SetCanEverAffectNavigation(false);
	Kugel->SetUsingAbsoluteLocation(true);
	Kugel->SetUsingAbsoluteRotation(true);
	Kugel->SetWorldScale3D(FVector(TDBalance::Wert(TEXT("Flieger.Manta.SaeureSkala"), 2.f)));
	Kugel->SetupAttachment(RootComponent);
	Kugel->RegisterComponent();
	const FVector Ort = GetActorLocation() - FVector(0.f, 0.f, 100.f);
	Kugel->SetWorldLocation(Ort);
	mSaeure.Add({ Kugel, Ort });
}

void ATDFliegerManta::PflegeSaeure(float DeltaSeconds)
{
	ATDDefenseCore* Kern = mKern.Get();
	const FVector Ziel = Kern ? Kern->GetActorLocation() + FVector(0.f, 0.f, 300.f) : FVector::ZeroVector;
	const float Tempo = TDBalance::Wert(TEXT("Flieger.Manta.SaeureTempo"), 25.f) * 100.f;
	for (int32 i = mSaeure.Num() - 1; i >= 0; --i)
	{
		FSaeure& S = mSaeure[i];
		UStaticMeshComponent* K = S.Kugel.Get();
		if (!K || !Kern)
		{
			if (K)
			{
				K->DestroyComponent();
			}
			mSaeure.RemoveAtSwap(i);
			continue;
		}
		const FVector Weg = Ziel - S.Ort;
		const float Schritt = Tempo * DeltaSeconds;
		if (Weg.Size() <= Schritt + 150.f)
		{
			Kern->ApplyTDDamage(mKernSchaden, this);
			++sSaeureTreffer;
			UE_LOG(LogFicsitTD, Log, TEXT("Flieger: Manta-Saeure am Kern, %.0f Schaden"), mKernSchaden);
			K->DestroyComponent();
			mSaeure.RemoveAtSwap(i);
			continue;
		}
		S.Ort += Weg.GetSafeNormal() * Schritt;
		K->SetWorldLocationAndRotation(S.Ort, Weg.Rotation());
	}
}

void ATDFliegerManta::Sterben()
{
	// faellt sichtbar vom Himmel, statt zu verschwinden; Saeure in der Luft trifft noch
	mFall = 0.f;
	mFallTempo = FVector(0.f, 0.f, -200.f);
	mKoerper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mMesh->Stop();
}
