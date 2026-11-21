#include "TDShop.h"
#include "TDBalance.h"
#include "FicsitTDModule.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "FGResourceSinkSubsystem.h"
#include "FGInventoryComponent.h"
#include "Buildables/FGBuildableResourceSinkShop.h"
#include "Patching/NativeHookManager.h"
#include "TDEinsatz.h"
#include "Engine/World.h"
#include "ItemAmount.h"

// 0.12 Etappe E1, Shop-Prototyp (Fahrplan-0.12 Abschnitt 4, Bericht A2 Abschnitt 5.3). Fuenf Fragen an das Spiel, alle als Logzeile
// "ShopProto: ..." beantwortet:
//   1 Laesst der Shop denselben Posten mehrfach kaufen?            -> "Kauf <Posten> Ergebnis 1" bei Kauf 2 und 3
//   2 Liest das Spiel den Preis nach dem Kauf neu aus dem CDO?     -> "Preis <Schluessel> 4 -> 5 (CDO)", "Kosten laut Spiel 5"
//   3 Ruft das Spiel Unlock() bei jedem Kauf?                      -> "Unlock <Schluessel> Nr. n" (nur Befund)
//   4 Wie rechnet das Spiel mehrere Stueck in einem Kauf?          -> "Kaufliste 2 x <Posten>, Kosten laut Spiel n"
//   5 Ueber welche Funktion holt das Shop-Fenster seine Liste?     -> "Liste aus <Funktion>, n Eintraege"
// Der Prototyp vergibt kein Kontingent und speichert nichts: Kaufzaehler und Preis leben nur bis zum Spielende.

static TMap<FString, int32>& TDShopKaeufe()
{
	static TMap<FString, int32> Zaehler;
	return Zaehler;
}

static TMap<FString, int32>& TDShopUnlocks()
{
	static TMap<FString, int32> Zaehler;
	return Zaehler;
}

bool TDShop::PrototypAn()
{
	return TDBalance::Wert(TEXT("Experiment.ShopPrototyp"), 0.f) >= 1.f;
}

const TArray<FString>& TDShop::PostenPfade()
{
	static const TArray<FString> Pfade = { TEXT("/FicsitTD/Shop/Shop_TD_Bolzenturm.Shop_TD_Bolzenturm_C") };
	return Pfade;
}

UClass* TDShop::Posten(const FString& Schluessel)
{
	return LoadClass<UFGSchematic>(nullptr, *FString::Printf(TEXT("/FicsitTD/Shop/Shop_TD_%s.Shop_TD_%s_C"), *Schluessel, *Schluessel));
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
	(*Kosten)[0].Amount = Preis;
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

/** Preis nach n Kaeufen wie im Laden: Grundpreis x (1 + Aufschlag x gekauft), gerundet */
static int32 TDShopPreisNach(const FString& Schluessel, int32 Gekauft)
{
	const float Grund = TDBalance::Wert(FString::Printf(TEXT("Einsatz.Laden.%s.Grundpreis"), *Schluessel), 4.f);
	const float Aufschlag = TDBalance::Wert(FString::Printf(TEXT("Einsatz.Laden.%s.Aufschlag"), *Schluessel), 0.3f);
	return FMath::RoundToInt(Grund * (1.f + Aufschlag * Gekauft));
}

void TDShop::SetzeZurueck()
{
	TDShopKaeufe().Reset();
	TDShopUnlocks().Reset();
	for (const FString& Pfad : PostenPfade())
	{
		UClass* Klasse = LoadClass<UFGSchematic>(nullptr, *Pfad);
		const FString Schluessel = TDShopSchluessel(Klasse);
		if (Klasse && !Schluessel.IsEmpty())
		{
			const int32 Alt = LiesPreis(Klasse);
			const int32 Grund = TDShopPreisNach(Schluessel, 0);
			SetzePreis(Klasse, Grund);
			if (PrototypAn() || Alt != Grund)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: %s Startpreis %d (vorher %d)"), *Schluessel, Grund, Alt);
			}
		}
	}
}

