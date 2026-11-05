#include "TDTestWall.h"
#include "FicsitTDModule.h"
#include "TDDecoy.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

static const FVector WandHalbmasse(400.f, 25.f, 200.f);

ATDTestWall::ATDTestWall()
{
	PrimaryActorTick.bCanEverTick = false;

	mCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	mCollision->SetBoxExtent(WandHalbmasse);
	mCollision->SetCollisionProfileName(TEXT("BlockAll"));
	mCollision->SetCanEverAffectNavigation(true);
	mCollision->SetHiddenInGame(true);
	RootComponent = mCollision;

	mVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	mVisual->SetupAttachment(mCollision);
	mVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mVisual->SetCanEverAffectNavigation(false);

	SetCanBeDamaged(true);
}

/** Sichtbares Modell: eine Spielwand (8x4) aus der Asset Registry, sonst der Engine-Wuerfel. */
void ATDTestWall::SetupVisual()
{
	UStaticMesh* Mesh = nullptr;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(TEXT("/Game/FactoryGame/Buildable/Building/Wall")), Assets, true);
	const FTopLevelAssetPath MeshClass = UStaticMesh::StaticClass()->GetClassPathName();
	FAssetData Fallback;
	for (const FAssetData& Asset : Assets)
	{
		if (Asset.AssetClassPath != MeshClass)
		{
			continue;
		}
		if (Asset.AssetName.ToString().Contains(TEXT("8x4")))
		{
			Mesh = Cast<UStaticMesh>(Asset.GetAsset());
			if (Mesh)
			{
				break;
			}
		}
		if (!Fallback.IsValid())
		{
			Fallback = Asset;
		}
	}
	if (!Mesh && Fallback.IsValid())
	{
		Mesh = Cast<UStaticMesh>(Fallback.GetAsset());
	}
	if (!Mesh)
	{
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	}
	if (!Mesh)
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Testmauer: kein Modell gefunden, Mauer ist unsichtbar"));
		return;
	}

	// Modell so skalieren und verschieben, dass es genau die Kollisionsbox fuellt
	mVisual->SetStaticMesh(Mesh);
	const FBox Bounds = Mesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize().ComponentMax(FVector(1.f));
	const FVector Scale = (WandHalbmasse * 2.f) / Size;
	mVisual->SetRelativeScale3D(Scale);
	mVisual->SetRelativeLocation(-Bounds.GetCenter() * Scale);
	UE_LOG(LogFicsitTD, Log, TEXT("Testmauer: Modell %s"), *Mesh->GetPathName());
}

void ATDTestWall::Init(float InMaxHealth)
{
	mMaxHealth = FMath::Max(1.f, InMaxHealth);
	mHealth = mMaxHealth;
	SetupVisual();

	ATDDecoy::SpawnForBox(this, FVector::ZeroVector, WandHalbmasse, 3, mDecoys);
	UE_LOG(LogFicsitTD, Log, TEXT("Testmauer %s: %.0f LP, %d Koeder"), *GetName(), mMaxHealth, mDecoys.Num());
}

void ATDTestWall::ApplyTDDamage(float Amount, const AActor* Causer)
{
	if (!IsTDIntact() || Amount <= 0.f)
	{
		return;
	}
	mHealth = FMath::Max(0.f, mHealth - Amount);
	const FString Quelle = Causer ? Causer->GetClass()->GetName() : TEXT("unbekannt");
	TrefferJeQuelle.FindOrAdd(Quelle)++;
	UE_LOG(LogFicsitTD, Log, TEXT("Testmauer %s: -%.1f von %s, Rest %.0f/%.0f"), *GetName(), Amount, *Quelle, mHealth, mMaxHealth);

	if (mHealth <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Testmauer %s zerstoert"), *GetName());
		Destroy();
	}
}

float ATDTestWall::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Direkte Treffer (z. B. Projektile, die die Mauer statt eines Koeders treffen)
	if (!TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		return 0.f;   // Giftwolken, Umgebung, eigene Waffen
	}
	ApplyTDDamage(DamageAmount, DamageCauser);
	return DamageAmount;
}

void ATDTestWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ATDDecoy::DestroyAll(mDecoys);
	Super::EndPlay(EndPlayReason);
}
