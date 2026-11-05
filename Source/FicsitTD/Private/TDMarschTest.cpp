#include "TDMarschTest.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDGegnerHirn.h"
#include "TDWaveSubsystem.h"
#include "Command/CommandSender.h"
#include "Creature/FGCreature.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

/** Je Abschnitt eine Kreatur jeder Art: Nahkampf schnell/zaeh, flink, Fernkampf */
static const TCHAR* MarschTestKlassen[] = {
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Char_Hog.Char_Hog_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"),
	TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestSmall.Char_SpitterForestSmall_C"),
};
static constexpr float MarschTestEnde = 160.f;

ATDMarschTest::ATDMarschTest()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Mitte"));
}

void ATDMarschTest::Melde(const FString& Text, bool bChat) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("TDMarsch %s"), *Text);
	if (bChat && mSender)
	{
		mSender->SendChatMessage(Text.Left(200));
	}
}

float ATDMarschTest::SpielerAbstand() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn && mKern.IsValid() ? FVector::Dist2D(Pawn->GetActorLocation(), mKern->GetActorLocation()) / 100.f : -1.f;
}

void ATDMarschTest::Starte(ATDDefenseCore* Kern, UCommandSender* Sender)
{
	mKern = Kern;
	mSender = Sender;
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!Kern || !Waves)
	{
		Melde(TEXT("Marschtest: kein Abwehrkern."), true);
		Destroy();
		return;
	}
	const TArray<FString> Zeilen = Waves->GetFrontenText(Kern);
	if (Waves->GetFronten().Num() == 0)
	{
		Melde(TEXT("Marschtest: keine Front gefunden."), true);
		Destroy();
		return;
	}
	mFront = Waves->GetFronten()[0].Ort;
	mFrontName = Zeilen.IsValidIndex(1) ? Zeilen[1] : TEXT("Front 1");
	mWeg = UTDGegnerHirn::BerechneWeg(GetWorld(), mFront, Kern);
	mKernLPStart = Kern->GetTDHealth();
	Melde(FString::Printf(TEXT("Marschtest: %s, Weg %d Punkte. 160 s, drei Abschnitte. Bitte den Ansagen folgen."), *mFrontName, mWeg.Num()), true);
	SpawneAbschnitt('A');
	Melde(TEXT("A (0-40 s): Schau zur Front und sieh den Gegnern zu."), true);
	GetWorldTimerManager().SetTimer(mTimer, this, &ATDMarschTest::Schritt, 1.f, true);
}

void ATDMarschTest::SpawneAbschnitt(TCHAR Abschnitt)
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	for (const TCHAR* Pfad : MarschTestKlassen)
	{
		UClass* Klasse = LoadClass<AFGCreature>(nullptr, Pfad);
		const FString Art = Klasse ? Klasse->GetName().Replace(TEXT("Char_"), TEXT("")).Replace(TEXT("_C"), TEXT("")) : FString(Pfad);
		FNavLocation Punkt;
		const FVector Ort = Nav && Nav->GetRandomReachablePointInRadius(mFront, 1000.f, Punkt) ? Punkt.Location + FVector(0, 0, 150.f) : mFront + FVector(0, 0, 150.f);
		AFGCreature* Kreatur = UTDGegnerHirn::Spawne(GetWorld(), Klasse, Ort, mWeg, mKern.Get(), 1.f);
		if (!Kreatur)
		{
			Melde(FString::Printf(TEXT("Abschnitt %c: Spawn %s fehlgeschlagen"), Abschnitt, *Art), true);
			continue;
		}
		FTDMarschTeil T;
		T.Kreatur = Kreatur;
		T.Abschnitt = Abschnitt;
		T.Art = Art;
		T.Spawnzeit = mZeit;
		if (const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(Kreatur))
		{
			T.StartRest = Hirn->GetRestweg();
		}
		mTeile.Add(T);
	}
	Melde(FString::Printf(TEXT("Abschnitt %c: Hog, Alpha-Hog, Stinger, Spitter gespawnt, Spieler %.0f m vom Kern"), Abschnitt, SpielerAbstand()), false);
}

