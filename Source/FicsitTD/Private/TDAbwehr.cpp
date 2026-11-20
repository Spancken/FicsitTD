#include "TDAbwehr.h"
#include "TDKernplatz.h"
#include "TDSpielstand.h"
#include "TDEinsatz.h"
#include "TDBalance.h"
#include "FicsitTDModule.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGRecipeManager.h"
#include "FGRecipe.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "ItemAmount.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "TDForschung.h"

/** Knoten des Baums "Industrie" (create_research.py, BAEUME[0]) und ihre Schematic-Pfade */
static const TCHAR* TDIndustrieKnoten[] = {
	TEXT("/FicsitTD/Research/Energie/Research_TD_Kohlekraft.Research_TD_Kohlekraft_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Assembler.Research_TD_Assembler_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Kupfer.Research_TD_Kupfer_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_BandMk2.Research_TD_BandMk2_C"),
	TEXT("/FicsitTD/Research/Logistik/Research_TD_Munitionspuffer.Research_TD_Munitionspuffer_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Giesserei.Research_TD_Giesserei_C"),
	TEXT("/FicsitTD/Research/Logistik/Research_TD_BandMk3.Research_TD_BandMk3_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Raffinerie.Research_TD_Raffinerie_C"),
	TEXT("/FicsitTD/Research/Energie/Research_TD_Treibstoffkraft.Research_TD_Treibstoffkraft_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Schwefel.Research_TD_Schwefel_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Quarz.Research_TD_Quarz_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Caterium.Research_TD_Caterium_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Bauxit.Research_TD_Bauxit_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Schwerindustrie.Research_TD_Schwerindustrie_C"),
	TEXT("/FicsitTD/Research/Produktion/Research_TD_Mischtechnik.Research_TD_Mischtechnik_C"),
};

static bool IstIndustrieKnoten(const FString& Pfad)
{
	for (const TCHAR* K : TDIndustrieKnoten)
	{
		if (Pfad == K)
		{
			return true;
		}
	}
	return false;
}

static TArray<FItemAmount>* SchematicKosten(UClass* Schematic)
{
	static FArrayProperty* Feld = FindFProperty<FArrayProperty>(UFGSchematic::StaticClass(), TEXT("mCost"));
	UObject* CDO = Schematic ? Schematic->GetDefaultObject() : nullptr;
	return Feld && CDO ? Feld->ContainerPtrToValuePtr<TArray<FItemAmount>>(CDO) : nullptr;
}

UTDAbwehrSubsystem* UTDAbwehrSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDAbwehrSubsystem>() : nullptr;
}

void UTDAbwehrSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(mTimer, this, &UTDAbwehrSubsystem::Pruefe, 3.f, true, 4.f);
}

void UTDAbwehrSubsystem::Deinitialize()
{
	StelleKostenWiederHer();
	Super::Deinitialize();
}

void UTDAbwehrSubsystem::Pruefe()
{
	SchalteStrahlenschutzFrei();
	if (!UTDKernplatz::IstAbwehr(this))
	{
		StelleKostenWiederHer();
		return;
	}
	TauscheKosten();
	ZieheIndustrieNach();
	if (ATDSpielstand* Stand = ATDSpielstand::Get(this))
	{
		Stand->Gleiche();
	}
}

void UTDAbwehrSubsystem::SchalteStrahlenschutzFrei()
{
	// 16.11. (Marius: Singularitaetswerfer strahlt): mit der Forschung Singularitaet kommen Hazmat-Anzug und Iod-Filter des Spiels
	// (sonst ueber den HUB, im Einsatz gesperrt). Gilt in allen Modi.
	if (mStrahlenschutz || !UTDKernplatz::ModAktiv(this) || !TDForschung::Hat(this, TEXT("Singularitaet")))
	{
		return;
	}
	mStrahlenschutz = true;
	AFGRecipeManager* Rezepte = AFGRecipeManager::Get(GetWorld());
	if (!Rezepte)
	{
		mStrahlenschutz = false;
		return;
	}
	// Fenster A 16.11.: die Suche ueber die Asset-Registry fand im Spiel 0 Rezepte (Plan Strahlenschutz). Die Pfade sind
	// bekannt (Starter Project Recipes/Equipment) - direkt laden, jeden Fehlschlag einzeln melden.
	static const TCHAR* Pfade[] = {
		TEXT("/Game/FactoryGame/Recipes/Equipment/Recipe_HazmatSuit.Recipe_HazmatSuit_C"),
		TEXT("/Game/FactoryGame/Recipes/Equipment/Recipe_FilterHazmat.Recipe_FilterHazmat_C"),
	};
	TArray<FString> Namen;
	for (const TCHAR* Pfad : Pfade)
	{
		UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Pfad);
		if (!Rezept)
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Strahlenschutz: Rezept %s nicht gefunden"), Pfad);
			continue;
		}
		if (!Rezepte->IsRecipeAvailable(Rezept))
		{
			Rezepte->AddAvailableRecipe(Rezept);
		}
		Namen.Add(Rezept->GetName());
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Strahlenschutz: %d Rezepte freigeschaltet (%s)"), Namen.Num(), *FString::Join(Namen, TEXT(", ")));
}

