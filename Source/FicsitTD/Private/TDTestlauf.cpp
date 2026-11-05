#include "TDTestlauf.h"
#include "FicsitTDModule.h"
#include "TDDamageable.h"
#include "TDDecoy.h"
#include "TDDefenseCore.h"
#include "TDGegnerHirn.h"
#include "TDWaveSubsystem.h"
#include "AI/FGCreatureController.h"
#include "Command/CommandSender.h"
#include "Creature/FGCreature.h"
#include "Engine/World.h"
#include "FGHealthComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

static constexpr float TestDauer = 25.f;
static constexpr float TestSchritt = 0.5f;

/** Alle Gegnertypen des Wellenkatalogs plus Bosse */
static const TCHAR* TestTypen[] = {
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Char_Hog.Char_Hog_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/CliffHog/Char_CliffHog.Char_CliffHog_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/NuclearHog/Char_NuclearHog.Char_NuclearHog_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Johnny/Char_Johnny.Char_Johnny_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestSmall.Char_SpitterForestSmall_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestAlpha.Char_SpitterForestAlpha_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterAquatic/Char_SpitterAquaticAlpha.Char_SpitterAquaticAlpha_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/SmallStinger/Char_Stinger_Child.Char_Stinger_Child_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/BigStinger/Char_EliteStinger.Char_EliteStinger_C"),
};

ATDTestlauf::ATDTestlauf()
{
	PrimaryActorTick.bCanEverTick = false;
	// Ohne Wurzelkomponente ignoriert SpawnActor den Ort - der Test lief sonst um den Kartenursprung (Test 31.10.)
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Mitte"));
}

void ATDTestlauf::Melde(const FString& Text, bool bChat) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("TDTest %s"), *Text);
	if (bChat && mSender)
	{
		mSender->SendChatMessage(Text.Left(200));
	}
}

/** Lebenspunkte traegt der Kern, nicht sein Koeder */
static const ITDDamageable* LPZiel(AActor* Ziel)
{
	if (const ATDDecoy* Koeder = Cast<ATDDecoy>(Ziel))
	{
		Ziel = Koeder->GetActiveTarget();
	}
	return Cast<ITDDamageable>(Ziel);
}

static FVector Boden(UWorld* World, const FVector& Ort, bool& bTreffer, bool& bWasser)
{
	FHitResult Hit;
	FCollisionQueryParams Query(TEXT("TDTestBoden"), false);
	bTreffer = World->LineTraceSingleByChannel(Hit, Ort + FVector(0, 0, 30000.f), Ort - FVector(0, 0, 60000.f), ECC_Visibility, Query);
	bWasser = bTreffer && Hit.GetActor() && Hit.GetActor()->GetClass()->GetName().Contains(TEXT("Water"));
	return bTreffer ? Hit.ImpactPoint : Ort;
}

void ATDTestlauf::Starte(AActor* Ziel, bool bKarte, bool bGegner, UCommandSender* Sender)
{
	mZiel = Ziel;
	mSender = Sender;
	if (const ITDDamageable* D = LPZiel(Ziel))
	{
		mKernLPStart = D->GetTDHealth();
	}
	Melde(FString::Printf(TEXT("Start um %s (%s)"), Ziel ? *Ziel->GetName() : TEXT("?"), *GetActorLocation().ToCompactString()), false);
	if (bKarte)
	{
		KartenTest();
	}
	if (bGegner)
	{
		StarteGegnerTest();
	}
	else
	{
		SetLifeSpan(1.f);
	}
}

