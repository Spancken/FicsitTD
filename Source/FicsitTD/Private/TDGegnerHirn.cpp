#include "TDGegnerHirn.h"
#include "TDForschung.h"
#include "TDPionier.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "TDLaufProtokoll.h"
#include "TDBalance.h"
#include "FicsitTDModule.h"
#include "TDDamageable.h"
#include "TDDecoy.h"
#include "TDDefenseCore.h"
#include "TDNavFilter.h"
#include "TDWaveSubsystem.h"
#include "TDLeistung.h"
#include "TDDefenseWall.h"
#include "TDBarrier.h"
#include "TDTestWall.h"
#include "AI/FGCreatureController.h"
#include "BrainComponent.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Creature/FGCreature.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "FGHealthComponent.h"
#include "TDSicherheitsnetz.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

float UTDGegnerHirn::TempoFaktor = 1.f;
int32 UTDGegnerHirn::AnimNurSichtbarErzwungen = -1;
int32 UTDGegnerHirn::KollisionErzwungen = -1;
int32 UTDGegnerHirn::RagdollErzwungen = -1;
bool UTDGegnerHirn::bAngriffAus = false;

namespace TDHirn
{
	static const TCHAR* MontageHog = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Animation/HogAttackMontage.HogAttackMontage");
	static const TCHAR* MontageSpitter = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/Animation/SpitterAttackMontage.SpitterAttackMontage");
	static const TCHAR* MontageStinger = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Animation/Attack_02_Montage.Attack_02_Montage");
	static const TCHAR* MontageStingerKlein = TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Animation/Attack_02_MontageSmall.Attack_02_MontageSmall");

	/** Punkt auf einem Weg (Polylinie) nach Strecke cm */
	static FVector PunktAufWeg(const TArray<FVector>& Weg, float Strecke)
	{
		for (int32 i = 1; i < Weg.Num(); ++i)
		{
			const float L = FVector::Dist(Weg[i - 1], Weg[i]);
			if (Strecke <= L)
			{
				return FMath::Lerp(Weg[i - 1], Weg[i], L > 0.f ? Strecke / L : 0.f);
			}
			Strecke -= L;
		}
		return Weg.Last();
	}

	/** Wegstrecke (cm) bis zum Punkt des Weges, der Ort am naechsten liegt; Gesamt = Laenge des Weges */
	static float WegFortschritt(const TArray<FVector>& Weg, const FVector& Ort, float& Gesamt)
	{
		float Fortschritt = 0.f, BesterAbstand = TNumericLimits<float>::Max();
		Gesamt = 0.f;
		for (int32 i = 1; i < Weg.Num(); ++i)
		{
			const FVector Nah = FMath::ClosestPointOnSegment(Ort, Weg[i - 1], Weg[i]);
			const float D = FVector::DistSquared2D(Nah, Ort);
			if (D < BesterAbstand)
			{
				BesterAbstand = D;
				Fortschritt = Gesamt + FVector::Dist(Weg[i - 1], Nah);
			}
			Gesamt += FVector::Dist(Weg[i - 1], Weg[i]);
		}
		return Fortschritt;
	}

	/** Ist der Weg in Richtung frei? Kugel in Koerperbreite 3 m voraus; flache Flaechen (Boden) zaehlen nicht */
	static bool RichtungFrei(UWorld* World, const AActor* Kreatur, const FVector& Richtung, float Radius)
	{
		const FVector Start = Kreatur->GetActorLocation();
		const FVector Ende = Start + Richtung * (300.f + Radius);
		FCollisionQueryParams Query(TEXT("TDHirnAusweichen"), false, Kreatur);
		// Logistik ist kein Hindernis (dieselben Actors, die die Kapsel beim Laufen ignoriert)
		if (const ACharacter* Figur = Cast<ACharacter>(Kreatur))
		{
			if (const UCapsuleComponent* Kapsel = Figur->GetCapsuleComponent())
			{
				Query.AddIgnoredActors(Kapsel->GetMoveIgnoreActors());
			}
		}
		FCollisionObjectQueryParams Objekte;
		Objekte.AddObjectTypesToQuery(ECC_WorldStatic);
		Objekte.AddObjectTypesToQuery(ECC_WorldDynamic);
		// 0.78.3 (Prueflauf 18:25: 94 Festgefahrene mit Antrieb, aber Tempo 0 vor Felsen/Pflanzen mit 30-35 Grad): mit der echten
		// Kapsel tasten (ohne die Stufe, die er hinaufsteigt) und die Neigungsgrenze der Bewegung nehmen statt Kugel in Koerpermitte und 45 Grad
		const ACharacter* Figur = Cast<ACharacter>(Kreatur);
		const UCapsuleComponent* Kapsel = Figur ? Figur->GetCapsuleComponent() : nullptr;
		const UCharacterMovementComponent* Bewegung = Figur ? Figur->GetCharacterMovement() : nullptr;
		const float Stufe = Bewegung ? Bewegung->MaxStepHeight : 45.f;
		const float Halb = Kapsel ? FMath::Max(Kapsel->GetScaledCapsuleHalfHeight() - Stufe * 0.5f, Radius) : Radius;
		const FVector Hoch(0.f, 0.f, Stufe * 0.5f);
		const float Begehbar = Bewegung ? Bewegung->GetWalkableFloorZ() : 0.7f;
		TArray<FHitResult> Treffer;
		World->SweepMultiByObjectType(Treffer, Start + Hoch, Ende + Hoch, FQuat::Identity, Objekte, FCollisionShape::MakeCapsule(Radius, Halb), Query);
		for (const FHitResult& Hit : Treffer)
		{
			if (Hit.bBlockingHit && Hit.ImpactNormal.Z < Begehbar)
			{
				return false;
			}
		}
		// 0.78.2 (Prueflauf 15:31: 56 Abstuerze an einer Klippe 130 m tief): Abgrund voraus zaehlt als versperrt, dann weicht er aus.
		// Die Engine-Sperre bCanWalkOffLedges blockierte schon Gefaelle von 1 m (Prueflauf 16:40: 132 Festgefahrene) - deshalb nur
		// Abbrueche tiefer als Gegner.Abgrund m
		FHitResult Boden;
		const FVector Voraus = Start + Richtung * (200.f + Radius);
		if (!World->LineTraceSingleByChannel(Boden, Voraus + FVector(0.f, 0.f, 200.f), Voraus - FVector(0.f, 0.f, TDBalance::Wert(TEXT("Gegner.Abgrund"), 6.f) * 100.f), ECC_WorldStatic, Query))
		{
			return false;
		}
		return true;
	}
}

