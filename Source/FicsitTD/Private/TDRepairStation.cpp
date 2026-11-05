#include "TDRepairStation.h"
#include "TDBalance.h"
#include "TDDamageable.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"

static constexpr float ReparaturRadius = 2500.f;

ATDRepairStation::ATDRepairStation()
{
	mTurmName = TEXT("Reparaturstation");
	mDreht = false;
	mDrehpunkt = FVector(0.f, 0.f, 225.f);
	mMuendung = FVector(155.f, 0.f, 260.f);   // Spitze des Schweisskopfs am Kranarm (im Drehteil-Raum, Blender 1,55/0/2,6)
	mRange = ReparaturRadius;
	mKlarraumHoehe = 700.f;
	mYawModell = TEXT("/FicsitTD/Buildings/RepairStation/Mesh/SM_TD_RepairStation_Yaw.SM_TD_RepairStation_Yaw");
	mStandardRezept = TEXT("/FicsitTD/Buildings/RepairStation/Recipe_TD_Repair_IronPlate.Recipe_TD_Repair_IronPlate_C");

	mStrahl = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Reparaturstrahl"));
	mStrahl->SetupAttachment(RootComponent);
	mStrahl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mStrahl->SetCastShadow(false);
	mStrahl->SetVisibility(false);
}

void ATDRepairStation::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mStrahl, TEXT("/FicsitTD/Effects/SM_TD_Lightning.SM_TD_Lightning"));
	// Gruener Reparaturstrahl statt des blauen Tesla-Blitzes (Marius 02.11.)
	if (UMaterialInterface* Gruen = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_RepairBeam.M_TD_RepairBeam")))
	{
		for (int32 i = 0; i < mStrahl->GetNumMaterials(); ++i)
		{
			mStrahl->SetMaterial(i, Gruen);
		}
	}
	mStrahl->SetVisibility(false);
}

float ATDRepairStation::ReparaturFuer(const UClass* Material)
{
	// Lebenspunkte je Zyklus aus der Balance-Datei (Munition.Reparatur.*)
	const FString Name = Material ? Material->GetName() : FString();
	if (Name.Contains(TEXT("Reinforced")))  return TDBalance::Wert(TEXT("Munition.Reparatur.VerstaerktePlatte"), 3000.f);
	if (Name.Contains(TEXT("SteelPlate")))  return TDBalance::Wert(TEXT("Munition.Reparatur.Stahltraeger"), 2000.f);   // Stahltraeger
	return TDBalance::Wert(TEXT("Munition.Reparatur.Eisenplatte"), 800.f);                                                // Eisenplatten (2 je Zyklus)
}

TArray<AActor*> ATDRepairStation::SucheBeschaedigte() const
{
	struct FEintrag { AActor* A; float Anteil; };
	TArray<FEintrag> Liste;
	const FVector Mitte = GetActorLocation();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const ITDDamageable* D = Cast<ITDDamageable>(*It);
		if (!D || !D->IsTDIntact() || D->GetTDHealth() >= D->GetTDMaxHealth() - 1.f)
		{
			continue;
		}
		if (FVector::Dist(It->GetActorLocation(), Mitte) < GetReichweite())
		{
			Liste.Add({ *It, D->GetTDHealth() / FMath::Max(D->GetTDMaxHealth(), 1.f) });
		}
	}
	Liste.Sort([](const FEintrag& X, const FEintrag& Y) { return X.Anteil < Y.Anteil; });
	TArray<AActor*> Ergebnis;
	for (const FEintrag& E : Liste)
	{
		Ergebnis.Add(E.A);
	}
	return Ergebnis;
}

void ATDRepairStation::Feuern(AFGCreature* Ziel)
{
	// Reparatur-Menge auf die schwaechsten Gebaeude verteilen
	float Rest = ReparaturFuer(AktuelleMunition()) * SchadenBonus();
	for (AActor* A : SucheBeschaedigte())
	{
		if (Rest <= 0.f)
		{
			break;
		}
		if (ITDDamageable* D = Cast<ITDDamageable>(A))
		{
			const float Geheilt = D->HealTD(Rest);
			Rest -= Geheilt;
			mGeheilt += Geheilt;
			if (Geheilt > 0.f && !mStrahlZiel.IsValid())
			{
				mStrahlZiel = A;
			}
		}
	}
	++mSchuesse;
	mStrahlRest = 0.6f;
}

void ATDRepairStation::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	// Kranarm dreht sich langsam, solange es Arbeit gibt
	mSuchTakt -= DeltaSeconds;
	if (mSuchTakt <= 0.f)
	{
		mSuchTakt = 1.f;
		mGibtArbeit = SucheBeschaedigte().Num() > 0;
	}
	if (mYawMesh && mGibtArbeit)
	{
		mYawMesh->AddLocalRotation(FRotator(0.f, 20.f * DeltaSeconds, 0.f));
	}
	mStrahlRest -= DeltaSeconds;
	AActor* StrahlZiel = mStrahlZiel.Get();
	if (mStrahlRest > 0.f && StrahlZiel)
	{
		const FVector Von = GetMuzzleLocation();
		const FVector Nach = StrahlZiel->GetActorLocation() + FVector(0.f, 0.f, 150.f);
		mStrahl->SetWorldLocationAndRotation(Von, (Nach - Von).Rotation());
		mStrahl->SetWorldScale3D(FVector(FVector::Dist(Von, Nach) / 100.f, 0.5f, 0.5f));
		FRotator R = mStrahl->GetComponentRotation();
		R.Roll = FMath::FRandRange(0.f, 360.f);
		mStrahl->SetWorldRotation(R);
		mStrahl->SetVisibility(true);
	}
	else
	{
		mStrahl->SetVisibility(false);
		mStrahlZiel.Reset();
	}
}

FString ATDRepairStation::ZusatzInfo() const
{
	return FString::Printf(TEXT("%s, insgesamt %.0f LP repariert"), mGibtArbeit ? TEXT("repariert") : TEXT("nichts zu reparieren"), mGeheilt);
}

void ATDRepairStation::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	const float Takt = Zykluszeit();
	Out.Add({ TEXT("Reparatur je Zyklus"), FString::Printf(TEXT("%.0f LP"), ReparaturFuer(AktuelleMunition()) * SchadenBonus()), true });
	if (Takt > 0.f)
	{
		Out.Add({ TEXT("Takt"), FString::Printf(TEXT("%.1f s"), Takt), true });
	}
	Out.Add({ TEXT("Umkreis"), FString::Printf(TEXT("%.0f m"), GetReichweite() / 100.f), true });
}
