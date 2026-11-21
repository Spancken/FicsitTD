#include "TDSpikeS0.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDDefenseWall.h"
#include "TDNavFilter.h"
#include "AbstractInstanceManager.h"
#include "InstanceData.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "FGPlayerController.h"
#include "FGRecipe.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWall.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

// Zustand des letzten Abrisses: prozessweit, damit "/td s0 nach" auch nach Speichern und Laden dieselbe Stelle prueft
namespace
{
	struct FTDS0Merker
	{
		bool bGesetzt = false;
		FString Klasse;
		FTransform Lage;
		FVector StrahlStart = FVector::ZeroVector;
		FVector StrahlEnde = FVector::ZeroVector;
		FVector WegA = FVector::ZeroVector;
		FVector WegB = FVector::ZeroVector;
	};
	FTDS0Merker GS0Merker;

	/** Ein Lightweight-Bauteil aus der Tabelle des Subsystems */
	struct FTDS0Teil
	{
		TSubclassOf<AFGBuildable> Klasse;
		int32 Index = INDEX_NONE;
		FTransform Lage;
		FBox Kasten = FBox(ForceInit);
		bool bBlaupause = false;
		bool IstGueltig() const { return Klasse != nullptr && Index != INDEX_NONE; }
	};
}

static FString TDS0Ort(const FVector& V)
{
	return FString::Printf(TEXT("%.0f/%.0f/%.0f"), V.X, V.Y, V.Z);
}

static ATDDefenseCore* TDS0Kern(UWorld* World)
{
	for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

/** Mittelpunkt fuer Suchen: Kern, sonst Spieler */
static FVector TDS0Mitte(UWorld* World, AFGPlayerController* Player)
{
	if (const ATDDefenseCore* Kern = TDS0Kern(World))
	{
		return Kern->GetActorLocation();
	}
	return Player && Player->GetPawn() ? Player->GetPawn()->GetActorLocation() : FVector::ZeroVector;
}

/** Treffer beschreiben und, wenn moeglich, in Instanz und Lightweight-Bauteil aufloesen */
static FString TDS0Beschreibe(UWorld* World, const FHitResult& Hit, FInstanceHandle* OutHandle, bool* bOutInstanz, FLightweightBuildableInstanceRef* OutRef, bool* bOutLeicht)
{
	const AActor* Actor = Hit.GetActor();
	const UPrimitiveComponent* Komponente = Hit.GetComponent();
	const UStaticMeshComponent* Netz = Cast<UStaticMeshComponent>(Komponente);
	FString Text = FString::Printf(TEXT("%s / %s / Modell %s"), Actor ? *Actor->GetClass()->GetName() : TEXT("kein Actor"),
		Komponente ? *Komponente->GetClass()->GetName() : TEXT("keine Komponente"), Netz && Netz->GetStaticMesh() ? *Netz->GetStaticMesh()->GetName() : TEXT("-"));
	FInstanceHandle Handle;
	AAbstractInstanceManager* Manager = AAbstractInstanceManager::GetInstanceManager(World);
	const bool bInstanz = Manager && Manager->ResolveHit(Hit, Handle);
	FLightweightBuildableInstanceRef Ref;
	const bool bLeicht = bInstanz && AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Handle, Ref);
	if (bInstanz)
	{
		const AActor* Besitzer = Handle.GetOwner();
		Text += FString::Printf(TEXT(" | Instanz ja, Besitzer %s, Kollisionstopf %s"), Besitzer ? *Besitzer->GetClass()->GetName() : TEXT("keiner"),
			Handle.GetCollisionInstanceComponent() ? *Handle.GetCollisionInstanceComponent()->GetName() : TEXT("keiner"));
	}
	else
	{
		Text += TEXT(" | Instanz nein");
	}
	if (bLeicht)
	{
		const FRuntimeBuildableInstanceData* Daten = Ref.ResolveBuildableInstanceData();
		Text += FString::Printf(TEXT(" | Lightweight %s bei %s, Rezept %s, Blaupause %s"), Ref.GetBuildableClass() ? *Ref.GetBuildableClass()->GetName() : TEXT("?"),
			*TDS0Ort(Ref.GetBuildableTransform().GetLocation()), Ref.GetBuiltWithRecipe() ? *Ref.GetBuiltWithRecipe()->GetName() : TEXT("keins"),
			Daten && Daten->BlueprintProxy ? TEXT("ja") : TEXT("nein"));
	}
	if (OutHandle) { *OutHandle = Handle; }
	if (bOutInstanz) { *bOutInstanz = bInstanz; }
	if (OutRef) { *OutRef = Ref; }
	if (bOutLeicht) { *bOutLeicht = bLeicht; }
	return Text;
}