FTDGegnerWerte FTDGegnerWerte::Fuer(const UClass* Klasse)
{
	const FString Name = Klasse ? Klasse->GetName() : FString();
	FTDGegnerWerte W;
	// Familie bestimmt Montage und den Abschnitt der Balance-Datei (Gegner.<Familie>.*)
	FString Familie = TEXT("Sonstige");
	if (Name.Contains(TEXT("Johnny")))             { Familie = TEXT("Johnny"); W.Montage = TDHirn::MontageHog; }
	else if (Name.Contains(TEXT("AlphaHog")))      { Familie = TEXT("AlphaHog"); W.Montage = TDHirn::MontageHog; }
	else if (Name.Contains(TEXT("Hog")))           { Familie = Name.Contains(TEXT("Nuclear")) ? TEXT("NuclearHog") : TEXT("Hog"); W.Montage = TDHirn::MontageHog; }
	else if (Name.Contains(TEXT("Spitter")))       { Familie = Name.Contains(TEXT("Alpha")) ? TEXT("SpitterAlpha") : TEXT("Spitter"); W.Montage = TDHirn::MontageSpitter; }   // Fernkampf: bleibt auf Abstand
	else if (Name.Contains(TEXT("Stinger_Child"))) { Familie = TEXT("StingerKlein"); W.Montage = TDHirn::MontageStingerKlein; }
	else if (Name.Contains(TEXT("Stinger")))       { Familie = Name.Contains(TEXT("Elite")) ? TEXT("StingerElite") : TEXT("Stinger"); W.Montage = TDHirn::MontageStinger; }
	const FString Vorne = TEXT("Gegner.") + Familie + TEXT(".");
	W.Schaden = TDBalance::Wert(Vorne + TEXT("Schaden"), W.Schaden);
	W.Takt = TDBalance::Wert(Vorne + TEXT("Takt"), W.Takt);
	W.Tempo = TDBalance::Wert(Vorne + TEXT("Tempo"), W.Tempo / 100.f) * 100.f;
	W.Reichweite = TDBalance::Wert(Vorne + TEXT("Reichweite"), W.Reichweite / 100.f) * 100.f;
	return W;
}

ATDGegnerController::ATDGegnerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ATDGegnerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Stillegen();
}

void ATDGegnerController::Tick(float DeltaSeconds)
{
	// Bewusst ohne AFGCreatureController::Tick (Wahrnehmung, Zustaende, Aktionen) - nur die AIController-Grundlage
	AAIController::Tick(DeltaSeconds);
}

void ATDGegnerController::Stillegen()
{
	if (BrainComponent && BrainComponent->IsRunning())
	{
		BrainComponent->StopLogic(TEXT("FicsitTD Gehirn"));
	}
}

UTDGegnerHirn::UTDGegnerHirn()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UTDGegnerHirn* UTDGegnerHirn::Von(const AActor* Kreatur)
{
	return Kreatur ? Kreatur->FindComponentByClass<UTDGegnerHirn>() : nullptr;
}

TArray<FVector> UTDGegnerHirn::BerechneWeg(UWorld* World, const FVector& Ort, const ATDDefenseCore* Kern, bool* bVoll)
{
	TArray<FVector> Weg;
	if (bVoll)
	{
		*bVoll = false;
	}
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	FNavLocation Start, Ziel;
	// Zielpunkt am Kern: seit 0.61 (Tiefbohrer, 8 x 8 m) liegt der naechste begehbare Boden erst 4,5-5 m von der Mitte -> 12 m suchen
	const bool bStart = Nav && Kern && Nav->ProjectPointToNavigation(Ort, Start, FVector(1500.f, 1500.f, 1000.f));
	const bool bZiel = bStart && Nav->ProjectPointToNavigation(Kern->GetActorLocation(), Ziel, FVector(1200.f, 1200.f, 1000.f));
	if (bStart && !bZiel)
	{
		static double Gemeldet = -100.0;
		if (FPlatformTime::Seconds() - Gemeldet > 30.0)
		{
			Gemeldet = FPlatformTime::Seconds();
			UE_LOG(LogFicsitTD, Warning, TEXT("Hirn: kein Navigationsnetz in 12 m um den Kern - Gegner finden keinen Weg zum Kern"));
		}
	}
	if (bStart && bZiel)
	{
		const UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start.Location, Ziel.Location, nullptr, UTDNavFilter::StaticClass());
		if (Pfad && Pfad->IsValid() && Pfad->PathPoints.Num() >= 2)
		{
			Weg = Pfad->PathPoints;
			if (bVoll)
			{
				*bVoll = UTDWaveSubsystem::PfadAmZiel(Pfad, Ziel.Location);
			}
		}
	}
	return Weg;
}

