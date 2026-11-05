#include "TDDefenseWall.h"
#include "TDForschung.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "FicsitTDModule.h"
#include "TDDecoy.h"
#include "FGFactoryColoringTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Halbmasse einer 8x4-Mauer (Ursprung unten mittig, Wand ca. 25 cm dick)
static const FVector MauerHalbmasse(400.f, 25.f, 200.f);
static const TCHAR* CustomSwatchPfad = TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/SwatchDesc_Custom.SwatchDesc_Custom_C");

ATDDefenseWall::ATDDefenseWall(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	mManagedByLightweightBuildableSubsystem = false;
	SetCanBeDamaged(true);
}

void ATDDefenseWall::BeginPlay()
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
	ATDDecoy::SpawnForBox(this, FVector(0.f, 0.f, MauerHalbmasse.Z), MauerHalbmasse, mDecoysPerSide, mDecoys);

	// Die Bau-Pistole setzt die Spielerfarbe direkt nach dem Spawnen - Erkennungsfarbe deshalb kurz danach
	if (!mFarbeGesetzt)
	{
		GetWorldTimerManager().SetTimer(mFarbTimer, this, &ATDDefenseWall::SetzeErkennungsfarbe, 0.5f, false);
	}
}

void ATDDefenseWall::SetzeErkennungsfarbe()
{
	UClass* CustomSwatch = LoadClass<UFGFactoryCustomizationDescriptor_Swatch>(nullptr, CustomSwatchPfad);
	if (!CustomSwatch)
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Verteidigungsmauer: SwatchDesc_Custom nicht gefunden"));
		return;
	}
	FFactoryCustomizationData Data = GetCustomizationData_Native();
	Data.SwatchDesc = CustomSwatch;
	Data.OverrideColorData = FFactoryCustomizationColorSlot(mDefensePrimaryColor, mDefenseSecondaryColor);
	SetCustomizationData_Native(Data);
	mFarbeGesetzt = true;
}

void ATDDefenseWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(mFarbTimer);
	ATDDecoy::DestroyAll(mDecoys);
	Super::EndPlay(EndPlayReason);
}

float ATDDefenseWall::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Direkte Treffer (z. B. Spitter-Projektile, die die Mauer statt eines Koeders treffen)
	if (!TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		return 0.f;   // Giftwolken, Umgebung, eigene Waffen
	}
	ApplyTDDamage(DamageAmount, DamageCauser);
	return DamageAmount;
}

void ATDDefenseWall::ApplyTDDamage(float Amount, const AActor* Causer)
{
	if (!HasAuthority() || !IsTDIntact() || Amount <= 0.f)
	{
		return;
	}
	mHealth = FMath::Max(0.f, mHealth - Amount);
	UE_LOG(LogFicsitTD, Verbose, TEXT("Verteidigungsmauer %s: -%.1f von %s, Rest %.0f/%.0f"), *GetName(), Amount,
		Causer ? *Causer->GetClass()->GetName() : TEXT("unbekannt"), mHealth, mMaxHealth);
	if (mHealth <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Verteidigungsmauer %s zerstoert"), *GetName());
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->GebaeudeZerstoert(GetClass());
		}
		ATDDecoy::DestroyAll(mDecoys);
		TurnOffAndDestroy();
	}
}

float ATDDefenseWall::GetTDMaxHealth() const
{
	return mMaxHealth * (TDForschung::Hat(this, TEXT("MauerMk2")) ? TDBalance::Wert(TEXT("Forschung.MauerMk2.Faktor"), 2.f) : 1.f);
}
