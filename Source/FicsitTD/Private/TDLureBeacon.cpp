#include "TDLureBeacon.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDDecoy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TDSprache.h"

TArray<TWeakObjectPtr<ATDLureBeacon>> ATDLureBeacon::sAlle;

// Lage der beweglichen Teile aus Modelle\td_lockfeuer.py (m -> cm)
static constexpr float LockKlappenRadius = 74.f;
static constexpr float LockKlappenZ = 320.f;
static const FVector LockLeuchteOrt(-55.f, 55.f, 445.f);

ATDLureBeacon::ATDLureBeacon()
{
	mTurmName = TEXT("Lockfeuer");
	mDreht = false;
	mRange = 4000.f;                              // Tuerme.Lockfeuer.Reichweite: lockt nur, wenn Gegner in 40 m sind
	mDrehpunkt = FVector(40.f, 0.f, 215.f);       // Lautsprecher am Mast (Pivot TD_Horn)
	mMuendung = FVector(0.f, 0.f, 0.f);
	mHalbmasse = FVector(100.f, 100.f, 45.f);
	mLampenOrt = FVector(80.f, -101.f, 60.f);
	mKlarraumHoehe = 450.f;
	mKlarraumHalb = FVector2D(105.f, 105.f);
	mBaseMaxHealth = 2000.f;
	mYawModell = TEXT("/FicsitTD/Buildings/LureBeacon/Mesh/SM_TD_LureBeacon_Horn.SM_TD_LureBeacon_Horn");
	mStandardRezept = TEXT("/FicsitTD/Buildings/LureBeacon/Recipe_TD_Lure_Coal.Recipe_TD_Lure_Coal_C");

	auto Teil = [this](const TCHAR* Name, const FVector& Ort, float Gier)
	{
		UStaticMeshComponent* T = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		T->SetupAttachment(RootComponent);
		T->SetRelativeLocation(Ort);
		T->SetRelativeRotation(FRotator(0.f, Gier, 0.f));
		T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		T->SetCanEverAffectNavigation(false);
		return T;
	};
	mLeuchte = Teil(TEXT("LockLeuchte"), LockLeuchteOrt, 0.f);
	TObjectPtr<UStaticMeshComponent>* Klappen[] = { &mKlappe0, &mKlappe1, &mKlappe2, &mKlappe3 };
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = 90.f * i;
		*Klappen[i] = Teil(*FString::Printf(TEXT("LockKlappe%d"), i),
			FVector(LockKlappenRadius * FMath::Cos(FMath::DegreesToRadians(A)), LockKlappenRadius * FMath::Sin(FMath::DegreesToRadians(A)), LockKlappenZ), A);
	}
}

void ATDLureBeacon::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mLeuchte, TEXT("/FicsitTD/Buildings/LureBeacon/Mesh/SM_TD_LureBeacon_Beacon.SM_TD_LureBeacon_Beacon"));
	UStaticMeshComponent* Klappen[] = { mKlappe0, mKlappe1, mKlappe2, mKlappe3 };
	for (int32 i = 0; i < 4; ++i)
	{
		SetzeModell(Klappen[i], FString::Printf(TEXT("/FicsitTD/Buildings/LureBeacon/Mesh/SM_TD_LureBeacon_Shutter%d.SM_TD_LureBeacon_Shutter%d"), i, i));
	}
	sAlle.RemoveAll([](const TWeakObjectPtr<ATDLureBeacon>& L) { return !L.IsValid(); });
	sAlle.AddUnique(this);
}

void ATDLureBeacon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	sAlle.Remove(this);
	Super::EndPlay(EndPlayReason);
}

double ATDLureBeacon::Jetzt() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

float ATDLureBeacon::LockRadius() const
{
	return TDBalance::Wert(TEXT("Tuerme.Lockfeuer.Radius"), 40.f) * 100.f;
}

bool ATDLureBeacon::IstAktiv() const
{
	return IsTDIntact() && IsProducing();
}

ATDDecoy* ATDLureBeacon::LockKoeder(const UWorld* Welt, const FVector& Ort)
{
	ATDDecoy* Best = nullptr;
	float BestAbstand = TNumericLimits<float>::Max();
	for (const TWeakObjectPtr<ATDLureBeacon>& L : sAlle)
	{
		const ATDLureBeacon* Feuer = L.Get();
		if (!Feuer || Feuer->GetWorld() != Welt || !Feuer->IstAktiv())
		{
			continue;
		}
		const float R = Feuer->LockRadius();
		if (FVector::DistSquared2D(Feuer->GetActorLocation(), Ort) > R * R)
		{
			continue;
		}
		for (ATDDecoy* K : Feuer->GetKoederListe())
		{
			if (!IsValid(K))
			{
				continue;
			}
			const float D = FVector::DistSquared2D(K->GetActorLocation(), Ort);
			if (D < BestAbstand)
			{
				BestAbstand = D;
				Best = K;
			}
		}
	}
	return Best;
}