AFGCreature* UTDGegnerHirn::Spawne(UWorld* World, UClass* Klasse, const FVector& Ort, const TArray<FVector>& Weg, ATDDefenseCore* Kern, float LPFaktor)
{
	if (!World || !Klasse || !Klasse->IsChildOf(AFGCreature::StaticClass()))
	{
		return nullptr;
	}
	const float Blick = Kern ? (Kern->GetActorLocation() - Ort).GetSafeNormal2D().Rotation().Yaw : 0.f;
	const FTransform Transform(FRotator(0.f, Blick, 0.f), Ort);
	AFGCreature* Kreatur = World->SpawnActorDeferred<AFGCreature>(Klasse, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Kreatur)
	{
		return nullptr;
	}
	// Statt des Spiel-Controllers (mit seiner KI) unser stillgelegter Ableger
	Kreatur->AIControllerClass = ATDGegnerController::StaticClass();
	Kreatur->FinishSpawning(Transform);
	UTDWaveSubsystem::KeinLoot(Kreatur);
	if (!Kreatur->GetController())
	{
		Kreatur->SpawnDefaultController();
	}
	Kreatur->SetIsPersistent(true);
	// Tag zuerst: der Hook auf AFGCreature::SetEnabled (FicsitTDModule) verhindert damit das Abschalten durch das Spiel
	Kreatur->Tags.AddUnique(TEXT("FicsitTD"));
	FFicsitTDModule::MerkeWellenKreatur(Kreatur);
	if (!Kreatur->GetIsEnabled())
	{
		Kreatur->SetEnabled(E_Enabled);
	}
	Kreatur->SetMoveSpeed(EMoveSpeed::MS_Run);
	// Einmal beim Spawn: auch ausser Sicht bewegen und animieren (Test 0.16.9). Kein wiederholtes Aufwecken (Test 0.16.10).
	if (UCharacterMovementComponent* Bewegung = Kreatur->GetCharacterMovement())
	{
		Bewegung->bUpdateOnlyIfRendered = false;
		// Test 0.17.1 "nicht sehr natuerlich": Drehung macht die Bewegung selbst (weich, in Laufrichtung) statt hartem SetActorRotation
		Bewegung->bOrientRotationToMovement = true;
		Bewegung->bUseControllerDesiredRotation = false;
		Bewegung->RotationRate = FRotator(0.f, 300.f, 0.f);
		Bewegung->MaxAcceleration = FMath::Max(Bewegung->MaxAcceleration, 2000.f);
		// Stinger laufen im Spiel im Modus NavWalking: ohne Kollision mit der Welt - sie glitten durch Mauern (Test 0.18.3)
		Bewegung->DefaultLandMovementMode = MOVE_Walking;
		if (Bewegung->MovementMode == MOVE_NavWalking)
		{
			Bewegung->SetMovementMode(MOVE_Walking);
		}
	}
	Kreatur->bUseControllerRotationYaw = false;
	if (USkeletalMeshComponent* Koerper = Kreatur->GetMesh())
	{
		// T2 (Stresstest 0.30: Engine-Animation ist der Hauptposten): Pose nur rechnen, wenn sichtbar - Montagen (Angriff) laufen weiter.
		// Bewegung haengt nicht daran (CharacterMovement + Gehirn), das Abschalten durch das Spiel verhindert der SetEnabled-Hook.
		const bool bNurSichtbar = AnimNurSichtbarErzwungen >= 0 ? AnimNurSichtbarErzwungen > 0 : TDBalance::Wert(TEXT("Leistung.AnimNurSichtbar"), 1.f) > 0.5f;
		Koerper->VisibilityBasedAnimTickOption = bNurSichtbar ? EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered
			: EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		// Ferne/kleine Gegner seltener animieren
		Koerper->bEnableUpdateRateOptimizations = TDBalance::Wert(TEXT("Leistung.URO"), 1.f) > 0.5f;
	}
	const bool bKollision = KollisionErzwungen >= 0 ? KollisionErzwungen > 0 : TDBalance::Wert(TEXT("Leistung.GegnerKollision"), 1.f) > 0.5f;
	if (!bKollision)
	{
		if (UCapsuleComponent* Kapsel = Kreatur->GetCapsuleComponent())
		{
			Kapsel->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);   // laufen durcheinander statt sich zu verkeilen
		}
	}
	if (UFGHealthComponent* Leben = Kreatur->GetHealthComponent())
	{
		if (!FMath::IsNearlyEqual(LPFaktor, 1.f))
		{
			Leben->SetMaxHealth(Leben->GetMaxHealth() * LPFaktor);
			Leben->ResetHealth();
		}
	}

	UTDGegnerHirn* Hirn = NewObject<UTDGegnerHirn>(Kreatur, TEXT("TDGegnerHirn"));
	Hirn->mLaufTempo = FTDGegnerWerte::Fuer(Klasse).Tempo;
	if (UCharacterMovementComponent* Bewegung = Kreatur->GetCharacterMovement())
	{
		Bewegung->MaxWalkSpeed = Hirn->mLaufTempo * TempoFaktor;
	}
	Hirn->mKern = Kern;
	Hirn->mWerte = FTDGegnerWerte::Fuer(Klasse);
	// Reichweite ab Koerperrand (Test 0.18.0: Alpha-Hog kam mit 4 m nicht an den Kernkoeder heran)
	if (const UCapsuleComponent* Kapsel = Kreatur->GetCapsuleComponent())
	{
		Hirn->mWerte.Reichweite += Kapsel->GetScaledCapsuleRadius();
	}
	// Gas aus der Umgebung (Gassaeulen, Sporenblumen) sperrt ein Hook in FicsitTDModule - Gas-Nobelisken wirken weiter
	Hirn->mMontage = Hirn->mWerte.Montage ? LoadObject<UAnimMontage>(nullptr, Hirn->mWerte.Montage) : nullptr;
	Hirn->mWeg = Weg;
	if (Hirn->mWeg.Num() >= 2)
	{
		Hirn->mWeg[0] = Kreatur->GetActorLocation();
		Hirn->mFortschritt = TDHirn::WegFortschritt(Hirn->mWeg, Kreatur->GetActorLocation(), Hirn->mGesamt);
		Hirn->mBester = Hirn->mFortschritt;
	}
	// Erste Pruefung des Weges gestaffelt, damit 60 Gegner einer Welle nicht im selben Bild rechnen
	Hirn->mNeuWeg = FMath::FRandRange(0.5f, 3.f);
	Hirn->RegisterComponent();
	return Kreatur;
}