void UTDUnlockLaden::Unlock(AFGUnlockSubsystem* unlockSubsystem)
{
	Super::Unlock(unlockSubsystem);
	const int32 Nr = ++TDShopUnlocks().FindOrAdd(mSchluessel);
	UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Unlock %s Nr. %d"), *mSchluessel, Nr);
}

void UTDUnlockLaden::Apply(AFGUnlockSubsystem* unlockSubsystem)
{
	Super::Apply(unlockSubsystem);
	UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Apply %s"), *mSchluessel);
}

bool UTDExperimentAbhaengigkeit::AreDependenciesMet(UObject* worldContext) const
{
	// ohne Welt (im Editor beim Pruefen der Assets) nicht sperren
	if (!worldContext || !worldContext->GetWorld())
	{
		return true;
	}
	return TDBalance::Wert(TEXT("Experiment.") + mSchalter, 0.f) >= 1.f;
}

/** Frage 5: hoechstens zehn Zeilen je Funktion, nur wenn sich die Zahl der Eintraege aendert */
static void TDShopMeldeListe(const TCHAR* Funktion, int32 Eintraege, int32 Eigene)
{
	static TMap<FString, int32> Zuletzt;
	static TMap<FString, int32> Zeilen;
	const int32 Merker = Eintraege * 1000 + Eigene;
	if (Zuletzt.FindRef(Funktion) == Merker + 1 || Zeilen.FindRef(Funktion) >= 10)
	{
		return;
	}
	Zuletzt.Add(Funktion, Merker + 1);
	Zeilen.FindOrAdd(Funktion)++;
	UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Liste aus %s, %d Eintraege, davon %d des Mods"), Funktion, Eintraege, Eigene);
}

static int32 TDShopZaehleEigene(const TArray<TSubclassOf<UFGSchematic>>& Liste)
{
	int32 Eigene = 0;
	for (const TSubclassOf<UFGSchematic>& S : Liste)
	{
		Eigene += S && S->GetPathName().StartsWith(TEXT("/FicsitTD/Shop/")) ? 1 : 0;
	}
	return Eigene;
}

// Die Hook-Koerper stehen in eigenen Funktionen: Kommas in Vorlagen-Argumenten (TMap<A, B>) zerlegen sonst das Makro SUBSCRIBE_METHOD.
using FTDShopListe = TArray<TSubclassOf<UFGSchematic>>;
using FTDShopStueck = TMap<UClass*, int32>;

static FTDShopStueck TDShopZaehle(const FTDShopListe& Liste)
{
	FTDShopStueck Stueck;
	for (const TSubclassOf<UFGSchematic>& S : Liste)
	{
		Stueck.FindOrAdd(S.Get())++;
	}
	return Stueck;
}

/** Frage 4: was wird gekauft, und was rechnet das Spiel dafuer? */
static void TDShopVorKauf(AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar, const FTDShopListe& Liste)
{
	if (!TDShop::PrototypAn() || !Self)
	{
		return;
	}
	const AFGResourceSinkSubsystem* Sink = AFGResourceSinkSubsystem::Get(Self->GetWorld());
	const int32 Kosten = Sink ? Sink->GetCostOfSchematics(Liste) : -1;
	const int32 Coupons = Sink && Inventar && Sink->GetCouponClass() ? Inventar->GetNumItems(Sink->GetCouponClass()) : -1;
	for (const TPair<UClass*, int32>& P : TDShopZaehle(Liste))
	{
		const FString Name = P.Key ? P.Key->GetName().LeftChop(2) : FString(TEXT("?"));
		UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Kaufliste %d x %s, Kosten laut Spiel %d, Coupons im Inventar %d, wiederholbar %s"),
			P.Value, *Name, Kosten, Coupons, P.Key && UFGSchematic::IsRepeatPurchasesAllowed(P.Key) ? TEXT("ja") : TEXT("nein"));
	}
}

