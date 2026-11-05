#include "TDLaufProtokoll.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "Creature/FGCreature.h"
#include "EngineUtils.h"
#include "FGCharacterPlayer.h"
#include "FGChatManager.h"
#include "FGInventoryComponent.h"
#include "FGItemPickup_Spawnable.h"
#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGSaveSession.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "Buildables/FGBuildableGenerator.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Resources/FGItemDescriptor.h"
#include "TimerManager.h"

bool UTDLaufProtokoll::bBalancing = false;

namespace TDBeute
{
	/** Beute-Tabelle (Marius 03.11.2026: Kreaturen lassen Beute fallen); Chancen je Gegnerart in Balance Loot.<Art>.<Stufe> */
	struct FEintrag { const TCHAR* Art; const TCHAR* Item; int32 Menge; };
	static const FEintrag Kampfdaten = { TEXT("Kampfdaten"), TEXT("/FicsitTD/Items/Desc_TD_Kampfdaten.Desc_TD_Kampfdaten_C"), 1 };
}

UTDLaufProtokoll* UTDLaufProtokoll::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDLaufProtokoll>() : nullptr;
}

void UTDLaufProtokoll::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bBalancing = false;
	// Modus kommt mit dem Kern aus dem Spielstand; kurz warten, bis er geladen ist
	FTimerHandle Einmal;
	InWorld.GetTimerManager().SetTimer(Einmal, [this]()
	{
		bBalancing = IstBalancing();
		if (bBalancing)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Lauf: Balancing-Lauf aktiv (aus dem Spielstand)"));
		}
	}, 3.f, false);
}

bool UTDLaufProtokoll::IstBalancing() const
{
	const UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	const ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	return Kern && Kern->mBalancingLauf;
}

void UTDLaufProtokoll::SetzeBalancing(bool bAn)
{
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr)
	{
		Kern->mBalancingLauf = bAn;
	}
	bBalancing = bAn;
	UE_LOG(LogFicsitTD, Log, TEXT("Lauf: Balancing-Lauf %s"), bAn ? TEXT("an") : TEXT("aus"));
}

void UTDLaufProtokoll::Melde(const FString& Text) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("Lauf: %s"), *Text);
	if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct Nachricht;
		Nachricht.MessageText = FText::FromString(Text);
		Nachricht.MessageType = EFGChatMessageType::CMT_CustomMessage;
		Nachricht.MessageSender = FText::FromString(TEXT("FICSIT TD"));
		Nachricht.MessageSenderColor = FLinearColor(1.f, 0.55f, 0.1f);
		Chat->BroadcastChatMessage(Nachricht);
	}
}

void UTDLaufProtokoll::SchreibeZeile(const FString& Datei, const FString& Kopf, const FString& Zeile) const
{
	const FString Pfad = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FicsitTD"), Datei);
	const FString Text = (IFileManager::Get().FileExists(*Pfad) ? FString() : Kopf + TEXT("\n")) + Zeile + TEXT("\n");
	FFileHelper::SaveStringToFile(Text, *Pfad, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}

int32 UTDLaufProtokoll::FPImInventar() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFGCharacterPlayer* Pioneer = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	UClass* FP = LoadClass<UFGItemDescriptor>(nullptr, TDBeute::Kampfdaten.Item);
	return Pioneer && Pioneer->GetInventory() && FP ? Pioneer->GetInventory()->GetNumItems(FP) : 0;
}

void UTDLaufProtokoll::WellenStart(int32 Welle)
{
	mBauphase = mWellenEndeZeit >= 0.0 ? static_cast<float>(FPlatformTime::Seconds() - mWellenEndeZeit) : -1.f;
	mWelleLaeuft = true;
	mLetzteWelle = Welle;
	SammleBeute();   // Rest der letzten Welle sofort an den Kern
	UE_LOG(LogFicsitTD, Log, TEXT("Lauf: Welle %d startet, Bauphase %.0f s"), Welle, mBauphase);
}