void UTDGegnerHirn::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TDLeistung::FUhr Uhr(TDLeistung::HirnZyklen);
	AFGCreature* Kreatur = Cast<AFGCreature>(GetOwner());
	if (!Kreatur || !Kreatur->IsAliveAndWell())
	{
		// T2: Leichen nicht bis zur naechsten Welle liegen lassen (Test 0.30: 9 Koerper aus Welle 4 lagen noch)
		float Leiche = TDBalance::Wert(TEXT("Leistung.LeicheSekunden"), 3.f);
		if (Kreatur && mPhase != ETDHirnPhase::Tot)
		{
			// T2b: ohne Ragdoll - Physik des Koerpers sofort stoppen, kurz stehen lassen, dann weg (Kampf-Ruckler eingrenzen)
			const bool bRagdoll = RagdollErzwungen >= 0 ? RagdollErzwungen > 0 : TDBalance::Wert(TEXT("Leistung.Ragdoll"), 1.f) > 0.5f;
			if (!bRagdoll)
			{
				if (USkeletalMeshComponent* Koerper = Kreatur->GetMesh())
				{
					Koerper->SetAllBodiesSimulatePhysics(false);
					Koerper->SetSimulatePhysics(false);
					Koerper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
				Leiche = Leiche > 0.f ? FMath::Min(Leiche, 0.5f) : 0.5f;
			}
			if (Leiche > 0.f)
			{
				Kreatur->SetLifeSpan(Leiche);
			}
			++UTDWaveSubsystem::GegnerToteGesamt;
			UTDLaufProtokoll::Beute(Kreatur);
			if (UTDPionier* Pionier = UTDPionier::Get(Kreatur))
			{
				Pionier->Abschuss(Kreatur);
			}
		}
		mPhase = ETDHirnPhase::Tot;
		SetComponentTickEnabled(false);
		return;
	}
	if (mWeg.Num() >= 2)
	{
		mFortschritt = TDHirn::WegFortschritt(mWeg, Kreatur->GetActorLocation(), mGesamt);
	}
	// T4c: Toenungskopie nur zeigen, wenn das Original in der letzten halben Sekunde im Bild gezeichnet wurde
	if (USkeletalMeshComponent* Kopie = mToenung.Get())
	{
		mToenungTakt -= DeltaTime;
		if (mToenungTakt <= 0.f)
		{
			mToenungTakt = 0.25f;
			const USkeletalMeshComponent* Original = Kreatur->GetMesh();
			const bool bSicht = Original && Original->IsVisible() && GetWorld()->GetTimeSeconds() - Original->GetLastRenderTimeOnScreen() < 0.5f;
			if (Kopie->IsVisible() != bSicht)
			{
				Kopie->SetVisibility(bSicht);
			}
		}
	}
	// Einmal je Sekunde: Lauftempo halten, Verhaltensbaum still halten
	mPflege -= DeltaTime;
	if (mPflege <= 0.f)
	{
		mPflege = 1.f;
		UCharacterMovementComponent* Bewegung = Kreatur->GetCharacterMovement();
		const float Soll = mLaufTempo * TempoFaktor * BremsFaktor();
		if (Bewegung && Soll > 0.f && !FMath::IsNearlyEqual(Bewegung->MaxWalkSpeed, Soll, 1.f))
		{
			Bewegung->MaxWalkSpeed = Soll;
		}
		if (Bewegung && Bewegung->MovementMode == MOVE_NavWalking)
		{
			Bewegung->SetMovementMode(MOVE_Walking);
		}
		if (ATDGegnerController* Steuerung = Cast<ATDGegnerController>(Kreatur->GetController()))
		{
			Steuerung->Stillegen();
		}
		// 0.78.1 (Paketlauf 12:41: Hog mit 0 LP lebte weiter und biss den Kern, 16 Tuerme zielten darauf): ohne LP sterben
		if (UFGHealthComponent* Rest = Kreatur->GetHealthComponent(); Rest && Rest->GetCurrentHealth() < 1.f)
		{
			if (mOhneLeben += 1.f; mOhneLeben >= 2.f)
			{
				// 0.78.2: Zustand mitschreiben, um die Ursache zu finden (warum kein Tod bei 0 LP?)
				UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s lebt mit unter 1 LP (genau %.6f von %.0f) seit %.0f s, wird getoetet | tot-Flag %s, betaeubt %s, Bewegung %s, Zustand %s"),
					*Kreatur->GetClass()->GetName(), Rest->GetCurrentHealth(), Rest->GetMaxHealth(), mOhneLeben, Rest->IsDead() ? TEXT("ja") : TEXT("nein"), Kreatur->IsStunned() ? TEXT("ja") : TEXT("nein"),
					Bewegung ? *UEnum::GetValueAsString(Bewegung->MovementMode.GetValue()) : TEXT("?"), *Diagnose());
				TDNetz::Melde(TDNetz::EArt::OhneLeben);
				Rest->Kill();
				return;
			}
		}
		if (Bewegung && Bewegung->MovementMode == MOVE_Walking)
		{
			mLetzterBoden = Kreatur->GetActorLocation();
		}
		// 0.78.1 (Pruefstand 05.11.: drei Niederlagen der Mischbasis durch Gegner, die nach dem Versetzen durch die Welt fielen):
		// im freien Fall und mehr als Gegner.AbsturzGrenze m unter dem Kern -> zurueck auf den Weg; beim dritten Mal sterben.
		// (Paketlauf 12:41: mit 30 m und ohne Fallbedingung wurden Gegner in einem Tal 35-50 m unter dem Kern zurueckgesetzt)
		if (mKern.IsValid() && Bewegung && Bewegung->MovementMode == MOVE_Falling
			&& Kreatur->GetActorLocation().Z < mKern->GetActorLocation().Z - TDBalance::Wert(TEXT("Gegner.AbsturzGrenze"), 150.f) * 100.f)
		{
			++mAbgestuerzt;
			const FVector Vorher = Kreatur->GetActorLocation();
			if (mAbgestuerzt >= 3 || mWeg.Num() < 2)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s zum %d. Mal aus der Welt gefallen (%s, zuletzt Boden bei %s), entfernt"), *Kreatur->GetClass()->GetName(), mAbgestuerzt,
					*Vorher.ToCompactString(), *mLetzterBoden.ToCompactString());
				TDNetz::Melde(TDNetz::EArt::AbsturzEntfernt);
				if (UFGHealthComponent* Leben = Kreatur->GetHealthComponent())
				{
					Leben->Kill();
				}
				return;
			}
			const FVector Zurueck = TDHirn::PunktAufWeg(mWeg, FMath::Max(0.f, mBester - 1000.f)) + FVector(0.f, 0.f, 300.f);
			if (Bewegung)
			{
				Bewegung->StopMovementImmediately();
			}
			Kreatur->TeleportTo(Zurueck, Kreatur->GetActorRotation(), false, true);
			UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s aus der Welt gefallen (%s, zuletzt Boden bei %s), zurueck auf den Weg %s"), *Kreatur->GetClass()->GetName(),
				*Vorher.ToCompactString(), *mLetzterBoden.ToCompactString(), *Zurueck.ToCompactString());
			TDNetz::Melde(TDNetz::EArt::Absturz);
		}
		// Logistik: durch sie hindurchlaufen (Marius 02.11.: Baender duerfen keine Wege lenken); nur bei neuer Liste uebernehmen
		const UTDWaveSubsystem* Wellen = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
		UCapsuleComponent* Kapsel = Kreatur->GetCapsuleComponent();
		if (Wellen && Kapsel && Wellen->GetLogistikVersion() != mLogistikVersion)
		{
			mLogistikVersion = Wellen->GetLogistikVersion();
			Kapsel->ClearMoveIgnoreActors();
			for (const TWeakObjectPtr<AActor>& Teil : Wellen->GetLogistik())
			{
				if (AActor* A = Teil.Get())
				{
					Kapsel->IgnoreActorWhenMoving(A, true);
				}
			}
		}
	}
	// Weg alle 10 s neu (Mauern koennen gebaut oder gefallen sein), nicht waehrend eine Mauer angegriffen wird
	mNeuWeg -= DeltaTime;
	if (mNeuWeg <= 0.f && !mBlockade.IsValid())
	{
		NeuerWeg(Kreatur);
	}
	mZielSuche -= DeltaTime;
	if (mZielSuche <= 0.f)
	{
		mZielSuche = 0.5f;
		mZiel = SucheZiel(Kreatur);
	}
	if (mZiel.IsValid() && mZiel->GetActiveTarget())
	{
		Greife(Kreatur, DeltaTime);
	}
	else
	{
		mZiel.Reset();
		Marschiere(Kreatur, DeltaTime);
	}
}

