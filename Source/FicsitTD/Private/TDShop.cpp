#include "TDShop.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "TDSprache.h"
#include "FicsitTDModule.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGResourceSinkSubsystem.h"
#include "FGInventoryComponent.h"
#include "Buildables/FGBuildableResourceSinkShop.h"
#include "Patching/NativeHookManager.h"
#include "ItemAmount.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// 0.12 E4 (Marius 14.11. und 21.11.): Kontingente, Ausruestung, Kernreparatur und Festplatten im AWESOME-Shop.
// Ergebnisse des Prototyps aus Testfenster 1 (21.11.):
//   1 derselbe Posten laesst sich beliebig oft kaufen (IsRepeatPurchasesAllowed)
//   2 das Spiel liest den Preis nach dem Kauf neu aus dem CDO (mCost)
//   3 Unlock() laeuft einmal je Kaufvorgang, nicht je Stueck -> Wirkung im Kauf-Hook
//   4 n Stueck in einem Kauf kosten n x aktuellen Preis, ohne Staffel -> der Hook zieht die Differenz nach
//   5 das Fenster holt seine Liste aus GetVisibleSchematicCategoryData
// Wahrheit bleiben die Kaufzaehler am Kern (mKontingentGekauft); der Preis im CDO wird daraus gesetzt und nicht gespeichert.

bool TDShop::PrototypAn()
{
	return TDBalance::Wert(TEXT("Experiment.ShopPrototyp"), 0.f) >= 1.f;
}

FString TDShop::AssetName(const FString& Schluessel)
{
	FString Name = Schluessel.Replace(TEXT("ä"), TEXT("ae")).Replace(TEXT("ö"), TEXT("oe")).Replace(TEXT("ü"), TEXT("ue"))
		.Replace(TEXT("Ä"), TEXT("Ae")).Replace(TEXT("Ö"), TEXT("Oe")).Replace(TEXT("Ü"), TEXT("Ue")).Replace(TEXT("ß"), TEXT("ss"));
	FString Aus;
	for (const TCHAR C : Name)
	{
		if (FChar::IsAlnum(C))
		{
			Aus.AppendChar(C);
		}
	}
	return Aus;
}

UClass* TDShop::Posten(const FString& Schluessel)
{
	const FString A = AssetName(Schluessel);
	return LoadClass<UFGSchematic>(nullptr, *FString::Printf(TEXT("/FicsitTD/Shop/Shop_TD_%s.Shop_TD_%s_C"), *A, *A));
}

const TArray<FString>& TDShop::PostenPfade()
{
	static TArray<FString> Pfade;
	if (Pfade.Num() == 0)
	{
		for (const FString& Key : UTDEinsatzSubsystem::LadenSchluessel())
		{
			const FString A = AssetName(Key);
			Pfade.Add(FString::Printf(TEXT("/FicsitTD/Shop/Shop_TD_%s.Shop_TD_%s_C"), *A, *A));
		}
	}
	return Pfade;
}

static TArray<FItemAmount>* TDShopKosten(UClass* Schematic)
{
	static FArrayProperty* Feld = FindFProperty<FArrayProperty>(UFGSchematic::StaticClass(), TEXT("mCost"));
	UObject* CDO = Schematic && Schematic->IsChildOf(UFGSchematic::StaticClass()) ? Schematic->GetDefaultObject() : nullptr;
	return Feld && CDO ? Feld->ContainerPtrToValuePtr<TArray<FItemAmount>>(CDO) : nullptr;
}

int32 TDShop::LiesPreis(UClass* Schematic)
{
	const TArray<FItemAmount>* Kosten = TDShopKosten(Schematic);
	return Kosten && Kosten->Num() > 0 ? (*Kosten)[0].Amount : -1;
}

bool TDShop::SetzePreis(UClass* Schematic, int32 Preis)
{
	TArray<FItemAmount>* Kosten = TDShopKosten(Schematic);
	if (!Kosten || Kosten->Num() == 0)
	{
		return false;
	}
	(*Kosten)[0].Amount = FMath::Max(1, Preis);
	return true;
}

