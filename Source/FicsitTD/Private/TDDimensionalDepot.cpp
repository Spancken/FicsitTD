#include "TDDimensionalDepot.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDTurretBase.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// Kugel aus Modelle\td_dimensionslager.py (KUGEL_Z = 3 m)
static const FVector DepotKugelOrt(0.f, 0.f, 300.f);

ATDDimensionalDepot::ATDDimensionalDepot()
{
	mKugel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DepotKugel"));
	mKugel->SetupAttachment(RootComponent);
	mKugel->SetRelativeLocation(DepotKugelOrt);
	mKugel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mKugel->SetCanEverAffectNavigation(false);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 1.f / 30.f;
}

void ATDDimensionalDepot::BeginPlay()
{
	Super::BeginPlay();
	if (mKugel && !mKugel->GetStaticMesh())
	{
		mKugel->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,
			TEXT("/FicsitTD/Buildings/DimensionalDepot/Mesh/SM_TD_DimensionalDepot_Sphere.SM_TD_DimensionalDepot_Sphere")));
	}
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(mTimer, this, &ATDDimensionalDepot::Verteile, FMath::Max(0.2f, TDBalance::Wert(TEXT("Bauten.Dimensionslager.Takt"), 1.f)), true);
	}
}

void ATDDimensionalDepot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(mTimer);
	Super::EndPlay(EndPlayReason);
}

void ATDDimensionalDepot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (mKugel)
	{
		// dreht schneller, solange es Tuerme versorgt
		mKugel->AddLocalRotation(FRotator(0.f, (mLetzteTuerme > 0 ? 60.f : 12.f) * DeltaSeconds, 0.f));
	}
}

void ATDDimensionalDepot::Verteile()
{
	UFGInventoryComponent* Lager = GetStorageInventory();
	if (!Lager || Lager->IsEmpty())
	{
		mLetzteTuerme = 0;
		return;
	}
	const float Radius = TDBalance::Wert(TEXT("Bauten.Dimensionslager.Radius"), 30.f) * 100.f;
	const int32 JeTurm = FMath::Max(1, TDBalance::Ganz(TEXT("Bauten.Dimensionslager.JeTurm"), 10));
	int32 Tuerme = 0;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		ATDTurretBase* Turm = *It;
		if (!IsValid(Turm) || !Turm->IsTDIntact() || FVector::DistSquared2D(Turm->GetActorLocation(), GetActorLocation()) > Radius * Radius)
		{
			continue;
		}
		UFGInventoryComponent* Eingang = Turm->GetInputInventory();
		const TSubclassOf<UFGRecipe> Rezept = Turm->GetCurrentRecipe();
		if (!Eingang || !Rezept)
		{
			continue;
		}
		bool bVersorgt = false;
		for (const FItemAmount& Zutat : UFGRecipe::GetIngredients(this, Rezept))
		{
			if (!Zutat.ItemClass || UFGItemDescriptor::GetForm(Zutat.ItemClass) != EResourceForm::RF_SOLID)
			{
				continue;   // Fluessigkeiten kommen weiter ueber das Rohr
			}
			int32 Rest = JeTurm;
			for (int32 i = 0; i < Lager->GetSizeLinear() && Rest > 0; ++i)
			{
				FInventoryStack Stapel;
				if (!Lager->GetStackFromIndex(i, Stapel) || Stapel.Item.GetItemClass() != Zutat.ItemClass)
				{
					continue;
				}
				const int32 Menge = FMath::Min(Rest, Stapel.NumItems);
				const int32 Rein = Eingang->AddStack(FInventoryStack(Menge, Zutat.ItemClass), true);
				if (Rein <= 0)
				{
					break;   // Eingang voll
				}
				Lager->RemoveFromIndex(i, Rein);
				Rest -= Rein;
				mVerteilt += Rein;
				bVersorgt = true;
			}
		}
		Tuerme += bVersorgt ? 1 : 0;
	}
	if (Tuerme != mLetzteTuerme)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Dimensionslager %s: versorgt %d Tuerme (bisher %d Stueck)"), *GetName(), Tuerme, mVerteilt);
	}
	mLetzteTuerme = Tuerme;
}