/** Mauern und Sperren formen den Weg; Tuerme und der Kern sind keine Hindernisse */
static bool IstHindernis(const AActor* Ziel)
{
	return Ziel && (Ziel->IsA<ATDDefenseWall>() || Ziel->IsA<ATDBarrier>() || Ziel->IsA<ATDTestWall>());
}

ATDDecoy* UTDGegnerHirn::KoederNahe(const FVector& Ort, float MaxAbstand, bool bHindernis) const
{
	ATDDecoy* Best = nullptr;
	float BestAbstand = FMath::Square(MaxAbstand);
	for (TActorIterator<ATDDecoy> It(GetWorld()); It; ++It)
	{
		ATDDecoy* Koeder = *It;
		const AActor* Ziel = IsValid(Koeder) ? Koeder->GetActiveTarget() : nullptr;
		if (!Ziel || (bHindernis ? !IstHindernis(Ziel) : Ziel != mKern.Get()))
		{
			continue;
		}
		const float D = FVector::DistSquared2D(Koeder->GetActorLocation(), Ort);
		if (D < BestAbstand)
		{
			BestAbstand = D;
			Best = Koeder;
		}
	}
	return Best;
}

ATDDecoy* UTDGegnerHirn::MauerRichtungKern(const AFGCreature* Kreatur) const
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern)
	{
		return nullptr;
	}
	// Strahl in Huefthoehe vom Gegner zum Kern: das erste Hindernis darauf versperrt den direkten Weg
	const FVector Start = Kreatur->GetActorLocation();
	const FVector Ende = FVector(Kern->GetActorLocation().X, Kern->GetActorLocation().Y, Start.Z);
	FCollisionQueryParams Query(TEXT("TDMauerRichtungKern"), false, Kreatur);
	FCollisionObjectQueryParams Objekte;
	Objekte.AddObjectTypesToQuery(ECC_WorldStatic);
	Objekte.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FHitResult> Treffer;
	GetWorld()->LineTraceMultiByObjectType(Treffer, Start, Ende, Objekte, Query);
	const AActor* Hindernis = nullptr;
	FVector Stelle = FVector::ZeroVector;
	for (const FHitResult& Hit : Treffer)
	{
		if (IstHindernis(Hit.GetActor()))
		{
			Hindernis = Hit.GetActor();
			Stelle = Hit.ImpactPoint;
			break;
		}
	}
	if (!Hindernis)
	{
		return nullptr;
	}
	// Koeder dieser Mauer, der der Trefferstelle am naechsten liegt (also auf unserer Seite)
	ATDDecoy* Best = nullptr;
	float BestAbstand = TNumericLimits<float>::Max();
	for (TActorIterator<ATDDecoy> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->GetActiveTarget() == Hindernis)
		{
			const float D = FVector::DistSquared(It->GetActorLocation(), Stelle) + FVector::DistSquared(It->GetActorLocation(), Start);
			if (D < BestAbstand)
			{
				BestAbstand = D;
				Best = *It;
			}
		}
	}
	return Best;
}

void UTDGegnerHirn::SetzeBlockade(const AFGCreature* Kreatur, ATDDecoy* Mauer, const TCHAR* Grund)
{
	mBlockade = Mauer;
	mStill = 0.f;
	mAnlaufStill = 0.f;
	UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s greift %s an (%s)"), *Kreatur->GetClass()->GetName(),
		Mauer && Mauer->GetActiveTarget() ? *Mauer->GetActiveTarget()->GetName() : TEXT("?"), Grund);
}

void UTDGegnerHirn::NeuerWeg(const AFGCreature* Kreatur)
{
	mNeuWeg = 10.f;
	bool bVoll = false;
	TArray<FVector> Weg = BerechneWeg(GetWorld(), Kreatur->GetActorLocation(), mKern.Get(), &bVoll);
	if (Weg.Num() < 2)
	{
		return;   // kein Netz hier (kurz neben dem Weg?) - alten Weg behalten
	}
	if (bVoll != mWegVoll)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s Weg zum Kern %s"), *Kreatur->GetClass()->GetName(), bVoll ? TEXT("wieder frei") : TEXT("VERSPERRT"));
	}
	Weg[0] = Kreatur->GetActorLocation();
	mWeg = MoveTemp(Weg);
	mWegVoll = bVoll;
	mFortschritt = TDHirn::WegFortschritt(mWeg, Kreatur->GetActorLocation(), mGesamt);
	mBester = mFortschritt;
	mStill = 0.f;
}

void UTDGegnerHirn::Neustart(const AFGCreature* Kreatur, const TArray<FVector>& Ersatz)
{
	mBlockade.Reset();
	mZiel.Reset();
	NeuerWeg(Kreatur);
	if (mWeg.Num() < 2 && Ersatz.Num() >= 2)
	{
		mWeg = Ersatz;
		mWegVoll = true;
		mFortschritt = TDHirn::WegFortschritt(mWeg, Kreatur->GetActorLocation(), mGesamt);
		mBester = mFortschritt;
		mStill = 0.f;
	}
}

