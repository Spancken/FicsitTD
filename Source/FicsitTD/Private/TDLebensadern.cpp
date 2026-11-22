// 0.13 K4 Wegpruefung beim Bauen (Fahrplan-0.13, Marius 21.11.: "wenn Fabrikgebaeude gebaut werden und die Wege schliessen
// wuerden, wird geprueft, ob noch ein Weg frei ist - sonst kann man nicht bauen"). Konzept 17.11. O2 (Abriegel-Wächter):
// 16 "Lebensadern" = volle Wege aus 16 Richtungen zum Kern, reihum neu gerechnet (eine je Bild). Ein Hologramm, dessen Kasten
// ALLE gueltigen Lebensadern schneidet, schliesst die letzte Luecke -> rot ("wuerde den letzten Weg zum Kern schliessen").
// Mehrere Luecken verteilen die Lebensadern, dann schneidet ein Bau nicht alle. TD-Mauer, Sperre, Tor und Zaun duerfen schliessen
// (die Gegner greifen sie an), Logistik ist frei. Was trotzdem durchrutscht, faengt die Pruefung vor der Welle (K1, Welle wartet).

#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "TDGegnerHirn.h"
#include "TDDefenseCore.h"
#include "TDDefenseWall.h"
#include "TDBarrier.h"
#include "TDTestWall.h"
#include "TDGate.h"
#include "TDBautenErweitert.h"
#include "FicsitTDModule.h"
#include "Hologram/FGHologram.h"
#include "Buildables/FGBuildable.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace
{
	/** Wuerde ein Bau dieser Klasse den Weg der Gegner sperren und darf er das nicht? (TD-Hindernisse duerfen, Logistik ist frei) */
	bool TDLAIstSperrKlasse(const UClass* K)
	{
		return K && K->IsChildOf(AFGBuildable::StaticClass()) && !UTDEinsatzSubsystem::IstLogistik(K) && !K->IsChildOf(ATDDefenseCore::StaticClass())
			&& !K->IsChildOf(ATDDefenseWall::StaticClass()) && !K->IsChildOf(ATDBarrier::StaticClass()) && !K->IsChildOf(ATDTestWall::StaticClass())
			&& !K->IsChildOf(ATDGate::StaticClass()) && !K->IsChildOf(ATDConductorWall::StaticClass());
	}

	/** 22.11. (Lauf 0948: Bolzenturm gegenueber der Luecke rot - der Reichweitenring des Hologramms zaehlte mit): Kasten aus den
	 *  Teilen des Hologramms ohne Reichweiten- und Radius-Anzeigen; einzelne Teile ueber 30 m nur, wenn sie Instanzen sind (gezogene Waende) */
	FBox TDLAHologrammKasten(const AFGHologram* Holo)
	{
		// Lauf 0958: dazu ein Freiraum-Pruefkasten (ClearanceDetector, 40 m) und ein Teil am Weltursprung -> nur sichtbare Modellteile
		// (StaticMesh/Instanzen) in der Naehe des Hologramms
		TArray<UStaticMeshComponent*> Teile;
		Holo->GetComponents<UStaticMeshComponent>(Teile, true);
		const FVector Ort = Holo->GetActorLocation();
		FBox Kasten(ForceInit);
		for (const UStaticMeshComponent* P : Teile)
		{
			if (!P || !P->IsRegistered() || !P->IsVisible())
			{
				continue;
			}
			const FString N = P->GetName();
			if (N.Contains(TEXT("Range")) || N.Contains(TEXT("Ring")) || N.Contains(TEXT("Reichweite")) || N.Contains(TEXT("Radius")) || N.Contains(TEXT("Clearance")))
			{
				continue;
			}
			const FBox B = P->Bounds.GetBox();
			const bool bInstanzen = P->IsA<UInstancedStaticMeshComponent>();
			// Instanzen-Komponenten ohne Instanzen liegen am Weltursprung (Lauf 1003): auch sie muessen das Hologramm beruehren
			if (!B.IsValid || FVector::Dist2D(B.GetClosestPointTo(Ort), Ort) > 1000.f
				|| (!bInstanzen && (B.GetSize().X > 3000.f || B.GetSize().Y > 3000.f || FVector::Dist2D(B.GetCenter(), Ort) > 4000.f)))
			{
				continue;
			}
			Kasten += B;
		}
		// ohne passendes Teil: ein 4 x 4 m grosses Stueck am Hologramm (lieber zu klein als falscher Alarm - die Welle prueft nach)
		return Kasten.IsValid ? Kasten : FBox::BuildAABB(Ort, FVector(200.f, 200.f, 200.f));
	}
}

void UTDWaveSubsystem::PflegeLebensadern()
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern || TDBalance::Wert(TEXT("KernZugang.Wegpruefung"), 1.f) <= 0.f)
	{
		return;
	}
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav || Nav->IsNavigationBuildInProgress())
	{
		return;   // waehrend des Umbaus die alten Adern behalten
	}
	if (mLebensadern.Num() != 16)
	{
		mLebensadern.SetNum(16);
	}
	const int32 i = mLebensaderIndex;
	mLebensaderIndex = (mLebensaderIndex + 1) % 16;
	const float Abstand = TDBalance::Wert(TEXT("KernZugang.LebensaderAbstand"), 120.f) * 100.f;
	const FVector Start = Kern->GetActorLocation() + FRotator(0.f, 22.5f * i, 0.f).Vector() * Abstand;
	bool bVoll = false;
	TArray<FVector> Weg = UTDGegnerHirn::BerechneWeg(GetWorld(), Start, Kern, &bVoll);
	mLebensadern[i] = bVoll ? MoveTemp(Weg) : TArray<FVector>();
}

