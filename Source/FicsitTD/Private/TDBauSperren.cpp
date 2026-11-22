// 0.13 N1 (Fahrplan-0.13 Abschnitt 7, Marius 22.11.): Maschinen und Tuerme als Sperrflaeche im Navigationsnetz der Gegner.
// Messung 22.11. (Plan KZ-0-Messung): Waende, Fundamente und TD-Mauern sparen das Netz aus, Maschinen und Tuerme nicht - unter ihnen
// liegt Netz, Gegner planen hindurch und bleiben haengen, und ein Ring aus Maschinen ist fuer die Pruefung "Kern erreichbar" unsichtbar.
// Jeder solche Bau im Umkreis Gegner.BauSperrenRadius um den Kern bekommt einen Kasten wie die Klippensperre: eigener Actor, keine
// Kollision (keine unsichtbare Wand fuer den Spieler), dynamisches Hindernis mit UNavArea_Null, gedreht wie der Bau, Masse aus den
// lokalen Grenzen. Nicht gespeichert - beim Laden, Laufstart und vor jeder Pruefung neu gesetzt; verschwindet mit dem Bau.

#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "TDDefenseCore.h"
#include "TDDefenseWall.h"
#include "TDBarrier.h"
#include "TDTestWall.h"
#include "TDGate.h"
#include "TDBautenErweitert.h"
#include "FicsitTDModule.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactoryBuilding.h"
#include "FGBuildableSubsystem.h"
#include "Components/BoxComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	/** 22.11. (Lauf 0831: mit allen Komponenten zaehlte der Reichweitenring der Tuerme mit - riesige Kaesten sperrten den Kern ein):
	 *  Grenzen nur aus Komponenten mit Kollision; ohne solche (Modell noch nicht fertig) alle Komponenten, aber hoechstens 12 m je Seite */
	FBox TDBSGrenzen(const AActor* Bau)
	{
		FBox Lokal = Bau->CalculateComponentsBoundingBoxInLocalSpace(false);
		if (!Lokal.IsValid || Lokal.GetSize().X < 100.f || Lokal.GetSize().Y < 100.f)
		{
			Lokal = Bau->CalculateComponentsBoundingBoxInLocalSpace(true);
			if (Lokal.IsValid)
			{
				const FVector Mitte = Lokal.GetCenter();
				const FVector Halb = Lokal.GetExtent().ComponentMin(FVector(600.f, 600.f, 2000.f));
				Lokal = FBox(Mitte - Halb, Mitte + Halb);
			}
		}
		return Lokal;
	}

	/** TD-Hindernisse haben ihre eigene Netzwirkung (Mauer im Netz, Tor mit eigener Sperre) und werden angegriffen */
	bool TDBSIstTDHindernis(const AActor* A)
	{
		return A->IsA<ATDDefenseWall>() || A->IsA<ATDBarrier>() || A->IsA<ATDTestWall>() || A->IsA<ATDGate>() || A->IsA<ATDConductorWall>();
	}
}

bool UTDWaveSubsystem::BrauchtBauSperre(const AFGBuildable* Bau) const
{
	if (!IsValid(Bau) || Bau->GetIsLightweightTemporary() || Bau->IsA<ATDDefenseCore>() || Bau->IsA<AFGBuildableFactoryBuilding>()
		|| TDBSIstTDHindernis(Bau) || UTDEinsatzSubsystem::IstLogistik(Bau->GetClass()))
	{
		return false;
	}
	// Kleinteile (Lampen, Schilder) nicht: Grundflaeche mindestens 1 x 1 m und 1 m hoch
	const FBox Lokal = TDBSGrenzen(Bau);
	const FVector Groesse = Lokal.IsValid ? Lokal.GetSize() : FVector::ZeroVector;
	return Groesse.X >= 100.f && Groesse.Y >= 100.f && Groesse.Z >= 100.f;
}