bool ATDLureBeacon::BereitZumProduzieren(AFGCreature* Ziel) const
{
	// lockt nur, wenn Gegner in Reichweite sind und die Pause vorbei ist
	return Ziel != nullptr && Jetzt() >= mPauseBis;
}

void ATDLureBeacon::Feuern(AFGCreature* Ziel)
{
	// Ein Zyklus (15 s Lockruf) ist vorbei: Pause, damit die Gegner wieder ihren Weg laufen
	mPauseBis = Jetzt() + TDBalance::Wert(TEXT("Tuerme.Lockfeuer.Pause"), 30.f);
	++mLockrufe;
	++mSchuesse;
	UE_LOG(LogFicsitTD, Log, TEXT("Lockfeuer %s: Lockruf %d beendet, Pause %.0f s"), *GetName(), mLockrufe, TDBalance::Wert(TEXT("Tuerme.Lockfeuer.Pause"), 30.f));
}

void ATDLureBeacon::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	const bool bAktiv = IstAktiv();
	// Klappen 75 Grad nach aussen, solange es lockt (Pivot an der Unterkante, lokale +X zeigt nach aussen)
	mKlappenWinkel = FMath::FInterpConstantTo(mKlappenWinkel, bAktiv ? 75.f : 0.f, DeltaSeconds, 90.f);
	UStaticMeshComponent* Klappen[] = { mKlappe0, mKlappe1, mKlappe2, mKlappe3 };
	for (int32 i = 0; i < 4; ++i)
	{
		if (Klappen[i])
		{
			Klappen[i]->SetRelativeRotation(FRotator(-mKlappenWinkel, 90.f * i, 0.f));
		}
	}
	if (bAktiv)
	{
		mHornZeit += DeltaSeconds;
		if (mYawMesh)
		{
			mYawMesh->SetRelativeRotation(FRotator(0.f, 40.f * FMath::Sin(mHornZeit * 1.3f), 0.f));
		}
		mLeuchteWinkel = FMath::Fmod(mLeuchteWinkel + 270.f * DeltaSeconds, 360.f);
		if (mLeuchte)
		{
			mLeuchte->SetRelativeRotation(FRotator(0.f, mLeuchteWinkel, 0.f));
		}
		// Feuer in der Schale: pulsierendes Leuchten (das Modell hat nur die Glut)
		mGlutTakt -= DeltaSeconds;
		if (mGlutTakt <= 0.f)
		{
			mGlutTakt = 0.9f;
			ZeigeLeuchten(GetActorLocation() + FVector(0.f, 0.f, 380.f), FLinearColor(1.f, 0.45f, 0.08f), 2.5f, 1.f, 0.f, true, 30.f);
		}
	}
}

FString ATDLureBeacon::ZusatzInfo() const
{
	if (IstAktiv())
	{
		return TDT(TEXT("lockt Gegner an"), TEXT("luring enemies"));
	}
	const double Rest = mPauseBis - Jetzt();
	return Rest > 0.0 ? TDF(TEXT("Pause, noch %.0f s"), TEXT("Paused, %.0f s left"), Rest) : TDT(TEXT("bereit, wartet auf Gegner in Reichweite"), TEXT("ready, waiting for enemies in range"));
}

void ATDLureBeacon::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Lockradius"), TEXT("Lure radius")), FString::Printf(TEXT("%.0f m"), LockRadius() / 100.f), true });
	Out.Add({ TDT(TEXT("Lockruf"), TEXT("Lure call")), TDF(TEXT("%.0f s je Zyklus"), TEXT("%.0f s per cycle"), Zykluszeit()), true });
	Out.Add({ TDT(TEXT("Pause"), TEXT("Pause")), FString::Printf(TEXT("%.0f s"), TDBalance::Wert(TEXT("Tuerme.Lockfeuer.Pause"), 30.f)), true });
	Out.Add({ TDT(TEXT("Lockrufe"), TEXT("Lure calls")), FString::Printf(TEXT("%d"), mLockrufe), true });
}