void ATDTestlauf::KartenTest()
{
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	const FVector Mitte = GetActorLocation();
	FNavLocation MitteNav;
	const bool bMitteNav = Nav && Nav->ProjectPointToNavigation(Mitte, MitteNav, FVector(1200.f, 1200.f, 1000.f));
	Melde(FString::Printf(TEXT("Karte: Navigationssystem %s, Ziel auf NavMesh: %s"), Nav ? TEXT("da") : TEXT("FEHLT"), bMitteNav ? TEXT("ja") : TEXT("nein")), true);
	Melde(UTDWaveSubsystem::NavDiagnose(World, Mitte), true);

	int32 Gesamt = 0, OhneBoden = 0, Wasser = 0, OhneNav = 0, PfadVoll = 0, PfadTeil = 0, PfadNein = 0;
	TArray<FString> GuteRichtungen;
	for (int32 r = 0; r < 16; ++r)
	{
		const float Winkel = 22.5f * r;
		int32 GutInRichtung = 0;
		for (const float Abstand : { 15000.f, 20000.f, 25000.f })
		{
			++Gesamt;
			const FVector Roh = Mitte + FRotator(0.f, Winkel, 0.f).Vector() * Abstand;
			bool bTreffer = false, bWasser = false;
			const FVector Ort = Boden(World, Roh, bTreffer, bWasser);
			FString Text = FString::Printf(TEXT("Karte %03.0f Grad %3.0f m: "), Winkel, Abstand / 100.f);
			if (!bTreffer)
			{
				++OhneBoden;
				Melde(Text + TEXT("kein Boden"), false);
				continue;
			}
			Wasser += bWasser ? 1 : 0;
			Text += FString::Printf(TEXT("Boden %+.0f m%s, "), (Ort.Z - Mitte.Z) / 100.f, bWasser ? TEXT(" (WASSER)") : TEXT(""));
			FNavLocation NavOrt;
			if (!Nav || !Nav->ProjectPointToNavigation(Ort, NavOrt, FVector(500.f, 500.f, 500.f)))
			{
				++OhneNav;
				Melde(Text + TEXT("kein NavMesh"), false);
				continue;
			}
			UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, NavOrt.Location, bMitteNav ? MitteNav.Location : Mitte);
			if (!Pfad || !Pfad->IsValid())
			{
				++PfadNein;
				Melde(Text + TEXT("NavMesh ja, KEIN Pfad"), false);
				continue;
			}
			const float Laenge = Pfad->GetPathLength();
			const bool bTeil = !UTDWaveSubsystem::PfadAmZiel(Pfad, bMitteNav ? MitteNav.Location : Mitte);
			(bTeil ? PfadTeil : PfadVoll)++;
			GutInRichtung += bTeil || bWasser ? 0 : 1;
			Melde(Text + FString::Printf(TEXT("Pfad %s %.0f m (Umweg x%.2f)"), bTeil ? TEXT("TEILWEISE") : TEXT("voll"), Laenge / 100.f,
				Laenge / FMath::Max(1.f, FVector::Dist(NavOrt.Location, Mitte))), false);
		}
		if (GutInRichtung > 0)
		{
			GuteRichtungen.Add(FString::Printf(TEXT("%.0f"), Winkel));
		}
	}
	Melde(FString::Printf(TEXT("Karte: %d Punkte - Pfad voll %d, teilweise %d, keiner %d; ohne NavMesh %d, ohne Boden %d, Wasser %d"),
		Gesamt, PfadVoll, PfadTeil, PfadNein, OhneNav, OhneBoden, Wasser), true);
	Melde(FString::Printf(TEXT("Karte: brauchbare Spawn-Richtungen (Grad, 0 = +X): %s"),
		GuteRichtungen.Num() ? *FString::Join(GuteRichtungen, TEXT(", ")) : TEXT("KEINE")), true);
}

void ATDTestlauf::StarteGegnerTest()
{
	UWorld* World = GetWorld();
	const FVector Mitte = GetActorLocation();
	// Ziel ist ein Koeder des Kerns: dessen Kern bekommt das Gehirn als Ziel
	const ATDDecoy* Koeder = Cast<ATDDecoy>(mZiel.Get());
	ATDDefenseCore* Kern = Koeder ? Cast<ATDDefenseCore>(Koeder->GetActiveTarget()) : nullptr;
	if (!Kern)
	{
		Melde(TEXT("Gegner: kein Abwehrkern - die Gegner haben kein Ziel und bleiben stehen."), true);
	}
	const int32 Anzahl = UE_ARRAY_COUNT(TestTypen);
	for (int32 i = 0; i < Anzahl; ++i)
	{
		UClass* Klasse = LoadClass<AFGCreature>(nullptr, TestTypen[i]);
		if (!Klasse)
		{
			Melde(FString::Printf(TEXT("Gegner: Klasse fehlt %s"), TestTypen[i]), true);
			continue;
		}
		bool bTreffer = false, bWasser = false;
		const FVector Ort = Boden(World, Mitte + FRotator(0.f, 360.f * i / Anzahl, 0.f).Vector() * 4000.f, bTreffer, bWasser) + FVector(0, 0, 150.f);
		// Mit Gehirn und Weg wie im echten Lauf; 20-fache LP, damit Tuerme die Messung nicht beenden
		AFGCreature* K = UTDGegnerHirn::Spawne(World, Klasse, Ort, UTDGegnerHirn::BerechneWeg(World, Ort, Kern), Kern, 20.f);
		if (!K)
		{
			Melde(FString::Printf(TEXT("Gegner: Spawn fehlgeschlagen %s"), *Klasse->GetName()), true);
			continue;
		}
		FTDTestGegner G;
		G.Kreatur = K;
		G.Name = Klasse->GetName().Replace(TEXT("Char_"), TEXT(""));
		G.Name.RemoveFromEnd(TEXT("_C"));
		G.StartAbstand = FVector::Dist(K->GetActorLocation(), Mitte);
		G.MinAbstand = G.StartAbstand;
		G.LetzterOrt = K->GetActorLocation();
		mGegner.Add(G);
	}
	Melde(FString::Printf(TEXT("Gegner: %d Typen in 40 m erzeugt, Messung %.0f s ..."), mGegner.Num(), TestDauer), true);
	GetWorldTimerManager().SetTimer(mTimer, this, &ATDTestlauf::GegnerSchritt, TestSchritt, true);
}

