#include "TDEffektProbe.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

// Masse aus den Blender-Entwuerfen (Turm-Fahrplan.md, Abschnitt Blender-Modelle), Unreal-cm
static const FVector ProbeDrehpunkt(0.f, 0.f, 225.f);
static const FVector ProbeKanzel(0.f, 0.f, 150.f);
static const FVector ProbeLaufNabe(125.f, 0.f, 0.f);       // Gewehr: Drehachse des Laufbuendels in der Kanzel
static const FVector ProbeMuendung(318.f, 0.f, 0.f);       // Gewehr: vor den Laufenden
static const FVector ProbeDuese(192.f, 0.f, -3.f);         // Flamme: Duesenspitze
static constexpr float ProbeTakt = 3.f;                    // s Feuern, s Pause
static constexpr float ProbeLaufMax = 1440.f;              // Grad/s bei voller Drehzahl
static constexpr float ProbeFlammeMax = 1500.f;            // cm (15 m Reichweite)

ATDEffektProbe::ATDEffektProbe()
{
	PrimaryActorTick.bCanEverTick = true;
	mWurzel = CreateDefaultSubobject<USceneComponent>(TEXT("Wurzel"));
	RootComponent = mWurzel;
}

UStaticMeshComponent* ATDEffektProbe::Teil(const TCHAR* Name, USceneComponent* Parent, const FString& Modell, const FVector& Ort)
{
	UStaticMeshComponent* T = NewObject<UStaticMeshComponent>(this, Name);
	T->SetupAttachment(Parent);
	T->SetRelativeLocation(Ort);
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Modell));
	T->RegisterComponent();
	return T;
}

void ATDEffektProbe::Einrichten(bool bFlamme)
{
	mFlamme = bFlamme;
	const FString Turm = bFlamme ? TEXT("FlameTurret") : TEXT("GunTurret");
	auto Pfad = [&Turm](const TCHAR* TeilName)
	{
		return FString::Printf(TEXT("/FicsitTD/Buildings/%s/Mesh/SM_TD_%s_%s.SM_TD_%s_%s"), *Turm, *Turm, TeilName, *Turm, TeilName);
	};
	Teil(TEXT("Sockel"), mWurzel, Pfad(TEXT("Base")), FVector::ZeroVector);
	mYaw = Teil(TEXT("Drehteil"), mWurzel, Pfad(TEXT("Yaw")), ProbeDrehpunkt);
	mHead = Teil(TEXT("Kanzel"), mYaw, Pfad(TEXT("Head")), ProbeKanzel);
	if (bFlamme)
	{
		mEffekt = Teil(TEXT("Flammenstrahl"), mHead, TEXT("/FicsitTD/Effects/SM_TD_FlameJet.SM_TD_FlameJet"), ProbeDuese);
	}
	else
	{
		mLaeufe = Teil(TEXT("Laufbuendel"), mHead, Pfad(TEXT("Barrels")), ProbeLaufNabe);
		mEffekt = Teil(TEXT("Muendungsfeuer"), mHead, TEXT("/FicsitTD/Effects/SM_TD_MuzzleFlash.SM_TD_MuzzleFlash"), ProbeMuendung);
	}
	mEffekt->SetCastShadow(false);
	mEffekt->SetVisibility(false);
	mEffektMaterial = mEffekt->CreateDynamicMaterialInstance(0);
}

void ATDEffektProbe::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!mYaw || !mHead || !mEffekt)
	{
		return;
	}
	mZeit += DeltaSeconds;
	const bool bFeuer = FMath::Fmod(mZeit, 2.f * ProbeTakt) < ProbeTakt;

	// Langsames Schwenken, damit man die Bewegung von allen Seiten sieht
	mYaw->SetRelativeRotation(FRotator(0.f, 35.f * FMath::Sin(mZeit * 0.4f), 0.f));
	mHead->SetRelativeRotation(FRotator(6.f + 6.f * FMath::Sin(mZeit * 0.7f), 0.f, 0.f));

	if (!mFlamme)
	{
		// Laufbuendel: zuegig hochdrehen, langsam auslaufen; gefeuert wird erst ab 60 % Drehzahl
		const float Ziel = bFeuer ? ProbeLaufMax : 0.f;
		const float Rampe = bFeuer ? 2.f * ProbeLaufMax : 0.45f * ProbeLaufMax;
		mLaufTempo = FMath::FInterpConstantTo(mLaufTempo, Ziel, DeltaSeconds, Rampe);
		if (mLaeufe)
		{
			mLaeufe->AddLocalRotation(FRotator(0.f, 0.f, mLaufTempo * DeltaSeconds));
		}
		if (bFeuer && mLaufTempo > 0.6f * ProbeLaufMax && mZeit >= mNaechsterBlitz)
		{
			mNaechsterBlitz = mZeit + 0.07f;
			mBlitzBis = mZeit + 0.035f;
			const float G = FMath::FRandRange(0.8f, 1.3f);
			mEffekt->SetRelativeScale3D(FVector(G * FMath::FRandRange(0.9f, 1.4f), G, G));
			mEffekt->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
			if (mEffektMaterial)
			{
				mEffektMaterial->SetScalarParameterValue(TEXT("Staerke"), FMath::FRandRange(18.f, 32.f));
			}
		}
		mEffekt->SetVisibility(mZeit < mBlitzBis);
		return;
	}

	// Flammenstrahl: waechst in ~0,3 s auf volle Laenge, flackert, zieht sich beim Abschalten zurueck
	mFlammenLaenge = FMath::FInterpConstantTo(mFlammenLaenge, bFeuer ? ProbeFlammeMax : 0.f, DeltaSeconds, bFeuer ? 5000.f : 8000.f);
	const bool bSichtbar = mFlammenLaenge > 20.f;
	mEffekt->SetVisibility(bSichtbar);
	if (bSichtbar)
	{
		const float Flackern = FMath::FRandRange(0.92f, 1.06f);
		const float Breite = (0.35f + 0.65f * mFlammenLaenge / ProbeFlammeMax) * FMath::FRandRange(0.93f, 1.08f);
		// Kegel ist 1 m lang und am Ende 1 m breit -> Laenge in m, Breite bis ~3 m am Ende
		mEffekt->SetRelativeScale3D(FVector(mFlammenLaenge / 100.f * Flackern, 3.f * Breite, 3.f * Breite));
		mEffekt->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
		if (mEffektMaterial)
		{
			mEffektMaterial->SetScalarParameterValue(TEXT("Staerke"), FMath::FRandRange(4.5f, 7.5f));
		}
	}
}