void UTDLaufProtokoll::SpeichereSpielstand(const FString& Name) const
{
	if (TDBalance::Wert(TEXT("Lauf.Spielstaende"), 1.f) < 0.5f)
	{
		return;
	}
	if (UFGSaveSession* Speicher = UFGSaveSession::Get(GetWorld()))
	{
		Speicher->SaveGame(Name, FOnSaveGameComplete(), nullptr);
		UE_LOG(LogFicsitTD, Log, TEXT("Lauf: Spielstand %s angelegt"), *Name);
	}
}

void UTDLaufProtokoll::Kauf(const FString& Key, int32 EP)
{
	mKaeufe.FindOrAdd(Key) += EP;
}

void UTDLaufProtokoll::Eingriff(const FString& Was)
{
	mEingriffe.Add(Was);
	UE_LOG(LogFicsitTD, Log, TEXT("Lauf: Eingriff in Welle %d: %s"), mLetzteWelle, *Was);
	if (bBalancing)
	{
		Melde(FString::Printf(TEXT("Balancing-Lauf: „%s“ wird als Eingriff in Welle %d vermerkt."), *Was, mLetzteWelle));
	}
}

FString UTDLaufProtokoll::Notiz(int32 Note, const FString& Text)
{
	Note = FMath::Clamp(Note, 1, 5);
	SchreibeZeile(TEXT("Notizen.csv"), TEXT("Zeit;Welle;WaehrendWelle;Note;Text;Balancing"),
		FString::Printf(TEXT("%s;%d;%d;%d;%s;%d"), *FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), mLetzteWelle, mWelleLaeuft ? 1 : 0, Note,
			*Text.Replace(TEXT(";"), TEXT(",")), bBalancing ? 1 : 0));
	static const TCHAR* Worte[] = { TEXT("zu leicht"), TEXT("eher leicht"), TEXT("passend"), TEXT("eher schwer"), TEXT("zu schwer") };
	return FString::Printf(TEXT("Notiz zu Welle %d: %d (%s)%s"), mLetzteWelle, Note, Worte[Note - 1], Text.IsEmpty() ? TEXT("") : *(TEXT(" - ") + Text));
}

