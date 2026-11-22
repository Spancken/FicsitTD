// 0.13 (Marius 22.11.): Ring "instabiler Boden" sichtbar - kleine leuchtende Signale im Baumodus - und die letzte Luecke markieren,
// wenn ein Hologramm den letzten Weg schliessen wuerde (K4). Lampenkopf SM_TD_StatusLight (24 cm, Slot 1 = Leuchtkoerper mit
// M_TD_StatusLight, Parameter Farbe/Leuchtkraft) wie die Statuslampe der Tuerme (Style Guide: Leuchtteile Orange oder Gelb).
// Ring: FICSIT-Orange, nur sichtbar, solange die Bau-Pistole im Baumodus ist (oder Einsatz.Ring.SignaleImmer fuer Tests).
// Luecke: Gruen ("hier frei lassen"), entlang der letzten Lebensader am roten Hologramm, verschwindet 0,6 s nach dem letzten Rot.

#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "TDDefenseCore.h"
#include "FicsitTDModule.h"
#include "FGCharacterPlayer.h"
#include "Equipment/FGBuildGun.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* TDRSLampe = TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_StatusLight.SM_TD_StatusLight");

	/** Lampenkopf als Komponente am Signal-Actor: ohne Kollision, ohne Netzwirkung, ohne Schatten, leuchtend in der Farbe */
	UStaticMeshComponent* TDRSNeueLampe(AActor* Besitzer, UStaticMesh* Modell, const FLinearColor& Farbe, float Groesse)
	{
		UStaticMeshComponent* L = NewObject<UStaticMeshComponent>(Besitzer);
		L->SetStaticMesh(Modell);
		L->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		L->SetCanEverAffectNavigation(false);
		L->SetCastShadow(false);
		L->SetWorldScale3D(FVector(Groesse));
		L->SetupAttachment(Besitzer->GetRootComponent());
		L->RegisterComponent();
		if (UMaterialInstanceDynamic* Mat = L->GetNumMaterials() > 1 ? L->CreateDynamicMaterialInstance(1) : nullptr)
		{
			Mat->SetVectorParameterValue(TEXT("Farbe"), Farbe);
			Mat->SetScalarParameterValue(TEXT("Leuchtkraft"), TDBalance::Wert(TEXT("Einsatz.Ring.Leuchtkraft"), 80.f));
		}
		return L;
	}

	AActor* TDRSNeuerTraeger(UWorld* World, const FVector& Ort, const TCHAR* Name)
	{
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Ort));
		if (A)
		{
			USceneComponent* Wurzel = NewObject<USceneComponent>(A, TEXT("Wurzel"));
			A->SetRootComponent(Wurzel);
			Wurzel->RegisterComponent();
			A->SetActorLocation(Ort);
			A->Tags.Add(Name);
		}
		return A;
	}

	/** Bodenhoehe unter einem Punkt (Gelaende und Bauten), sonst der Punkt selbst */
	FVector TDRSBoden(UWorld* World, const FVector& P)
	{
		FHitResult Treffer;
		FCollisionQueryParams Q(TEXT("TDRingSignale"), false);
		if (World->LineTraceSingleByObjectType(Treffer, P + FVector(0.f, 0.f, 3000.f), P - FVector(0.f, 0.f, 3000.f), FCollisionObjectQueryParams(ECC_WorldStatic), Q))
		{
			return Treffer.ImpactPoint;
		}
		return P;
	}
}

bool UTDWaveSubsystem::RingSignaleSichtbar() const
{
	const AActor* Ring = mRingSignale.Get();
	return Ring && !Ring->IsHidden();
}

int32 UTDWaveSubsystem::RingSignaleZahl() const
{
	const AActor* Ring = mRingSignale.Get();
	return Ring ? Ring->GetComponents().Num() - 1 : 0;
}

