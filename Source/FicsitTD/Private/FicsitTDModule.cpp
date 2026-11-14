#include "FicsitTDModule.h"
#include "TDPionier.h"
#include "AI/FGCreatureController.h"
#include "Patching/NativeHookManager.h"
#include "TDDecoy.h"
#include "Creature/FGCreature.h"
#include "Creature/FGCreatureMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TDLeistung.h"
#include "FGHealthComponent.h"
#include "TDGegnerHirn.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "FGDotReceiverComponent.h"
#include "FGGasPillar.h"
#include "FGGasPillarCloud.h"
#include "FGSporeFlower.h"
#include "Hologram/FGHologram.h"
#include "TDEinsatz.h"
#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "FGResearchManager.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGChatManager.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Buildables/FGBuildable.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Command/ChatCommandLibrary.h"
#include "TDLaufProtokoll.h"
#include "FGSaveSession.h"

DEFINE_LOG_CATEGORY(LogFicsitTD);

int32 FFicsitTDModule::AbschaltenVerhindert = 0;

static TMap<const void*, TWeakObjectPtr<AActor>>& GemerkteKreaturen()
{
	static TMap<const void*, TWeakObjectPtr<AActor>> Karte;
	return Karte;
}

void FFicsitTDModule::MerkeWellenKreatur(AActor* Kreatur)
{
	TMap<const void*, TWeakObjectPtr<AActor>>& Karte = GemerkteKreaturen();
	if (Karte.Num() > 400)
	{
		for (auto It = Karte.CreateIterator(); It; ++It)
		{
			if (!It->Value.IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	if (Kreatur)
	{
		Karte.Add(Kreatur, Kreatur);
		// 0.9.6 (Gesamtreview 12.11.: Interface-Falle, Adjustment 0x668): der ShouldSave-Hook bekommt den Zeiger auf die
		// IFGSaveInterface-Basis, nicht auf die Kreatur - deshalb auch diese Adresse merken
		if (const AFGCreature* K = Cast<AFGCreature>(Kreatur))
		{
			Karte.Add(static_cast<const IFGSaveInterface*>(K), Kreatur);
		}
	}
}

bool FFicsitTDModule::IstGemerkteWellenKreatur(const void* Adresse)
{
	const TWeakObjectPtr<AActor>* Eintrag = Adresse ? GemerkteKreaturen().Find(Adresse) : nullptr;
	if (!Eintrag || !Eintrag->IsValid())
	{
		return false;
	}
	const AActor* A = Eintrag->Get();
	const AFGCreature* K = Cast<AFGCreature>(A);
	return static_cast<const void*>(A) == Adresse || (K && static_cast<const void*>(static_cast<const IFGSaveInterface*>(K)) == Adresse);
}

/** Abzweigungen im MAM (Einsatz-Konzept 5, Marius: endgueltig je Durchlauf) und Voraussetzungen: aus Scripts/create_research.py */
#include "FGRecipeManager.h"
#include "FGUnlockSubsystem.h"
#include "FGRecipe.h"
#include "TDForschung.h"
#include "TDSprache.h"
#include "TDForschungVoraussetzungen.inl"

void TDForschung::GibStilleRezepte(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	AFGSchematicManager* Schemata = World ? AFGSchematicManager::Get(World) : nullptr;
	AFGRecipeManager* Rezepte = World ? AFGRecipeManager::Get(World) : nullptr;
	if (!Schemata || !Rezepte || World->GetNetMode() == NM_Client)
	{
		return;
	}
	int32 Neu = 0;
	for (const TPair<const TCHAR*, const TCHAR*>& Paar : TDForschungStilleRezepte)
	{
		UClass* Knoten = LoadClass<UFGSchematic>(nullptr, Paar.Key);
		UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Paar.Value);
		if (Knoten && Rezept && Schemata->IsSchematicPurchased(Knoten) && !Rezepte->IsRecipeAvailable(Rezept))
		{
			Rezepte->AddAvailableRecipe(Rezept);
			++Neu;
		}
	}
	// 0.75 (Selbsttest: Turm nahm keine Energiesplitter an, "Splitter erlaubt 0"): Uebertakten ist im Spiel an die MAM-Forschung
	// der Energieschnecken gebunden, die es im Einsatz nicht gibt -> im TD-Spiel (mind. ein FicsitTD-Knoten erforscht) freischalten
	AFGUnlockSubsystem* Freischaltung = AFGUnlockSubsystem::Get(World);
	if (Freischaltung && !Freischaltung->GetIsBuildingOverclockUnlocked())
	{
		TArray<TSubclassOf<UFGSchematic>> Gekauft;
		Schemata->GetAllPurchasedSchematics(Gekauft);
		if (Gekauft.ContainsByPredicate([](const TSubclassOf<UFGSchematic>& S) { return S && S->GetPathName().StartsWith(TEXT("/FicsitTD/Research/")); }))
		{
			Freischaltung->UnlockBuildOverclock();
			UE_LOG(LogFicsitTD, Log, TEXT("Uebertakten freigeschaltet (Energiesplitter fuer Tuerme)"));
		}
	}
	// 0.72.1 (Marius: "was soll die Panzersperre im Spiel?"): aus alten Spielstaenden entfernen, gebaute bleiben stehen
	if (UClass* Sperre = LoadClass<UFGRecipe>(nullptr, TEXT("/FicsitTD/Buildings/Barrier/Recipe_TD_Barrier.Recipe_TD_Barrier_C")))
	{
		if (Rezepte->IsRecipeAvailable(Sperre))
		{
			TArray<TSubclassOf<UFGRecipe>> Weg;
			Weg.Add(Sperre);
			Rezepte->RemoveAvailableRecipes(Weg);
			UE_LOG(LogFicsitTD, Log, TEXT("Panzersperre aus den verfuegbaren Rezepten entfernt"));
		}
	}
	if (Neu > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Turm-Aktionen freigeschaltet: %d Rezepte"), Neu);
	}
}

/** MAM-Sperre mit Grund im Chat, hoechstens alle 10 s je Knoten (das MAM selbst zeigt nur "Fehlende Materialien", Test 0.28.0) */
static void MeldeForschungGesperrt(const AFGResearchManager* Forschung, UClass* Schematic, const FString& Text)
{
	static TMap<FString, double> Gemeldet;
	const double Jetzt = FPlatformTime::Seconds();
	double& Zuletzt = Gemeldet.FindOrAdd(Schematic->GetPathName());
	AFGChatManager* Chat = AFGChatManager::Get(Forschung->GetWorld());
	if (Chat && Jetzt - Zuletzt > 10.0)
	{
		Zuletzt = Jetzt;
		FChatMessageStruct Nachricht;
		Nachricht.MessageText = FText::FromString(Text);
		Nachricht.MessageType = EFGChatMessageType::CMT_CustomMessage;
		Nachricht.MessageSender = FText::FromString(TEXT("MAM"));
		Nachricht.MessageSenderColor = FLinearColor(1.f, 0.55f, 0.1f);
		Chat->BroadcastChatMessage(Nachricht);
		UE_LOG(LogFicsitTD, Log, TEXT("MAM gesperrt: %s"), *Text);
	}
}

/** 0.10.0 (Marius 14.11.): Im Einsatz sind Festplatten die Waehrung der FicsitTD-Forschung. Die Festplatten-Forschung des Spiels
 *  (Alternativrezepte) ist dann gesperrt: alles ausserhalb von /FicsitTD/, das Festplatten kostet. Im Normalspiel bleibt sie frei. */
static bool FremdeFestplattenForschung(const AFGResearchManager* Forschung, UClass* Schematic)
{
	if (!Forschung || !Schematic || Schematic->GetPathName().StartsWith(TEXT("/FicsitTD/")))
	{
		return false;
	}
	const UTDEinsatzSubsystem* Einsatz = Forschung->GetWorld() ? Forschung->GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	UClass* Festplatte = UTDEinsatzSubsystem::FPKlasse();
	if (!Einsatz || !Einsatz->IstAktiv() || !Festplatte)
	{
		return false;
	}
	for (const FItemAmount& Kosten : UFGSchematic::GetCost(Schematic))
	{
		if (Kosten.ItemClass == Festplatte)
		{
			MeldeForschungGesperrt(Forschung, Schematic, TDF(TEXT("%s ist im Einsatz gesperrt: Festplatten sind hier für die FICSIT-Abwehr reserviert."),
				TEXT("%s is locked during an operation: Hard Drives are reserved for FICSIT Defense research here."),
				*TDForschung::Basisname(Schematic)));
			return true;
		}
	}
	return false;
}

/** Ist die andere Seite der Abzweigung schon erforscht oder in Arbeit? */
static bool AndereSeiteGewaehlt(const AFGResearchManager* Forschung, UClass* Schematic)
{
	if (!Forschung || !Schematic)
	{
		return false;
	}
	const FString Pfad = Schematic->GetPathName();
	for (const TPair<const TCHAR*, const TCHAR*>& Paar : TDForschungAbzweigungen)
	{
		const TCHAR* Andere = Pfad == Paar.Key ? Paar.Value : Pfad == Paar.Value ? Paar.Key : nullptr;
		UClass* AndereKlasse = Andere ? LoadClass<UFGSchematic>(nullptr, Andere) : nullptr;
		const AFGSchematicManager* Schemata = AFGSchematicManager::Get(Forschung->GetWorld());
		if (AndereKlasse && (Forschung->IsResearchComplete(AndereKlasse) || Forschung->IsResearchBeingConducted(AndereKlasse)
			|| (Schemata && Schemata->IsSchematicPurchased(AndereKlasse))))
		{
			// Test 0.53: Puls-Nobelisk liess sich ohne jede Rueckmeldung nicht erforschen
			MeldeForschungGesperrt(Forschung, Schematic, TDF(TEXT("%s ist gesperrt: Abzweigung, in diesem Einsatz ist schon %s gewählt."),
				TEXT("%s is locked: branch, %s is already chosen in this operation."),
				*TDForschung::Basisname(Schematic), *TDForschung::Basisname(AndereKlasse)));
			return true;
		}
	}
	return false;
}

/** 0.9.6: waehrend einer Welle im Einsatz nicht speichern (Beute-Wiederholung); abschaltbar mit Einsatz.SpeichernInWelle 1 */
static bool SpeichernGesperrt(const UObject* Kontext, bool bMelden)
{
	UWorld* World = Kontext ? Kontext->GetWorld() : nullptr;
	const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	if (!Waves || !Einsatz || !Waves->LaeuftWelle() || !Einsatz->IstAktiv() || TDBalance::Wert(TEXT("Einsatz.SpeichernInWelle"), 0.f) > 0.f)
	{
		return false;
	}
	if (bMelden)
	{
		Einsatz->Melde(TDT(TEXT("Während einer Welle kann nicht gespeichert werden. Nach der Welle speichert der Einsatz selbst."),
			TEXT("You cannot save during a wave. The operation saves on its own after the wave.")));
	}
	return true;
}

/** 0.9.6 (Gesamtreview 12.11., Marius: HUB sperren): im Einsatz keine Meilensteine - Technik kommt nur ueber das MAM.
 *  Tutorial-Stufen des HUB bleiben offen (der Einsatz baut darauf auf). Abschaltbar mit Einsatz.HubGesperrt 0. */
static bool MeilensteinGesperrt(const UObject* Kontext, TSubclassOf<UFGSchematic> Schematic, bool bMelden)
{
	if (!Kontext || !Schematic || UFGSchematic::GetType(Schematic) != ESchematicType::EST_Milestone || TDBalance::Wert(TEXT("Einsatz.HubGesperrt"), 1.f) <= 0.f)
	{
		return false;
	}
	UWorld* World = Kontext->GetWorld();
	const UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	if (!Einsatz || !Einsatz->IstAktiv() || Einsatz->IstFrei())
	{
		return false;
	}
	if (bMelden)
	{
		Einsatz->Melde(TDT(TEXT("Im Einsatz sind Meilensteine am HUB gesperrt - neue Technik gibt es im MAM über die FICSIT-Abwehr-Bäume."),
			TEXT("Milestones at the HUB are locked during an operation - new technology comes from the FICSIT Defense trees in the MAM.")));
	}
	return true;
}

/** Freigabe je Phase (Wellen-Progression 7): Knoten erst ab Welle N (Forschung.<Knoten>.AbWelle), gezaehlt wird die naechste Welle */
static bool WelleZuFrueh(const AFGResearchManager* Forschung, UClass* Schematic)
{
	if (!Forschung || !Schematic)
	{
		return false;
	}
	const FString Pfad = Schematic->GetPathName();
	for (const FTDForschungAbWelle& E : TDForschungAbWelle)
	{
		if (Pfad != E.Pfad)
		{
			continue;
		}
		const int32 Ab = TDBalance::Ganz(FString::Printf(TEXT("Forschung.%s.AbWelle"), E.Schluessel), E.Welle);
		const UTDWaveSubsystem* Waves = Forschung->GetWorld() ? Forschung->GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
		const int32 Naechste = Waves ? Waves->GetWelle() + 1 : 1;
		if (Naechste < Ab)
		{
			MeldeForschungGesperrt(Forschung, Schematic, TDF(TEXT("%s ist erst ab Welle %d erforschbar (nächste Welle: %d)."),
				TEXT("%s can only be researched from wave %d (next wave: %d)."),
				*TDForschung::Basisname(Schematic), Ab, Naechste));
			return true;
		}
	}
	return false;
}

/** Voraussetzung aus einem anderen MAM-Baum noch nicht erforscht? (Munitions-Konzept 7, Vorschlag A) */
static bool VoraussetzungFehlt(const AFGResearchManager* Forschung, UClass* Schematic)
{
	if (!Forschung || !Schematic)
	{
		return false;
	}
	const AFGSchematicManager* Schemata = AFGSchematicManager::Get(Forschung->GetWorld());
	const FString Pfad = Schematic->GetPathName();
	for (const TPair<const TCHAR*, const TCHAR*>& Paar : TDForschungVoraussetzungen)
	{
		if (Pfad != Paar.Key)
		{
			continue;
		}
		UClass* Noetig = LoadClass<UFGSchematic>(nullptr, Paar.Value);
		if (Noetig && !Forschung->IsResearchComplete(Noetig) && !(Schemata && Schemata->IsSchematicPurchased(Noetig)))
		{
			MeldeForschungGesperrt(Forschung, Schematic, TDF(TEXT("%s ist noch gesperrt: zuerst %s im MAM erforschen."),
				TEXT("%s is still locked: research %s in the MAM first."),
				*TDForschung::Basisname(Schematic), *TDForschung::Basisname(Noetig)));
			return true;
		}
	}
	return false;
}

/** Balancing-Lauf: Befehle, die Material, Treibstoff oder Freischaltungen schenken, als Eingriff vermerken
 *  (eigene Funktion: die Kommas der Liste vertragen sich nicht mit dem Hook-Makro) */
static void PruefeEingriff(const AChatCommandSubsystem* Self, const FString& Zeile)
{
	static const TCHAR* Eingriffe[] = { TEXT("gib "), TEXT("td fuellen"), TEXT("td füllen"), TEXT("td freischalten"), TEXT("td einsatz frei"), TEXT("td kern tempo") };
	const FString Befehl = Zeile.TrimStartAndEnd().ToLower().Replace(TEXT("/"), TEXT(""));
	for (const TCHAR* E : Eingriffe)
	{
		if (Befehl.StartsWith(E))
		{
			if (UTDLaufProtokoll* Lauf = Self ? UTDLaufProtokoll::Get(Self) : nullptr)
			{
				Lauf->Eingriff(TEXT("/") + Befehl);
			}
			return;
		}
	}
}

/** Wellengegner (Tag FicsitTD)? */
static bool IstWellenKreatur(const AActor* Actor)
{
	return Actor && Actor->IsA<AFGCreature>() && Actor->Tags.Contains(TEXT("FicsitTD"));
}

/** Schaden stammt von einer Kreatur (direkt, ueber ihr Geschoss oder ihren Controller) */
static bool VonKreatur(const AController* Ausloeser, const AActor* Verursacher)
{
	if (Ausloeser && Cast<AFGCreature>(Ausloeser->GetPawn()))
	{
		return true;
	}
	if (!Verursacher)
	{
		return false;
	}
	return Verursacher->IsA<AFGCreature>() || Cast<AFGCreature>(Verursacher->GetInstigator()) || Cast<AFGCreature>(Verursacher->GetOwner());
}

/** Gestoppten Neustart der Spiel-KI zaehlen und gelegentlich melden */
static void MeldeKIStart(const TCHAR* Wo)
{
	static int32 Gestoppt = 0;
	if (++Gestoppt <= 3 || Gestoppt % 100 == 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Spiel-KI eines Wellengegners wollte starten (%s), unterbunden: %d"), Wo, Gestoppt);
	}
}

/** Verworfenen Kreatur-Schaden zaehlen und gelegentlich melden */
static void MeldeKreaturSchaden()
{
	static int32 Verworfen = 0;
	if (++Verworfen == 1 || Verworfen % 100 == 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Kreatur-Schaden an Wellengegnern verworfen: %d"), Verworfen);
	}
}

/** Koeder eines noch stehenden Verteidigungsziels? */
static bool IstAktiverKoeder(const AActor* Actor)
{
	const ATDDecoy* Decoy = Cast<ATDDecoy>(Actor);
	return Decoy && Decoy->GetActiveTarget() != nullptr;
}

void FFicsitTDModule::StartupModule()
{
	UE_LOG(LogFicsitTD, Log, TEXT("FICSIT Tower Defense geladen"));

#if !WITH_EDITOR
	// Die Kreatur-KI soll Koeder wie Spieler behandeln: gueltiges Ziel, Feindschaft, Bedrohung
	AFGCreatureController* Sample = GetMutableDefault<AFGCreatureController>();

	// Test 0.43 (Marius): Gegner griffen sich gegenseitig an - Wellengegner sind fuer keine Kreatur Ziel, Feind oder Bedrohung
	SUBSCRIBE_METHOD_VIRTUAL(AFGCreatureController::IsValidTarget, Sample, [](auto& Scope, const AFGCreatureController* Self, const AActor* Actor)
	{
		if (IstAktiverKoeder(Actor))
		{
			Scope.Override(true);
		}
		else if (IstWellenKreatur(Actor) || IstWellenKreatur(Self ? Self->GetPawn() : nullptr))
		{
			Scope.Override(false);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCreatureController::CanBeHostileTowards, Sample, [](auto& Scope, const AFGCreatureController* Self, const AActor* Actor)
	{
		if (IstAktiverKoeder(Actor))
		{
			Scope.Override(true);
		}
		else if (IstWellenKreatur(Actor) || IstWellenKreatur(Self ? Self->GetPawn() : nullptr))
		{
			Scope.Override(false);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCreatureController::IsActorConsideredThreat_Implementation, Sample, [](auto& Scope, const AFGCreatureController* Self, const AActor* Actor)
	{
		if (IstAktiverKoeder(Actor))
		{
			Scope.Override(true);
		}
		else if (IstWellenKreatur(Actor))
		{
			Scope.Override(false);
		}
	});
	// Test 0.44 (Marius): Wellengegner liefen umher und kaempften - die Spiel-KI (Verhaltensbaum mit Familien-Feindschaft) startete
	// zwischen den sekuendlichen Stillegen wieder. Fuer unsere Controller wird jeder Start unterbunden.
	SUBSCRIBE_METHOD_VIRTUAL(AAIController::RunBehaviorTree, GetMutableDefault<AAIController>(), [](auto& Scope, AAIController* Self, UBehaviorTree* Baum)
	{
		if (Self && Self->IsA<ATDGegnerController>())
		{
			MeldeKIStart(TEXT("RunBehaviorTree"));
			Scope.Override(false);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(UBehaviorTreeComponent::RestartLogic, GetMutableDefault<UBehaviorTreeComponent>(), [](auto& Scope, UBehaviorTreeComponent* Self)
	{
		if (Self && Cast<ATDGegnerController>(Self->GetOwner()))
		{
			MeldeKIStart(TEXT("RestartLogic"));
			Scope.Cancel();
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(UBehaviorTreeComponent::ResumeLogic, GetMutableDefault<UBehaviorTreeComponent>(), [](auto& Scope, UBehaviorTreeComponent* Self, const FString& Grund)
	{
		if (Self && Cast<ATDGegnerController>(Self->GetOwner()))
		{
			MeldeKIStart(TEXT("ResumeLogic"));
			Scope.Override(EAILogicResuming::Continue);
		}
	});
	// Test 0.45.1: Wellengegner nicht speichern - nach dem Laden kamen sie ohne Tag als Wildtiere mit Spiel-KI zurueck
	SUBSCRIBE_METHOD_VIRTUAL(AFGCreature::ShouldSave_Implementation, GetMutableDefault<AFGCreature>(), [](auto& Scope, const AFGCreature* Self)
	{
		// nur Adressvergleich, kein Zugriff auf Self (siehe MerkeWellenKreatur)
		if (FFicsitTDModule::IstGemerkteWellenKreatur(Self))
		{
			Scope.Override(false);
			// 0.9.6: Nachweis, dass der Hook greift (vorher nie, siehe MerkeWellenKreatur)
			static int32 Verhindert = 0;
			if (++Verhindert == 1 || Verhindert % 50 == 0)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Speichern: Wellengegner nicht in den Spielstand genommen (%d. Mal)"), Verhindert);
			}
		}
	});
	// ... und Schaden von Kreaturen (Bisse, Spucke, Explosionen) an Wellengegnern wird verworfen
	UFGHealthComponent* Gesundheit = GetMutableDefault<UFGHealthComponent>();
	SUBSCRIBE_METHOD_VIRTUAL(UFGHealthComponent::TakeDamage, Gesundheit,
		[](auto& Scope, UFGHealthComponent* Self, AActor* Getroffen, float Menge, const UDamageType* Art, AController* Ausloeser, AActor* Verursacher)
	{
		if (IstWellenKreatur(Getroffen) && VonKreatur(Ausloeser, Verursacher))
		{
			MeldeKreaturSchaden();
			Scope.Cancel();
		}
		// Pionier-Talente (0.58): Waffenschaden des Spielers, Ruestung gegen Kreaturen
		else if (const float F = UTDPionier::Schadensfaktor(Getroffen, Ausloeser, Verursacher, Menge, IstWellenKreatur(Getroffen), VonKreatur(Ausloeser, Verursacher)); F != 1.f)
		{
			Scope(Self, Getroffen, Menge * F, Art, Ausloeser, Verursacher);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(UFGHealthComponent::TakePointDamage, Gesundheit,
		[](auto& Scope, UFGHealthComponent* Self, AActor* Getroffen, float Menge, AController* Ausloeser, FVector Ort, UPrimitiveComponent* Teil, FName Knochen, FVector Richtung, const UDamageType* Art, AActor* Verursacher)
	{
		if (IstWellenKreatur(Getroffen) && VonKreatur(Ausloeser, Verursacher))
		{
			MeldeKreaturSchaden();
			Scope.Cancel();
		}
		else if (const float F = UTDPionier::Schadensfaktor(Getroffen, Ausloeser, Verursacher, Menge, IstWellenKreatur(Getroffen), VonKreatur(Ausloeser, Verursacher), Knochen); F != 1.f)
		{
			Scope(Self, Getroffen, Menge * F, Ausloeser, Ort, Teil, Knochen, Richtung, Art, Verursacher);
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(UFGHealthComponent::TakeRadialDamage, Gesundheit,
		[](auto& Scope, UFGHealthComponent* Self, AActor* Getroffen, float Menge, const UDamageType* Art, FVector Ort, const FHitResult& Treffer, AController* Ausloeser, AActor* Verursacher)
	{
		if (IstWellenKreatur(Getroffen) && VonKreatur(Ausloeser, Verursacher))
		{
			MeldeKreaturSchaden();
			Scope.Cancel();
		}
		else if (const float F = UTDPionier::Schadensfaktor(Getroffen, Ausloeser, Verursacher, Menge, IstWellenKreatur(Getroffen), VonKreatur(Ausloeser, Verursacher)); F != 1.f)
		{
			Scope(Self, Getroffen, Menge * F, Art, Ort, Treffer, Ausloeser, Verursacher);
		}
	});
	// T2j (Gegnertakt-Recherche 14): Ruckel-Zeit steckt in TG_PrePhysics - inklusive Tick-Zeit unserer Wellengegner je Teil messen
	static auto IstWellengegner = [](const AActor* A) { return A && A->Tags.Contains(TEXT("FicsitTD")); };
	SUBSCRIBE_METHOD_VIRTUAL(UCharacterMovementComponent::TickComponent, GetMutableDefault<UFGCreatureMovementComponent>(),
		[](auto& Scope, UCharacterMovementComponent* Self, float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
	{
		if (!Self || !IstWellengegner(Self->GetOwner()))
		{
			return;
		}
		const uint64 Start = FPlatformTime::Cycles64();
		Scope(Self, DeltaTime, TickType, TickFunction);
		TDLeistung::BewegungZyklen += FPlatformTime::Cycles64() - Start;
		++TDLeistung::BewegungTicks;
	});
	SUBSCRIBE_METHOD_VIRTUAL(USkeletalMeshComponent::TickComponent, GetMutableDefault<USkeletalMeshComponent>(),
		[](auto& Scope, USkeletalMeshComponent* Self, float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
	{
		if (!Self || !IstWellengegner(Self->GetOwner()))
		{
			return;
		}
		const uint64 Start = FPlatformTime::Cycles64();
		Scope(Self, DeltaTime, TickType, TickFunction);
		TDLeistung::SkelettZyklen += FPlatformTime::Cycles64() - Start;
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCreatureController::Tick, Sample, [](auto& Scope, AFGCreatureController* Self, float DeltaSeconds)
	{
		if (!Self || !IstWellengegner(Self->GetPawn()))
		{
			return;
		}
		const uint64 Start = FPlatformTime::Cycles64();
		Scope(Self, DeltaSeconds);
		TDLeistung::ControllerZyklen += FPlatformTime::Cycles64() - Start;
	});
	SUBSCRIBE_METHOD_VIRTUAL(AFGCreature::Tick, GetMutableDefault<AFGCreature>(), [](auto& Scope, AFGCreature* Self, float DeltaSeconds)
	{
		if (!IstWellengegner(Self))
		{
			return;
		}
		const uint64 Start = FPlatformTime::Cycles64();
		Scope(Self, DeltaSeconds);
		TDLeistung::KreaturZyklen += FPlatformTime::Cycles64() - Start;
	});
	SUBSCRIBE_METHOD_VIRTUAL(UFGCreatureMovementComponent::ResolvePenetrationImpl, GetMutableDefault<UFGCreatureMovementComponent>(),
		[](auto& Scope, UFGCreatureMovementComponent* Self, const FVector& Adjustment, const FHitResult& Hit, const FQuat& NewRotation)
	{
		if (Self && IstWellengegner(Self->GetOwner()))
		{
			++TDLeistung::Durchdringung;
		}
	});
	SUBSCRIBE_METHOD_VIRTUAL(UFGCreatureMovementComponent::OnCharacterStuckInGeometry, GetMutableDefault<UFGCreatureMovementComponent>(),
		[](auto& Scope, UFGCreatureMovementComponent* Self, const FHitResult* Hit)
	{
		if (Self && IstWellengegner(Self->GetOwner()))
		{
			++TDLeistung::Feststecken;
		}
	});
	// T2k (Gegnertakt-Recherche 15): jeder Actor- und Komponenten-Tick, inklusive Zeit je Klasse des Ziels
	static FActorTickFunction BeispielActorTick;
	static FActorComponentTickFunction BeispielKomponentenTick;
	SUBSCRIBE_METHOD_VIRTUAL(FActorTickFunction::ExecuteTick, &BeispielActorTick,
		[](auto& Scope, FActorTickFunction* Self, float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& Ereignis)
	{
		if (!TDLeistung::bTickProfil.load(std::memory_order_relaxed) || !IsInGameThread() || !Self || !Self->Target)
		{
			return;
		}
		const UClass* Klasse = Self->Target->GetClass();
		const uint64 Start = FPlatformTime::Cycles64();
		Scope(Self, DeltaTime, TickType, CurrentThread, Ereignis);
		TDLeistung::TickJeKlasse.FindOrAdd(Klasse) += FPlatformTime::Cycles64() - Start;
	});
	SUBSCRIBE_METHOD_VIRTUAL(FActorComponentTickFunction::ExecuteTick, &BeispielKomponentenTick,
		[](auto& Scope, FActorComponentTickFunction* Self, float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& Ereignis)
	{
		if (!TDLeistung::bTickProfil.load(std::memory_order_relaxed) || !IsInGameThread() || !Self || !Self->Target)
		{
			return;
		}
		const UClass* Klasse = Self->Target->GetClass();
		const uint64 Start = FPlatformTime::Cycles64();
		Scope(Self, DeltaTime, TickType, CurrentThread, Ereignis);
		TDLeistung::TickJeKlasse.FindOrAdd(Klasse) += FPlatformTime::Cycles64() - Start;
	});
	// Weg C (Test 0.17.0): das Kreatur-System schaltet Kreaturen ohne Zuschauer ab, sie froren in der Luft ein.
	// Fuer unsere Wellengegner (Tag FicsitTD) wird nur das Abschalten verhindert, sonst bleibt alles beim Spiel.
	SUBSCRIBE_METHOD(AFGCreature::SetEnabled, [](auto& Scope, AFGCreature* Self, EEnabled Enabled)
	{
		if (Enabled == E_Disabled && Self && Self->Tags.Contains(TEXT("FicsitTD")))
		{
			++FFicsitTDModule::AbschaltenVerhindert;
			Scope.Cancel();
		}
	});
	// Test 0.18.0: Hogs und Spitter starben in Gaswolken an der Front - die Balance soll nicht vom Gelaende abhaengen.
	// Gesperrt wird nur Gas aus der Umgebung; Gas-Nobelisken (Werfer und von Hand) wirken weiter.
	SUBSCRIBE_METHOD(UFGDotReceiverComponent::RegisterActiveDOT, [](auto& Scope, UFGDotReceiverComponent* Self, TSubclassOf<UFGDamageOverTime> DotClass, AActor* SourceActor)
	{
		const AActor* Besitzer = Self ? Self->GetOwner() : nullptr;
		if (Besitzer && Besitzer->Tags.Contains(TEXT("FicsitTD")) && SourceActor
			&& (SourceActor->IsA<AFGGasPillar>() || SourceActor->IsA<AFGGasPillarCloud>() || SourceActor->IsA<AFGSporeFlower>()))
		{
			Scope.Cancel();
		}
	});
	// Balancing-Lauf: Befehle, die Material, Treibstoff oder Freischaltungen schenken, als Eingriff vermerken
	SUBSCRIBE_METHOD(AChatCommandSubsystem::RunChatCommand, [](auto& Scope, AChatCommandSubsystem* Self, const FString& Zeile, UCommandSender* Sender)
	{
		PruefeEingriff(Self, Zeile);
	});
	// Einsatz: Kontingente und Bausperre waehrend der Welle als eigener Ablehnungsgrund im Hologramm
	SUBSCRIBE_METHOD_AFTER(AFGHologram::ValidatePlacementAndCost, [](AFGHologram* Self, UFGInventoryComponent* Inventar)
	{
		UWorld* World = Self ? Self->GetWorld() : nullptr;
		if (UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr)
		{
			Einsatz->PruefeHologramm(Self);
		}
	});
	// Logistik, HUB und MAM kostenlos: seit 0.27.1 ueber leere Rezeptzutaten (UTDEinsatzSubsystem::AktualisiereGratisRezepte).
	// Die frueheren Hooks auf GetNoBuildCost und GetDismantleRefund_Implementation sind entfernt: der virtuelle Hook auf die
	// Rueckerstattung bekam ueber die Interface-Verschiebung einen ungueltigen Zeiger (Absturz 0.28.3 in der Abriss-Vorschau).
	SUBSCRIBE_METHOD(AFGResearchManager::CanResearchBeInitiated, [](auto& Scope, const AFGResearchManager* Self, TSubclassOf<UFGSchematic> Schematic)
	{
		if (AndereSeiteGewaehlt(Self, Schematic) || VoraussetzungFehlt(Self, Schematic) || WelleZuFrueh(Self, Schematic)
			|| FremdeFestplattenForschung(Self, Schematic))
		{
			Scope.Override(false);
		}
	});
	// 0.9.6 (Gesamtreview 12.11., Marius: Speichern in der Welle sperren): Laden in der Welle wiederholte sie, die Beute blieb -
	// Boss-Beute liess sich so beliebig oft holen. Waehrend einer Welle kein Speichern und kein Autosave.
	SUBSCRIBE_METHOD(UFGSaveSession::SaveGame, [](auto& Scope, UFGSaveSession* Self, const FString& Name, FOnSaveGameComplete Fertig, void* Daten)
	{
		if (SpeichernGesperrt(Self, true))
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Speichern: %s waehrend der Welle abgelehnt"), *Name);
			Scope.Cancel();
		}
	});
	SUBSCRIBE_METHOD(UFGSaveSession::ShouldPerformAutoSave, [](auto& Scope, UFGSaveSession* Self)
	{
		if (SpeichernGesperrt(Self, false))
		{
			Scope.Override(false);
		}
	});
	SUBSCRIBE_METHOD(AFGSchematicManager::CanSetAsActiveSchematic, [](auto& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic)
	{
		if (MeilensteinGesperrt(Self, Schematic, false))
		{
			Scope.Override(false);
		}
	});
	SUBSCRIBE_METHOD(AFGSchematicManager::SetActiveSchematic, [](auto& Scope, AFGSchematicManager* Self, TSubclassOf<UFGSchematic> Schematic)
	{
		if (MeilensteinGesperrt(Self, Schematic, true))
		{
			Scope.Override(false);
		}
	});
	UE_LOG(LogFicsitTD, Log, TEXT("KI-Hooks fuer Koeder und Wellengegner registriert"));
#endif
}

IMPLEMENT_GAME_MODULE(FFicsitTDModule, FicsitTD);