/** Fragen 1 und 2: Ergebnis des Kaufs, danach neuer Preis ins CDO und Gegenprobe ueber die Kostenfunktion des Spiels */
static void TDShopNachKauf(bool bErgebnis, AFGBuildableResourceSinkShop* Self, const FTDShopListe& Liste)
{
	if (!TDShop::PrototypAn() || !Self)
	{
		return;
	}
	const AFGResourceSinkSubsystem* Sink = AFGResourceSinkSubsystem::Get(Self->GetWorld());
	const AFGSchematicManager* Schemata = AFGSchematicManager::Get(Self->GetWorld());
	for (const TPair<UClass*, int32>& P : TDShopZaehle(Liste))
	{
		const FString Name = P.Key ? P.Key->GetName().LeftChop(2) : FString(TEXT("?"));
		UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Kauf %s Ergebnis %d (%d Stueck, danach als gekauft gefuehrt: %s)"), *Name, bErgebnis ? 1 : 0, P.Value,
			Schemata && P.Key && Schemata->IsSchematicPurchased(P.Key) ? TEXT("ja") : TEXT("nein"));
		const FString Schluessel = TDShopSchluessel(P.Key);
		if (!bErgebnis || Schluessel.IsEmpty())
		{
			continue;
		}
		int32& Gekauft = TDShopKaeufe().FindOrAdd(Schluessel);
		const int32 Alt = TDShop::LiesPreis(P.Key);
		Gekauft += P.Value;
		const int32 Neu = TDShopPreisNach(Schluessel, Gekauft);
		const bool bGesetzt = TDShop::SetzePreis(P.Key, Neu);
		UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Preis %s %d -> %d (CDO%s), %d gekauft"), *Schluessel, Alt, Neu, bGesetzt ? TEXT("") : TEXT(", nicht gesetzt"), Gekauft);
		if (Sink)
		{
			FTDShopListe Einer;
			Einer.Add(P.Key);
			UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Kosten laut Spiel %d (%s nach dem Kauf)"), Sink->GetCostOfSchematics(Einer), *Schluessel);
		}
	}
}

/** Fragen 2 und 4: jede Kostenabfrage, die einen Posten des Mods enthaelt (das Shop-Fenster fragt beim Fuellen des Korbs) */
static void TDShopNachKosten(int32 Kosten, const FTDShopListe& Liste)
{
	static int32 Zeilen = 0;
	static int32 Zuletzt = MIN_int32;
	if (!TDShop::PrototypAn() || Zeilen >= 40)
	{
		return;
	}
	const int32 Eigene = TDShopZaehleEigene(Liste);
	const int32 Merker = Liste.Num() * 100000 + Eigene * 1000 + Kosten;
	if (Eigene > 0 && Merker != Zuletzt)
	{
		Zuletzt = Merker;
		++Zeilen;
		UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Kostenabfrage %d Posten (%d des Mods) -> %d Coupons"), Liste.Num(), Eigene, Kosten);
	}
}

/** Frage 5: welche Funktion fuellt das Shop-Fenster? */
static void TDShopNachListe(const TCHAR* Funktion, bool bShop, int32 Eintraege, int32 Eigene)
{
	if (TDShop::PrototypAn() && bShop)
	{
		TDShopMeldeListe(Funktion, Eintraege, Eigene);
	}
}

// ---- 21.11. (Marius, A5 Nr. 1 "alles gesperrt"): Artikel des Spiels im Einsatz und Endlos ausblenden und nicht verkaufen ----
// Anlass Fenster 1 (T1-3): Der Shop bot im Einsatz Fundamente und Waende an, obwohl der Spieler keine Bauteil-Rezepte hat.

static bool TDShopIstEigener(const UClass* Schematic)
{
	return Schematic && Schematic->GetPathName().StartsWith(TEXT("/FicsitTD/"));
}

bool TDShop::SpielArtikelGesperrt(const UObject* Kontext)
{
	const UWorld* World = Kontext ? Kontext->GetWorld() : nullptr;
	const UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	return Einsatz && Einsatz->IstAktiv() && TDBalance::Wert(TEXT("Einsatz.Shop.SpielArtikelSperren"), 1.f) >= 1.f;
}

/** Zahl der ausgeblendeten Artikel nur melden, wenn sie sich aendert (das Fenster fragt bei jedem Oeffnen) */
static void TDShopMeldeSperre(int32 Weg, int32 Bleiben)
{
	static int32 Zuletzt = -1;
	if (Weg != Zuletzt)
	{
		Zuletzt = Weg;
		UE_LOG(LogFicsitTD, Log, TEXT("Shop: %d Artikel des Spiels im Einsatz ausgeblendet, %d des Mods sichtbar"), Weg, Bleiben);
	}
}

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
	TDShopMeldeSperre(Weg, Bleiben);
}