ATDDecoy* UTDGegnerHirn::SucheZiel(const AFGCreature* Kreatur)
{
	const FVector Ort = Kreatur->GetActorLocation();
	// Regel 2/3: eine versperrende Mauer bleibt Ziel, bis sie faellt - dann sofort neuer Weg
	if (mBlockade.IsValid() && mBlockade->GetActiveTarget())
	{
		return mBlockade.Get();
	}
	if (mBlockade.IsValid() || !mBlockade.IsExplicitlyNull())
	{
		mBlockade.Reset();
		NeuerWeg(Kreatur);
	}
	// Regel 1: Kern - auf freiem Weg in den letzten 40 m, direkt davor (15 m) oder ohne bekannten Weg
	if (mWeg.Num() < 2 || (mWegVoll && GetRestweg() < 4000.f))
	{
		return KoederNahe(Ort, TNumericLimits<float>::Max(), false);
	}
	if (ATDDecoy* Kern = KoederNahe(Ort, 1500.f, false))
	{
		return Kern;
	}
	// Regel 2: Weg versperrt und an seinem Ende angekommen: die Mauer angreifen, die in Richtung Kern im Weg steht
	// (Test 0.18.1: die naechste Mauer war oft die Wand einer Sackgasse, die nichts freigibt)
	if (!mWegVoll && GetRestweg() < 1500.f)
	{
		ATDDecoy* Mauer = MauerRichtungKern(Kreatur);
		if (!Mauer)
		{
			Mauer = KoederNahe(Ort, 1500.f, true);
		}
		if (Mauer)
		{
			SetzeBlockade(Kreatur, Mauer, TEXT("Weg versperrt"));
			return Mauer;
		}
	}
	// Regel 6: Weg laufen, Mauern und Tuerme ignorieren
	return nullptr;
}

void UTDGegnerHirn::Marschiere(AFGCreature* Kreatur, float DeltaTime)
{
	mPhase = ETDHirnPhase::Marsch;
	if (mWeg.Num() < 2)
	{
		return;
	}
	// Festgefahren (8 s keine 3 m weiter): 10 m weiter auf den Weg setzen
	mStill += DeltaTime;
	if (mFortschritt > mBester + 300.f)
	{
		mBester = mFortschritt;
		mStill = 0.f;
	}
	else if (mStill >= 8.f && GetRestweg() > 4000.f)
	{
		// Regel 3: haengt er an einer Mauer, wird sie angegriffen statt den Gegner zu versetzen
		if (ATDDecoy* Mauer = KoederNahe(Kreatur->GetActorLocation(), 600.f, true))
		{
			SetzeBlockade(Kreatur, Mauer, TEXT("festgefahren"));
			mZiel = Mauer;
			return;
		}
		const FVector Vorher = Kreatur->GetActorLocation();
		FVector Ziel = TDHirn::PunktAufWeg(mWeg, mBester + 1000.f) + FVector(0.f, 0.f, 150.f);
		// 0.78.2 Ursachensuche (Prueflauf 16:10: 226 von 306 Festgefahrenen standen, meist an einer Stelle): was steht vor ihm?
		FString Vorn = TEXT("frei");
		{
			const FVector Richtung = (TDHirn::PunktAufWeg(mWeg, mFortschritt + 600.f) - Vorher).GetSafeNormal2D();
			FCollisionQueryParams Sonde(TEXT("TDFestgefahren"), false, Kreatur);
			FHitResult Hindernis, Boden;
			if (GetWorld()->SweepSingleByChannel(Hindernis, Vorher, Vorher + Richtung * 250.f, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(40.f), Sonde))
			{
				// 0.78.2 (Prueflauf 17:12: 156 von 241 "Festgefahrenen" standen hinter anderen Wellengegnern): Stau in der Kolonne ist
				// kein Festhaengen - nicht versetzen, weiter warten (der Vordermann laeuft weiter oder greift an)
				if (const AFGCreature* Vordermann = Cast<AFGCreature>(Hindernis.GetActor()); Vordermann && Vordermann->Tags.Contains(TEXT("FicsitTD")))
				{
					mStill = 0.f;
					return;
				}
				Vorn = FString::Printf(TEXT("Hindernis %s/%s in %.1f m, Neigung %.0f Grad"), Hindernis.GetActor() ? *Hindernis.GetActor()->GetClass()->GetName() : TEXT("-"),
					Hindernis.GetComponent() ? *Hindernis.GetComponent()->GetName() : TEXT("-"), Hindernis.Distance / 100.f,
					FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hindernis.ImpactNormal.Z, -1.f, 1.f))));
			}
			const FVector Voraus = Vorher + Richtung * 200.f;
			const bool bBodenVorn = GetWorld()->LineTraceSingleByChannel(Boden, Voraus + FVector(0.f, 0.f, 100.f), Voraus - FVector(0.f, 0.f, 400.f), ECC_WorldStatic, Sonde);
			Vorn += bBodenVorn ? FString::Printf(TEXT(", Boden 2 m voraus %+.1f m"), (Boden.ImpactPoint.Z - Vorher.Z) / 100.f) : FString(TEXT(", 2 m voraus KEIN Boden (Kante)"));
		}
		// 0.78.2 (Prueflauf 17:44: 81 Festgefahrene mit Tempo 0, vorn frei, nicht in Geometrie): setzt die Bewegung die Eingabe um?
		FString Antrieb;
		{
			const UCharacterMovementComponent* B = Kreatur->GetCharacterMovement();
			const AController* C = Kreatur->GetController();
			Antrieb = FString::Printf(TEXT("Controller %s, Bewegung tickt %s, ohne Controller %s, Eingabe %.2f, Boden begehbar %s, Beschleunigung %.0f"),
				C ? *C->GetClass()->GetName() : TEXT("KEINER"), B && B->IsComponentTickEnabled() && B->IsActive() ? TEXT("ja") : TEXT("NEIN"),
				B && B->bRunPhysicsWithNoController ? TEXT("ja") : TEXT("nein"), Kreatur->GetLastMovementInputVector().Size(),
				B && B->CurrentFloor.IsWalkableFloor() ? TEXT("ja") : TEXT("NEIN"), B ? B->GetCurrentAcceleration().Size() : -1.f);
		}
		const FString Zustand = Diagnose() + TEXT(" | vorn: ") + Vorn + TEXT(" | Antrieb: ") + Antrieb;
		// 0.78.1 (Pruefstand: versetzte Gegner fielen bis -100 km): nur auf echten Boden versetzen - von 5 m ueber dem Wegpunkt
		// nach unten suchen; ohne Boden in 20 m bleibt er stehen und wird beim naechsten Mal weiter vorn versucht
		FHitResult Boden;
		FCollisionQueryParams Abfrage(TEXT("TDVersetzen"), false, Kreatur);
		const bool bBoden = GetWorld()->LineTraceSingleByChannel(Boden, Ziel + FVector(0.f, 0.f, 500.f), Ziel - FVector(0.f, 0.f, 2000.f), ECC_WorldStatic, Abfrage);
		if (bBoden)
		{
			Ziel = Boden.ImpactPoint + FVector(0.f, 0.f, 150.f);
		}
		else
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s festgefahren, kein Boden am Wegpunkt %s - nicht versetzt"), *Kreatur->GetClass()->GetName(), *Ziel.ToCompactString());
			TDNetz::Melde(TDNetz::EArt::KeinBoden);
		}
		if (bBoden && Kreatur->TeleportTo(Ziel, Kreatur->GetActorRotation()))
		{
			++mVersetzt;
			TDNetz::Melde(TDNetz::EArt::Festgefahren);
			UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s festgefahren bei %s, versetzt | %s"), *Kreatur->GetClass()->GetName(), *Vorher.ToCompactString(), *Zustand);
		}
		mBester += 1000.f;
		mStill = 0.f;
	}
	// 6 m voraus anpeilen: folgt den Ecken des Netz-Weges, schneidet sie aber weich an
	const FVector Wunsch = (TDHirn::PunktAufWeg(mWeg, mFortschritt + 600.f) - Kreatur->GetActorLocation()).GetSafeNormal2D();
	if (!Wunsch.IsNearlyZero())
	{
		Laufe(Kreatur, Wunsch, DeltaTime);
	}
}