bool UTDWaveSubsystem::SperreBau(AFGBuildable* Bau)
{
	if (mBauSperren.Contains(Bau) || !BrauchtBauSperre(Bau))
	{
		return false;
	}
	UWorld* World = GetWorld();
	// 22.11. (Lauf 0800: unter allen 57 Bolzentuermen weiter Netz): alle Komponenten, auch ohne Kollision, und bis 1,5 m unter den Fuss
	FBox Lokal = TDBSGrenzen(Bau);
	Lokal.Min.Z = FMath::Min(Lokal.Min.Z, -150.f);
	// je Bauart einmal die Kastengroesse ins Log (Pruefung der Masse)
	static TSet<FName> Gemeldet;
	if (!Gemeldet.Contains(Bau->GetClass()->GetFName()))
	{
		Gemeldet.Add(Bau->GetClass()->GetFName());
		UE_LOG(LogFicsitTD, Log, TEXT("BauSperren: Kasten fuer %s %.1f x %.1f x %.1f m"), *Bau->GetClass()->GetName(),
			Lokal.GetSize().X / 100.f, Lokal.GetSize().Y / 100.f, Lokal.GetSize().Z / 100.f);
	}
	FActorSpawnParameters P;
	P.Owner = Bau;
	const FTransform Lage = Bau->GetActorTransform();
	AActor* Kasten = World->SpawnActor<AActor>(AActor::StaticClass(), Lage, P);
	UBoxComponent* Box = Kasten ? NewObject<UBoxComponent>(Kasten, TEXT("TDBauSperre")) : nullptr;
	if (!Box)
	{
		return false;
	}
	// wie die Klippensperre: keine Kollision, dynamisches Hindernis, Flaeche "nicht begehbar"; Lage vor dem Anmelden setzen
	Box->SetBoxExtent(Lokal.GetExtent());
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->bDynamicObstacle = true;
	Box->SetAreaClassOverride(UNavArea_Null::StaticClass());
	Box->SetCanEverAffectNavigation(true);
	Kasten->SetRootComponent(Box);
	Box->SetWorldTransform(FTransform(Lage.GetRotation(), Lage.TransformPosition(Lokal.GetCenter()), FVector::OneVector));
	Box->RegisterComponent();
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		Nav->UpdateComponentInNavOctree(*Box);
	}
	Kasten->Tags.Add(TEXT("TDBauSperre"));
	mBauSperren.Add(Bau, Kasten);
	return true;
}

int32 UTDWaveSubsystem::SperreBautenUm(const FVector& Mitte)
{
	if (TDBalance::Wert(TEXT("Gegner.BauSperren"), 1.f) <= 0.f)
	{
		return 0;
	}
	RaeumeBauSperren(false);
	const float Radius = TDBalance::Wert(TEXT("Gegner.BauSperrenRadius"), 250.f) * 100.f;
	int32 Neu = 0;
	for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
	{
		if (FVector::DistSquared2D(It->GetActorLocation(), Mitte) <= Radius * Radius && SperreBau(*It))
		{
			++Neu;
		}
	}
	if (Neu > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("BauSperren: %d neu im Netz (%d gesamt, Umkreis %.0f m)"), Neu, mBauSperren.Num(), Radius / 100.f);
	}
	return Neu;
}

void UTDWaveSubsystem::RaeumeBauSperren(bool bAlle)
{
	int32 Weg = 0;
	for (auto It = mBauSperren.CreateIterator(); It; ++It)
	{
		if (bAlle || !It->Key.IsValid())
		{
			if (AActor* Kasten = It->Value.Get())
			{
				Kasten->Destroy();
			}
			It.RemoveCurrent();
			++Weg;
		}
	}
	if (Weg > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("BauSperren: %d entfernt (%s), %d bleiben"), Weg, bAlle ? TEXT("alle") : TEXT("Bau abgerissen"), mBauSperren.Num());
	}
}

void UTDWaveSubsystem::BauGebaut(AFGBuildable* Bau)
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern || mLauf == ETDLauf::Aus || !Bau || TDBalance::Wert(TEXT("Gegner.BauSperren"), 1.f) <= 0.f)
	{
		return;
	}
	// 22.11.: beim Ereignis sind Modelle und Instanzen evtl. noch nicht fertig - die Sperre setzt der naechste LaufTick nach 1 s
	if (BrauchtBauSperre(Bau))
	{
		mBauSperrenUhr = 1.f;
	}
}

void UTDWaveSubsystem::VerbindeBauSperren()
{
	if (mBauVerbunden)
	{
		return;
	}
	if (AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(GetWorld()))
	{
		Bau->BuildableConstructedGlobalDelegate.AddUniqueDynamic(this, &UTDWaveSubsystem::BauGebaut);
		mBauVerbunden = true;
	}
}
