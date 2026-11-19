// 19.11.2026 (Marius: "Ja, Diagnose-Build"): Satisfactory 1.2.5.1 (CL 520140) laesst FicsitTD abstuerzen.
// Vergleicht Klassengroessen und Feld-Offsets der Header, gegen die gebaut wurde, mit dem laufenden Spiel.
// Ergebnis im Log unter "Layout-Pruefung". Abweichung = unsere C++-Unterklassen liegen falsch im Speicher.
#include "FicsitTDModule.h"
#include "UObject/UnrealType.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableWall.h"
#include "Buildables/FGBuildableStorage.h"
#include "FGCharacterBase.h"
#include "FGCharacterPlayer.h"
#include "AI/FGCreatureController.h"
#include "Creature/FGCreature.h"
#include "UI/FGInteractWidget.h"
#include "UI/FGOptionsValueController.h"
#include "FGPowerConnectionComponent.h"
#include "FGCircuitConnectionComponent.h"
#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"
#include "FGConstructDisqualifier.h"
#include "AvailabilityDependencies/FGAvailabilityDependency.h"
#include "Components/ActorComponent.h"
#include "Blueprint/UserWidget.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "TDLayoutGroessen.inl"

class FTDLayoutDiagnose
{
public:
	static void Pruefen()
	{
		int32 Groessen = 0, GroessenFalsch = 0;
		auto Groesse = [&](UStruct* Klasse, int32 Gebaut)
		{
			const int32 Echt = Klasse->GetStructureSize();
			++Groessen;
			if (Echt != Gebaut) ++GroessenFalsch;
			if (Echt != Gebaut) UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung Groesse %s: gebaut %d, Spiel %d, Differenz %d%s"),
				*Klasse->GetName(), Gebaut, Echt, Echt - Gebaut, Echt != Gebaut ? TEXT("  <== ABWEICHUNG") : TEXT(""));
		};
#define TD_GR(K) Groesse(K::StaticClass(), (int32)sizeof(K));
#define TD_GRS(K) Groesse(K::StaticStruct(), (int32)sizeof(K));
		TD_GR(AActor) TD_GR(UActorComponent) TD_GR(UUserWidget)
		TD_ALLE_GROESSEN
#undef TD_GR
#undef TD_GRS

		int32 Felder = 0, FelderFalsch = 0, FelderFehlen = 0;
		auto Feld = [&](UClass* Klasse, const TCHAR* Name, int32 Gebaut)
		{
			++Felder;
			FProperty* P = Klasse->FindPropertyByName(FName(Name));
			if (!P || P->GetOwnerStruct() != Klasse)
			{
				++FelderFehlen;
				UE_LOG(LogFicsitTD, Warning, TEXT("Layout-Pruefung Feld %s::%s: gebaut %d, im Spiel nicht vorhanden"), *Klasse->GetName(), Name, Gebaut);
				return;
			}
			const int32 Echt = P->GetOffset_ForInternal();
			if (Echt != Gebaut) ++FelderFalsch;
			UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung Feld %s::%s: gebaut %d, Spiel %d, Differenz %d%s"),
				*Klasse->GetName(), Name, Gebaut, Echt, Echt - Gebaut, Echt != Gebaut ? TEXT("  <== ABWEICHUNG") : TEXT(""));
		};
#define TD_OFF(K, F) Feld(K::StaticClass(), TEXT(#F), (int32)STRUCT_OFFSET(K, F));
#include "TDLayoutDiagnose.inl"
#undef TD_OFF

		// Felder im Spiel, die unsere Header nicht kennen (neu hinzugekommen)
		int32 Neu = 0;
		for (UClass* Klasse : { AFGBuildable::StaticClass(), AFGBuildableFactory::StaticClass(), AFGBuildableManufacturer::StaticClass(), AFGBuildableConveyorBase::StaticClass() })
		{
			for (TFieldIterator<FProperty> It(Klasse, EFieldIteratorFlags::ExcludeSuper); It; ++It)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung Spielfeld %s::%s Offset %d Groesse %d"),
					*Klasse->GetName(), *It->GetName(), It->GetOffset_ForInternal(), It->GetSize());
				++Neu;
			}
		}

		// Runde 2: Schnittstellen (Interface-Basen verschieben die Daten) und Typ des neuen Feldes
		for (UClass* Klasse : { AFGBuildable::StaticClass(), AFGBuildableFactory::StaticClass(), AFGBuildableManufacturer::StaticClass(), AFGBuildableConveyorBase::StaticClass() })
		{
			for (const FImplementedInterface& I : Klasse->Interfaces)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung Schnittstelle %s: %s Offset %d nativ %d"),
					*Klasse->GetName(), I.Class ? *I.Class->GetName() : TEXT("?"), I.PointerOffset, I.bImplementedByK2 ? 0 : 1);
			}
		}
		{
			AFGBuildableFactory* F = reinterpret_cast<AFGBuildableFactory*>(0x10000);
			UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung Schnittstelle gebaut: FGSignificanceInterface %d, FGConditionalReplicationInterface %d"),
				(int32)((UPTRINT)static_cast<IFGSignificanceInterface*>(F) - 0x10000), (int32)((UPTRINT)static_cast<IFGConditionalReplicationInterface*>(F) - 0x10000));
			if (FProperty* P = AFGBuildableFactory::StaticClass()->FindPropertyByName(TEXT("mFactoryConnectionStates")))
			{
				FString Typ = P->GetCPPType();
				if (FObjectPropertyBase* O = CastField<FObjectPropertyBase>(P)) Typ += TEXT(" ") + GetNameSafe(O->PropertyClass);
				UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung neues Feld mFactoryConnectionStates: %s (%s) Flags %llx"), *Typ, *P->GetClass()->GetName(), (uint64)P->PropertyFlags);
			}
		}

		UE_LOG(LogFicsitTD, Warning, TEXT("Layout-Pruefung Ergebnis: Groessen %d/%d abweichend, Felder %d/%d abweichend, %d fehlen, %d Spielfelder gelistet"),
			GroessenFalsch, Groessen, FelderFalsch, Felder, FelderFehlen, Neu);
	}
};

void TDLayoutPruefen() { FTDLayoutDiagnose::Pruefen(); }