void UTDGegnerHirn::Greife(AFGCreature* Kreatur, float DeltaTime)
{
	ATDDecoy* Koeder = mZiel.Get();
	const FVector Zu = Koeder->GetActorLocation() - Kreatur->GetActorLocation();
	// Marius 04.11.2026: Gegner muessen den Kern erreichen. Wer direkt am Kern steht (Mitte + Kern.AngriffsAbstand + eigene Reichweite),
	// greift an, auch wenn der gewaehlte Koeder hinter einer Ecke des 8-m-Sockels liegt
	const bool bAmKern = mKern.IsValid() && Koeder->GetActiveTarget() == mKern.Get()
		&& FVector::Dist2D(Kreatur->GetActorLocation(), mKern->GetActorLocation()) <= TDBalance::Wert(TEXT("Kern.AngriffsAbstand"), 6.f) * 100.f + mWerte.Reichweite;
	// 0.65 (Marius: Stau am Kern, die hinteren kommen nicht heran): wer im Gedraenge um den Kern (Kern.Gedraenge m) einige Sekunden
	// nicht naeher kommt, greift von dort an
	const bool bKernZiel = mKern.IsValid() && Koeder->GetActiveTarget() == mKern.Get();
	const bool bImGedraenge = bKernZiel
		&& FVector::Dist2D(Kreatur->GetActorLocation(), mKern->GetActorLocation()) <= TDBalance::Wert(TEXT("Kern.Gedraenge"), 16.f) * 100.f;
	const bool bGedraenge = bImGedraenge && (mGedraenge
		|| (mPhase == ETDHirnPhase::Anlauf && mAnlaufStill >= TDBalance::Wert(TEXT("Kern.GedraengeSekunden"), 3.f)));
	if (bGedraenge && !mGedraenge)
	{
		mGedraenge = true;   // bleibt, solange er im Gedraenge am Kern steht (sonst wechselt er jedes Bild zwischen Anlauf und Angriff)
		UE_LOG(LogFicsitTD, Log, TEXT("Hirn: %s greift den Kern aus dem Gedraenge an (%.1f m)"), *Kreatur->GetClass()->GetName(),
			FVector::Dist2D(Kreatur->GetActorLocation(), mKern->GetActorLocation()) / 100.f);
	}
	if (!bImGedraenge)
	{
		mGedraenge = false;
	}
	// 0.78.1: nur angreifen, wenn er auf Hoehe des Ziels steht (vorher zaehlte nur der waagerechte Abstand)
	const bool bHoehe = FMath::Abs(Zu.Z) <= TDBalance::Wert(TEXT("Gegner.AngriffHoehe"), 12.f) * 100.f;
	if ((Zu.Size2D() > mWerte.Reichweite && !bAmKern && !bGedraenge) || !bHoehe)
	{
		if (mPhase != ETDHirnPhase::Anlauf)
		{
			mAnlaufBest = Zu.Size2D();
			mAnlaufStill = 0.f;
		}
		mPhase = ETDHirnPhase::Anlauf;
		// Regel 3 auch im Anlauf: 4 s keine Annaeherung und eine Mauer in 6 m -> Mauer angreifen
		if (Zu.Size2D() < mAnlaufBest - 100.f)
		{
			mAnlaufBest = Zu.Size2D();
			mAnlaufStill = 0.f;
		}
		else if ((mAnlaufStill += DeltaTime) >= 4.f && Koeder != mBlockade.Get())
		{
			mAnlaufStill = 0.f;
			if (ATDDecoy* Mauer = KoederNahe(Kreatur->GetActorLocation(), 600.f, true))
			{
				SetzeBlockade(Kreatur, Mauer, TEXT("Anlauf blockiert"));
				mZiel = Mauer;
				return;
			}
		}
		// Zum Kern weiter dem Weg folgen (Labyrinth direkt am Kern), erst die letzten 8 m geradeaus
		FVector Richtung = Zu.GetSafeNormal2D();
		if (Koeder != mBlockade.Get() && mWegVoll && mWeg.Num() >= 2 && GetRestweg() > 800.f)
		{
			const FVector AufWeg = (TDHirn::PunktAufWeg(mWeg, mFortschritt + 600.f) - Kreatur->GetActorLocation()).GetSafeNormal2D();
			Richtung = AufWeg.IsNearlyZero() ? Richtung : AufWeg;
		}
		Laufe(Kreatur, Richtung, DeltaTime);
		return;
	}
	mPhase = ETDHirnPhase::Angriff;
	const FRotator Blick = FMath::RInterpTo(Kreatur->GetActorRotation(), FRotator(0.f, Zu.Rotation().Yaw, 0.f), DeltaTime, 6.f);
	Kreatur->SetActorRotation(Blick);
	if (bAngriffAus)
	{
		return;
	}
	mAngriffTakt -= DeltaTime;
	if (mAngriffTakt <= 0.f)
	{
		mAngriffTakt = mWerte.Takt;
		if (mMontage)
		{
			Kreatur->PlayAnimMontage(mMontage);
		}
		// Zielname vor dem Schaden merken: der Treffer kann die Mauer zerstoeren (Absturz in Test 0.18.1)
		AActor* ZielActor = Koeder->GetActiveTarget();
		if (ITDDamageable* Ziel = Cast<ITDDamageable>(ZielActor))
		{
			mLetztesOpfer = ZielActor->GetClass()->GetName().Replace(TEXT("Build_TD_"), TEXT("")).Replace(TEXT("_C"), TEXT(""));
			mSchaden += mWerte.Schaden;
			if (!UTDWaveSubsystem::bStressSchutz)   // Stresstest: angreifen ja, Schaden nein
			{
				Ziel->ApplyTDDamage(mWerte.Schaden, Kreatur);
				// Forschung Stachelmauern: ein Viertel des Schadens zurueck (Mauern und Sperren)
				if (IstHindernis(ZielActor) && TDForschung::Hat(Kreatur, TEXT("Stachelmauer")))
				{
					UGameplayStatics::ApplyDamage(Kreatur, FMath::Max(TDBalance::Wert(TEXT("Forschung.Stachelmauer.Minimum"), 15.f),
						mWerte.Schaden * TDBalance::Wert(TEXT("Forschung.Stachelmauer.Anteil"), 0.25f)), nullptr, nullptr, UDamageType::StaticClass());
				}
			}
		}
	}
}