static bool TDS0Strahl(UWorld* World, const FVector& Start, const FVector& Ende, TArray<FHitResult>& OutTreffer, const AActor* Ignoriere = nullptr)
{
	// wie UTDGegnerHirn::MauerRichtungKern: feste und bewegliche Weltobjekte, einfache Kollision
	FCollisionQueryParams Query(TEXT("TDSpikeS0"), false, Ignoriere);
	FCollisionObjectQueryParams Objekte;
	Objekte.AddObjectTypesToQuery(ECC_WorldStatic);
	Objekte.AddObjectTypesToQuery(ECC_WorldDynamic);
	return World->LineTraceMultiByObjectType(OutTreffer, Start, Ende, Objekte, Query);
}

/** Strahl durch die duennste Achse eines Kastens (Wand: quer, Fundament und Rampe: von oben) */
static void TDS0StrahlDurch(const FTransform& Lage, const FBox& Kasten, FVector& OutStart, FVector& OutEnde, FVector& OutWegA, FVector& OutWegB)
{
	const FBox Box = Kasten.IsValid ? Kasten : FBox(FVector(-400.f, -25.f, 0.f), FVector(400.f, 25.f, 400.f));
	const FVector Mitte = Lage.TransformPosition(Box.GetCenter());
	const FVector Halb = Box.GetExtent();
	const bool bWand = Halb.Y <= Halb.Z && Halb.Y <= Halb.X;
	const FVector Achse = bWand ? Lage.GetUnitAxis(EAxis::Y) : Halb.X < Halb.Z ? Lage.GetUnitAxis(EAxis::X) : FVector::UpVector;
	const float Weite = (bWand ? Halb.Y : Halb.X < Halb.Z ? Halb.X : Halb.Z) + 300.f;
	OutStart = Mitte + Achse * Weite;
	OutEnde = Mitte - Achse * Weite;
	// Wegprobe quer durch das Bauteil, 3 m vor und hinter dem Kasten, auf Fusshoehe
	const FVector Quer = bWand ? Lage.GetUnitAxis(EAxis::Y) : Lage.GetUnitAxis(EAxis::X);
	const float QuerWeite = (bWand ? Halb.Y : Halb.X) + 300.f;
	const FVector Fuss = Lage.TransformPosition(FVector(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z));
	OutWegA = Fuss + Quer * QuerWeite;
	OutWegB = Fuss - Quer * QuerWeite;
}

/** Wegsuche mit dem Filter der Gegner (UTDNavFilter) */
static FString TDS0Weg(UWorld* World, const FVector& A, const FVector& B)
{
	UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World);
	FNavLocation NavA, NavB;
	if (!Nav || !Nav->ProjectPointToNavigation(A, NavA, FVector(300.f, 300.f, 600.f)) || !Nav->ProjectPointToNavigation(B, NavB, FVector(300.f, 300.f, 600.f)))
	{
		return TEXT("kein Netz an einem der beiden Punkte");
	}
	const UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, NavA.Location, NavB.Location, nullptr, UTDNavFilter::StaticClass());
	if (!Pfad || !Pfad->IsValid())
	{
		return TEXT("kein Weg");
	}
	return FString::Printf(TEXT("%s, %.1f m (Luftlinie %.1f m)"), Pfad->IsPartial() ? TEXT("Teilweg") : TEXT("voll"), Pfad->GetPathLength() / 100.f,
		FVector::Dist(NavA.Location, NavB.Location) / 100.f);
}