/** Schluessel des Laden-Postens, wenn das Schematic eine UTDUnlockLaden traegt (sonst leer: Artikel des Spiels) */
static FString TDShopSchluessel(TSubclassOf<UFGSchematic> Schematic)
{
	if (Schematic)
	{
		for (const UFGUnlock* U : UFGSchematic::GetUnlocks(Schematic))
		{
			if (const UTDUnlockLaden* Laden = Cast<UTDUnlockLaden>(U))
			{
				return Laden->mSchluessel;
			}
		}
	}
	return FString();
}

static UTDEinsatzSubsystem* TDShopEinsatz(const UObject* Kontext)
{
	UWorld* World = Kontext ? Kontext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
}

void TDShop::SetzePreise(const UObject* Kontext)
{
	const UTDEinsatzSubsystem* Einsatz = TDShopEinsatz(Kontext);
	int32 Gesetzt = 0;
	for (const FString& Key : UTDEinsatzSubsystem::LadenSchluessel())
	{
		if (UClass* Klasse = Posten(Key))
		{
			// ohne Einsatz: Grundpreis (PreisFuer liest dann 0 Kaeufe)
			const int32 Preis = Einsatz ? Einsatz->PreisFuer(Key, 1) : UTDEinsatzSubsystem::GrundpreisFuer(Key);
			Gesetzt += SetzePreis(Klasse, Preis) ? 1 : 0;
		}
	}
	UE_LOG(LogFicsitTD, Verbose, TEXT("Shop: Preise von %d Posten gesetzt"), Gesetzt);
}

void TDShop::SetzeZurueck()
{
	SetzePreise(nullptr);
}

void TDShop::StelleHooksSicher()
{
}

bool UTDExperimentAbhaengigkeit::AreDependenciesMet(UObject* worldContext) const
{
	if (!worldContext || !worldContext->GetWorld())
	{
		return true;
	}
	return TDBalance::Wert(TEXT("Experiment.") + mSchalter, 0.f) >= 1.f;
}

bool UTDShopAbhaengigkeit::AreDependenciesMet(UObject* worldContext) const
{
	// ohne Welt (Editor, Pruefung der Assets) nicht sperren
	if (!worldContext || !worldContext->GetWorld())
	{
		return true;
	}
	const UTDEinsatzSubsystem* Einsatz = TDShopEinsatz(worldContext);
	return Einsatz && Einsatz->ShopSichtbar(mSchluessel);
}

bool TDShop::ShopSteht(const UObject* Kontext)
{
	UWorld* World = Kontext ? Kontext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	for (TActorIterator<AFGBuildableResourceSinkShop> It(World); It; ++It)
	{
		return true;
	}
	return false;
}

// ---- 21.11. (Marius, A5 Nr. 1 "alles gesperrt"): Artikel des Spiels im Einsatz und Endlos ausblenden und nicht verkaufen ----
// Anlass Fenster 1 (T1-3): Der Shop bot im Einsatz Fundamente und Waende an, obwohl der Spieler keine Bauteil-Rezepte hat.

static bool TDShopIstEigener(const UClass* Schematic)
{
	return Schematic && Schematic->GetPathName().StartsWith(TEXT("/FicsitTD/"));
}

bool TDShop::SpielArtikelGesperrt(const UObject* Kontext)
{
	const UTDEinsatzSubsystem* Einsatz = TDShopEinsatz(Kontext);
	return Einsatz && Einsatz->IstAktiv() && TDBalance::Wert(TEXT("Einsatz.Shop.SpielArtikelSperren"), 1.f) >= 1.f;
}