void UTDAbwehrSubsystem::TauscheKosten()
{
	if (mGetauscht)
	{
		return;
	}
	mGetauscht = true;
	UClass* Festplatte = UTDEinsatzSubsystem::FPKlasse();
	UClass* Coupon = UTDEinsatzSubsystem::CouponKlasse();
	const int32 JeFestplatte = FMath::Max(1, TDBalance::Ganz(TEXT("Abwehr.CouponsJeFestplatte"), 10));
	int32 Getauscht = 0;
	for (const FTDForschungKnoten& Knoten : TDForschung::Alle())
	{
		if (IstIndustrieKnoten(Knoten.Pfad))
		{
			continue;
		}
		UClass* Schematic = LoadClass<UFGSchematic>(nullptr, *Knoten.Pfad);
		TArray<FItemAmount>* Kosten = SchematicKosten(Schematic);
		if (!Kosten || !Festplatte || !Coupon)
		{
			continue;
		}
		TArray<FItemAmount> Neu;
		bool bGeaendert = false;
		for (const FItemAmount& K : *Kosten)
		{
			if (K.ItemClass == Festplatte)
			{
				Neu.Add(FItemAmount(Coupon, K.Amount * JeFestplatte));
				bGeaendert = true;
			}
			else
			{
				Neu.Add(K);
			}
		}
		if (bGeaendert)
		{
			mOriginalKosten.Add(Schematic, *Kosten);
			*Kosten = Neu;
			++Getauscht;
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Abwehr: Forschungskosten von %d Knoten auf Coupons umgestellt (%d Coupons je Festplatte)"), Getauscht, JeFestplatte);
}

void UTDAbwehrSubsystem::StelleKostenWiederHer()
{
	if (!mGetauscht)
	{
		return;
	}
	int32 Zurueck = 0;
	for (const TPair<TWeakObjectPtr<UClass>, TArray<FItemAmount>>& E : mOriginalKosten)
	{
		if (TArray<FItemAmount>* Kosten = SchematicKosten(E.Key.Get()))
		{
			*Kosten = E.Value;
			++Zurueck;
		}
	}
	mOriginalKosten.Reset();
	mGetauscht = false;
	UE_LOG(LogFicsitTD, Log, TEXT("Abwehr: Forschungskosten von %d Knoten zurueckgestellt"), Zurueck);
}

void UTDAbwehrSubsystem::ZieheIndustrieNach()
{
	AFGSchematicManager* Schemata = AFGSchematicManager::Get(GetWorld());
	const AFGRecipeManager* Rezepte = AFGRecipeManager::Get(GetWorld());
	if (!Schemata || !Rezepte)
	{
		return;
	}
	UClass* Schwefel = LoadClass<UObject>(nullptr, TEXT("/Game/FactoryGame/Resource/RawResources/Sulfur/Desc_Sulfur.Desc_Sulfur_C"));
	for (const TCHAR* Pfad : TDIndustrieKnoten)
	{
		UClass* Schematic = LoadClass<UFGSchematic>(nullptr, Pfad);
		if (!Schematic || Schemata->IsSchematicPurchased(Schematic))
		{
			continue;
		}
		// Spielrezepte des Knotens (eigene FicsitTD-Rezepte wie das Feldrezept Aluminium kommen mit dem Knoten dazu)
		TArray<UClass*> Spielrezepte;
		for (UFGUnlock* Freigabe : UFGSchematic::GetUnlocks(Schematic))
		{
			if (const UFGUnlockRecipe* R = Cast<UFGUnlockRecipe>(Freigabe))
			{
				for (const TSubclassOf<UFGRecipe>& Rezept : R->GetRecipesToUnlock())
				{
					if (Rezept && Rezept->GetPathName().StartsWith(TEXT("/Game/")))
					{
						Spielrezepte.Add(Rezept.Get());
					}
				}
			}
		}
		bool bFrei = Spielrezepte.Num() > 0;
		for (UClass* Rezept : Spielrezepte)
		{
			bFrei &= Rezepte->IsRecipeAvailable(Rezept);
		}
		const FString Name = Schematic->GetName();
		if (Spielrezepte.Num() == 0)
		{
			if (Name.Contains(TEXT("Schwefel")))
			{
				// ohne eigenes Rezept: frei, sobald ein verfuegbares Rezept des Spiels Schwefel verarbeitet (MAM-Schwefelbaum)
				TArray<TSubclassOf<UFGRecipe>> Verfuegbar;
				Rezepte->GetAllAvailableRecipes(Verfuegbar);
				for (const TSubclassOf<UFGRecipe>& R : Verfuegbar)
				{
					for (const FItemAmount& Z : UFGRecipe::GetIngredients(GetWorld(), R))
					{
						bFrei |= Schwefel && Z.ItemClass == Schwefel;
					}
				}
			}
			else
			{
				// Munitionspuffer: kommt mit Band Mk2
				UClass* Band = LoadClass<UFGSchematic>(nullptr, TEXT("/FicsitTD/Research/Produktion/Research_TD_BandMk2.Research_TD_BandMk2_C"));
				bFrei = Band && Schemata->IsSchematicPurchased(Band);
			}
		}
		if (bFrei)
		{
			Schemata->GiveAccessToSchematic(Schematic, nullptr);
			++mNachgezogen;
			UE_LOG(LogFicsitTD, Log, TEXT("Abwehr: Industrie-Knoten %s folgt dem HUB (Spielrezepte verfuegbar)"), *Name);
		}
	}
}
