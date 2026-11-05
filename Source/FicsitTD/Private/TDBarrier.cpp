#include "TDBarrier.h"
#include "TDForschung.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "FicsitTDModule.h"
#include "TDDecoy.h"

ATDBarrier::ATDBarrier()
{
	SetCanBeDamaged(true);
}

void ATDBarrier::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (mHealth < 0.f)
	{
		mHealth = mMaxHealth;
	}
	// ein Koeder je Seite, mitten im Traegerkreuz verdeckt
	ATDDecoy::SpawnForBox(this, FVector(0.f, 0.f, 60.f), FVector(100.f, -30.f, 60.f), 1, mDecoys);
}

void ATDBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ATDDecoy::DestroyAll(mDecoys);
	Super::EndPlay(EndPlayReason);
}

void ATDBarrier::GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const
{
	FFGClearanceData Data;
	Data.ClearanceBox = FBox(FVector(-100.f, -100.f, 0.f), FVector(100.f, 100.f, 200.f));
	out_data.Add(Data);
}

FText ATDBarrier::GetLookAtDecription_Implementation(AFGCharacterPlayer* byCharacter, const FUseState& state) const
{
	return FText::FromString(FString::Printf(TEXT("Panzersperre\nLebenspunkte %.0f / %.0f"), mHealth, mMaxHealth));
}

float ATDBarrier::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		return 0.f;   // Giftwolken, Umgebung, eigene Waffen
	}
	ApplyTDDamage(DamageAmount, DamageCauser);
	return DamageAmount;
}

void ATDBarrier::ApplyTDDamage(float Amount, const AActor* Causer)
{
	if (!HasAuthority() || !IsTDIntact() || Amount <= 0.f)
	{
		return;
	}
	mHealth = FMath::Max(0.f, mHealth - Amount);
	if (mHealth <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Panzersperre %s zerstoert"), *GetName());
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->GebaeudeZerstoert(GetClass());
		}
		ATDDecoy::DestroyAll(mDecoys);
		TurnOffAndDestroy();
	}
}

float ATDBarrier::GetTDMaxHealth() const
{
	return mMaxHealth * (TDForschung::Hat(this, TEXT("MauerMk2")) ? TDBalance::Wert(TEXT("Forschung.MauerMk2.Faktor"), 2.f) : 1.f);
}