/** Zahl der ausgeblendeten Artikel nur melden, wenn sie sich aendert (das Fenster fragt bei jedem Oeffnen) */
static void TDShopMeldeSperre(const UWorld* Welt, int32 Weg, int32 Bleiben)
{
	static int32 Zuletzt = -1;
	static int32 ZuletztBleiben = -1;
	static TWeakObjectPtr<const UWorld> ZuletztWelt;
	if (Weg != Zuletzt || Bleiben != ZuletztBleiben || ZuletztWelt.Get() != Welt)
	{
		Zuletzt = Weg;
		ZuletztBleiben = Bleiben;
		ZuletztWelt = Welt;
		UE_LOG(LogFicsitTD, Log, TEXT("Shop: %d Artikel des Spiels im Einsatz ausgeblendet, %d des Mods sichtbar"), Weg, Bleiben);
	}
}

// Die Hook-Koerper stehen in eigenen Funktionen: Kommas in Vorlagen-Argumenten (TMap<A, B>) zerlegen sonst das Makro SUBSCRIBE_METHOD.
using FTDShopListe = TArray<TSubclassOf<UFGSchematic>>;
using FTDShopStueck = TMap<FString, int32>;

static void TDShopFilterKategorien(AFGSchematicManager* Self, ESchematicType Typ, TArray<FSchematicCategoryData>& Daten)
{
	if (Typ != ESchematicType::EST_ResourceSink || !TDShop::SpielArtikelGesperrt(Self))
	{
		return;
	}
	int32 Weg = 0, Bleiben = 0;
	for (FSchematicCategoryData& Kategorie : Daten)
	{
		for (FSchematicSubCategoryData& Unter : Kategorie.SchematicSubCategoryData)
		{
			Weg += Unter.Schematics.RemoveAll([](const TSubclassOf<UFGSchematic>& S) { return !TDShopIstEigener(S.Get()); });
			Bleiben += Unter.Schematics.Num();
		}
		Kategorie.SchematicSubCategoryData.RemoveAll([](const FSchematicSubCategoryData& U) { return U.Schematics.Num() == 0; });
	}
	Daten.RemoveAll([](const FSchematicCategoryData& K) { return K.SchematicSubCategoryData.Num() == 0; });
	TDShopMeldeSperre(Self ? Self->GetWorld() : nullptr, Weg, Bleiben);
}

static void TDShopFilterListe(const AFGSchematicManager* Self, ESchematicType Typ, FTDShopListe& Liste)
{
	if (Typ == ESchematicType::EST_ResourceSink && TDShop::SpielArtikelGesperrt(Self))
	{
		Liste.RemoveAll([](const TSubclassOf<UFGSchematic>& S) { return !TDShopIstEigener(S.Get()); });
	}
}

/** Posten des Mods im Korb, je Schluessel die Stueckzahl */
static FTDShopStueck TDShopZaehle(const FTDShopListe& Liste)
{
	FTDShopStueck Stueck;
	for (const TSubclassOf<UFGSchematic>& S : Liste)
	{
		const FString Key = TDShopSchluessel(S);
		if (!Key.IsEmpty())
		{
			Stueck.FindOrAdd(Key)++;
		}
	}
	return Stueck;
}

/** Zwischen dem Hook vor und dem nach dem Kauf: Staffel-Summe je Schluessel und was das Spiel selbst abzieht */
struct FTDShopKauf
{
	FTDShopStueck Stueck;
	TMap<FString, int32> Soll;
	int32 SollSumme = 0;
	int32 SpielSumme = 0;
	/** 23.11. (Gegencheck 0.13 F8): Coupons, die der Hook vor dem Kauf geliehen hat (Mindestpreis der Festplattenabgabe ohne Coupons) */
	int32 Vorschuss = 0;
	bool bAktiv = false;
};
static FTDShopKauf GTDShopKauf;