void UTDLaufProtokoll::WellenEnde(int32 Welle, int32 EPVerdient, int32 FPVerdient, float KernAnteil)
{
	mWelleLaeuft = false;
	mLetzteWelle = Welle;
	mWellenEndeZeit = FPlatformTime::Seconds();
	UWorld* World = GetWorld();
	UTDEinsatzSubsystem* Einsatz = World->GetSubsystem<UTDEinsatzSubsystem>();
	ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	const int32 Konto = Kern ? Kern->mEinsatzPunkte : 0;
	const int32 FPJetzt = FPImInventar() + (Kern ? Kern->mFPOffen : 0);
	// Ausgaben seit dem letzten Wellenende: Konto vorher + verdient - Konto jetzt
	const int32 EPAus = mKontoNachLetzter >= 0 ? mKontoNachLetzter + EPVerdient - Konto : -1;
	const int32 FPAus = mFPNachLetzter >= 0 ? mFPNachLetzter + FPVerdient - FPJetzt : -1;
	mKontoNachLetzter = Konto;
	mFPNachLetzter = FPJetzt;

	// neu erforschte FicsitTD-Knoten
	TArray<FString> Neu;
	if (const AFGSchematicManager* Schemata = AFGSchematicManager::Get(World))
	{
		TArray<TSubclassOf<UFGSchematic>> Gekauft;
		Schemata->GetAllPurchasedSchematics(Gekauft);
		for (const TSubclassOf<UFGSchematic>& S : Gekauft)
		{
			if (S && S->GetPathName().StartsWith(TEXT("/FicsitTD/Research/")))
			{
				const FString Name = UFGSchematic::GetSchematicDisplayName(S).ToString();
				if (!mErforscht.Contains(Name))
				{
					mErforscht.Add(Name);
					if (mErforschtGelesen)
					{
						Neu.Add(Name);
					}
				}
			}
		}
	}
	const bool bErsteZeile = !mErforschtGelesen;
	mErforschtGelesen = true;

	// Munition: Schuesse dieser Welle und Vorrat je Turmtyp
	TMap<FString, FIntPoint> Tuerme;   // Typ -> (Schuesse, Vorrat)
	for (TActorIterator<ATDTurretBase> It(World); It; ++It)
	{
		FIntPoint& T = Tuerme.FindOrAdd(UTDEinsatzSubsystem::Schluessel(It->GetClass()));
		T.X += It->GetBilanz().Schuesse + It->GetShotsFired();
		T.Y += It->GetAmmo();
	}
	TArray<FString> Munition;
	for (const TPair<FString, FIntPoint>& T : Tuerme)
	{
		Munition.Add(FString::Printf(TEXT("%s %d/%d"), *T.Key, T.Value.X, T.Value.Y));
	}

	// Strom: alle Netze mit Kraftwerk
	TSet<UFGPowerCircuit*> Netze;
	for (TActorIterator<AFGBuildableGenerator> It(World); It; ++It)
	{
		if (UFGPowerInfoComponent* Info = It->GetPowerInfo())
		{
			if (UFGPowerCircuit* Netz = Info->GetPowerCircuit())
			{
				Netze.Add(Netz);
			}
		}
	}
	float Kapazitaet = 0.f, Verbrauch = 0.f, Spitze = 0.f;
	for (UFGPowerCircuit* Netz : Netze)
	{
		FPowerCircuitStats Stat;
		Netz->GetStats(Stat);
		Kapazitaet += Stat.PowerProductionCapacity;
		Verbrauch += Stat.PowerConsumed;
		Spitze += Stat.MaximumPowerConsumption;
	}

	// Gebaeude je Kontingent
	TArray<FString> Gebaeude;
	if (Einsatz)
	{
		for (const FTDKontingentZeile& Z : Einsatz->KontingentZeilen())
		{
			if (Z.Belegt > 0)
			{
				Gebaeude.Add(FString::Printf(TEXT("%s %d"), *Z.Key, Z.Belegt));
			}
		}
	}
	TArray<FString> Kaeufe, Beute;
	for (const TPair<FString, int32>& K : mKaeufe) Kaeufe.Add(FString::Printf(TEXT("%s %d"), *K.Key, K.Value));
	for (const TPair<FString, int32>& B : mBeuteWelle) Beute.Add(FString::Printf(TEXT("%s %d"), *B.Key, B.Value));
	const UTDWaveSubsystem* Waves = World->GetSubsystem<UTDWaveSubsystem>();

	SchreibeZeile(TEXT("Wirtschaft.csv"),
		TEXT("Zeit;Stufe;Welle;Balancing;Bauphase_s;Kern_Proz;EP_verdient;EP_ausgegeben;EP_Konto;Kaeufe_EP;FP_verdient;FP_ausgegeben;FP_offen;Erforscht;Beute;Munition_Schuss_Vorrat;Strom_Kapazitaet_MW;Strom_Verbrauch_MW;Strom_Spitze_MW;Gebaeude;Eingriffe"),
		FString::Printf(TEXT("%s;%d;%d;%d;%.0f;%.1f;%d;%d;%d;%s;%d;%d;%d;%s;%s;%s;%.1f;%.1f;%.1f;%s;%s"),
			*FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")), Waves ? Waves->GetStufeIndex() : -1, Welle, bBalancing ? 1 : 0, mBauphase, KernAnteil,
			EPVerdient, bErsteZeile ? -1 : EPAus, Konto, *FString::Join(Kaeufe, TEXT(", ")),
			FPVerdient, bErsteZeile ? -1 : FPAus, FPJetzt, *FString::Join(Neu, TEXT(", ")), *FString::Join(Beute, TEXT(", ")),
			*FString::Join(Munition, TEXT(", ")), Kapazitaet, Verbrauch, Spitze, *FString::Join(Gebaeude, TEXT(", ")), *FString::Join(mEingriffe, TEXT(", "))));
	UE_LOG(LogFicsitTD, Log, TEXT("Wirtschaft Welle %d: Bauphase %.0f s, EP +%d -%d = %d, FP +%d -%d (%d da), erforscht [%s], Beute [%s], Munition [%s], Strom %.0f/%.0f MW (Spitze %.0f), Eingriffe [%s]"),
		Welle, mBauphase, EPVerdient, EPAus, Konto, FPVerdient, FPAus, FPJetzt, *FString::Join(Neu, TEXT(", ")), *FString::Join(Beute, TEXT(", ")),
		*FString::Join(Munition, TEXT(", ")), Verbrauch, Kapazitaet, Spitze, *FString::Join(mEingriffe, TEXT(", ")));
	mKaeufe.Reset();
	mEingriffe.Reset();
	mBeuteWelle.Reset();

	// Liegengebliebene Beute nach einer Weile an den Kern
	World->GetTimerManager().SetTimer(mSammelTimer, this, &UTDLaufProtokoll::SammleBeute, TDBalance::Wert(TEXT("Loot.EinsammelnNachSekunden"), 60.f), false);

	// Balancing-Lauf: am Ende jeder Phase einen eigenen Spielstand
	if (bBalancing && Welle % 10 == 0)
	{
		if (UFGSaveSession* Speicher = UFGSaveSession::Get(World))
		{
			const FString Name = FString::Printf(TEXT("FicsitTD_Balancing_W%02d"), Welle);
			Speicher->SaveGame(Name, FOnSaveGameComplete(), nullptr);
			Melde(FString::Printf(TEXT("Balancing-Lauf: Phase geschafft, Spielstand „%s“ angelegt."), *Name));
		}
	}
}

