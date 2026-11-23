// 23.11.2026 (Marius, Stromzweig, Notizen/Konzept-Stromzweig-2026-11-23.md): bessere Brennstoffe geben jedem Kraftwerk mehr MW.
// Leistung = Grundleistung des Kraftwerks (CDO, z. B. Kohlekraftwerk 75 MW) x Strom.<Brennstoff>.Faktor (TDBalance.cpp).
// Gilt nur, wenn FICSIT-Abwehr aktiv ist (Einsatz, Endlos, Freies Spiel mit Abwehr) - im Modus Aus bleibt das Spiel unveraendert.
// Der Brennstoff verbrennt entsprechend schneller (das Spiel rechnet Brenndauer = Energie / Leistung), je MW bleibt der Vorteil
// des besseren Brennstoffs erhalten.
#include "TDStrom.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDKernplatz.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Resources/FGItemDescriptor.h"
#include "FGInventoryComponent.h"

namespace
{
	struct FTDBrennstoff
	{
		const TCHAR* Klasse;      // Name der Deskriptor-Klasse
		const TCHAR* Schluessel;  // Strom.<Schluessel>.Faktor
	};
	const FTDBrennstoff TDBrennstoffe[] = {
		{ TEXT("Desc_Coal_C"), TEXT("Kohle") },
		{ TEXT("Desc_CompactedCoal_C"), TEXT("Kompaktkohle") },
		{ TEXT("Desc_PetroleumCoke_C"), TEXT("Petrolkoks") },
		{ TEXT("Desc_LiquidFuel_C"), TEXT("Treibstoff") },
		{ TEXT("Desc_LiquidBiofuel_C"), TEXT("Biotreibstoff") },
		{ TEXT("Desc_LiquidTurboFuel_C"), TEXT("Turbotreibstoff") },
		{ TEXT("Desc_RocketFuel_C"), TEXT("Raketentreibstoff") },
		{ TEXT("Desc_IonizedFuel_C"), TEXT("Ionisiert") },
		{ TEXT("Desc_NuclearFuelRod_C"), TEXT("Uran") },
		{ TEXT("Desc_PlutoniumFuelRod_C"), TEXT("Plutonium") },
		{ TEXT("Desc_FicsoniumFuelRod_C"), TEXT("Ficsonium") },
	};

	const TCHAR* TDSchluesselFuer(const UClass* Brennstoff)
	{
		if (!Brennstoff)
		{
			return nullptr;
		}
		const FString Name = Brennstoff->GetName();
		for (const FTDBrennstoff& B : TDBrennstoffe)
		{
			if (Name == B.Klasse)
			{
				return B.Schluessel;
			}
		}
		return nullptr;
	}
}

float FTDStrom::Faktor(const UClass* Brennstoff, const UObject* WorldContext)
{
	if (!Brennstoff || TDBalance::Wert(TEXT("Strom.Aktiv"), 1.f) <= 0.f || (WorldContext && !UTDKernplatz::ModAktiv(WorldContext)))
	{
		return 1.f;
	}
	const TCHAR* Schluessel = TDSchluesselFuer(Brennstoff);
	return Schluessel ? FMath::Max(0.1f, TDBalance::Wert(FString::Printf(TEXT("Strom.%s.Faktor"), Schluessel), 1.f)) : 1.f;
}

int32 FTDStrom::Pflege(UWorld* World)
{
	AFGBuildableSubsystem* Bauten = World ? AFGBuildableSubsystem::Get(World) : nullptr;
	if (!Bauten)
	{
		return 0;
	}
	const bool bAktiv = TDBalance::Wert(TEXT("Strom.Aktiv"), 1.f) > 0.f && UTDKernplatz::ModAktiv(World);
	TArray<AFGBuildableGeneratorFuel*> Kraftwerke;
	Bauten->GetTypedBuildable<AFGBuildableGeneratorFuel>(Kraftwerke);
	int32 Geaendert = 0;
	for (AFGBuildableGeneratorFuel* K : Kraftwerke)
	{
		if (!K)
		{
			continue;
		}
		const AFGBuildableGenerator* Vorlage = K->GetClass()->GetDefaultObject<AFGBuildableGenerator>();
		AFGBuildableGenerator* G = K;
		const float Grund = Vorlage ? Vorlage->mPowerProduction : G->mPowerProduction;
		UClass* Brennstoff = K->GetCurrentFuelClass().Get();
		if (bAktiv && !Brennstoff)
		{
			continue;   // zwischen zwei Brennstoff-Einheiten: Wert behalten (sonst springt die Leistung und das Log laeuft voll)
		}
		const float Soll = bAktiv ? Grund * Faktor(Brennstoff, nullptr) : Grund;
		if (!FMath::IsNearlyEqual(G->mPowerProduction, Soll, 0.01f))
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Strom: %s mit %s: %.0f -> %.0f MW (Grund %.0f MW)"), *K->GetClass()->GetName(),
				Brennstoff ? *Brennstoff->GetName() : TEXT("-"), G->mPowerProduction, Soll, Grund);
			G->mPowerProduction = Soll;
			++Geaendert;
		}
	}
	return Geaendert;
}

float FTDStrom::TestBefuelle(UWorld* World, const FVector& Ort, UClass* Brennstoff, int32 Menge, FString& OutText)
{
	AFGBuildableSubsystem* Bauten = World ? AFGBuildableSubsystem::Get(World) : nullptr;
	if (!Bauten || !Brennstoff)
	{
		OutText = TEXT("kein Bau-Subsystem oder Brennstoff");
		return -1.f;
	}
	TArray<AFGBuildableGeneratorFuel*> Kraftwerke;
	Bauten->GetTypedBuildable<AFGBuildableGeneratorFuel>(Kraftwerke);
	AFGBuildableGeneratorFuel* Naechstes = nullptr;
	// zuerst leere Kraftwerke (je Brennstoff ein eigenes), dann das naechste
	for (int32 Runde = 0; Runde < 2 && !Naechstes; ++Runde)
	{
		for (AFGBuildableGeneratorFuel* K : Kraftwerke)
		{
			if (!K || !K->GetFuelInventory() || (Runde == 0 && !K->GetFuelInventory()->IsEmpty()))
			{
				continue;
			}
			if (!Naechstes || FVector::DistSquared(K->GetActorLocation(), Ort) < FVector::DistSquared(Naechstes->GetActorLocation(), Ort))
			{
				Naechstes = K;
			}
		}
	}
	if (!Naechstes || !Naechstes->GetFuelInventory())
	{
		OutText = TEXT("kein Brennstoff-Kraftwerk");
		return -1.f;
	}
	const int32 Rein = Naechstes->GetFuelInventory()->AddStack(FInventoryStack(Menge, Brennstoff), true);
	Naechstes->LoadFuel();   // Friend (AccessTransformers.ini): sonst laedt das Kraftwerk erst, wenn das Netz Strom abnimmt
	Pflege(World);
	const float MW = Naechstes->GetPowerProductionCapacity();
	OutText = FString::Printf(TEXT("%s: %d %s eingelegt, Brennstoff %s, %.0f MW"), *Naechstes->GetClass()->GetName(), Rein, *Brennstoff->GetName(),
		Naechstes->GetCurrentFuelClass() ? *Naechstes->GetCurrentFuelClass()->GetName() : TEXT("-"), MW);
	return MW;
}
