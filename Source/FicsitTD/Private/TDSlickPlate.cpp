#include "TDSlickPlate.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDGegnerHirn.h"
#include "Creature/FGCreature.h"
#include "FGPipeConnectionFactory.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// Masse aus Modelle\td_oelschlick.py: Platte 8 x 8 m, Deck 0,3 m, Rohr auf +X (Flansch bei 5,15 m), Handrad bei (4,75 / -0,6 / 0,62)
static constexpr float SchlickHalb = 400.f;
static const FVector SchlickRohr(515.f, 0.f, 32.f);
static const FVector SchlickRad(475.f, -60.f, 62.f);

ATDSlickPlate::ATDSlickPlate()
{
	mTurmName = TEXT("Oelschlick");
	mSchadensart = ETDSchadensart::Feuer;
	mDreht = false;
	mRange = 650.f;                     // Tuerme.Oelschlick.Reichweite: Film nur, wenn Gegner nah an der Platte sind
	mDrehpunkt = SchlickRad;            // Handrad (Pivot TD_Valve) dreht sich, solange Oel fliesst
	mMuendung = FVector::ZeroVector;
	mHalbmasse = FVector(SchlickHalb, SchlickHalb, 15.f);
	mLampenOrt = FVector(370.f, 370.f, 55.f);
	mKlarraumHoehe = 60.f;
	mKlarraumHalb = FVector2D(SchlickHalb, SchlickHalb);
	mBaseMaxHealth = 3000.f;
	mYawModell = TEXT("/FicsitTD/Buildings/SlickPlate/Mesh/SM_TD_SlickPlate_Valve.SM_TD_SlickPlate_Valve");
	mStandardRezept = TEXT("/FicsitTD/Buildings/SlickPlate/Recipe_TD_Slick_HeavyOil.Recipe_TD_Slick_HeavyOil_C");

	// Rohr-Eingang aussen am Flansch, Blickrichtung +X (wie beim Flammenwerfer von der Maschine eingesammelt oder von Hand eingehaengt)
	mRohrEingang = CreateDefaultSubobject<UFGPipeConnectionFactory>(TEXT("PipeInput"));
	mRohrEingang->SetupAttachment(RootComponent);
	mRohrEingang->SetRelativeLocation(SchlickRohr);
	mRohrEingang->SetRelativeRotation(FRotator::ZeroRotator);
	mRohrEingang->SetPipeConnectionType(EPipeConnectionType::PCT_CONSUMER);
}

void ATDSlickPlate::NachMaschinenStart()
{
	if (mRohrEingang && !mPipeInputConnections.Contains(mRohrEingang))
	{
		mPipeInputConnections.Add(mRohrEingang);
		mSelbstPumpen = true;
	}
}

void ATDSlickPlate::BeginPlay()
{
	Super::BeginPlay();
	if (mSelbstPumpen && GetCurrentRecipe())
	{
		AssignInputAccessIndices(GetCurrentRecipe());
	}
}

void ATDSlickPlate::Factory_Tick(float dt)
{
	Super::Factory_Tick(dt);
	if (mSelbstPumpen)
	{
		Factory_PullPipeInput_Implementation(dt);
	}
}

double ATDSlickPlate::Jetzt() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

bool ATDSlickPlate::IstNass() const
{
	return IsTDIntact() && (IsProducing() || Jetzt() < mNassBis);
}

bool ATDSlickPlate::AufPlatte(const FVector& Ort) const
{
	const FVector Lokal = GetActorTransform().InverseTransformPosition(Ort);
	return FMath::Abs(Lokal.X) < SchlickHalb + 30.f && FMath::Abs(Lokal.Y) < SchlickHalb + 30.f && Lokal.Z > -150.f && Lokal.Z < 350.f;
}

void ATDSlickPlate::Entzuende(UWorld* Welt, const FVector& Ort)
{
	if (!Welt)
	{
		return;
	}
	for (TActorIterator<ATDSlickPlate> It(Welt); It; ++It)
	{
		if (It->IstNass() && It->AufPlatte(Ort))
		{
			if (It->Jetzt() >= It->mBrandBis)
			{
				++It->mBraende;
				UE_LOG(LogFicsitTD, Log, TEXT("Oelschlick %s: entzuendet"), *It->GetName());
			}
			It->mBrandBis = It->Jetzt() + TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandDauer"), 5.f);
		}
	}
}

void ATDSlickPlate::Feuern(AFGCreature* Ziel)
{
	// Ein Zyklus Oel verteilt: der Film haelt noch ein paar Sekunden nach
	mNassBis = Jetzt() + TDBalance::Wert(TEXT("Tuerme.Oelschlick.Nachlauf"), 3.f);
	++mSchuesse;
}

void ATDSlickPlate::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	const bool bNass = IstNass();
	if (mYawMesh && IsProducing())
	{
		mYawMesh->AddLocalRotation(FRotator(0.f, 90.f * DeltaSeconds, 0.f));
	}
	mWirkTakt -= DeltaSeconds;
	if (mWirkTakt > 0.f || !bNass)
	{
		return;
	}
	mWirkTakt = 0.5f;
	const bool bBrennt = Jetzt() < mBrandBis;
	const float Bremse = TDBalance::Wert(TEXT("Tuerme.Oelschlick.Bremse"), 0.65f);
	const float Brand = TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandSchaden"), 20.f) * 0.5f;   // je Takt von 0,5 s
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	for (AFGCreature* F : Feinde)
	{
		if (!F || !F->IsAliveAndWell() || !AufPlatte(F->GetActorLocation()))
		{
			continue;
		}
		if (UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(F))
		{
			Hirn->Bremse(F, Bremse, 0.8f);
			++mGebremst;
		}
		if (bBrennt)
		{
			Treffer(F, Brand, FVector::UpVector, 0.f);
		}
	}
	if (bBrennt)
	{
		mFunkenTakt -= 0.5f;
		if (mFunkenTakt <= 0.f)
		{
			mFunkenTakt = 1.f;
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector Lokal(FMath::FRandRange(-330.f, 330.f), FMath::FRandRange(-330.f, 330.f), 30.f);
				ZeigeLeuchten(GetActorTransform().TransformPosition(Lokal), FLinearColor(1.f, 0.3f, 0.03f), 4.f, 1.1f, 0.f, true, 20.f);
			}
		}
	}
}

FString ATDSlickPlate::MunitionsText() const
{
	return FString::Printf(TEXT("%.1f m³ %s"), GetAmmo() / 1000.f, *GetMunitionsName());
}

FString ATDSlickPlate::ZusatzInfo() const
{
	if (Jetzt() < mBrandBis)
	{
		return TEXT("brennt");
	}
	return IstNass() ? TEXT("Ölfilm liegt, Gegner werden gebremst") : TEXT("trocken, wartet auf Gegner");
}

void ATDSlickPlate::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TEXT("Bremse"), FString::Printf(TEXT("%.0f %%"), (1.f - TDBalance::Wert(TEXT("Tuerme.Oelschlick.Bremse"), 0.65f)) * 100.f), true });
	Out.Add({ TEXT("Brand"), FString::Printf(TEXT("%.0f Schaden/s für %.0f s"), TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandSchaden"), 20.f),
		TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandDauer"), 5.f)), true });
	Out.Add({ TEXT("Entzündet"), FString::Printf(TEXT("%d-mal"), mBraende), true });
}