int32 UTDWaveSubsystem::GetLebensadernZahl() const
{
	int32 N = 0;
	for (const TArray<FVector>& A : mLebensadern)
	{
		N += A.Num() >= 2 ? 1 : 0;
	}
	return N;
}

bool UTDWaveSubsystem::WuerdeAbriegeln(const FBox& Kasten, int32* OutGeschnitten, int32* OutGueltig) const
{
	const ATDDefenseCore* Kern = mKern.Get();
	int32 Gueltig = 0, Geschnitten = 0;
	if (Kern && Kasten.IsValid)
	{
		const float Rand = TDBalance::Wert(TEXT("KernZugang.WegpruefungRand"), 60.f);
		// nur in der Ebene: Kasten flach, Wege auf Hoehe 0
		const FBox Flach(FVector(Kasten.Min.X - Rand, Kasten.Min.Y - Rand, -10.f), FVector(Kasten.Max.X + Rand, Kasten.Max.Y + Rand, 10.f));
		const FVector2D KernXY(Kern->GetActorLocation());
		// die letzten Meter am Kern zaehlen nicht: dort laufen alle Wege zusammen (Koederring 9,4 m)
		const float Frei = TDBalance::Wert(TEXT("KernZugang.WegpruefungKernFrei"), 14.f) * 100.f;
		for (const TArray<FVector>& A : mLebensadern)
		{
			if (A.Num() < 2)
			{
				continue;
			}
			++Gueltig;
			for (int32 k = 0; k + 1 < A.Num(); ++k)
			{
				const FVector S(A[k].X, A[k].Y, 0.f), E(A[k + 1].X, A[k + 1].Y, 0.f);
				if (FVector2D::Distance(FVector2D(S), KernXY) < Frei && FVector2D::Distance(FVector2D(E), KernXY) < Frei)
				{
					continue;
				}
				if (Flach.IsInside(S) || Flach.IsInside(E) || (!S.Equals(E, 1.f) && FMath::LineBoxIntersection(Flach, S, E, E - S)))
				{
					++Geschnitten;
					break;
				}
			}
		}
	}
	if (OutGeschnitten) { *OutGeschnitten = Geschnitten; }
	if (OutGueltig) { *OutGueltig = Gueltig; }
	// mindestens 3 gueltige Adern, sonst kein Urteil (Netz noch nicht da, Kern am Klippenrand)
	return Gueltig >= 3 && Geschnitten == Gueltig;
}

bool UTDWaveSubsystem::PruefeHologrammZugang(AFGHologram* Holo)
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Holo || !Kern || mLauf == ETDLauf::Aus || TDBalance::Wert(TEXT("KernZugang.Wegpruefung"), 1.f) <= 0.f || !TDLAIstSperrKlasse(Holo->GetBuildClass()))
	{
		return false;
	}
	const float Pruefradius = (TDBalance::Wert(TEXT("KernZugang.LebensaderAbstand"), 120.f) + 20.f) * 100.f;
	if (FVector::Dist2D(Holo->GetActorLocation(), Kern->GetActorLocation()) > Pruefradius)
	{
		return false;
	}
	int32 Geschnitten = 0, Gueltig = 0;
	const FBox Kasten = TDLAHologrammKasten(Holo);
	const bool bRot = WuerdeAbriegeln(Kasten, &Geschnitten, &Gueltig);
	// Diagnose (22.11., Lauf 0953): Lage und Groesse des Kastens sowie die drei groessten Teile
	if (TDBalance::Wert(TEXT("KernZugang.HologrammLog"), 1.f) > 0.f)
	{
		static double Zuletzt = -100.0;
		if (FPlatformTime::Seconds() - Zuletzt > 2.0)
		{
			Zuletzt = FPlatformTime::Seconds();
			TArray<UPrimitiveComponent*> Teile;
			Holo->GetComponents<UPrimitiveComponent>(Teile, true);
			Teile.Sort([](const UPrimitiveComponent& A, const UPrimitiveComponent& B) { return A.Bounds.BoxExtent.Size2D() > B.Bounds.BoxExtent.Size2D(); });
			TArray<FString> Gross;
			for (int32 k = 0; k < FMath::Min(3, Teile.Num()); ++k)
			{
				Gross.Add(FString::Printf(TEXT("%s %s %.0fx%.0f m"), *Teile[k]->GetName(), *Teile[k]->GetClass()->GetName(),
					Teile[k]->Bounds.BoxExtent.X / 50.f, Teile[k]->Bounds.BoxExtent.Y / 50.f));
			}
			UE_LOG(LogFicsitTD, Log, TEXT("KernZugang: Hologramm %s bei %.0f m vom Kern, Kasten %.1f x %.1f m um %s, Adern %d/%d, groesste Teile: %s"),
				*GetNameSafe(Holo->GetBuildClass()), FVector::Dist2D(Holo->GetActorLocation(), Kern->GetActorLocation()) / 100.f,
				Kasten.GetSize().X / 100.f, Kasten.GetSize().Y / 100.f, *Kasten.GetCenter().ToCompactString(), Geschnitten, Gueltig, *FString::Join(Gross, TEXT("; ")));
		}
	}
	if (bRot)
	{
		Holo->AddConstructDisqualifier(UTDCDKernAbriegelung::StaticClass());
		static double Gemeldet = -100.0;
		if (FPlatformTime::Seconds() - Gemeldet > 2.0)
		{
			Gemeldet = FPlatformTime::Seconds();
			UE_LOG(LogFicsitTD, Log, TEXT("KernZugang: Hologramm %s rot - wuerde den letzten Weg schliessen (Lebensadern %d/%d geschnitten)"),
				*GetNameSafe(Holo->GetBuildClass()), Geschnitten, Gueltig);
		}
	}
	return bRot;
}