void ATDTestlauf::GegnerSchritt()
{
	mZeit += TestSchritt;
	for (FTDTestGegner& G : mGegner)
	{
		AFGCreature* K = G.Kreatur.Get();
		if (!K || !K->IsAliveAndWell())
		{
			continue;
		}
		const FVector Ort = K->GetActorLocation();
		G.Weg += FVector::Dist(Ort, G.LetzterOrt);
		G.LetzterOrt = Ort;
		G.MinAbstand = FMath::Min(G.MinAbstand, FVector::Dist(Ort, GetActorLocation()));
		// Sekunden je Phase des Gehirns (Marsch, Anlauf, Angriff)
		if (const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(K))
		{
			static const TCHAR* Namen[] = { TEXT("Marsch"), TEXT("Anlauf"), TEXT("Angriff"), TEXT("tot") };
			G.Zustaende.FindOrAdd(Namen[static_cast<int32>(Hirn->GetPhase())])++;
		}
	}
	if (mZeit >= TestDauer)
	{
		GetWorldTimerManager().ClearTimer(mTimer);
		GegnerAuswerten();
	}
}

void ATDTestlauf::GegnerAuswerten()
{
	int32 Erreicht = 0, Bewegt = 0, Tot = 0;
	float Angerichtet = 0.f;
	TMap<FString, float> Opfer;
	for (const FTDTestGegner& G : mGegner)
	{
		AFGCreature* K = G.Kreatur.Get();
		FString Hirn;
		if (const UTDGegnerHirn* Gehirn = UTDGegnerHirn::Von(K))
		{
			Hirn = Gehirn->Diagnose();
			Angerichtet += Gehirn->GetSchaden();
			if (Gehirn->GetSchaden() > 0.f)
			{
				Opfer.FindOrAdd(Gehirn->GetLetztesOpfer()) += Gehirn->GetSchaden();
			}
		}
		const bool bLebt = K && K->IsAliveAndWell();
		Tot += bLebt ? 0 : 1;
		Erreicht += G.MinAbstand < 800.f ? 1 : 0;
		Bewegt += G.Weg > 500.f ? 1 : 0;
		FString Zustaende;
		for (const TPair<FString, int32>& Z : G.Zustaende)
		{
			Zustaende += FString::Printf(TEXT("%s=%.0fs "), *Z.Key, Z.Value * TestSchritt);
		}
		Melde(FString::Printf(TEXT("Gegner %s: Start %.0f m, naechster Abstand %.0f m, Weg %.0f m, %s | %s| %s"), *G.Name, G.StartAbstand / 100.f,
			G.MinAbstand / 100.f, G.Weg / 100.f, bLebt ? TEXT("lebt") : TEXT("tot"), *Zustaende, *Hirn), false);
		if (K)
		{
			if (AController* C = K->GetController())
			{
				C->Destroy();
			}
			K->Destroy();
		}
	}
	float Schaden = 0.f;
	if (const ITDDamageable* D = LPZiel(mZiel.Get()))
	{
		Schaden = mKernLPStart - D->GetTDHealth();
	}
	Melde(FString::Printf(TEXT("Gegner: %d Typen - %d bewegten sich, %d kamen auf 8 m ans Ziel, %d starben; Schaden am Kern %.0f LP. Details im Log (TDTest)."),
		mGegner.Num(), Bewegt, Erreicht, Tot, Schaden), true);
	FString OpferText;
	for (const TPair<FString, float>& O : Opfer)
	{
		OpferText += FString::Printf(TEXT("%s%s %.0f"), OpferText.IsEmpty() ? TEXT("") : TEXT(", "), *O.Key, O.Value);
	}
	Melde(FString::Printf(TEXT("Angerichteter Schaden gesamt %.0f LP: %s"), Angerichtet, OpferText.IsEmpty() ? TEXT("keiner") : *OpferText), true);
	mGegner.Reset();
	SetLifeSpan(1.f);
}