static void TDShopFilterListe(const AFGSchematicManager* Self, ESchematicType Typ, FTDShopListe& Liste)
{
	if (Typ == ESchematicType::EST_ResourceSink && TDShop::SpielArtikelGesperrt(Self))
	{
		Liste.RemoveAll([](const TSubclassOf<UFGSchematic>& S) { return !TDShopIstEigener(S.Get()); });
	}
}

/** true = Kauf ablehnen (ein Artikel des Spiels im Korb, z. B. aus einem alten Korb oder per Blueprint) */
static bool TDShopKaufGesperrt(AFGBuildableResourceSinkShop* Self, const FTDShopListe& Liste)
{
	if (!TDShop::SpielArtikelGesperrt(Self))
	{
		return false;
	}
	for (const TSubclassOf<UFGSchematic>& S : Liste)
	{
		if (!TDShopIstEigener(S.Get()))
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Shop: Kauf abgelehnt - %s ist ein Artikel des Spiels, im Einsatz gesperrt"), S ? *S->GetName() : TEXT("?"));
			return true;
		}
	}
	return false;
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
	UE_LOG(LogFicsitTD, Log, TEXT("Shop: Sperre der Spiel-Artikel im Einsatz eingerichtet (drei Hooks)"));
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
		if (TDShopKaufGesperrt(Self, Liste))
		{
			Scope.Override(false);
		}
	});
#endif
}

void TDShop::StelleHooksSicher()
{
#if !WITH_EDITOR
	static bool bEingerichtet = false;
	if (!PrototypAn())
	{
		return;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("ShopProto: Experiment.ShopPrototyp an - Hooks auf Shop und Schematic-Verwaltung %s"),
		bEingerichtet ? TEXT("waren schon eingerichtet") : TEXT("werden eingerichtet (sechs)"));
	if (bEingerichtet)
	{
		return;
	}
	bEingerichtet = true;

	SUBSCRIBE_METHOD(AFGBuildableResourceSinkShop::PurchaseResourceSinkSchematics,
		[](auto& Scope, AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar, FTDShopListe Liste)
	{
		TDShopVorKauf(Self, Inventar, Liste);
	});
	SUBSCRIBE_METHOD_AFTER(AFGBuildableResourceSinkShop::PurchaseResourceSinkSchematics,
		[](const bool& bErgebnis, AFGBuildableResourceSinkShop* Self, UFGInventoryComponent* Inventar, FTDShopListe Liste)
	{
		TDShopNachKauf(bErgebnis, Self, Liste);
	});
	SUBSCRIBE_METHOD_AFTER(AFGResourceSinkSubsystem::GetCostOfSchematics,
		[](const int32& Kosten, const AFGResourceSinkSubsystem* Self, FTDShopListe Liste)
	{
		TDShopNachKosten(Kosten, Liste);
	});
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GetVisibleSchematicCategoryData,
		[](AFGSchematicManager* Self, ESchematicType Typ, TArray<FSchematicCategoryData>& Daten)
	{
		TDShopNachListe(TEXT("GetVisibleSchematicCategoryData"), Typ == ESchematicType::EST_ResourceSink, Daten.Num(), -1);
	});
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GetAllVisibleSchematicsOfType,
		[](const AFGSchematicManager* Self, ESchematicType Typ, FTDShopListe& Liste)
	{
		TDShopNachListe(TEXT("GetAllVisibleSchematicsOfType"), Typ == ESchematicType::EST_ResourceSink, Liste.Num(), TDShopZaehleEigene(Liste));
	});
	SUBSCRIBE_METHOD_AFTER(AFGSchematicManager::GetAvailableSchematicsOfTypes,
		[](const AFGSchematicManager* Self, TArray<ESchematicType> Typen, FTDShopListe& Liste)
	{
		TDShopNachListe(TEXT("GetAvailableSchematicsOfTypes"), Typen.Contains(ESchematicType::EST_ResourceSink), Liste.Num(), TDShopZaehleEigene(Liste));
	});
#endif
}
