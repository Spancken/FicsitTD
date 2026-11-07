#include "TDDecoy.h"
#include "TDDamageable.h"
#include "TDBautenErweitert.h"
#include "FGHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

ATDDecoy::ATDDecoy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Moeglichst billig: kein Tick, keine Bewegung, kein Possess
	PrimaryActorTick.bCanEverTick = false;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	GetCapsuleComponent()->InitCapsuleSize(60.f, 90.f);
	// Kreaturen sollen die Mauer blockieren, nicht den Koeder - er bleibt aber fuer Treffer-Abfragen sichtbar
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	SetCanBeDamaged(true);
}

void ATDDecoy::SpawnForBox(AActor* Owner, const FVector& LocalCenter, const FVector& HalfExtent, int32 PerSide, TArray<TObjectPtr<ATDDecoy>>& OutDecoys)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || PerSide <= 0)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const float Seite : { -1.f, 1.f })
	{
		for (int32 i = 0; i < PerSide; ++i)
		{
			// gleichmaessig ueber die Laenge verteilt
			const float X = PerSide == 1 ? 0.f : FMath::Lerp(-HalfExtent.X * 0.6f, HalfExtent.X * 0.6f, float(i) / (PerSide - 1));
			const FVector Local = LocalCenter + FVector(X, Seite * (HalfExtent.Y + 70.f), -HalfExtent.Z + 95.f);
			const FVector Location = Owner->GetActorTransform().TransformPosition(Local);
			if (ATDDecoy* Decoy = World->SpawnActor<ATDDecoy>(ATDDecoy::StaticClass(), Location, Owner->GetActorRotation(), Params))
			{
				Decoy->SetTarget(Owner);
				OutDecoys.Add(Decoy);
			}
		}
	}
}

void ATDDecoy::SpawnRing(AActor* Owner, float Radius, float Hoehe, int32 Anzahl, TArray<TObjectPtr<ATDDecoy>>& OutDecoys)
{
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || Anzahl <= 0)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.Owner = Owner;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < Anzahl; ++i)
	{
		const FVector Local = FRotator(0.f, 360.f * i / Anzahl, 0.f).Vector() * Radius + FVector(0.f, 0.f, Hoehe);
		const FVector Location = Owner->GetActorTransform().TransformPosition(Local);
		if (ATDDecoy* Decoy = World->SpawnActor<ATDDecoy>(ATDDecoy::StaticClass(), Location, Owner->GetActorRotation(), Params))
		{
			Decoy->SetTarget(Owner);
			OutDecoys.Add(Decoy);
		}
	}
}

void ATDDecoy::DestroyAll(TArray<TObjectPtr<ATDDecoy>>& Decoys)
{
	for (ATDDecoy* Decoy : Decoys)
	{
		if (IsValid(Decoy))
		{
			Decoy->Destroy();
		}
	}
	Decoys.Reset();
}

AActor* ATDDecoy::GetActiveTarget() const
{
	AActor* Target = mTarget.Get();
	const ITDDamageable* Damageable = Cast<ITDDamageable>(Target);
	return Damageable && Damageable->IsTDIntact() ? Target : nullptr;
}

void ATDDecoy::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->GravityScale = 0.f;
		Movement->SetComponentTickEnabled(false);
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetComponentTickEnabled(false);
	}
	// Alles Sichtbare abschalten (Test 0.6.0: ein schwarzes Teil schwebte neben dem Turm). Kollision bleibt fuer Treffer.
	TInlineComponentArray<UPrimitiveComponent*> Teile(this);
	for (UPrimitiveComponent* Teil : Teile)
	{
		Teil->SetVisibility(false, true);
		Teil->SetHiddenInGame(true, true);
		Teil->SetCastShadow(false);
	}
	// Test 0.6.3: trotzdem noch ein schwarzer Umriss sichtbar -> ganzen Actor verstecken (Kollision und Treffer bleiben)
	SetActorHiddenInGame(true);
	static bool bProtokolliert = false;
	if (!bProtokolliert)
	{
		bProtokolliert = true;
		TInlineComponentArray<UActorComponent*> Alle(this);
		FString Liste;
		for (const UActorComponent* K : Alle)
		{
			Liste += FString::Printf(TEXT("%s(%s) "), *K->GetName(), *K->GetClass()->GetName());
		}
		UE_LOG(LogTemp, Log, TEXT("FicsitTD Koeder-Komponenten: %s"), *Liste);
	}
	if (UFGHealthComponent* Health = GetHealthComponent())
	{
		Health->SetMaxHealth(1000000.f);
		Health->ResetHealth();
		Health->OnTakeAnyDamageDelegate.AddDynamic(this, &ATDDecoy::OnHealthDamage);
	}
}

void ATDDecoy::Weiterleiten(float DamageAmount, const AActor* DamageCauser)
{
	if (ITDDamageable* Damageable = Cast<ITDDamageable>(GetActiveTarget()))
	{
		// 0.83 Schildgenerator: auch Geschosse der Spucker gehen zuerst auf die Kuppel
		const float Durch = ATDShieldGenerator::Schlucke(GetWorld(), GetActiveTarget(), DamageAmount);
		if (Durch > 0.f)
		{
			Damageable->ApplyTDDamage(Durch, DamageCauser);
		}
	}
}

float ATDDecoy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Nicht Super aufrufen: der Koeder verliert selbst nie Lebenspunkte; nur Gegner-Schaden weiterreichen (keine Giftwolken)
	if (TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		Weiterleiten(DamageAmount, DamageCauser);
	}
	return DamageAmount;
}

void ATDDecoy::OnHealthDamage(AActor* DamagedActor, float DamageAmount, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (TDSchaden::VonGegner(DamageCauser, InstigatedBy))
	{
		Weiterleiten(DamageAmount, DamageCauser);
	}
	if (UFGHealthComponent* Health = GetHealthComponent())
	{
		Health->ResetHealth();
	}
}