/** Naechstes gueltiges Lightweight-Bauteil, dessen Klassenname NamensTeil enthaelt (leer = jedes), im Umkreis um Mitte */
static FTDS0Teil TDS0Suche(UWorld* World, const FVector& Mitte, const FString& NamensTeil, float Radius, bool bNurSpielwand = false)
{
	FTDS0Teil Best;
	float BestAbstand = Radius * Radius;
	AFGLightweightBuildableSubsystem* Leicht = AFGLightweightBuildableSubsystem::Get(World);
	if (!Leicht)
	{
		return Best;
	}
	for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& P : Leicht->GetAllLightweightBuildableInstances())
	{
		if (!P.Key || (!NamensTeil.IsEmpty() && !P.Key->GetName().Contains(NamensTeil)))
		{
			continue;
		}
		if (bNurSpielwand && (!P.Key->IsChildOf(AFGBuildableWall::StaticClass()) || P.Key->IsChildOf(ATDDefenseWall::StaticClass())))
		{
			continue;
		}
		for (int32 i = 0; i < P.Value.Num(); ++i)
		{
			const FRuntimeBuildableInstanceData& D = P.Value[i];
			const float Abstand = FVector::DistSquared(D.Transform.GetLocation(), Mitte);
			if (D.IsValid() && Abstand < BestAbstand)
			{
				BestAbstand = Abstand;
				Best.Klasse = P.Key;
				Best.Index = i;
				Best.Lage = D.Transform;
				Best.Kasten = D.BoundingBox;
				Best.bBlaupause = D.BlueprintProxy != nullptr;
			}
		}
	}
	return Best;
}

/** Ist an der gemerkten Lage noch (oder wieder) ein Bauteil der Klasse? */
static bool TDS0InstanzDa(UWorld* World, const FString& Klasse, const FVector& Ort)
{
	const FTDS0Teil Teil = TDS0Suche(World, Ort, Klasse, 20.f);
	return Teil.IstGueltig();
}

static void TDS0Sage(TArray<FString>& Out, const FString& Zeile)
{
	Out.Add(Zeile);
}