static void TDShopMelde(const UObject* Kontext, const FString& Text)
{
	if (const UTDEinsatzSubsystem* Einsatz = TDShopEinsatz(Kontext))
	{
		Einsatz->Melde(Text);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Shop: %s"), *Text);
}

/** true = Kauf ablehnen */
static bool TDShopVorKauf(AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar, const FTDShopListe& Liste)
{
	GTDShopKauf = FTDShopKauf();
	if (!Self)
	{
		return false;
	}
	// Spiel-Artikel im Einsatz (alter Korb, Blueprint)
	if (TDShop::SpielArtikelGesperrt(Self))
	{
		for (const TSubclassOf<UFGSchematic>& S : Liste)
		{
			if (!TDShopIstEigener(S.Get()))
			{
				TDShopMelde(Self, TDF(TEXT("Im Einsatz verkauft der Shop keine Artikel des Spiels (%s)."), TEXT("During an operation the Shop sells no base game items (%s)."),
					S ? *UFGSchematic::GetSchematicDisplayName(S).ToString() : TEXT("?")));
				return true;
			}
		}
	}
	const FTDShopStueck Stueck = TDShopZaehle(Liste);
	if (Stueck.Num() == 0)
	{
		return false;
	}
	UTDEinsatzSubsystem* Einsatz = TDShopEinsatz(Self);
	if (!Einsatz)
	{
		return true;
	}
	int32 Soll = 0, Spiel = 0;
	for (const TPair<FString, int32>& P : Stueck)
	{
		FString Grund;
		if (!Einsatz->KaufErlaubt(P.Key, Grund))
		{
			TDShopMelde(Self, Grund);
			return true;
		}
		const int32 S = Einsatz->PreisFuer(P.Key, P.Value);
		GTDShopKauf.Soll.Add(P.Key, S);
		Soll += S;
		// das Spiel rechnet Stueck x Preis im CDO (Frage 4)
		Spiel += P.Value * FMath::Max(0, TDShop::LiesPreis(TDShop::Posten(P.Key)));
	}
	const AFGResourceSinkSubsystem* Sink = AFGResourceSinkSubsystem::Get(Self->GetWorld());
	const int32 Coupons = Sink && Inventar && Sink->GetCouponClass() ? Inventar->GetNumItems(Sink->GetCouponClass()) : 0;
	// Spiel-Artikel im selben Korb (ausserhalb des Einsatzes) bezahlt das Spiel; hier zaehlt nur der Teil des Mods
	if (Coupons < Soll + FMath::Max(0, (Sink ? Sink->GetCostOfSchematics(Liste) : 0) - Spiel))
	{
		TDShopMelde(Self, TDF(TEXT("Zu wenig FICSIT-Coupons: %d nötig (der Preis steigt mit jedem Stück), %d im Inventar."),
			TEXT("Not enough FICSIT Coupons: %d needed (the price rises with every piece), %d in your inventory."), Soll, Coupons));
		return true;
	}
	GTDShopKauf.Stueck = Stueck;
	GTDShopKauf.SollSumme = Soll;
	GTDShopKauf.SpielSumme = Spiel;
	GTDShopKauf.bAktiv = true;
	// 23.11. (Gegencheck 0.13 F8): Posten mit Preis 0 (Festplatte abgeben) zeigen im Shop mindestens 1 Coupon, den das Spiel vor dem
	// Hook verlangt. Fehlt er, leiht der Hook ihn vor; TDShopNachKauf rechnet ihn gegen die Erstattung (oder nimmt ihn bei Abbruch zurueck)
	const int32 SpielKosten = Sink ? Sink->GetCostOfSchematics(Liste) : Spiel;
	if (Soll < Spiel && Coupons < SpielKosten && Inventar && Sink && Sink->GetCouponClass())
	{
		GTDShopKauf.Vorschuss = Inventar->AddStack(FInventoryStack(SpielKosten - Coupons, Sink->GetCouponClass()), true);
		UE_LOG(LogFicsitTD, Log, TEXT("Shop: Vorschuss %d Coupons fuer den Mindestpreis"), GTDShopKauf.Vorschuss);
	}
	return false;
}

static void TDShopNachKauf(bool bErgebnis, AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar)
{
	const FTDShopKauf Kauf = GTDShopKauf;
	GTDShopKauf = FTDShopKauf();
	if (!bErgebnis || !Kauf.bAktiv || !Self)
	{
		// 23.11. (F8): Kauf abgebrochen - geliehene Coupons zurueck
		if (Kauf.Vorschuss > 0 && Inventar && Self)
		{
			if (const AFGResourceSinkSubsystem* SinkAb = AFGResourceSinkSubsystem::Get(Self->GetWorld()))
			{
				Inventar->Remove(SinkAb->GetCouponClass(), FMath::Min(Kauf.Vorschuss, Inventar->GetNumItems(SinkAb->GetCouponClass())));
			}
		}
		return;
	}
	UTDEinsatzSubsystem* Einsatz = TDShopEinsatz(Self);
	const AFGResourceSinkSubsystem* Sink = AFGResourceSinkSubsystem::Get(Self->GetWorld());
	// Staffel: das Spiel nahm Stueck x aktuellen Preis, die Differenz zur Staffel-Summe nachziehen
	const int32 Differenz = Kauf.SollSumme - Kauf.SpielSumme;
	if (Differenz > 0 && Inventar && Sink && Sink->GetCouponClass())
	{
		Inventar->Remove(Sink->GetCouponClass(), Differenz);
	}
	// A6 E7: Posten mit Preis 0 (Festplatte abgeben) zeigen im Shop mindestens 1 Coupon (SetzePreis) - den bekommt der Spieler zurueck
	// 23.11. (F8): ein Vorschuss vor dem Kauf ist schon im Inventar - nur den Rest erstatten
	const int32 Erstattung = -Differenz - Kauf.Vorschuss;
	if (Differenz < 0 && Erstattung > 0 && Inventar && Sink && Sink->GetCouponClass())
	{
		Inventar->AddStack(FInventoryStack(Erstattung, Sink->GetCouponClass()), true);
	}
	for (const TPair<FString, int32>& P : Kauf.Stueck)
	{
		const FString Text = Einsatz ? Einsatz->WirkeKauf(P.Key, P.Value, Kauf.Soll.FindRef(P.Key)) : FString();
		if (!Text.IsEmpty())
		{
			TDShopMelde(Self, Text);
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Shop: Kauf %d Posten, Spiel %d + Staffel %d = %d Coupons"), Kauf.Stueck.Num(), Kauf.SpielSumme, Differenz, Kauf.SollSumme);
	TDShop::SetzePreise(Self);
}

void TDShop::RichteSperreEin()
{
#if !WITH_EDITOR
	static bool bEingerichtet = false;
	if (bEingerichtet)
	{
		return;
	}
	bEingerichtet = true;
	UE_LOG(LogFicsitTD, Log, TEXT("Shop: Sperre der Spiel-Artikel im Einsatz eingerichtet (vier Hooks)"));
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GetVisibleSchematicCategoryData,
		[](AFGSchematicManager* Self, ESchematicType Typ, TArray<FSchematicCategoryData>& Daten)
	{
		TDShopFilterKategorien(Self, Typ, Daten);
	});
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GetAllVisibleSchematicsOfType,
		[](const AFGSchematicManager* Self, ESchematicType Typ, FTDShopListe& Liste)
	{
		TDShopFilterListe(Self, Typ, Liste);
	});
	SUBSCRIBE_METHOD(AFGBuildableResourceSinkShop::PurchaseResourceSinkSchematics,
		[](auto& Scope, AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar, FTDShopListe Liste)
	{
		if (TDShopVorKauf(Self, Inventar, Liste))
		{
			Scope.Override(false);
		}
	});
	SUBSCRIBE_METHOD_AFTER(AFGBuildableResourceSinkShop::PurchaseResourceSinkSchematics,
		[](const bool& bErgebnis, AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar, FTDShopListe Liste)
	{
		TDShopNachKauf(bErgebnis, Self, Inventar);
	});
#endif
}