void UTDWaveSubsystem::PflegeRingSignale(float DeltaTime)
{
	UWorld* World = GetWorld();
	const ATDDefenseCore* Kern = mKern.Get();
	// Luecke ausblenden, wenn kein rotes Hologramm mehr gemeldet wird
	if (AActor* Luecke = mLueckeSignale.Get(); Luecke && !Luecke->IsHidden() && FPlatformTime::Seconds() > mLueckeBis)
	{
		Luecke->SetActorHiddenInGame(true);
	}
	mRingSignalUhr -= DeltaTime;
	if (mRingSignalUhr > 0.f)
	{
		return;
	}
	mRingSignalUhr = 0.25f;
	const UTDEinsatzSubsystem* Einsatz = World->GetSubsystem<UTDEinsatzSubsystem>();
	const bool bRing = Kern && Einsatz && Einsatz->IstAktiv() && TDBalance::Wert(TEXT("Einsatz.Ring.Aktiv"), 1.f) > 0.f
		&& TDBalance::Wert(TEXT("Einsatz.Ring.Signale"), 1.f) > 0.f;
	if (!bRing)
	{
		if (AActor* Ring = mRingSignale.Get())
		{
			Ring->SetActorHiddenInGame(true);
		}
		return;
	}
	// anlegen (einmal je Kern; Lage aus Bodenstrahlen, auch auf der Plattform)
	if (!mRingSignale.IsValid() || mRingSignaleKern.Get() != Kern)
	{
		if (AActor* Alt = mRingSignale.Get())
		{
			Alt->Destroy();
		}
		UStaticMesh* Modell = LoadObject<UStaticMesh>(nullptr, TDRSLampe);
		AActor* Ring = Modell ? TDRSNeuerTraeger(World, Kern->GetActorLocation(), TEXT("TDRingSignale")) : nullptr;
		if (!Ring)
		{
			return;
		}
		const float Radius = TDBalance::Wert(TEXT("Einsatz.Ring.Radius"), 40.f) * 100.f;
		const int32 Anzahl = FMath::Clamp(FMath::RoundToInt(2.f * PI * Radius / (TDBalance::Wert(TEXT("Einsatz.Ring.SignalAbstand"), 6.f) * 100.f)), 12, 96);
		const FLinearColor Orange = FLinearColor(FColor(250, 149, 73));
		for (int32 i = 0; i < Anzahl; ++i)
		{
			const FVector P = TDRSBoden(World, Kern->GetActorLocation() + FRotator(0.f, 360.f * i / Anzahl, 0.f).Vector() * Radius);
			UStaticMeshComponent* L = TDRSNeueLampe(Ring, Modell, Orange, TDBalance::Wert(TEXT("Einsatz.Ring.SignalGroesse"), 4.f));
			L->SetWorldLocation(P + FVector(0.f, 0.f, 15.f));
		}
		Ring->SetActorHiddenInGame(true);
		mRingSignale = Ring;
		mRingSignaleKern = Kern;
		UE_LOG(LogFicsitTD, Log, TEXT("Ringsignale: %d Lampen im Abstand %.0f m um den Kern"), Anzahl, Radius / 100.f);
	}
	// sichtbar im Baumodus der Bau-Pistole (oder fuer Tests immer)
	bool bBaumodus = TDBalance::Wert(TEXT("Einsatz.Ring.SignaleImmer"), 0.f) > 0.f;
	if (!bBaumodus)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		const AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		AFGBuildGun* Pistole = Spieler ? Spieler->GetBuildGun() : nullptr;
		bBaumodus = Pistole && Pistole->IsEquipped() && Pistole->IsInState(EBuildGunState::BGS_BUILD);
	}
	AActor* Ring = mRingSignale.Get();
	if (Ring && Ring->IsHidden() == bBaumodus)
	{
		Ring->SetActorHiddenInGame(!bBaumodus);
		UE_LOG(LogFicsitTD, Verbose, TEXT("Ringsignale %s"), bBaumodus ? TEXT("sichtbar") : TEXT("ausgeblendet"));
	}
}

void UTDWaveSubsystem::ZeigeLetzteLuecke(const FBox& Kasten, const TArray<FVector>& Ader)
{
	UWorld* World = GetWorld();
	if (Ader.Num() < 2 || TDBalance::Wert(TEXT("KernZugang.LueckeMarkieren"), 1.f) <= 0.f)
	{
		return;
	}
	const int32 Max = 16;
	AActor* Luecke = mLueckeSignale.Get();
	if (!Luecke)
	{
		UStaticMesh* Modell = LoadObject<UStaticMesh>(nullptr, TDRSLampe);
		Luecke = Modell ? TDRSNeuerTraeger(World, Kasten.GetCenter(), TEXT("TDLueckeSignale")) : nullptr;
		if (!Luecke)
		{
			return;
		}
		const FLinearColor Gruen = FLinearColor(FColor(150, 215, 125));
		for (int32 i = 0; i < Max; ++i)
		{
			TDRSNeueLampe(Luecke, Modell, Gruen, TDBalance::Wert(TEXT("Einsatz.Ring.SignalGroesse"), 4.f));
		}
		mLueckeSignale = Luecke;
	}
	// Punkte alle 3 m entlang der Ader, nur innerhalb von 25 m um das Hologramm: zeigt, wo der Weg durch die Luecke fuehrt
	TArray<FVector> Punkte;
	const FVector Mitte = Kasten.GetCenter();
	for (int32 k = 0; k + 1 < Ader.Num() && Punkte.Num() < Max; ++k)
	{
		const float Laenge = FVector::Dist(Ader[k], Ader[k + 1]);
		for (float s = 0.f; s < Laenge && Punkte.Num() < Max; s += 300.f)
		{
			const FVector P = FMath::Lerp(Ader[k], Ader[k + 1], s / FMath::Max(Laenge, 1.f));
			if (FVector::Dist2D(P, Mitte) <= 2500.f)
			{
				Punkte.Add(P);
			}
		}
	}
	TArray<UStaticMeshComponent*> Lampen;
	Luecke->GetComponents<UStaticMeshComponent>(Lampen);
	for (int32 i = 0; i < Lampen.Num(); ++i)
	{
		const bool bAn = Punkte.IsValidIndex(i);
		Lampen[i]->SetVisibility(bAn);
		if (bAn)
		{
			Lampen[i]->SetWorldLocation(Punkte[i] + FVector(0.f, 0.f, 15.f));
		}
	}
	Luecke->SetActorHiddenInGame(false);
	mLueckeBis = FPlatformTime::Seconds() + 0.6;
	static double Gemeldet = -100.0;
	if (FPlatformTime::Seconds() - Gemeldet > 5.0)
	{
		Gemeldet = FPlatformTime::Seconds();
		UE_LOG(LogFicsitTD, Log, TEXT("KernZugang: letzte Luecke markiert, %d Signale entlang des letzten Wegs"), Punkte.Num());
	}
}