void TDSpikeS0::Befehl(UWorld* World, AFGPlayerController* Player, const TArray<FString>& Args, TArray<FString>& Out)
{
	const FString Was = Args.Num() > 0 ? Args[0].ToLower() : FString();
	const FVector Mitte = TDS0Mitte(World, Player);
	AFGLightweightBuildableSubsystem* Leicht = AFGLightweightBuildableSubsystem::Get(World);
	if (!World || !Leicht)
	{
		TDS0Sage(Out, TEXT("S0: kein Lightweight-Subsystem in dieser Welt"));
		return;
	}

	// ---- a0: Strahl durch Verteidigungsmauern, Vergleich des Kollisionstopfs mit einer Spielwand -------------------------------
	if (Was == TEXT("strahl"))
	{
		const ULightweightCollisionComponent* TopfMauer = nullptr;
		int32 Mauern = 0;
		for (TActorIterator<ATDDefenseWall> It(World); It && Mauern < 3; ++It)
		{
			++Mauern;
			const FTransform Lage = It->GetActorTransform();
			FVector Start, Ende, WegA, WegB;
			TDS0StrahlDurch(Lage, FBox(FVector(-400.f, -25.f, 0.f), FVector(400.f, 25.f, 400.f)), Start, Ende, WegA, WegB);
			TArray<FHitResult> Treffer;
			TDS0Strahl(World, Start, Ende, Treffer);
			if (Treffer.Num() == 0)
			{
				TDS0Sage(Out, FString::Printf(TEXT("S0a0: Strahl durch Verteidigungsmauer %s trifft nichts"), *It->GetName()));
			}
			for (int32 i = 0; i < FMath::Min(Treffer.Num(), 3); ++i)
			{
				FInstanceHandle Handle;
				bool bInstanz = false;
				const FString Text = TDS0Beschreibe(World, Treffer[i], &Handle, &bInstanz, nullptr, nullptr);
				if (bInstanz && Handle.GetOwner() == *It && !TopfMauer)
				{
					TopfMauer = Handle.GetCollisionInstanceComponent();
				}
				TDS0Sage(Out, FString::Printf(TEXT("S0a0: Strahl durch Verteidigungsmauer %s, Treffer %d von %d: %s%s"), *It->GetName(), i + 1, Treffer.Num(), *Text,
					Treffer[i].GetActor() == *It ? TEXT(" | Actor ist die Mauer selbst") : TEXT("")));
			}
			// die Instanz-Handles der Mauer selbst (unabhaengig vom Strahl)
			const FInstanceHandleArray Eigene = It->GetLightweightInstanceHandles();
			for (const FInstanceHandlePtr& H : Eigene.HandleArray)
			{
				if (H.IsValid())
				{
					TopfMauer = TopfMauer ? TopfMauer : H->GetCollisionInstanceComponent();
					TDS0Sage(Out, FString::Printf(TEXT("S0a0: Verteidigungsmauer %s hat Instanz-Handle, Kollisionstopf %s, Besitzer %s"), *It->GetName(),
						H->GetCollisionInstanceComponent() ? *H->GetCollisionInstanceComponent()->GetName() : TEXT("keiner"),
						H->GetOwner() ? *H->GetOwner()->GetClass()->GetName() : TEXT("keiner")));
					break;
				}
			}
			if (Eigene.HandleArray.Num() == 0)
			{
				TDS0Sage(Out, FString::Printf(TEXT("S0a0: Verteidigungsmauer %s hat keine Instanz-Handles"), *It->GetName()));
			}
		}
		if (Mauern == 0)
		{
			TDS0Sage(Out, TEXT("S0a0: keine Verteidigungsmauer in der Welt"));
		}
		// Spielwand zum Vergleich
		const FTDS0Teil Wand = TDS0Suche(World, Mitte, FString(), 50000.f, true);
		if (!Wand.IstGueltig())
		{
			TDS0Sage(Out, TEXT("S0a0: keine Spielwand in der Welt - erst /td s0 baue wand"));
			return;
		}
		FVector Start, Ende, WegA, WegB;
		TDS0StrahlDurch(Wand.Lage, Wand.Kasten, Start, Ende, WegA, WegB);
		TArray<FHitResult> Treffer;
		TDS0Strahl(World, Start, Ende, Treffer);
		const ULightweightCollisionComponent* TopfWand = nullptr;
		for (int32 i = 0; i < FMath::Min(Treffer.Num(), 3); ++i)
		{
			FInstanceHandle Handle;
			bool bInstanz = false;
			const FString Text = TDS0Beschreibe(World, Treffer[i], &Handle, &bInstanz, nullptr, nullptr);
			TopfWand = bInstanz && !TopfWand ? Handle.GetCollisionInstanceComponent() : TopfWand;
			TDS0Sage(Out, FString::Printf(TEXT("S0a0: Strahl durch Spielwand %s, Treffer %d von %d: %s"), *Wand.Klasse->GetName(), i + 1, Treffer.Num(), *Text));
		}
		if (Mauern > 0)
		{
			TDS0Sage(Out, FString::Printf(TEXT("S0a0: Kollisionstopf TD-Mauer == Spielwand: %s (Mauer %s, Wand %s)"),
				!TopfMauer || !TopfWand ? TEXT("nicht messbar") : TopfMauer == TopfWand ? TEXT("ja") : TEXT("nein"),
				TopfMauer ? *TopfMauer->GetName() : TEXT("keiner"), TopfWand ? *TopfWand->GetName() : TEXT("keiner")));
		}
		return;
	}

	// ---- Hilfsbau: eine Spielwand oder ein Fundament wie beim Bauen im Spiel (wird danach Lightweight) -------------------------
	if (Was == TEXT("baue"))
	{
		const bool bFundament = Args.Num() > 1 && Args[1].StartsWith(TEXT("fund"), ESearchCase::IgnoreCase);
		const float Abstand = (Args.Num() > 2 ? FMath::Clamp(FCString::Atof(*Args[2]), 5.f, 200.f) : 34.f) * 100.f;
		const float Winkel = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
		const TCHAR* KlassenPfad = bFundament ? TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_8x1_01.Build_Foundation_8x1_01_C")
			: TEXT("/Game/FactoryGame/Buildable/Building/Wall/Build_Wall_8x4_01.Build_Wall_8x4_01_C");
		const TCHAR* RezeptPfad = bFundament ? TEXT("/Game/FactoryGame/Recipes/Buildings/Foundations/Recipe_Foundation_8x1_01.Recipe_Foundation_8x1_01_C")
			: TEXT("/Game/FactoryGame/Recipes/Buildings/Walls/Recipe_Wall_8x4_01.Recipe_Wall_8x4_01_C");
		UClass* Klasse = LoadClass<AFGBuildable>(nullptr, KlassenPfad);
		UClass* Rezept = LoadClass<UFGRecipe>(nullptr, RezeptPfad);
		AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(World);
		const FVector Richtung = FRotator(0.f, Winkel, 0.f).Vector();
		FVector Ort = Mitte + Richtung * Abstand;
		FHitResult Boden;
		FCollisionQueryParams Query(TEXT("TDSpikeS0Boden"), false);
		if (World->LineTraceSingleByChannel(Boden, Ort + FVector(0.f, 0.f, 3000.f), Ort - FVector(0.f, 0.f, 6000.f), ECC_WorldStatic, Query))
		{
			Ort = Boden.ImpactPoint;
		}
		// Wand quer zur Richtung zum Kern (Riegel), Fundament flach
		const FTransform Lage(FRotator(0.f, Winkel + (bFundament ? 0.f : 90.f), 0.f), Ort);
		AFGBuildable* Neu = Klasse && Bau ? Bau->BeginSpawnBuildable(Klasse, Lage) : nullptr;
		if (Neu)
		{
			Neu->SetBuiltWithRecipe(Rezept);
			Neu->FinishSpawning(Lage);
		}
		TDS0Sage(Out, FString::Printf(TEXT("S0: %s %s bei %s, %.0f m von der Mitte (Klasse %s, Rezept %s)"), bFundament ? TEXT("Spielfundament") : TEXT("Spielwand"),
			Neu ? TEXT("gebaut") : TEXT("NICHT gebaut"), *TDS0Ort(Ort), Abstand / 100.f, Klasse ? TEXT("da") : TEXT("fehlt"), Rezept ? TEXT("da") : TEXT("fehlt")));
		return;
	}

	// ---- b: Uebersicht der Lightweight-Bauteile im Umkreis --------------------------------------------------------------------
	if (Was == TEXT("liste"))
	{
		const float Radius = (Args.Num() > 1 ? FMath::Clamp(FCString::Atof(*Args[1]), 5.f, 500.f) : 60.f) * 100.f;
		int32 Klassen = 0, Alle = 0, Blaupausen = 0;
		TArray<FString> Zeilen;
		for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& P : Leicht->GetAllLightweightBuildableInstances())
		{
			int32 Zahl = 0, MitBlaupause = 0;
			for (const FRuntimeBuildableInstanceData& D : P.Value)
			{
				if (D.IsValid() && FVector::DistSquared(D.Transform.GetLocation(), Mitte) <= Radius * Radius)
				{
					++Zahl;
					MitBlaupause += D.BlueprintProxy ? 1 : 0;
				}
			}
			if (Zahl > 0 && P.Key)
			{
				++Klassen;
				Alle += Zahl;
				Blaupausen += MitBlaupause;
				Zeilen.Add(FString::Printf(TEXT("S0b: %d x %s%s"), Zahl, *P.Key->GetName(), MitBlaupause ? *FString::Printf(TEXT(" (%d aus Blaupausen)"), MitBlaupause) : TEXT("")));
			}
		}
		TDS0Sage(Out, FString::Printf(TEXT("S0b: Liste %d Klassen, %d Lightweight-Bauteile im Umkreis %.0f m, davon %d aus Blaupausen"), Klassen, Alle, Radius / 100.f, Blaupausen));
		Zeilen.Sort();
		for (int32 i = 0; i < FMath::Min(Zeilen.Num(), 20); ++i)
		{
			TDS0Sage(Out, Zeilen[i]);
		}
		return;
	}

	// ---- b: Treffer aufloesen - "blick" (Spielersicht) oder "probe <Klassenteil>" (naechstes Bauteil an der Mitte) -----------
	// ---- c: "abriss blick|<Klassenteil>" entfernt das aufgeloeste Bauteil --------------------------------------------------
	const bool bAbriss = Was == TEXT("abriss");
	if (Was == TEXT("blick") || Was == TEXT("probe") || bAbriss)
	{
		const FString Ziel = bAbriss || Was == TEXT("probe") ? (Args.Num() > 1 ? Args[1] : FString()) : FString(TEXT("blick"));
		const bool bBlick = Was == TEXT("blick") || Ziel.Equals(TEXT("blick"), ESearchCase::IgnoreCase);
		FVector Start, Ende, WegA = FVector::ZeroVector, WegB = FVector::ZeroVector;
		FTDS0Teil Gesucht;
		if (bBlick)
		{
			if (!Player)
			{
				TDS0Sage(Out, TEXT("S0b: kein Spieler fuer den Blickstrahl"));
				return;
			}
			FRotator Blick;
			Player->GetPlayerViewPoint(Start, Blick);
			Ende = Start + Blick.Vector() * 5000.f;
		}
		else
		{
			if (Ziel.IsEmpty())
			{
				TDS0Sage(Out, TEXT("S0: Klassenteil fehlt, z. B. /td s0 probe Foundation | Ramp | Wall_8x4"));
				return;
			}
			Gesucht = TDS0Suche(World, Mitte, Ziel, 20000.f);
			if (!Gesucht.IstGueltig())
			{
				TDS0Sage(Out, FString::Printf(TEXT("S0b: kein Lightweight-Bauteil mit '%s' im Umkreis von 200 m"), *Ziel));
				return;
			}
			TDS0StrahlDurch(Gesucht.Lage, Gesucht.Kasten, Start, Ende, WegA, WegB);
		}
		TArray<FHitResult> Treffer;
		TDS0Strahl(World, Start, Ende, Treffer, Player ? Player->GetPawn() : nullptr);
		FLightweightBuildableInstanceRef Ref;
		bool bGefunden = false;
		FString Text = TEXT("Strahl trifft nichts");
		for (const FHitResult& Hit : Treffer)
		{
			FLightweightBuildableInstanceRef Kandidat;
			bool bLeicht = false;
			const FString Zeile = TDS0Beschreibe(World, Hit, nullptr, nullptr, &Kandidat, &bLeicht);
			if (!bGefunden)
			{
				Text = Zeile;   // erster Treffer, falls keiner passt
			}
			// Blick: das erste Lightweight-Bauteil; Probe: genau das gesuchte (Klasse und Lage)
			const bool bPasst = bLeicht && (bBlick || (Kandidat.GetBuildableClass() == Gesucht.Klasse
				&& Kandidat.GetBuildableTransform().GetLocation().Equals(Gesucht.Lage.GetLocation(), 5.f)));
			if (bPasst)
			{
				Ref = Kandidat;
				bGefunden = true;
				Text = Zeile;
				break;
			}
		}
		TDS0Sage(Out, FString::Printf(TEXT("S0b: %s%s -> %s"), bBlick ? TEXT("Blick") : *FString::Printf(TEXT("Probe '%s' (%s bei %s)"), *Ziel,
			*Gesucht.Klasse->GetName(), *TDS0Ort(Gesucht.Lage.GetLocation())), bGefunden ? TEXT(", aufgeloest ja") : TEXT(", aufgeloest NEIN"), *Text));
		if (!bAbriss)
		{
			return;
		}
		if (!bGefunden)
		{
			TDS0Sage(Out, TEXT("S0c: kein Abriss - der Strahl hat das Bauteil nicht aufgeloest"));
			return;
		}
		const FRuntimeBuildableInstanceData* Daten = Ref.ResolveBuildableInstanceData();
		const FTransform Lage = Ref.GetBuildableTransform();
		const FBox Kasten = Daten ? Daten->BoundingBox : FBox(ForceInit);
		const bool bBlaupause = Daten && Daten->BlueprintProxy;
		if (bBlick)
		{
			FVector S2, E2;
			TDS0StrahlDurch(Lage, Kasten, S2, E2, WegA, WegB);
			Start = S2;
			Ende = E2;
		}
		GS0Merker.bGesetzt = true;
		GS0Merker.Klasse = Ref.GetBuildableClass() ? Ref.GetBuildableClass()->GetName() : FString();
		GS0Merker.Lage = Lage;
		GS0Merker.StrahlStart = Start;
		GS0Merker.StrahlEnde = Ende;
		GS0Merker.WegA = WegA;
		GS0Merker.WegB = WegB;
		TDS0Sage(Out, FString::Printf(TEXT("S0c: Ziel %s bei %s, Blaupause %s, Weg quer vorher: %s"), *GS0Merker.Klasse, *TDS0Ort(Lage.GetLocation()),
			bBlaupause ? TEXT("ja") : TEXT("nein"), *TDS0Weg(World, WegA, WegB)));
		const bool bEntfernt = Ref.Remove();
		TDS0Sage(Out, FString::Printf(TEXT("S0c: Remove %s"), bEntfernt ? TEXT("true") : TEXT("false")));
		// sofort danach: derselbe Strahl
		TArray<FHitResult> Danach;
		TDS0Strahl(World, Start, Ende, Danach, Player ? Player->GetPawn() : nullptr);
		TDS0Sage(Out, Danach.Num() == 0 ? FString(TEXT("S0c: Strahl nach Abriss frei"))
			: FString::Printf(TEXT("S0c: Strahl nach Abriss trifft %s"), *TDS0Beschreibe(World, Danach[0], nullptr, nullptr, nullptr, nullptr)));
		TDS0Sage(Out, FString::Printf(TEXT("S0c: Instanz nach Abriss %s"), TDS0InstanzDa(World, GS0Merker.Klasse, Lage.GetLocation()) ? TEXT("NOCH DA") : TEXT("weg")));
		return;
	}

	// ---- Schlupfloch "Rampen der Kernplattform abreissen" (Bericht A1): alle Bauteile einer Art im Umkreis entfernen ------------
	if (Was == TEXT("abrissalle"))
	{
		const FString Teil = Args.Num() > 1 ? Args[1] : FString();
		const float Radius = (Args.Num() > 2 ? FMath::Clamp(FCString::Atof(*Args[2]), 5.f, 200.f) : 60.f) * 100.f;
		if (Teil.IsEmpty())
		{
			TDS0Sage(Out, TEXT("S0: Klassenteil fehlt, z. B. /td s0 abrissalle Ramp 60"));
			return;
		}
		// erst sammeln, dann entfernen - Remove() aendert die Tabelle, ueber die gesucht wird
		TArray<FLightweightBuildableInstanceRef> Ziele;
		for (const TPair<TSubclassOf<AFGBuildable>, TArray<FRuntimeBuildableInstanceData>>& P : Leicht->GetAllLightweightBuildableInstances())
		{
			if (!P.Key || !P.Key->GetName().Contains(Teil))
			{
				continue;
			}
			for (int32 i = 0; i < P.Value.Num(); ++i)
			{
				if (P.Value[i].IsValid() && !P.Value[i].BlueprintProxy && FVector::DistSquared(P.Value[i].Transform.GetLocation(), Mitte) <= Radius * Radius)
				{
					FLightweightBuildableInstanceRef Ref;
					Ref.Initialize(Leicht, P.Key, i);
					Ziele.Add(Ref);
				}
			}
		}
		int32 Entfernt = 0;
		for (FLightweightBuildableInstanceRef& Ref : Ziele)
		{
			Entfernt += Ref.IsValid() && Ref.Remove() ? 1 : 0;
		}
		TDS0Sage(Out, FString::Printf(TEXT("S0c: Abriss aller '%s' im Umkreis %.0f m: %d von %d entfernt"), *Teil, Radius / 100.f, Entfernt, Ziele.Num()));
		return;
	}

	// ---- c: Nachmessung an der gemerkten Stelle (nach einigen Sekunden, nach Speichern und Laden) -----------------------------
	if (Was == TEXT("nach"))
	{
		if (!GS0Merker.bGesetzt)
		{
			TDS0Sage(Out, TEXT("S0c: nichts gemerkt - erst /td s0 abriss ..."));
			return;
		}
		TArray<FHitResult> Treffer;
		TDS0Strahl(World, GS0Merker.StrahlStart, GS0Merker.StrahlEnde, Treffer, Player ? Player->GetPawn() : nullptr);
		const bool bDa = TDS0InstanzDa(World, GS0Merker.Klasse, GS0Merker.Lage.GetLocation());
		TDS0Sage(Out, FString::Printf(TEXT("S0c: Nachmessung %s bei %s: Instanz %s, Strahl %s, Weg quer: %s"), *GS0Merker.Klasse, *TDS0Ort(GS0Merker.Lage.GetLocation()),
			bDa ? TEXT("WIEDER DA") : TEXT("weiter weg"),
			Treffer.Num() == 0 ? TEXT("frei") : *(TEXT("trifft ") + TDS0Beschreibe(World, Treffer[0], nullptr, nullptr, nullptr, nullptr)),
			*TDS0Weg(World, GS0Merker.WegA, GS0Merker.WegB)));
		return;
	}

	TDS0Sage(Out, TEXT("S0: /td s0 strahl | baue wand|fundament [Abstand_m] [Winkel] | liste [Radius_m] | blick | probe <Klassenteil> | abriss blick|<Klassenteil> | abrissalle <Klassenteil> [Radius_m] | nach"));
}
