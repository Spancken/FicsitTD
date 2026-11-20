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
#include "TDGunTurret.h"
#include "Misc/CoreDelegates.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformTypes.h"
extern "C" __declspec(dllimport) DWORD __stdcall K32GetModuleBaseNameW(HANDLE, HMODULE, LPWSTR, DWORD);

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

		// Warnung nur bei verschobenen Feldern der Fabrik-/Band-Klassen; bekannte Groessenabweichungen (GameState, GameUI,
		// ConveyorAttachment, ConveyorLift) betreffen keine Felder, die FicsitTD liest (README 20.11.)
		if (FelderFalsch > 0 || FelderFehlen > 0)
			UE_LOG(LogFicsitTD, Warning, TEXT("Layout-Pruefung: verschobene Felder - Header passen nicht zum Spiel"));
		UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung Ergebnis: Groessen %d/%d abweichend, Felder %d/%d abweichend, %d fehlen, %d Spielfelder gelistet"),
			GroessenFalsch, Groessen, FelderFalsch, Felder, FelderFehlen, Neu);
	}
};


// 20.11.2026: Turmfenster stuerzt ab (UpdateRotation) - vtable des Turms gegen die des Spiels vergleichen.
static void* TDZiel(void* Adr)
{
	// Import-Sprung "jmp [rip+disp32]" (FF 25) bis zur echten Funktion verfolgen
	for (int32 i = 0; i < 4 && Adr; ++i)
	{
		const uint8* B = (const uint8*)Adr;
		if (B[0] == 0xFF && B[1] == 0x25) { Adr = *(void**)(B + 6 + *(const int32*)(B + 2)); continue; }
		if (B[0] == 0xE9) { Adr = (void*)(B + 5 + *(const int32*)(B + 1)); continue; }
		break;
	}
	return Adr;
}
static FString TDModul(void* Adr, uint64& Rva)
{
	HMODULE M = nullptr; Rva = 0;
	if (!Adr || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)Adr, &M) || !M) return TEXT("-");
	WCHAR Name[MAX_PATH]; K32GetModuleBaseNameW(GetCurrentProcess(), M, Name, MAX_PATH);
	Rva = (uint64)((UPTRINT)Adr - (UPTRINT)M);
	FString S(Name); S.ReplaceInline(TEXT("FactoryGameSteam-"), TEXT("")); S.ReplaceInline(TEXT("-Win64-Shipping.dll"), TEXT(""));
	return S;
}
static void TDVergleicheTabelle(const TCHAR* Was, void** Spiel, void** Wir, int32 Max)
{
	int32 Gleich = 0, Ueberschrieben = 0, Verschoben = 0;
	for (int32 i = 0; i < Max; ++i)
	{
		uint64 RS, RW;
		void* S = TDZiel(Spiel[i]); void* W = TDZiel(Wir[i]);
		const FString MS = TDModul(S, RS), MW = TDModul(W, RW);
		if (MS == TEXT("-") || MW == TEXT("-")) { UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung VTable %s Ende bei Slot %d (Spiel %s, wir %s)"), Was, i, *MS, *MW); break; }
		if (S == W) { ++Gleich; continue; }
		if (MW.Contains(TEXT("FicsitTD"))) { ++Ueberschrieben; continue; }
		++Verschoben;
		if (Verschoben <= 5) UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung VTable %s Slot %d: Spiel %s+%llx, wir %s+%llx  <== ABWEICHUNG"), Was, i, *MS, RS, *MW, RW);
	}
	// 20.11.: nur Log - am Tabellenende liest der Vergleich in die naechste Tabelle bzw. unsere eigenen virtuellen Funktionen (keine echte Abweichung).
	// Aussagekraeftig ist der Beginn einer Abweichung innerhalb einer Schnittstelle (so wurde GetDependantHighPriorityReplicationActors gefunden).
	UE_LOG(LogFicsitTD, Log, TEXT("Layout-Pruefung VTable %s: %d gleich, %d von FicsitTD ueberschrieben, %d abweichend"), Was, Gleich, Ueberschrieben, Verschoben);
}
static void TDVTablePruefen()
{
	UObject* Spiel = AFGBuildableManufacturer::StaticClass()->GetDefaultObject();
	UObject* Wir = ATDGunTurret::StaticClass()->GetDefaultObject();
	if (!Spiel || !Wir) { UE_LOG(LogFicsitTD, Warning, TEXT("Layout-Pruefung VTable: kein CDO")); return; }
	TDVergleicheTabelle(TEXT("Primaer"), *(void***)Spiel, *(void***)Wir, 1200);
	for (UClass* K = AFGBuildableManufacturer::StaticClass(); K && K != AActor::StaticClass(); K = K->GetSuperClass())
	{
		for (const FImplementedInterface& I : K->Interfaces)
		{
			TDVergleicheTabelle(*I.Class->GetName(), *(void***)((uint8*)Spiel + I.PointerOffset), *(void***)((uint8*)Wir + I.PointerOffset), 200);
		}
	}
}
void TDLayoutPruefen()
{
	FTDLayoutDiagnose::Pruefen();
	FCoreDelegates::OnPostEngineInit.AddStatic(&TDVTablePruefen);
}