void UTDLaufProtokoll::Beute(AFGCreature* Kreatur)
{
	if (UTDLaufProtokoll* P = Kreatur ? Get(Kreatur) : nullptr)
	{
		P->WirfBeute(Kreatur);
	}
}

void UTDLaufProtokoll::WirfBeute(AFGCreature* Kreatur)
{
	if (TDBalance::Wert(TEXT("Loot.An"), 1.f) < 0.5f)
	{
		return;
	}
	const bool bBoss = Kreatur->Tags.Contains(TEXT("FicsitTDBoss"));
	const bool bElite = Kreatur->Tags.Contains(TEXT("FicsitTDElite"));
	const TCHAR* Stufe = bBoss ? TEXT("Boss") : bElite ? TEXT("Elite") : TEXT("Normal");
	auto Chance = [Stufe](const TCHAR* Art, float Normal, float Elite, float Boss)
	{
		const float Std = FCString::Strcmp(Stufe, TEXT("Boss")) == 0 ? Boss : FCString::Strcmp(Stufe, TEXT("Elite")) == 0 ? Elite : Normal;
		return TDBalance::Wert(FString::Printf(TEXT("Loot.%s.%s"), Art, Stufe), Std);
	};
	TArray<FInventoryStack> Stapel;
	auto Nimm = [&Stapel, this](const TDBeute::FEintrag& E, int32 Faktor)
	{
		if (UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, E.Item))
		{
			Stapel.Add(FInventoryStack(E.Menge * Faktor, Klasse));
			mBeuteWelle.FindOrAdd(E.Art) += 1;
		}
	};
	const int32 Faktor = bBoss ? 3 : 1;
	if (FMath::FRand() < Chance(TEXT("Kampfdaten"), 0.02f, 0.15f, 1.f))
	{
		Nimm(TDBeute::Kampfdaten, Faktor);
	}
	// Einsatzpunkte statt Bauteile: direkt aufs Konto (Loot.EP.<Stufe> = Chance, Loot.EPMenge.<Stufe> = Punkte)
	if (FMath::FRand() < Chance(TEXT("EP"), 0.03f, 0.5f, 1.f))
	{
		const int32 Punkte = FMath::RoundToInt(Chance(TEXT("EPMenge"), 1.f, 2.f, 10.f));
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->GibEP(Punkte, TEXT("Beute"));
			mBeuteWelle.FindOrAdd(TEXT("EP")) += Punkte;
		}
	}
	// Munition nur fuer Tuerme, die stehen - und zwar die Sorte, die sie gerade verschiessen (feste Items, kein Treibstoff)
	if (FMath::FRand() < Chance(TEXT("Munition"), 0.08f, 0.4f, 1.f))
	{
		TArray<UClass*> Sorten;
		for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
		{
			const TSubclassOf<UFGRecipe> Rezept = It->GetCurrentRecipe();
			const TArray<FItemAmount> Zutaten = Rezept ? UFGRecipe::GetIngredients(*It, Rezept) : TArray<FItemAmount>();
			UClass* Sorte = Zutaten.Num() > 0 ? Zutaten[0].ItemClass.Get() : nullptr;
			if (Sorte && UFGItemDescriptor::GetForm(Sorte) == EResourceForm::RF_SOLID)
			{
				Sorten.AddUnique(Sorte);
			}
		}
		if (Sorten.Num() > 0)
		{
			UClass* Sorte = Sorten[FMath::RandRange(0, Sorten.Num() - 1)];
			const int32 Menge = FMath::Clamp(UFGItemDescriptor::GetStackSize(Sorte) / 5, 5, 30) * Faktor;
			Stapel.Add(FInventoryStack(Menge, Sorte));
			mBeuteWelle.FindOrAdd(TEXT("Munition")) += 1;
		}
	}
	// 0.73 (Konzept-Spezialisierung 7): Energiesplitter zum Uebertakten einzelner Tuerme, sicher vom Boss, selten von Elite
	if (FMath::FRand() < Chance(TEXT("Splitter"), 0.f, 0.05f, 1.f))
	{
		if (UClass* Splitter = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Environment/Crystal/Desc_CrystalShard.Desc_CrystalShard_C")))
		{
			const int32 Menge = FMath::Max(1, FMath::RoundToInt(Chance(TEXT("SplitterMenge"), 1.f, 1.f, 2.f)));
			Stapel.Add(FInventoryStack(Menge, Splitter));
			mBeuteWelle.FindOrAdd(TEXT("Energiesplitter")) += Menge;
		}
	}
	const FVector Ort = Kreatur->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	for (const FInventoryStack& S : Stapel)
	{
		if (AFGItemPickup_Spawnable* Drop = AFGItemPickup_Spawnable::CreateItemDrop(GetWorld(), S, Ort, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f)))
		{
			mBeuteLiegt.Add(Drop);
		}
	}
	if (Stapel.Num() > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Beute: %s %s laesst %d Stapel fallen"), Stufe, *Kreatur->GetClass()->GetName(), Stapel.Num());
	}
}

void UTDLaufProtokoll::SammleBeute()
{
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	int32 Gebracht = 0;
	for (const TWeakObjectPtr<AFGItemPickup_Spawnable>& Zeiger : mBeuteLiegt)
	{
		AFGItemPickup_Spawnable* Drop = Zeiger.Get();
		if (!Drop || Drop->IsPickedUp())
		{
			continue;
		}
		const FInventoryStack S = Drop->GetPickupItems();
		if (Kern && S.Item.GetItemClass())
		{
			Kern->mMaterialOffen.FindOrAdd(S.Item.GetItemClass()->GetPathName()) += S.NumItems;
			++Gebracht;
		}
		Drop->Destroy();
	}
	mBeuteLiegt.Reset();
	if (Gebracht > 0)
	{
		Melde(FString::Printf(TEXT("%d liegengebliebene Beutestücke sind zum Kern gebracht worden (E am Kern holt sie)."), Gebracht));
	}
}