void ATDMarschTest::Schritt()
{
	mZeit += 1.f;
	const int32 Sekunde = FMath::RoundToInt(mZeit);
	if (Sekunde == 40)
	{
		SpawneAbschnitt('B');
		Melde(TEXT("B (40-80 s): Neue Gegner. Dreh dich jetzt weg und schau NICHT zur Front."), true);
	}
	else if (Sekunde == 80)
	{
		Melde(TEXT("C: Lauf jetzt mindestens 400 m vom Kern weg (weg von der Front). In 20 s kommen die letzten Gegner."), true);
	}
	else if (Sekunde == 100)
	{
		SpawneAbschnitt('C');
		Melde(FString::Printf(TEXT("C (100-160 s): Gegner gespawnt, du bist %.0f m vom Kern. Bleib dort bis zum Ergebnis."), SpielerAbstand()), true);
	}
	for (FTDMarschTeil& T : mTeile)
	{
		const AFGCreature* Kreatur = T.Kreatur.Get();
		const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(Kreatur);
		if (!Kreatur || !Hirn || !Kreatur->IsAliveAndWell())
		{
			continue;
		}
		T.bAmZiel |= Hirn->GetPhase() == ETDHirnPhase::Angriff || Hirn->GetPhase() == ETDHirnPhase::Anlauf || Hirn->GetRestweg() < 4000.f;
		T.bAbgeschaltet |= !Kreatur->GetIsEnabled();
		T.MaxUngesehen = FMath::Max(T.MaxUngesehen, GetWorld()->GetTimeSeconds() - Kreatur->GetLastRenderTime());
		if (T.Rest30 < 0.f && mZeit - T.Spawnzeit >= 30.f)
		{
			T.Rest30 = Hirn->GetRestweg();
		}
		if (Sekunde % 5 == 0)
		{
			Melde(FString::Printf(TEXT("%c %s: %s"), T.Abschnitt, *T.Art, *Hirn->Diagnose()), false);
		}
	}
	if (mZeit >= MarschTestEnde)
	{
		GetWorldTimerManager().ClearTimer(mTimer);
		Auswerten();
	}
}

void ATDMarschTest::Auswerten()
{
	Melde(FString::Printf(TEXT("Ergebnis (Spieler jetzt %.0f m vom Kern, Abschalten durch das Spiel verhindert: %d x):"), SpielerAbstand(), FFicsitTDModule::AbschaltenVerhindert), true);
	for (const TCHAR Abschnitt : { 'A', 'B', 'C' })
	{
		int32 Anzahl = 0, AmZiel = 0, Versetzt = 0, Abgeschaltet = 0;
		float Ungesehen = 0.f, Angerichtet = 0.f;
		TSet<FString> Opfer;
		FString Arten;
		for (const FTDMarschTeil& T : mTeile)
		{
			if (T.Abschnitt != Abschnitt)
			{
				continue;
			}
			++Anzahl;
			AmZiel += T.bAmZiel ? 1 : 0;
			Abgeschaltet += T.bAbgeschaltet ? 1 : 0;
			Ungesehen = FMath::Max(Ungesehen, T.MaxUngesehen);
			// Rest30 fehlt, wenn der Gegner vorher starb oder am Ziel war: dann zaehlt der ganze Weg
			const float Meter = (T.StartRest - (T.Rest30 >= 0.f ? T.Rest30 : (T.bAmZiel ? 0.f : T.StartRest))) / 100.f;
			int32 Mal = 0;
			if (const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(T.Kreatur.Get()))
			{
				Mal = Hirn->GetVersetzt();
				Angerichtet += Hirn->GetSchaden();
				if (!Hirn->GetLetztesOpfer().IsEmpty())
				{
					Opfer.Add(Hirn->GetLetztesOpfer());
				}
			}
			Versetzt += Mal;
			Arten += FString::Printf(TEXT("%s%s %.0f m%s"), Arten.IsEmpty() ? TEXT("") : TEXT(", "), *T.Art, Meter, Mal > 0 ? *FString::Printf(TEXT(" (%dx versetzt)"), Mal) : TEXT(""));
		}
		if (Anzahl > 0)
		{
			Melde(FString::Printf(TEXT("%c: am Ziel %d/%d, versetzt %d, abgeschaltet %d, ungesehen bis %.0f s, Schaden %.0f an %s"), Abschnitt, AmZiel, Anzahl, Versetzt, Abgeschaltet, Ungesehen,
				Angerichtet, Opfer.Num() ? *FString::Join(Opfer.Array(), TEXT(", ")) : TEXT("-")), true);
			Melde(FString::Printf(TEXT("%c in 30 s: %s"), Abschnitt, *Arten), true);
		}
	}
	if (ATDDefenseCore* Kern = mKern.Get())
	{
		Melde(FString::Printf(TEXT("Schaden am Kern: %.0f LP. Kern wird wieder aufgeladen, Testgegner entfernt."), mKernLPStart - Kern->GetTDHealth()), true);
		Kern->Aufladen();
	}
	for (const FTDMarschTeil& T : mTeile)
	{
		if (AFGCreature* Kreatur = T.Kreatur.Get())
		{
			if (AController* C = Kreatur->GetController())
			{
				C->Destroy();
			}
			Kreatur->Destroy();
		}
	}
	mTeile.Reset();
	Destroy();
}