void UTDGegnerHirn::Laufe(AFGCreature* Kreatur, const FVector& Wunsch, float DeltaTime)
{
	// 4x je Sekunde voraus tasten und seitlich ausweichen (Felsen fehlen im Navigationsnetz, Test 0.16.7),
	// zuerst zur zuletzt gewaehlten Seite, damit der Gegner nicht pendelt
	mAusweichZeit -= DeltaTime;
	if (mAusweichZeit <= 0.f)
	{
		mAusweichZeit = 0.25f;
		const float Radius = Kreatur->GetCapsuleComponent() ? FMath::Clamp(Kreatur->GetCapsuleComponent()->GetScaledCapsuleRadius(), 40.f, 150.f) : 60.f;
		mAusweich = Wunsch;
		if (!TDHirn::RichtungFrei(GetWorld(), Kreatur, Wunsch, Radius))
		{
			bool bGefunden = false;
			for (const float Winkel : { 30.f, 60.f, 90.f, 120.f })
			{
				for (const float S : { mSeite, -mSeite })
				{
					const FVector Probe = Wunsch.RotateAngleAxis(Winkel * S, FVector::UpVector);
					if (TDHirn::RichtungFrei(GetWorld(), Kreatur, Probe, Radius))
					{
						mAusweich = Probe;
						mSeite = S;
						bGefunden = true;
						break;
					}
				}
				if (bGefunden)
				{
					break;
				}
			}
		}
	}
	const FVector Ziel = mAusweich.IsNearlyZero() ? Wunsch : mAusweich;
	// Richtung weich nachfuehren (etwa 0,2 s), damit Ausweichen und Wegecken keine Zuckungen erzeugen
	mRichtung = mRichtung.IsNearlyZero() ? Ziel : (mRichtung + (Ziel - mRichtung) * FMath::Min(1.f, DeltaTime * 5.f)).GetSafeNormal2D();
	if (AController* Steuerung = Kreatur->GetController())
	{
		if (Steuerung->IsMoveInputIgnored())
		{
			Steuerung->ResetIgnoreMoveInput();
		}
	}
	Kreatur->AddMovementInput(mRichtung.IsNearlyZero() ? Ziel : mRichtung, 1.f);
}

FString UTDGegnerHirn::Diagnose() const
{
	const AFGCreature* Kreatur = Cast<AFGCreature>(GetOwner());
	const UCharacterMovementComponent* Bewegung = Kreatur ? Kreatur->GetCharacterMovement() : nullptr;
	static const TCHAR* Phasen[] = { TEXT("Marsch"), TEXT("Anlauf"), TEXT("Angriff"), TEXT("tot") };
	const float ZielAbstand = mZiel.IsValid() && Kreatur ? FVector::Dist2D(mZiel->GetActorLocation(), Kreatur->GetActorLocation()) / 100.f : -1.f;
	return FString::Printf(TEXT("%s, Weg %s, Rest %.0f m, Tempo %.0f/%.0f, %s, aktiv %s, gesehen vor %.0f s, versetzt %d, Ziel %s in %.0f m, Schaden %.0f an %s"),
		Phasen[static_cast<int32>(mPhase)], mWegVoll ? TEXT("frei") : TEXT("VERSPERRT"), GetRestweg() / 100.f,
		Bewegung ? Bewegung->Velocity.Size2D() : 0.f, Bewegung ? Bewegung->GetMaxSpeed() : 0.f,
		Bewegung ? *UEnum::GetValueAsString(Bewegung->MovementMode.GetValue()) : TEXT("?"),
		Kreatur && Kreatur->GetIsEnabled() ? TEXT("ja") : TEXT("NEIN"),
		Kreatur && GetWorld() ? GetWorld()->GetTimeSeconds() - Kreatur->GetLastRenderTime() : 0.f,
		mVersetzt + 100 * mAbgestuerzt, mZiel.IsValid() && mZiel->GetActiveTarget() ? *mZiel->GetActiveTarget()->GetName() : TEXT("-"), ZielAbstand,
		mSchaden, mLetztesOpfer.IsEmpty() ? TEXT("-") : *mLetztesOpfer);
}

float UTDGegnerHirn::BremsFaktor() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() < mBremseBis ? mBremseFaktor : 1.f;
}

void UTDGegnerHirn::Bremse(AFGCreature* Kreatur, float Faktor, float Dauer)
{
	if (!Kreatur || !GetWorld())
	{
		return;
	}
	const bool bNeu = BremsFaktor() >= 1.f;
	mBremseFaktor = FMath::Clamp(Faktor, 0.1f, 1.f);
	mBremseBis = GetWorld()->GetTimeSeconds() + Dauer;
	if (UCharacterMovementComponent* Bewegung = Kreatur->GetCharacterMovement())
	{
		if (bNeu && mLaufTempo > 0.f)
		{
			Bewegung->MaxWalkSpeed = mLaufTempo * TempoFaktor * mBremseFaktor;
		}
	}
	mPflege = FMath::Min(mPflege, Dauer + 0.05f);   // nach dem Ende spaetestens dann wieder volles Tempo
}
