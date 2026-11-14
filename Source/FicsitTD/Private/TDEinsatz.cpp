#include "TDEinsatz.h"
#include "Hologram/FGBlueprintHologram.h"
#include "TDGate.h"
#include "TDDimensionalDepot.h"
#include "TDWellenStatistik.h"
#include "TDForschung.h"
#include "TDPionier.h"
#include "TDLaufProtokoll.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "UObject/TextProperty.h"
#include "TDBarrier.h"
#include "TDDamageable.h"
#include "TDDefenseCore.h"
#include "TDDefenseWall.h"
#include "TDTestWall.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildablePipeReservoir.h"
#include "Buildables/FGBuildableResourceExtractor.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "Buildables/FGBuildablePoleBase.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildableTradingPost.h"
#include "Buildables/FGBuildableMAM.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Hologram/FGHologram.h"
#include "Resources/FGItemDescriptor.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Resources/FGResourceDescriptor.h"
#include "FGUnlockSubsystem.h"
#include "Unlocks/FGUnlockScannableResource.h"
#include "FGBuildableSubsystem.h"
#include "TDKernTerminal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "FGSaveSystem.h"
#include "FGChatManager.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGResearchManager.h"
#include "FGResearchTree.h"
#include "FGSchematicManager.h"
#include "FGPlayerState.h"
#include "ShoppingList/FGShoppingListComponent.h"
#include "ShoppingList/FGShoppingListObject.h"

/** Startkontingent Stufe 1 (Einsatz-Konzept Abschnitt 3): genau die erste Kette Erz -> Barren -> Rohbolzen -> Bolzenturm */
static const TPair<const TCHAR*, int32> StartKontingent[] = {
	{ TEXT("Abwehrkern"), 1 },
	{ TEXT("Miner"), 1 },
	{ TEXT("Schmelzofen"), 1 },
	{ TEXT("Konstruktor"), 1 },
	{ TEXT("Biomassegenerator"), 1 },
	{ TEXT("Lager"), 0 },        // Marius 02.11.: Lager, Splitter, Verbinder nicht im Start, nur im Laden
	{ TEXT("Bolzenturm"), 1 },
	{ TEXT("Mauer"), 5 },
	{ TEXT("MAM"), 1 },          // Basis: HUB und MAM setzt der Spieler am Start selbst (kostenlos)
	{ TEXT("Splitter"), 0 },
	{ TEXT("Verbinder"), 0 },
};

/** Startressourcen zusaetzlich zum Baumaterial des Startkontingents (das wird aus den Rezepten berechnet, Marius 02.11.):
 *  nur Brennstoff fuer HUB-Brenner und Biomassegenerator */
static const TPair<const TCHAR*, int32> StartRessourcen[] = {
	{ TEXT("/Game/FactoryGame/Resource/Parts/GenericBiomass/Desc_Leaves.Desc_Leaves_C"), 200 },
	{ TEXT("/Game/FactoryGame/Resource/Parts/GenericBiomass/Desc_Wood.Desc_Wood_C"), 100 },
	{ TEXT("/Game/FactoryGame/Resource/Parts/GenericBiomass/Desc_GenericBiomass.Desc_GenericBiomass_C"), 300 },   // Test 0.27: Strom fuer HUB-Brenner (~30 min)
};

/** Laden (Einsatz-Konzept Abschnitt 4): Preis = Grundpreis x (1 + Aufschlag x bisher gekauft); Paket = Gebaeude je Kauf;
 *  kaufbar erst, wenn das Bau-Rezept verfuegbar ist (Start oder MAM) */
struct FTDLadenEintrag
{
	const TCHAR* Key;
	int32 Grundpreis;
	float Aufschlag;
	int32 Paket;
	const TCHAR* Rezept;
};
static const FTDLadenEintrag LadenListe[] = {
	{ TEXT("Miner"), 6, 0.5f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C") },
	{ TEXT("Schmelzofen"), 4, 0.3f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterBasicMk1.Recipe_SmelterBasicMk1_C") },
	{ TEXT("Konstruktor"), 4, 0.3f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1.Recipe_ConstructorMk1_C") },
	{ TEXT("Assembler"), 8, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_AssemblerMk1.Recipe_AssemblerMk1_C") },
	{ TEXT("Giesserei"), 8, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterMk1.Recipe_SmelterMk1_C") },   // Rezept der Giesserei heisst so
	{ TEXT("Biomassegenerator"), 6, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorBiomass_Automated.Recipe_GeneratorBiomass_Automated_C") },
	{ TEXT("Kohlekraftwerk"), 14, 0.6f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorCoal.Recipe_GeneratorCoal_C") },
	{ TEXT("Wasserpumpe"), 4, 0.3f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_WaterPump.Recipe_WaterPump_C") },
	{ TEXT("Lager"), 4, 0.5f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_StorageContainerMk1.Recipe_StorageContainerMk1_C") },
	{ TEXT("Bolzenturm"), 5, 0.3f, 1, TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_BoltTurret.Recipe_TD_BoltTurret_C") },
	{ TEXT("Gewehrturm"), 8, 0.3f, 1, TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_GunTurret.Recipe_TD_GunTurret_C") },
	{ TEXT("Teslaspule"), 10, 0.4f, 1, TEXT("/FicsitTD/Buildings/TeslaCoil/Recipe_TD_TeslaCoil.Recipe_TD_TeslaCoil_C") },
	{ TEXT("Mauer"), 3, 0.2f, 5, TEXT("/FicsitTD/Buildings/DefenseWall/Recipe_TD_DefenseWall.Recipe_TD_DefenseWall_C") },
	{ TEXT("Panzersperre"), 3, 0.2f, 5, TEXT("/FicsitTD/Buildings/Barrier/Recipe_TD_Barrier.Recipe_TD_Barrier_C") },
	// 0.9.3: Reparaturstation durch den Drohnenplatz ersetzt (Marius 09.11.2026)
	{ TEXT("Drohnenplatz"), 14, 0.5f, 1, TEXT("/FicsitTD/Buildings/DronePort/Recipe_TD_DronePort.Recipe_TD_DronePort_C") },
	// 0.81 passive Bauten P1 (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07): Tor ab Mauern Mk2, Lockfeuer, Oelschlick-Feld ab Raffinerie
	{ TEXT("Tor"), 4, 0.3f, 1, TEXT("/FicsitTD/Buildings/Gate/Recipe_TD_Gate.Recipe_TD_Gate_C") },
	{ TEXT("Lockfeuer"), 4, 0.4f, 1, TEXT("/FicsitTD/Buildings/LureBeacon/Recipe_TD_LureBeacon.Recipe_TD_LureBeacon_C") },
	{ TEXT("Oelschlick"), 4, 0.3f, 1, TEXT("/FicsitTD/Buildings/SlickPlate/Recipe_TD_SlickPlate.Recipe_TD_SlickPlate_C") },
	// 0.83 Bauten P2/P3 (Konzept-Rohstoffe-Tuerme-Bauten-2026-11-07)
	{ TEXT("Elektrozaun"), 5, 0.3f, 1, TEXT("/FicsitTD/Buildings/ConductorWall/Recipe_TD_ConductorWall.Recipe_TD_ConductorWall_C") },
	{ TEXT("Funkmast"), 8, 0.5f, 1, TEXT("/FicsitTD/Buildings/RadioMast/Recipe_TD_RadioMast.Recipe_TD_RadioMast_C") },
	{ TEXT("Schildgenerator"), 12, 0.5f, 1, TEXT("/FicsitTD/Buildings/ShieldGenerator/Recipe_TD_ShieldGenerator.Recipe_TD_ShieldGenerator_C") },
	{ TEXT("Himmelsturm"), 10, 0.4f, 1, TEXT("/FicsitTD/Buildings/FlakTurret/Recipe_TD_FlakTurret.Recipe_TD_FlakTurret_C") },
	{ TEXT("Stoerfeld"), 14, 0.5f, 1, TEXT("/FicsitTD/Buildings/FieldProjector/Recipe_TD_FieldProjector.Recipe_TD_FieldProjector_C") },
	{ TEXT("Singularitaet"), 20, 0.6f, 1, TEXT("/FicsitTD/Buildings/SingularityLauncher/Recipe_TD_SingularityLauncher.Recipe_TD_SingularityLauncher_C") },
	// 0.9.3: Dimensionslager gestrichen (Marius 09.11.2026)
	// Wellen-Progression P2: Leiter L4 (Feuer, Chemie, Raffinerie)
	{ TEXT("Flammenturm"), 8, 0.3f, 1, TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_FlameTurret.Recipe_TD_FlameTurret_C") },
	{ TEXT("Nobelisk-Werfer"), 10, 0.3f, 1, TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_NobeliskLauncher.Recipe_TD_NobeliskLauncher_C") },
	{ TEXT("Oelpumpe"), 6, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_OilPump.Recipe_OilPump_C") },
	{ TEXT("Raffinerie"), 8, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_OilRefinery.Recipe_OilRefinery_C") },
	{ TEXT("Treibstoffkraftwerk"), 8, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorFuel.Recipe_GeneratorFuel_C") },
	// 0.9.1 Endgame-Konzept (Marius 09.11.2026): spaete Maschinen. Ohne sie waren Schild, Himmelsturm, Stoerfeld nicht herstellbar.
	{ TEXT("Fluessigkeitspuffer"), 3, 0.3f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeStorageTank.Recipe_PipeStorageTank_C") },
	{ TEXT("Fertiger"), 14, 0.5f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ManufacturerMk1.Recipe_ManufacturerMk1_C") },
	{ TEXT("Mischer"), 10, 0.5f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Blender.Recipe_Blender_C") },
	{ TEXT("Abfueller"), 8, 0.4f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Packager.Recipe_Packager_C") },
	{ TEXT("Foerderanlage"), 6, 0.4f, 3, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_FrackingExtractor.Recipe_FrackingExtractor_C") },
	{ TEXT("Splitter"), 2, 0.2f, 2, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter.Recipe_ConveyorAttachmentSplitter_C") },
	{ TEXT("Verbinder"), 2, 0.2f, 2, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMerger.Recipe_ConveyorAttachmentMerger_C") },
	// Spieler aufbessern (Marius 03.11.2026, 0.56): EP-Ausgaben ohne neue Technik; Rezept = ab wann kaufbar
	{ TEXT("Inventarplätze"), 6, 0.5f, 5, TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_BoltTurret.Recipe_TD_BoltTurret_C") },
	{ TEXT("Handplatz"), 8, 0.5f, 1, TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_BoltTurret.Recipe_TD_BoltTurret_C") },
	{ TEXT("Armierpistole"), 6, 0.f, 1, TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_BoltTurret.Recipe_TD_BoltTurret_C") },
	{ TEXT("Klingenläufer"), 8, 0.f, 1, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_AssemblerMk1.Recipe_AssemblerMk1_C") },
	{ TEXT("Gewehr"), 10, 0.f, 1, TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_GunTurret.Recipe_TD_GunTurret_C") },
	{ TEXT("Nobelisken-Zünder"), 12, 0.f, 1, TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_NobeliskLauncher.Recipe_TD_NobeliskLauncher_C") },
	// Kern reparieren (Marius 03.11.2026, 0.57): teuer und mit jedem Kauf teurer; Paket = Prozent der Kern-LP
	{ TEXT("Kernreparatur"), 15, 0.5f, 25, TEXT("/FicsitTD/Buildings/DefenseCore/Recipe_TD_DefenseCore.Recipe_TD_DefenseCore_C") },
};

/** Spieler-Posten: Wirkung statt Kontingent. Art 0 = Ausruestung ins Inventar, 1 = Inventarplaetze, 2 = Handplatz, 3 = Kern heilen (Prozent).
 *  Max 0 = unbegrenzt */
struct FTDSpielerPosten
{
	const TCHAR* Key;
	int32 Art;
	int32 Max;
	/** Ausruestung (Art 0) bzw. nur Symbol */
	const TCHAR* Item;
};
static const FTDSpielerPosten SpielerPosten[] = {
	{ TEXT("Inventarplätze"), 1, 6, TEXT("/Game/FactoryGame/Buildable/Factory/StorageContainerMk1/Desc_StorageContainerMk1.Desc_StorageContainerMk1_C") },
	{ TEXT("Handplatz"), 2, 3, TEXT("/Game/FactoryGame/Resource/Parts/ModularFrame/Desc_ModularFrame.Desc_ModularFrame_C") },
	{ TEXT("Armierpistole"), 0, 1, TEXT("/Game/FactoryGame/Resource/Equipment/NailGun/Desc_RebarGunProjectile.Desc_RebarGunProjectile_C") },
	{ TEXT("Klingenläufer"), 0, 1, TEXT("/Game/FactoryGame/Resource/Equipment/JumpingStilts/BP_EquipmentDescriptorJumpingStilts.BP_EquipmentDescriptorJumpingStilts_C") },
	{ TEXT("Gewehr"), 0, 1, TEXT("/Game/FactoryGame/Resource/Equipment/Rifle/BP_EquipmentDescriptorRifle.BP_EquipmentDescriptorRifle_C") },
	{ TEXT("Nobelisken-Zünder"), 0, 1, TEXT("/Game/FactoryGame/Resource/Equipment/NobeliskDetonator/BP_EquipmentDescriptorNobeliskDetonator.BP_EquipmentDescriptorNobeliskDetonator_C") },
	{ TEXT("Kernreparatur"), 3, 0, TEXT("/Game/FactoryGame/Resource/Parts/IronPlateReinforced/Desc_IronPlateReinforced.Desc_IronPlateReinforced_C") },
};
static const FTDSpielerPosten* SpielerFuer(const FString& Key)
{
	for (const FTDSpielerPosten& P : SpielerPosten)
	{
		if (Key == P.Key)
		{
			return &P;
		}
	}
	return nullptr;
}
static int32 SpielerMax(const FTDSpielerPosten& P)
{
	return TDBalance::Ganz(FString::Printf(TEXT("Einsatz.Laden.%s.Max"), P.Key), P.Max);
}

static FString TDAnzeige(const FString& Key);
FString UTDEinsatzSubsystem::Anzeige(const FString& Key)
{
	return TDAnzeige(Key);
}

bool UTDEinsatzSubsystem::IstSpielerPosten(const FString& Key)
{
	return SpielerFuer(Key) != nullptr;
}

static const FTDLadenEintrag* LadenFuer(const FString& Key)
{
	for (const FTDLadenEintrag& E : LadenListe)
	{
		if (Key == E.Key)
		{
			return &E;
		}
	}
	return nullptr;
}

/** Ladenwerte aus der Balance-Datei (Einsatz.Laden.<Key>.*); die Tabelle oben ist der Standard */
static float LadenGrundpreis(const FTDLadenEintrag& E) { return TDBalance::Wert(FString::Printf(TEXT("Einsatz.Laden.%s.Grundpreis"), E.Key), E.Grundpreis); }
static float LadenAufschlag(const FTDLadenEintrag& E) { return TDBalance::Wert(FString::Printf(TEXT("Einsatz.Laden.%s.Aufschlag"), E.Key), E.Aufschlag); }
static int32 LadenPaket(const FTDLadenEintrag& E) { return FMath::Max(1, TDBalance::Ganz(FString::Printf(TEXT("Einsatz.Laden.%s.Paket"), E.Key), E.Paket)); }
static int32 LadenPreis(const FTDLadenEintrag& E, int32 Bisher) { return FMath::RoundToInt(LadenGrundpreis(E) * (1.f + LadenAufschlag(E) * Bisher)); }
/** Startkontingent aus der Balance-Datei (Einsatz.Start.<Key>) */
static int32 StartMenge(const TCHAR* Key, int32 Standard) { return TDBalance::Ganz(FString::Printf(TEXT("Einsatz.Start.%s"), Key), Standard); }

/** Gebaeude und Rezepte, die Stufe 1 braucht - im neuen Spiel sonst erst ueber den HUB */
static const TCHAR* StartRezepte[] = {
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_MinerMk1.Recipe_MinerMk1_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_SmelterBasicMk1.Recipe_SmelterBasicMk1_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConstructorMk1.Recipe_ConstructorMk1_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_GeneratorBiomass_Automated.Recipe_GeneratorBiomass_Automated_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_StorageContainerMk1.Recipe_StorageContainerMk1_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorPole.Recipe_ConveyorPole_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentSplitter.Recipe_ConveyorAttachmentSplitter_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorAttachmentMerger.Recipe_ConveyorAttachmentMerger_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerPoleMk1.Recipe_PowerPoleMk1_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"),
	TEXT("/Game/FactoryGame/Recipes/Smelter/Recipe_IngotIron.Recipe_IngotIron_C"),
	TEXT("/Game/FactoryGame/Recipes/Constructor/Recipe_IronRod.Recipe_IronRod_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Mam.Recipe_Mam_C"),
	TEXT("/Game/FactoryGame/Recipes/Constructor/Recipe_Biomass_Leaves.Recipe_Biomass_Leaves_C"),
	TEXT("/Game/FactoryGame/Recipes/Constructor/Recipe_Biomass_Wood.Recipe_Biomass_Wood_C"),
	TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_TradingPost.Recipe_TradingPost_C"),
};
static const TCHAR* KernKlassePfad = TEXT("/FicsitTD/Buildings/DefenseCore/Build_TD_DefenseCore.Build_TD_DefenseCore_C");
static const TCHAR* KernRezeptPfad = TEXT("/FicsitTD/Buildings/DefenseCore/Recipe_TD_DefenseCore.Recipe_TD_DefenseCore_C");

/** Verteidigungsfeld um den Kern, in dem waehrend einer Welle nicht gebaut werden darf (cm) */
static constexpr float EinsatzFeldRadius = 12000.f;

/** 0.85 (L1): Anzeigename eines Kontingent- bzw. Laden-Schluessels. Die Schluessel selbst bleiben deutsch
 *  (Vergleiche, Balance-Datei, Spielstand mKontingentGekauft, Laufprotokoll) - nur die Ausgabe an den Spieler wird uebersetzt. */
static FString TDAnzeige(const FString& Key)
{
	if (!TDSprache::Englisch())
	{
		// 0.9.6 (Gesamtreview 12.11.: Kern-Menue zeigte "Oelschlick", "Giesserei"): Schluessel ohne Umlaute bekommen ihren Spielnamen
		static const TMap<FString, FString> De = {
			{ TEXT("Giesserei"), TEXT("Gießerei") }, { TEXT("Oelschlick"), TEXT("Ölschlick-Feld") }, { TEXT("Stoerfeld"), TEXT("Störfeld") },
			{ TEXT("Singularitaet"), TEXT("Singularitätswerfer") }, { TEXT("Oelpumpe"), TEXT("Ölpumpe") }, { TEXT("Abfueller"), TEXT("Abfüller") },
			{ TEXT("Foerderanlage"), TEXT("Förderanlage") }, { TEXT("Fluessigkeitspuffer"), TEXT("Flüssigkeitspuffer") },
			{ TEXT("Flammenturm"), TEXT("Flammenwerfer") }, { TEXT("Splitter"), TEXT("Förderbandsplitter") }, { TEXT("Wasserpumpe"), TEXT("Wasserextraktor") },
		};
		const FString* Name = De.Find(Key);
		return Name ? *Name : Key;
	}
	static const TMap<FString, FString> En = {
		{ TEXT("Abwehrkern"), TEXT("Defense Core") }, { TEXT("Miner"), TEXT("Miner") }, { TEXT("Schmelzofen"), TEXT("Smelter") },
		{ TEXT("Konstruktor"), TEXT("Constructor") }, { TEXT("Biomassegenerator"), TEXT("Biomass Burner") }, { TEXT("Lager"), TEXT("Storage Container") },
		{ TEXT("Bolzenturm"), TEXT("Bolt Turret") }, { TEXT("Mauer"), TEXT("Wall") }, { TEXT("MAM"), TEXT("MAM") },
		{ TEXT("Splitter"), TEXT("Conveyor Splitter") }, { TEXT("Verbinder"), TEXT("Conveyor Merger") }, { TEXT("AWESOME-Sink"), TEXT("AWESOME Sink") },
		{ TEXT("Panzersperre"), TEXT("Barrier") }, { TEXT("Tor"), TEXT("Gate") }, { TEXT("Dimensionslager"), TEXT("Dimensional Storage") },
		{ TEXT("Elektrozaun"), TEXT("Electric Fence") }, { TEXT("Funkmast"), TEXT("Radio Mast") }, { TEXT("Schildgenerator"), TEXT("Shield Generator") },
		{ TEXT("Himmelsturm"), TEXT("Sky Tower") }, { TEXT("Stoerfeld"), TEXT("Jammer Projector") }, { TEXT("Singularitaet"), TEXT("Singularity Launcher") },
		{ TEXT("Gewehrturm"), TEXT("Rifle Turret") }, { TEXT("Flammenturm"), TEXT("Flamethrower") }, { TEXT("Flammenwerfer"), TEXT("Flamethrower") },
		{ TEXT("Fertiger"), TEXT("Manufacturer") }, { TEXT("Mischer"), TEXT("Blender") }, { TEXT("Abfueller"), TEXT("Packager") },
		{ TEXT("Foerderanlage"), TEXT("Resource Well") }, { TEXT("Fluessigkeitspuffer"), TEXT("Fluid Buffer") },
		{ TEXT("Teslaspule"), TEXT("Tesla Coil") }, { TEXT("Nobelisk-Werfer"), TEXT("Nobelisk Launcher") }, { TEXT("Reparaturstation"), TEXT("Repair Station") }, { TEXT("Drohnenplatz"), TEXT("Drone Pad") },
		{ TEXT("Lockfeuer"), TEXT("Lure Fire") }, { TEXT("Oelschlick"), TEXT("Oil Slick Field") }, { TEXT("Wasserpumpe"), TEXT("Water Extractor") },
		{ TEXT("Oelpumpe"), TEXT("Oil Extractor") }, { TEXT("Foerderanlage"), TEXT("Extractor") }, { TEXT("Kohlekraftwerk"), TEXT("Coal-Powered Generator") },
		{ TEXT("Treibstoffkraftwerk"), TEXT("Fuel-Powered Generator") }, { TEXT("Atomkraftwerk"), TEXT("Nuclear Power Plant") }, { TEXT("Kraftwerk"), TEXT("Power Generator") },
		{ TEXT("Giesserei"), TEXT("Foundry") }, { TEXT("Assembler"), TEXT("Assembler") }, { TEXT("Fertiger"), TEXT("Manufacturer") },
		{ TEXT("Raffinerie"), TEXT("Refinery") }, { TEXT("Abfueller"), TEXT("Packager") }, { TEXT("Mischer"), TEXT("Blender") },
		{ TEXT("Fabrik"), TEXT("Factory") }, { TEXT("Fluessigkeitspuffer"), TEXT("Fluid Buffer") }, { TEXT("Inventarplätze"), TEXT("Inventory Slots") },
		{ TEXT("Handplatz"), TEXT("Hand Slot") }, { TEXT("Armierpistole"), TEXT("Rebar Gun") }, { TEXT("Klingenläufer"), TEXT("Blade Runners") },
		{ TEXT("Gewehr"), TEXT("Rifle") }, { TEXT("Nobelisken-Zünder"), TEXT("Nobelisk Detonator") }, { TEXT("Kernreparatur"), TEXT("Core Repair") },
	};
	const FString* Gefunden = En.Find(Key);
	return Gefunden ? *Gefunden : Key;
}

/** Sperrtexte der Bau-Hologramme: stehen am CDO. Im Konstruktor ist die Spielsprache evtl. noch nicht die endgueltige,
 *  deshalb setzt PruefeHologramm sie bei Bedarf neu (TDAktualisiereSperrTexte) */
static FText TDTextKontingent() { return TDTX(TEXT("Kontingent erschöpft (/td einsatz status)"), TEXT("Quota used up (/td einsatz status)")); }
static FText TDTextSinkGesperrt() { return TDTX(TEXT("Im Einsatz gesperrt: FICSIT-Coupons gibt es nur vom Abwehrkern"), TEXT("Locked during an operation: FICSIT Coupons only come from the Defense Core")); }
static FText TDTextBlaupause() { return TDTX(TEXT("Im Einsatz gesperrt: Blaupausen sind nicht erlaubt"), TEXT("Locked during an operation: blueprints are not allowed")); }
static FText TDTextWellenSperre() { return TDTX(TEXT("Während der Welle ist Bauen am Kern gesperrt"), TEXT("Building near the Core is locked during a wave")); }

UTDCDBlaupause::UTDCDBlaupause()
{
	mDisqfualifyingText = TDTextBlaupause();
}

UTDCDKontingent::UTDCDKontingent()
{
	mDisqfualifyingText = TDTextKontingent();
}

UTDCDSinkGesperrt::UTDCDSinkGesperrt()
{
	mDisqfualifyingText = TDTextSinkGesperrt();
}

UTDCDWellenSperre::UTDCDWellenSperre()
{
	mDisqfualifyingText = TDTextWellenSperre();
}

static void TDAktualisiereSperrTexte()
{
	static int32 Stand = -1;
	const int32 Jetzt = TDSprache::Englisch() ? 1 : 0;
	if (Stand == Jetzt)
	{
		return;
	}
	FTextProperty* Feld = FindFProperty<FTextProperty>(UFGConstructDisqualifier::StaticClass(), TEXT("mDisqfualifyingText"));
	if (!Feld)
	{
		return;
	}
	Stand = Jetzt;
	Feld->SetPropertyValue_InContainer(GetMutableDefault<UTDCDKontingent>(), TDTextKontingent());
	Feld->SetPropertyValue_InContainer(GetMutableDefault<UTDCDSinkGesperrt>(), TDTextSinkGesperrt());
	Feld->SetPropertyValue_InContainer(GetMutableDefault<UTDCDWellenSperre>(), TDTextWellenSperre());
	Feld->SetPropertyValue_InContainer(GetMutableDefault<UTDCDBlaupause>(), TDTextBlaupause());
}

bool UTDEinsatzSubsystem::IstLogistik(const UClass* BauKlasse)
{
	return BauKlasse && (BauKlasse->IsChildOf(AFGBuildableConveyorBase::StaticClass()) || BauKlasse->IsChildOf(AFGBuildableConveyorAttachment::StaticClass())
		|| BauKlasse->IsChildOf(AFGBuildablePoleBase::StaticClass()) || BauKlasse->IsChildOf(AFGBuildablePowerPole::StaticClass())
		|| BauKlasse->IsChildOf(AFGBuildableWire::StaticClass()) || BauKlasse->IsChildOf(AFGBuildablePipeline::StaticClass())
		|| BauKlasse->IsChildOf(AFGBuildablePipelineJunction::StaticClass()));
}

bool UTDEinsatzSubsystem::Kostenlos(const UClass* BauKlasse) const
{
	return mAktiv && BauKlasse && (IstLogistik(BauKlasse) || BauKlasse->IsChildOf(AFGBuildableTradingPost::StaticClass())
		|| BauKlasse->IsChildOf(AFGBuildableMAM::StaticClass()));
}

/** Originalzutaten der kostenlos gemachten Rezepte (prozessweit, die Rezept-CDOs leben laenger als eine Welt) */
static TMap<TWeakObjectPtr<UClass>, TArray<FItemAmount>>& GratisOriginale()
{
	static TMap<TWeakObjectPtr<UClass>, TArray<FItemAmount>> Originale;
	return Originale;
}

static TArray<FItemAmount>* RezeptZutaten(UClass* Rezept)
{
	static FArrayProperty* Feld = FindFProperty<FArrayProperty>(UFGRecipe::StaticClass(), TEXT("mIngredients"));
	UObject* CDO = Rezept ? Rezept->GetDefaultObject() : nullptr;
	return Feld && CDO ? Feld->ContainerPtrToValuePtr<TArray<FItemAmount>>(CDO) : nullptr;
}

void UTDEinsatzSubsystem::AktualisiereGratisRezepte()
{
	if (!mAktiv)
	{
		StelleRezepteWiederHer();
		return;
	}
	AFGRecipeManager* Rezepte = AFGRecipeManager::Get(GetWorld());
	if (!Rezepte)
	{
		return;
	}
	int32 Neu = 0;
	for (const TSubclassOf<UFGRecipe>& Rezept : Rezepte->GetAllAvailableRecipes())
	{
		if (!Rezept || GratisOriginale().Contains(Rezept.Get()))
		{
			continue;
		}
		const TArray<FItemAmount> Produkte = UFGRecipe::GetProducts(Rezept);
		UClass* Produkt = Produkte.Num() > 0 ? Produkte[0].ItemClass.Get() : nullptr;
		UClass* Gebaeude = Produkt && Produkt->IsChildOf(UFGBuildingDescriptor::StaticClass()) ? UFGBuildingDescriptor::GetBuildableClass(Produkt).Get() : nullptr;
		TArray<FItemAmount>* Zutaten = Kostenlos(Gebaeude) ? RezeptZutaten(Rezept) : nullptr;
		if (Zutaten)
		{
			GratisOriginale().Add(Rezept.Get(), *Zutaten);
			Zutaten->Reset();
			++Neu;
		}
	}
	if (Neu > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: %d Rezepte kostenlos gemacht (Logistik, HUB, MAM), insgesamt %d"), Neu, GratisOriginale().Num());
	}
}

void UTDEinsatzSubsystem::SchalteScannerFrei()
{
	AFGRecipeManager* Rezepte = AFGRecipeManager::Get(GetWorld());
	AFGUnlockSubsystem* Freischaltung = AFGUnlockSubsystem::Get(GetWorld());
	if (!mAktiv || !Rezepte || !Freischaltung)
	{
		return;
	}
	TSet<UClass*> Brauche;
	for (const TSubclassOf<UFGRecipe>& Rezept : Rezepte->GetAllAvailableRecipes())
	{
		if (!Rezept)
		{
			continue;
		}
		for (const FItemAmount& Zutat : UFGRecipe::GetIngredients(GetWorld(), Rezept))
		{
			if (Zutat.ItemClass && Zutat.ItemClass->IsChildOf(UFGResourceDescriptor::StaticClass()))
			{
				Brauche.Add(Zutat.ItemClass.Get());
			}
		}
		// Kraftwerke verbrauchen ihren Brennstoff nicht ueber ein Rezept
		if (Rezept->GetName().Contains(TEXT("GeneratorCoal")))
		{
			if (UClass* Kohle = LoadClass<UFGResourceDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/RawResources/Coal/Desc_Coal.Desc_Coal_C")))
			{
				Brauche.Add(Kohle);
			}
		}
	}
	const TArray<TSubclassOf<UFGResourceDescriptor>> Schon = Freischaltung->GetScannableResources();
	TArray<FString> Neu;
	for (UClass* Rohstoff : Brauche)
	{
		if (!Schon.Contains(Rohstoff))
		{
			Freischaltung->UnlockScannableResource(FScannableResourcePair(Rohstoff));
			Neu.Add(UFGItemDescriptor::GetItemName(Rohstoff).ToString());
		}
	}
	if (Neu.Num() > 0)
	{
		Melde(TDF(TEXT("Ressourcenscanner (V): jetzt suchbar - %s"), TEXT("Resource Scanner (V): now scannable - %s"), *FString::Join(Neu, TEXT(", "))));
	}
}

void UTDEinsatzSubsystem::PruefeForschungsbaeume()
{
	// 0.83.1 (Marius 07.11.: eine neue Seite wie Ballistik erschien erst nach Schliessen und Oeffnen des MAM): das offene MAM baut
	// die Liste der Baeume nicht neu auf. Deshalb sind alle vier Baeume ab Start freigeschaltet (create_research.py ohne
	// Baum-Voraussetzung); ihre Wurzeln bleiben grau, bis die Voraussetzung erforscht ist. Hier nur noch fuer alte Spielstaende,
	// in denen Ballistik oder Elementar noch gesperrt sind - vor dem ersten Oeffnen des MAM, ohne Meldung.
	AFGResearchManager* Forschung = AFGResearchManager::Get(GetWorld());
	if (!Forschung)
	{
		return;
	}
	static const TCHAR* Baeume[] = {
		TEXT("/FicsitTD/Research/BPD_ResearchTree_TD_Industrie.BPD_ResearchTree_TD_Industrie_C"),
		TEXT("/FicsitTD/Research/BPD_ResearchTree_TD_Ballistik.BPD_ResearchTree_TD_Ballistik_C"),
		TEXT("/FicsitTD/Research/BPD_ResearchTree_TD_Elementar.BPD_ResearchTree_TD_Elementar_C"),
		TEXT("/FicsitTD/Research/BPD_ResearchTree_TD_Festung.BPD_ResearchTree_TD_Festung_C"),
	};
	for (const TCHAR* Pfad : Baeume)
	{
		UClass* Baum = LoadClass<UFGResearchTree>(nullptr, Pfad);
		if (!Baum || Forschung->IsResesearchTreeUnlocked(Baum))
		{
			continue;
		}
		Forschung->UpdateUnlockedResearchTrees();
		if (!Forschung->IsResesearchTreeUnlocked(Baum))
		{
			Forschung->UnlockResearchTree(Baum);
		}
		UE_LOG(LogFicsitTD, Log, TEXT("MAM: Baum %s freigeschaltet (%s)"), *Baum->GetName(), Forschung->IsResesearchTreeUnlocked(Baum) ? TEXT("ok") : TEXT("FEHLGESCHLAGEN"));
	}
}

void UTDEinsatzSubsystem::StelleRezepteWiederHer()
{
	int32 Zurueck = 0;
	for (const TPair<TWeakObjectPtr<UClass>, TArray<FItemAmount>>& Eintrag : GratisOriginale())
	{
		if (TArray<FItemAmount>* Zutaten = RezeptZutaten(Eintrag.Key.Get()))
		{
			*Zutaten = Eintrag.Value;
			++Zurueck;
		}
	}
	if (Zurueck > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: Kosten von %d Rezepten wiederhergestellt"), Zurueck);
	}
	GratisOriginale().Reset();
}

void UTDEinsatzSubsystem::Deinitialize()
{
	StelleRezepteWiederHer();
	Super::Deinitialize();
}

FString UTDEinsatzSubsystem::Schluessel(const UClass* BauKlasse)
{
	if (!BauKlasse)
	{
		return FString();
	}
	const FString Name = BauKlasse->GetName();
	// Fest eingebaute Teile des HUB (Biomasse-Brenner, Lager) zaehlen nicht (Test 0.19.1: "Biomassegenerator 2/1")
	if (Name.Contains(TEXT("Integrated")) || Name.Contains(TEXT("Hub")) || Name.Contains(TEXT("TradingPost")))
	{
		return FString();
	}
	// 0.84.4 Coupons (Marius 08.11.): der Kern zahlt FICSIT-Coupons statt EP; der AWESOME-Sink ist im Einsatz gesperrt, sonst gaebe
	// er eine zweite Coupon-Quelle. Kein Start- und kein Ladenkontingent -> Grenze 0 -> nicht baubar (Shop bleibt erlaubt)
	if (Name.Contains(TEXT("ResourceSink")) && !Name.Contains(TEXT("Shop"))) return TEXT("AWESOME-Sink");
	if (BauKlasse->IsChildOf(ATDDefenseCore::StaticClass())) return TEXT("Abwehrkern");
	if (BauKlasse->IsChildOf(AFGBuildableMAM::StaticClass())) return TEXT("MAM");
	// Band-Splitter (auch Smart/Programmierbar/Lift) und Verbinder: Kontingent wie Gebaeude
	if (Name.Contains(TEXT("ConveyorAttachmentSplitter"))) return TEXT("Splitter");
	if (Name.Contains(TEXT("ConveyorAttachmentMerger"))) return TEXT("Verbinder");
	if (BauKlasse->IsChildOf(ATDDefenseWall::StaticClass())) return TEXT("Mauer");
	if (BauKlasse->IsChildOf(ATDBarrier::StaticClass())) return TEXT("Panzersperre");
	if (BauKlasse->IsChildOf(ATDGate::StaticClass())) return TEXT("Tor");
	if (BauKlasse->IsChildOf(ATDDimensionalDepot::StaticClass())) return TEXT("Dimensionslager");   // vor dem allgemeinen Lager
	if (BauKlasse->IsChildOf(ATDTurretBase::StaticClass()))
	{
		// 0.83 Bauten P2/P3 zuerst (FlakTurret enthaelt sonst nichts Passendes, aber sicher ist sicher)
		if (Name.Contains(TEXT("ConductorWall"))) return TEXT("Elektrozaun");
		if (Name.Contains(TEXT("RadioMast"))) return TEXT("Funkmast");
		if (Name.Contains(TEXT("ShieldGenerator"))) return TEXT("Schildgenerator");
		if (Name.Contains(TEXT("FlakTurret"))) return TEXT("Himmelsturm");
		if (Name.Contains(TEXT("FieldProjector"))) return TEXT("Stoerfeld");
		if (Name.Contains(TEXT("SingularityLauncher"))) return TEXT("Singularitaet");
		if (Name.Contains(TEXT("Bolt"))) return TEXT("Bolzenturm");
		if (Name.Contains(TEXT("Gun"))) return TEXT("Gewehrturm");
		if (Name.Contains(TEXT("Flame"))) return TEXT("Flammenturm");
		if (Name.Contains(TEXT("Tesla"))) return TEXT("Teslaspule");
		if (Name.Contains(TEXT("Nobelisk"))) return TEXT("Nobelisk-Werfer");
		if (Name.Contains(TEXT("DronePort"))) return TEXT("Drohnenplatz");
		if (Name.Contains(TEXT("Repair"))) return TEXT("Reparaturstation");
		if (Name.Contains(TEXT("Lure"))) return TEXT("Lockfeuer");
		if (Name.Contains(TEXT("Slick"))) return TEXT("Oelschlick");
		return Name;
	}
	if (BauKlasse->IsChildOf(AFGBuildableResourceExtractor::StaticClass()))
	{
		if (Name.Contains(TEXT("Miner"))) return TEXT("Miner");
		if (Name.Contains(TEXT("Water"))) return TEXT("Wasserpumpe");
		if (Name.Contains(TEXT("Oil"))) return TEXT("Oelpumpe");
		return TEXT("Foerderanlage");
	}
	if (BauKlasse->IsChildOf(AFGBuildableGenerator::StaticClass()))
	{
		if (Name.Contains(TEXT("Biomass"))) return TEXT("Biomassegenerator");
		if (Name.Contains(TEXT("Coal"))) return TEXT("Kohlekraftwerk");
		if (Name.Contains(TEXT("Fuel"))) return TEXT("Treibstoffkraftwerk");
		if (Name.Contains(TEXT("Nuclear"))) return TEXT("Atomkraftwerk");
		return TEXT("Kraftwerk");
	}
	if (BauKlasse->IsChildOf(AFGBuildableManufacturer::StaticClass()))
	{
		if (Name.Contains(TEXT("Smelter"))) return TEXT("Schmelzofen");
		if (Name.Contains(TEXT("Constructor"))) return TEXT("Konstruktor");
		if (Name.Contains(TEXT("Foundry"))) return TEXT("Giesserei");
		if (Name.Contains(TEXT("Assembler"))) return TEXT("Assembler");
		if (Name.Contains(TEXT("Manufacturer"))) return TEXT("Fertiger");
		if (Name.Contains(TEXT("Refinery"))) return TEXT("Raffinerie");
		if (Name.Contains(TEXT("Packager"))) return TEXT("Abfueller");
		if (Name.Contains(TEXT("Blender"))) return TEXT("Mischer");
		return TEXT("Fabrik");
	}
	if (BauKlasse->IsChildOf(AFGBuildableStorage::StaticClass())) return TEXT("Lager");
	if (BauKlasse->IsChildOf(AFGBuildablePipeReservoir::StaticClass())) return TEXT("Fluessigkeitspuffer");
	return FString();   // Logistik, Fundamente, Mauern des Spiels, MAM ...: frei
}

int32 UTDEinsatzSubsystem::Grenze(const FString& Key) const
{
	int32 Start = 0;   // nicht im Startkontingent = erst nach Kauf im Laden
	for (const TPair<const TCHAR*, int32>& Eintrag : StartKontingent)
	{
		if (Key == Eintrag.Key)
		{
			Start = StartMenge(Eintrag.Key, Eintrag.Value);
		}
	}
	const ATDDefenseCore* Kern = mKern.Get();
	const FTDLadenEintrag* Laden = LadenFuer(Key);
	const int32* Gekauft = Kern ? Kern->mKontingentGekauft.Find(Key) : nullptr;
	int32 Geschenk = 0;
	// 0.9.1 (Simulation 09.11.): die Raketen-Kette war zu teuer - die Forschung Raketentreibstoff schenkt Foerderanlagen fuer den Stickstoff
	if (Key == TEXT("Foerderanlage") && TDForschung::Hat(this, TEXT("Raketentreibstoff")))
	{
		Geschenk = TDBalance::Ganz(TEXT("Einsatz.Geschenk.Raketentreibstoff.Foerderanlage"), 3);
	}
	return Start + Geschenk + (Gekauft && Laden ? *Gekauft * LadenPaket(*Laden) : 0);
}

void UTDEinsatzSubsystem::WelleBeendet(int32 Welle, float KernAnteil)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		return;
	}
	// EP = Grundwert + Welle x Steigerung + Kernbonus; Bosswelle doppelt (Stellschrauben, spaeter Balance-Datei)
	int32 Punkte = TDBalance::Ganz(TEXT("Einsatz.EP.Grund"), 8) + FMath::RoundToInt(TDBalance::Wert(TEXT("Einsatz.EP.JeWelle"), 2.f) * Welle)
		+ (KernAnteil >= 99.5f ? TDBalance::Ganz(TEXT("Einsatz.EP.KernVoll"), 5) : KernAnteil >= 75.f ? TDBalance::Ganz(TEXT("Einsatz.EP.KernAb75"), 2) : 0);
	if (Welle % 10 == 0)
	{
		// Bosswelle (Marius 02.11.): doppelt plus Extra-EP fuer die neue Front danach
		Punkte = FMath::RoundToInt(Punkte * TDBalance::Wert(TEXT("Einsatz.EP.BossFaktor"), 2.f)) + TDBalance::Ganz(TEXT("Einsatz.EP.BossBonus"), 20);
	}
	Kern->mEinsatzPunkte += Punkte;
	GibOffeneCoupons();
	if (mZerstoert.Num() > 0)
	{
		TArray<FString> Teile;
		for (const TPair<FString, int32>& Z : mZerstoert)
		{
			Teile.Add(FString::Printf(TEXT("%d %s"), Z.Value, *TDAnzeige(Z.Key)));
		}
		Melde(TDF(TEXT("Zerstoert in dieser Welle: %s. Das Baumaterial liegt im Inventar (oder wartet am Kern) - Kontingent ist wieder frei."),
			TEXT("Destroyed this wave: %s. The building materials are in your inventory (or waiting at the Core) - the quota is free again."), *FString::Join(Teile, TEXT(", "))));
		mZerstoert.Reset();
	}
	Melde(TDF(TEXT("+%d FICSIT-Coupons%s (Konto %d Coupons%s). Ausgeben im Kern-Menue (E am Kern)."),
		TEXT("+%d FICSIT Coupons%s (balance %d Coupons%s). Spend them in the Core Menu (E at the Core)."), Punkte,
		Welle % 10 == 0 ? *TDT(TEXT(" inkl. Boss-Bonus"), TEXT(" incl. boss bonus")) : TEXT(""), Konto(),
		Kern->mEinsatzPunkte > 0 ? *TDF(TEXT(", %d warten am Kern, Inventar voll"), TEXT(", %d waiting at the Core, inventory full"), Kern->mEinsatzPunkte) : TEXT("")));

	// Biomasse je Welle fuer den Anfangsstrom (bis Kohle laeuft); 0 = aus
	if (const int32 Bio = TDBalance::Ganz(TEXT("Einsatz.Biomasse.JeWelle"), 100))
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const AFGCharacterPlayer* Pioneer = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		UFGInventoryComponent* Inventar = Pioneer ? Pioneer->GetInventory() : nullptr;
		UClass* Biomasse = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/GenericBiomass/Desc_GenericBiomass.Desc_GenericBiomass_C"));
		if (Inventar && Biomasse)
		{
			// Test 0.53: bei vollem Inventar verfiel die Biomasse, danach fehlte der Strom - jetzt wartet der Rest am Kern
			const FString Text = GibMaterial({ { Biomasse, Bio } });
			Melde(TDF(TEXT("Biomasse fuer die Brenner: %s."), TEXT("Biomass for the burners: %s."), *Text));
		}
	}
	// Forschungspunkte (Schritt B): je Welle, Bonus bei gutem Kern und nach dem Boss; als Item ins Inventar (MAM-Kosten)
	const int32 FP = TDBalance::Ganz(TEXT("Einsatz.FP.JeWelle"), 1) + (KernAnteil >= 75.f ? TDBalance::Ganz(TEXT("Einsatz.FP.KernAb75"), 1) : 0)
		+ (Welle % 10 == 0 ? TDBalance::Ganz(TEXT("Einsatz.FP.Boss"), 3) : 0);
	Kern->mFPOffen += FMath::Max(0, FP);
	const int32 Gegeben = GibOffeneFP();
	Melde(TDF(TEXT("+%d Festplatten, jetzt %d im Inventar%s. Im MAM für die Forschung einsetzen."),
		TEXT("+%d Hard Drives, now %d in your inventory%s. Use them for research in the MAM."), FP, FPImInventar(),
		Kern->mFPOffen > 0 ? *TDF(TEXT(" - %d warten, Inventar voll"), TEXT(" - %d waiting, inventory full"), Kern->mFPOffen) : TEXT("")));
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: Welle %d -> %d FP (%d ins Inventar, %d offen)"), Welle, FP, Gegeben, Kern->mFPOffen);
	// 0.65 (Lauf 04.11.: Kern fiel ohne Gegenmittel von 82 % auf 10 %): Grundreparatur nach jeder Welle
	{
		const float Grund = Kern->HealTD(Kern->GetTDMaxHealth() * TDBalance::Wert(TEXT("Kern.Grundreparatur"), 0.05f));
		if (Grund > 0.f)
		{
			Melde(TDF(TEXT("Kern repariert sich nach der Welle: +%.0f LP."), TEXT("The Core repairs itself after the wave: +%.0f HP."), Grund));
		}
	}
	// Forschung Notreparatur: der Kern heilt sich nach jeder Welle um 10 %
	if (TDForschung::Hat(this, TEXT("KernNotrep")))
	{
		const float Geheilt = Kern->HealTD(Kern->GetTDMaxHealth() * TDBalance::Wert(TEXT("Forschung.KernNotrep.Anteil"), 0.1f));
		if (Geheilt > 0.f)
		{
			Melde(TDF(TEXT("Notreparatur: Kern +%.0f LP."), TEXT("Emergency repair: Core +%.0f HP."), Geheilt));
		}
	}
	// Warnung: der Kern ist knapp (Marius hat die Kernreparatur im Laden nicht gefunden)
	if (KernAnteil < TDBalance::Wert(TEXT("Kern.Warnung"), 50.f))
	{
		Melde(TDF(TEXT("ACHTUNG: Kern nur noch bei %.0f %%. Kernreparatur im Kern-Menü (Laden), Notreparatur im MAM (Kern-Ausbau) - und mehr oder stärkere Türme."),
			TEXT("WARNING: Core only at %.0f %%. Core Repair in the Core Menu (Shop), Emergency Repair in the MAM (Core upgrades) - and more or stronger turrets."), KernAnteil));
	}
	if (UTDPionier* Pionier = UTDPionier::Get(this))
	{
		Pionier->WelleBeendet(Welle, KernAnteil);
	}
	if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
	{
		Lauf->WellenEnde(Welle, Punkte, FP, KernAnteil);
	}
	if (UTDWellenStatistik* Statistik = UTDWellenStatistik::Get(this))
	{
		Statistik->WellenEnde(Welle, Punkte, FP, KernAnteil);
	}
}

void UTDEinsatzSubsystem::SammleBaumaterial(const FString& Key, int32 Anzahl, TMap<UClass*, int32>& Out) const
{
	const FTDLadenEintrag* E = LadenFuer(Key);
	UClass* Rezept = E ? LoadClass<UFGRecipe>(nullptr, E->Rezept) : nullptr;
	if (!Rezept || Anzahl <= 0)
	{
		return;
	}
	for (const FItemAmount& Zutat : UFGRecipe::GetIngredients(GetWorld(), Rezept))
	{
		if (Zutat.ItemClass)
		{
			Out.FindOrAdd(Zutat.ItemClass.Get()) += Zutat.Amount * Anzahl;
		}
	}
}

FString UTDEinsatzSubsystem::GibMaterial(const TMap<UClass*, int32>& Items)
{
	ATDDefenseCore* Kern = mKern.Get();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFGCharacterPlayer* Pioneer = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	UFGInventoryComponent* Inventar = Pioneer ? Pioneer->GetInventory() : nullptr;
	TArray<FString> Teile;
	int32 Wartet = 0;
	for (const TPair<UClass*, int32>& Item : Items)
	{
		if (!Item.Key || Item.Value <= 0)
		{
			continue;
		}
		const int32 Rein = Inventar ? Inventar->AddStack(FInventoryStack(Item.Value, Item.Key), true) : 0;
		Teile.Add(FString::Printf(TEXT("%d %s"), Item.Value, *UFGItemDescriptor::GetItemName(Item.Key).ToString()));
		if (Rein < Item.Value && Kern)
		{
			Kern->mMaterialOffen.FindOrAdd(Item.Key->GetPathName()) += Item.Value - Rein;
			Wartet += Item.Value - Rein;
		}
	}
	FString Text = FString::Join(Teile, TEXT(", "));
	if (Wartet > 0)
	{
		Text += TDF(TEXT(" (%d Teile warten am Kern, Inventar voll: E am Kern holt sie)"), TEXT(" (%d items waiting at the Core, inventory full: press E at the Core to collect them)"), Wartet);
	}
	return Text;
}

int32 UTDEinsatzSubsystem::GibOffenesMaterial()
{
	ATDDefenseCore* Kern = mKern.Get();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFGCharacterPlayer* Pioneer = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	UFGInventoryComponent* Inventar = Pioneer ? Pioneer->GetInventory() : nullptr;
	if (!Kern || !Inventar)
	{
		return 0;
	}
	int32 Gesamt = 0;
	for (auto It = Kern->mMaterialOffen.CreateIterator(); It; ++It)
	{
		UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, *It->Key);
		const int32 Rein = Klasse && It->Value > 0 ? Inventar->AddStack(FInventoryStack(It->Value, Klasse), true) : 0;
		It->Value -= Rein;
		Gesamt += Rein;
		if (!Klasse || It->Value <= 0)
		{
			It.RemoveCurrent();
		}
	}
	return Gesamt;
}

void UTDEinsatzSubsystem::GebaeudeZerstoert(const UClass* BauKlasse)
{
	const FString Key = Schluessel(BauKlasse);
	if (!mAktiv || Key.IsEmpty())
	{
		return;
	}
	TMap<UClass*, int32> Material;
	SammleBaumaterial(Key, 1, Material);
	const FString Text = Material.Num() ? GibMaterial(Material) : FString();
	++mZerstoert.FindOrAdd(Key);
	mBelegtZeit = -1.f;
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: %s zerstoert, Material zurueck: %s"), *Key, *Text);
}

/** Inventar des lokalen Spielers (Einzelspieler) */
static UFGInventoryComponent* SpielerInventar(const UWorld* World)
{
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const AFGCharacterPlayer* Pioneer = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	return Pioneer ? Pioneer->GetInventory() : nullptr;
}

UClass* UTDEinsatzSubsystem::FPKlasse()
{
	// 0.10.0 (Marius 14.11.): Forschung mit der Festplatte des Spiels statt eigener FICSIT-Kampfdaten (passt zum MAM)
	static TWeakObjectPtr<UClass> Klasse;
	if (!Klasse.IsValid())
	{
		Klasse = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Environment/CrashSites/Desc_HardDrive.Desc_HardDrive_C"));
	}
	return Klasse.Get();
}

UClass* UTDEinsatzSubsystem::CouponKlasse()
{
	// 0.10.0 (Marius 14.11.): Coupons als Item des Spiels, damit der AWESOME-Shop mit ihnen bezahlt
	static TWeakObjectPtr<UClass> Klasse;
	if (!Klasse.IsValid())
	{
		Klasse = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/ResourceSinkCoupon/Desc_ResourceSinkCoupon.Desc_ResourceSinkCoupon_C"));
	}
	return Klasse.Get();
}

int32 UTDEinsatzSubsystem::FPImInventar() const
{
	const UFGInventoryComponent* Inventar = SpielerInventar(GetWorld());
	UClass* FP = FPKlasse();
	return Inventar && FP ? Inventar->GetNumItems(FP) : 0;
}

int32 UTDEinsatzSubsystem::GibOffeneFP()
{
	ATDDefenseCore* Kern = mKern.Get();
	UFGInventoryComponent* Inventar = SpielerInventar(GetWorld());
	UClass* FP = FPKlasse();
	if (!Kern || !Inventar || !FP || Kern->mFPOffen <= 0)
	{
		return 0;
	}
	const int32 Gegeben = Inventar->AddStack(FInventoryStack(Kern->mFPOffen, FP), true);
	Kern->mFPOffen -= Gegeben;
	return Gegeben;
}

int32 UTDEinsatzSubsystem::GibOffeneCoupons()
{
	ATDDefenseCore* Kern = mKern.Get();
	UFGInventoryComponent* Inventar = SpielerInventar(GetWorld());
	UClass* Coupon = CouponKlasse();
	if (!Kern || !Inventar || !Coupon || Kern->mEinsatzPunkte <= 0)
	{
		return 0;
	}
	const int32 Gegeben = Inventar->AddStack(FInventoryStack(Kern->mEinsatzPunkte, Coupon), true);
	Kern->mEinsatzPunkte -= Gegeben;
	if (Gegeben > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Coupons: %d ins Inventar, %d warten am Kern, Konto %d"), Gegeben, Kern->mEinsatzPunkte, Konto());
	}
	return Gegeben;
}

int32 UTDEinsatzSubsystem::TauscheAlteKampfdaten()
{
	ATDDefenseCore* Kern = mKern.Get();
	UFGInventoryComponent* Inventar = SpielerInventar(GetWorld());
	UClass* Alt = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/FicsitTD/Items/Desc_TD_Kampfdaten.Desc_TD_Kampfdaten_C"));
	const int32 Anzahl = Inventar && Alt ? Inventar->GetNumItems(Alt) : 0;
	if (!Kern || Anzahl <= 0)
	{
		return 0;
	}
	Inventar->Remove(Alt, Anzahl);
	Kern->mFPOffen += Anzahl;
	GibOffeneFP();
	UE_LOG(LogFicsitTD, Log, TEXT("Umstellung 0.10: %d FICSIT-Kampfdaten gegen Festplatten getauscht (%d im Inventar, %d warten)"), Anzahl, FPImInventar(), Kern->mFPOffen);
	Melde(TDF(TEXT("Umstellung: %d FICSIT-Kampfdaten sind jetzt Festplatten."), TEXT("Conversion: %d FICSIT Combat Data are now Hard Drives."), Anzahl));
	return Anzahl;
}

/** Coupons abbuchen: zuerst aus dem Inventar, Rest von den am Kern wartenden. Vorher Konto() pruefen */
static void NimmCoupons(const UWorld* World, ATDDefenseCore* Kern, int32 Preis)
{
	UFGInventoryComponent* Inventar = SpielerInventar(World);
	UClass* Coupon = UTDEinsatzSubsystem::CouponKlasse();
	const int32 AusInventar = Inventar && Coupon ? FMath::Min(Preis, Inventar->GetNumItems(Coupon)) : 0;
	if (AusInventar > 0)
	{
		Inventar->Remove(Coupon, AusInventar);
	}
	Kern->mEinsatzPunkte = FMath::Max(0, Kern->mEinsatzPunkte - (Preis - AusInventar));
}

static bool RezeptVerfuegbar(UWorld* World, const TCHAR* Pfad)
{
	AFGRecipeManager* Rezepte = AFGRecipeManager::Get(World);
	UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Pfad);
	return Rezepte && Rezept && Rezepte->IsRecipeAvailable(Rezept);
}

void UTDEinsatzSubsystem::GibEP(int32 Punkte, const TCHAR* Grund)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern || Punkte <= 0)
	{
		return;
	}
	Kern->mEinsatzPunkte += Punkte;
	GibOffeneCoupons();
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: +%d EP (%s), Konto %d EP"), Punkte, Grund, Konto());
}

int32 UTDEinsatzSubsystem::Konto() const
{
	const ATDDefenseCore* Kern = mKern.Get();
	const UFGInventoryComponent* Inventar = SpielerInventar(GetWorld());
	UClass* Coupon = CouponKlasse();
	return (Kern ? Kern->mEinsatzPunkte : 0) + (Inventar && Coupon ? Inventar->GetNumItems(Coupon) : 0);
}

FString UTDEinsatzSubsystem::ZahleEP(int32 Preis, const FString& Posten, bool bAuchInWelle)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		return TDT(TEXT("Nur im Einsatz."), TEXT("Only during an operation."));
	}
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!bAuchInWelle && Waves && Waves->LaeuftWelle())
	{
		return TDT(TEXT("Nur in der Bauphase."), TEXT("Only in the build phase."));
	}
	if (Konto() < Preis)
	{
		return TDF(TEXT("Zu wenig FICSIT-Coupons: %d Coupons nötig, Konto %d Coupons."), TEXT("Not enough FICSIT Coupons: %d Coupons needed, balance %d Coupons."), Preis, Konto());
	}
	NimmCoupons(GetWorld(), Kern, Preis);
	if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
	{
		Lauf->Kauf(Posten, Preis);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: %s fuer %d EP, Konto %d EP"), *Posten, Preis, Konto());
	return FString();
}

void UTDEinsatzSubsystem::MerkeTurmErbe(const FString& Turm, int32 Kills, int32 Level)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		return;
	}
	// 0.63: "Typ|Abschuesse|L<Level>" (vorher "Typ|Abschuesse|R|F|S|Sp")
	Kern->mTurmErbe.Add(FString::Printf(TEXT("%s|%d|L%d"), *Turm, Kills, Level));
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: %s abgerissen, Level %d fuer den naechsten gemerkt (%d Abschuesse)"), *Turm, Level, Kills);
	Melde(TDF(TEXT("%s abgerissen: Der nächste neu gebaute %s übernimmt sein Level %d."), TEXT("%s dismantled: the next %s you build inherits its level %d."),
		*TDAnzeige(Turm), *TDAnzeige(Turm), Level));
}

/** Level aus einem Erbe-Eintrag; alte Eintraege (R|F|S) wie beim Laden umrechnen: hoechste Stufe + 1 */
static int32 ErbeLevel(const TArray<FString>& Teile)
{
	if (Teile.Num() >= 3 && Teile[2].StartsWith(TEXT("L")))
	{
		return FMath::Clamp(FCString::Atoi(*Teile[2].Mid(1)), 1, 5);
	}
	if (Teile.Num() >= 5)
	{
		return FMath::Clamp(1 + FMath::Max3(FCString::Atoi(*Teile[2]), FCString::Atoi(*Teile[3]), FCString::Atoi(*Teile[4])), 1, 4);
	}
	return 1;
}

bool UTDEinsatzSubsystem::HoleTurmErbe(const FString& Turm, int32& Kills, int32& Level)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		return false;
	}
	// das wertvollste Erbe dieses Typs zuerst
	int32 Bester = INDEX_NONE, BestWert = -1;
	for (int32 i = 0; i < Kern->mTurmErbe.Num(); ++i)
	{
		TArray<FString> Teile;
		Kern->mTurmErbe[i].ParseIntoArray(Teile, TEXT("|"), false);
		if (Teile.Num() >= 3 && Teile[0] == Turm)
		{
			const int32 Wert = FCString::Atoi(*Teile[1]) + 100000 * ErbeLevel(Teile);
			if (Wert > BestWert)
			{
				BestWert = Wert;
				Bester = i;
			}
		}
	}
	if (Bester == INDEX_NONE)
	{
		return false;
	}
	TArray<FString> Teile;
	Kern->mTurmErbe[Bester].ParseIntoArray(Teile, TEXT("|"), false);
	Kills = FCString::Atoi(*Teile[1]);
	Level = ErbeLevel(Teile);
	Kern->mTurmErbe.RemoveAt(Bester);
	return true;
}

FString UTDEinsatzSubsystem::SpielerWirkung(const FTDSpielerPosten& P, int32 Menge)
{
	if (P.Art == 3)
	{
		ATDDefenseCore* Kern = mKern.Get();
		if (!Kern)
		{
			return FString();
		}
		const float Geheilt = Kern->HealTD(Kern->GetTDMaxHealth() * Menge / 100.f);
		UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: Kern repariert +%.0f LP, jetzt %.0f/%.0f"), Geheilt, Kern->GetTDHealth(), Kern->GetTDMaxHealth());
		return TDF(TEXT("Kern +%.0f LP, jetzt %.0f %%."), TEXT("Core +%.0f HP, now %.0f %%."), Geheilt, 100.f * Kern->GetTDHealth() / FMath::Max(Kern->GetTDMaxHealth(), 1.f));
	}
	if (P.Art == 1 || P.Art == 2)
	{
		AFGUnlockSubsystem* Freischaltung = AFGUnlockSubsystem::Get(GetWorld());
		if (!Freischaltung)
		{
			return TDT(TEXT("(Freischaltung nicht erreichbar)"), TEXT("(unlocks not available)"));
		}
		if (P.Art == 1)
		{
			Freischaltung->UnlockInventorySlots(Menge);
			return TDF(TEXT("+%d Inventarplätze, jetzt %d."), TEXT("+%d inventory slots, now %d."), Menge, Freischaltung->GetNumTotalInventorySlots());
		}
		Freischaltung->UnlockArmEquipmentSlots(Menge);
		return TDF(TEXT("+%d Handplatz, jetzt %d."), TEXT("+%d hand slot, now %d."), Menge, Freischaltung->GetNumTotalArmEquipmentSlots());
	}
	TMap<UClass*, int32> Item;
	if (UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, P.Item))
	{
		Item.Add(Klasse, Menge);
	}
	return Item.Num() ? TDT(TEXT("Erhalten: "), TEXT("Received: ")) + GibMaterial(Item) + TEXT(".") : TDT(TEXT("(Ausrüstung nicht gefunden)"), TEXT("(equipment not found)"));
}

FString FTDLadenZeile::KategorieName(int32 Index)
{
	switch (Index)
	{
	case 0:  return TDT(TEXT("Türme"), TEXT("Turrets"));
	case 1:  return TDT(TEXT("Verteidigung"), TEXT("Defense"));
	case 2:  return TDT(TEXT("Produktion"), TEXT("Production"));
	case 3:  return TDT(TEXT("Strom"), TEXT("Power"));
	case 4:  return TDT(TEXT("Logistik"), TEXT("Logistics"));
	default: return TDT(TEXT("Pionier und Kern"), TEXT("Pioneer and Core"));
	}
}

/** 0.9.6 (Marius 11.11.: "je mehr Gebaeude, umso unstrukturierter"): Abschnitt je Laden-Eintrag */
static int32 LadenKategorie(const FString& Key)
{
	static const TCHAR* Tuerme[] = { TEXT("Bolzenturm"), TEXT("Gewehrturm"), TEXT("Teslaspule"), TEXT("Himmelsturm"), TEXT("Flammenturm"), TEXT("Nobelisk-Werfer"), TEXT("Singularitaet") };
	static const TCHAR* Abwehr[] = { TEXT("Mauer"), TEXT("Panzersperre"), TEXT("Tor"), TEXT("Lockfeuer"), TEXT("Oelschlick"), TEXT("Elektrozaun"), TEXT("Funkmast"), TEXT("Schildgenerator"), TEXT("Stoerfeld"), TEXT("Drohnenplatz") };
	static const TCHAR* Strom[] = { TEXT("Biomassegenerator"), TEXT("Kohlekraftwerk"), TEXT("Treibstoffkraftwerk") };
	static const TCHAR* Logistik[] = { TEXT("Lager"), TEXT("Splitter"), TEXT("Verbinder"), TEXT("Fluessigkeitspuffer") };
	for (const TCHAR* K : Tuerme)   { if (Key == K) return 0; }
	for (const TCHAR* K : Abwehr)   { if (Key == K) return 1; }
	for (const TCHAR* K : Strom)    { if (Key == K) return 3; }
	for (const TCHAR* K : Logistik) { if (Key == K) return 4; }
	return SpielerFuer(Key) ? 5 : 2;   // Spieler-Posten, sonst Produktion (Miner, Oefen, Maschinen, Pumpen)
}

TArray<FTDLadenZeile> UTDEinsatzSubsystem::LadenZeilen() const
{
	TArray<FTDLadenZeile> Zeilen;
	const ATDDefenseCore* Kern = mKern.Get();
	for (const FTDLadenEintrag& E : LadenListe)
	{
		// 0.72.1: Panzersperre entfernt (Eintrag bleibt fuer das Kontingent gebauter Sperren alter Spielstaende)
		if (FCString::Strcmp(E.Key, TEXT("Panzersperre")) == 0)
		{
			continue;
		}
		FTDLadenZeile Z;
		Z.Key = E.Key;
		Z.Paket = LadenPaket(E);
		Z.bVerfuegbar = RezeptVerfuegbar(GetWorld(), E.Rezept);
		const int32* Gekauft = Kern ? Kern->mKontingentGekauft.Find(E.Key) : nullptr;
		Z.Grundpreis = LadenGrundpreis(E);
		Z.Aufschlag = LadenAufschlag(E);
		Z.Gekauft = Gekauft ? *Gekauft : 0;
		Z.Preis = Z.PreisNach(0);
		Z.Symbol = Symbol(E.Key);
		Z.Kategorie = LadenKategorie(E.Key);
		if (const FTDSpielerPosten* SP = SpielerFuer(E.Key))
		{
			Z.Einheit = SP->Art == 1 ? TDT(TEXT("Inventarplätze"), TEXT("inventory slots")) : SP->Art == 2 ? TDT(TEXT("Handplatz"), TEXT("hand slot"))
				: SP->Art == 3 ? TDT(TEXT("% Kern-LP"), TEXT("% Core HP")) : TDT(TEXT("Ausrüstung"), TEXT("equipment"));
			Z.Max = SpielerMax(*SP);
			// Marius 03.11.2026: eine gekaufte Waffe noch einmal anzubieten ergibt keinen Sinn
			if (Z.Max > 0 && Z.Gekauft >= Z.Max)
			{
				continue;
			}
			if (SP->Art == 3 && (!Kern || Kern->GetTDHealth() >= Kern->GetTDMaxHealth() - 1.f))
			{
				continue;
			}
		}
		Zeilen.Add(Z);
	}
	// Erforschtes zuerst
	Zeilen.StableSort([](const FTDLadenZeile& A, const FTDLadenZeile& B) { return A.bVerfuegbar && !B.bVerfuegbar; });
	return Zeilen;
}

TArray<FTDKontingentZeile> UTDEinsatzSubsystem::KontingentZeilen()
{
	TArray<FTDKontingentZeile> Zeilen;
	ZaehleNeu();
	TSet<FString> Gezeigt;
	auto Neu = [&](const FString& Key)
	{
		if (Gezeigt.Contains(Key) || SpielerFuer(Key))
		{
			return;
		}
		Gezeigt.Add(Key);
		FTDKontingentZeile Z;
		Z.Key = Key;
		Z.Belegt = Belegt(Key);
		Z.Grenze = Grenze(Key);
		Z.Symbol = Symbol(Key);
		if (Z.Grenze > 0 || Z.Belegt > 0)
		{
			Zeilen.Add(Z);
		}
	};
	for (const TPair<const TCHAR*, int32>& Eintrag : StartKontingent)
	{
		Neu(Eintrag.Key);
	}
	if (const ATDDefenseCore* Kern = mKern.Get())
	{
		for (const TPair<FString, int32>& Kauf : Kern->mKontingentGekauft)
		{
			Neu(Kauf.Key);
		}
	}
	return Zeilen;
}

void UTDEinsatzSubsystem::ZeigeHud()
{
	if (mHud && mHud->IsInViewport())
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		// Beim Laden steht der Spieler-Controller noch nicht: kurz spaeter erneut
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateUObject(this, &UTDEinsatzSubsystem::ZeigeHud), 2.f, false);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(mToDoTimer, FTimerDelegate::CreateUObject(this, &UTDEinsatzSubsystem::AktualisiereToDo), 3.f, true, 0.5f);
	if (AFGPlayerState* PS = PC->GetPlayerState<AFGPlayerState>())
	{
		// To-do-Liste mit Notiz und Rezepten einblenden
		FShoppingListSettings Anzeige = PS->GetShoppingListSettings();
		if (!Anzeige.PrivateTodoListVisibilty || !Anzeige.RecipeListVisibilty)
		{
			Anzeige.PrivateTodoListVisibilty = true;
			Anzeige.RecipeListVisibilty = true;
			PS->SetShoppingListSettings(Anzeige);
		}
	}
	mHud = CreateWidget<UTDHud>(PC, UTDHud::StaticClass());
	if (mHud)
	{
		mHud->AddToViewport(-10);
	}
}

void UTDEinsatzSubsystem::VersteckeHud()
{
	GetWorld()->GetTimerManager().ClearTimer(mToDoTimer);
	if (mHud)
	{
		mHud->RemoveFromParent();
		mHud = nullptr;
	}
}

TArray<FString> UTDEinsatzSubsystem::LadenText() const
{
	TArray<FString> Zeilen;
	const ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		Zeilen.Add(TDT(TEXT("Kein Einsatz aktiv."), TEXT("No operation active.")));
		return Zeilen;
	}
	Zeilen.Add(TDF(TEXT("Laden - Konto: %d Coupons"), TEXT("Shop - balance: %d Coupons"), Konto()));
	TArray<FString> Kaufbar, Gesperrt;
	for (const FTDLadenEintrag& E : LadenListe)
	{
		if (!RezeptVerfuegbar(GetWorld(), E.Rezept))
		{
			Gesperrt.Add(TDAnzeige(E.Key));
			continue;
		}
		const int32* Gekauft = Kern->mKontingentGekauft.Find(E.Key);
		const int32 Preis = LadenPreis(E, Gekauft ? *Gekauft : 0);
		Kaufbar.Add(FString::Printf(TEXT("%s +%d: %d Coupons"), *TDAnzeige(E.Key), LadenPaket(E), Preis));
	}
	Zeilen.Add(FString::Join(Kaufbar, TEXT(", ")));
	if (Gesperrt.Num() > 0)
	{
		Zeilen.Add(TDT(TEXT("Erst im MAM erforschen: "), TEXT("Research in the MAM first: ")) + FString::Join(Gesperrt, TEXT(", ")));
	}
	return Zeilen;
}

FString UTDEinsatzSubsystem::Kaufe(const FString& Name, int32 Anzahl)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		return TDT(TEXT("Kein Einsatz aktiv."), TEXT("No operation active."));
	}
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (Waves && Waves->LaeuftWelle())
	{
		return TDT(TEXT("Waehrend der Welle ist der Laden geschlossen."), TEXT("The shop is closed during a wave."));
	}
	// erst der genaue Name, dann der Anfang (Test 0.56: "Gewehr" traf zuerst "Gewehrturm" - drei Gewehrtuerme statt der Waffe)
	const FTDLadenEintrag* E = nullptr;
	for (const FTDLadenEintrag& Kandidat : LadenListe)
	{
		if (Name.Equals(Kandidat.Key, ESearchCase::IgnoreCase))
		{
			E = &Kandidat;
			break;
		}
	}
	for (const FTDLadenEintrag& Kandidat : LadenListe)
	{
		if (!E && FString(Kandidat.Key).StartsWith(Name, ESearchCase::IgnoreCase))
		{
			E = &Kandidat;
			break;
		}
	}
	if (!E)
	{
		return TDF(TEXT("Unbekannt: %s (Liste: /td laden)"), TEXT("Unknown: %s (list: /td laden)"), *Name);
	}
	if (!RezeptVerfuegbar(GetWorld(), E->Rezept))
	{
		return TDF(TEXT("%s ist noch nicht erforscht (MAM)."), TEXT("%s is not researched yet (MAM)."), *TDAnzeige(E->Key));
	}
	int32 Gekauft = 0, Kosten = 0;
	int32& Bisher = Kern->mKontingentGekauft.FindOrAdd(E->Key);
	const FTDSpielerPosten* SP = SpielerFuer(E->Key);
	if (SP && SpielerMax(*SP) > 0 && Bisher >= SpielerMax(*SP))
	{
		return TDF(TEXT("%s: schon %d von %d gekauft."), TEXT("%s: already bought %d of %d."), *TDAnzeige(E->Key), Bisher, SpielerMax(*SP));
	}
	if (SP && SP->Art == 3 && Kern->GetTDHealth() >= Kern->GetTDMaxHealth() - 1.f)
	{
		return TDT(TEXT("Der Kern ist unbeschädigt."), TEXT("The Core is undamaged."));
	}
	for (int32 i = 0; i < FMath::Max(1, Anzahl); ++i)
	{
		if (SP && SpielerMax(*SP) > 0 && Bisher >= SpielerMax(*SP))
		{
			break;
		}
		const int32 Preis = LadenPreis(*E, Bisher);
		if (Konto() < Preis)
		{
			break;
		}
		NimmCoupons(GetWorld(), Kern, Preis);
		Kosten += Preis;
		if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
		{
			Lauf->Kauf(E->Key, Preis);
		}
		++Bisher;
		++Gekauft;
	}
	if (Gekauft == 0)
	{
		return TDF(TEXT("Zu wenig FICSIT-Coupons (Konto %d Coupons)."), TEXT("Not enough FICSIT Coupons (balance %d Coupons)."), Konto());
	}
	mBelegtZeit = -1.f;
	if (SP)
	{
		return TDF(TEXT("Gekauft: %s für %d Coupons. %s Konto %d Coupons."), TEXT("Bought: %s for %d Coupons. %s Balance %d Coupons."),
			*TDAnzeige(E->Key), Kosten, *SpielerWirkung(*SP, Gekauft * LadenPaket(*E)), Konto());
	}
	// Mit dem Kontingent kommt das Baumaterial (Marius 02.11.: sonst fehlte es z. B. fuer Mauern und Miner)
	TMap<UClass*, int32> Material;
	SammleBaumaterial(E->Key, Gekauft * LadenPaket(*E), Material);
	const FString MaterialText = Material.Num() ? GibMaterial(Material) : FString();
	return TDF(TEXT("Gekauft: %s +%d fuer %d Coupons. Jetzt %d/%d, Konto %d Coupons.%s"), TEXT("Bought: %s +%d for %d Coupons. Now %d/%d, balance %d Coupons.%s"),
		*TDAnzeige(E->Key), Gekauft * LadenPaket(*E), Kosten,
		Belegt(E->Key), Grenze(E->Key), Konto(), MaterialText.IsEmpty() ? TEXT("") : *(TDT(TEXT(" Material: "), TEXT(" Materials: ")) + MaterialText));
}

FString UTDEinsatzSubsystem::KaufeKorb(const TMap<FString, int32>& Korb)
{
	ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || !Kern)
	{
		return TDT(TEXT("Kein Einsatz aktiv."), TEXT("No operation active."));
	}
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (Waves && Waves->LaeuftWelle())
	{
		return TDT(TEXT("Während der Welle ist der Laden geschlossen."), TEXT("The shop is closed during a wave."));
	}
	int32 Summe = 0;
	TArray<FString> Teile;
	for (const TPair<FString, int32>& Posten : Korb)
	{
		const FTDLadenEintrag* E = LadenFuer(Posten.Key);
		if (!E || Posten.Value <= 0)
		{
			continue;
		}
		if (!RezeptVerfuegbar(GetWorld(), E->Rezept))
		{
			return TDF(TEXT("%s ist noch nicht erforscht (MAM)."), TEXT("%s is not researched yet (MAM)."), *TDAnzeige(E->Key));
		}
		const int32* Gekauft = Kern->mKontingentGekauft.Find(E->Key);
		if (const FTDSpielerPosten* SP = SpielerFuer(E->Key))
		{
			if (SpielerMax(*SP) > 0 && (Gekauft ? *Gekauft : 0) + Posten.Value > SpielerMax(*SP))
			{
				return TDF(TEXT("%s: höchstens %d (schon %d gekauft)."), TEXT("%s: at most %d (already bought %d)."), *TDAnzeige(E->Key), SpielerMax(*SP), Gekauft ? *Gekauft : 0);
			}
		}
		for (int32 i = 0; i < Posten.Value; ++i)
		{
			Summe += LadenPreis(*E, (Gekauft ? *Gekauft : 0) + i);
		}
		Teile.Add(FString::Printf(TEXT("%s +%d"), *TDAnzeige(E->Key), Posten.Value * LadenPaket(*E)));
	}
	if (Teile.Num() == 0)
	{
		return TDT(TEXT("Der Warenkorb ist leer."), TEXT("The cart is empty."));
	}
	if (Summe > Konto())
	{
		return TDF(TEXT("Zu wenig FICSIT-Coupons: %d Coupons nötig, Konto %d Coupons."), TEXT("Not enough FICSIT Coupons: %d Coupons needed, balance %d Coupons."), Summe, Konto());
	}
	auto Offen = [Kern]() { int32 N = 0; for (const auto& P : Kern->mMaterialOffen) { N += P.Value; } return N; };
	const int32 OffenVorher = Offen();
	for (const TPair<FString, int32>& Posten : Korb)
	{
		if (Posten.Value > 0 && LadenFuer(Posten.Key))
		{
			Kaufe(Posten.Key, Posten.Value);
		}
	}
	mBelegtZeit = -1.f;
	// Test 0.53: bei vollem Inventar wartete das Baumaterial am Kern, ohne dass das Kern-Menue es sagte
	const int32 Wartet = Offen() - OffenVorher;
	return TDF(TEXT("Gekauft: %s für %d Coupons. Konto %d Coupons.%s"), TEXT("Bought: %s for %d Coupons. Balance %d Coupons.%s"), *FString::Join(Teile, TEXT(", ")), Summe, Konto(),
		Wartet > 0 ? *TDF(TEXT(" Inventar voll: %d Teile Baumaterial warten am Kern, E am Kern holt sie."), TEXT(" Inventory full: %d building materials are waiting at the Core, press E at the Core to collect them."), Wartet) : TEXT(""));
}

static UClass* BauRezeptFuer(const FString& Key)
{
	const FTDLadenEintrag* E = LadenFuer(Key);
	const TCHAR* Pfad = E ? E->Rezept : Key == TEXT("Abwehrkern") ? KernRezeptPfad : nullptr;
	return Pfad ? LoadClass<UFGRecipe>(nullptr, Pfad) : nullptr;
}

void UTDEinsatzSubsystem::SetzeEinkauf(UFGShoppingListComponent* Liste, UClass* Rezept, int32 Soll)
{
	bool bGefunden = false;
	if (UFGShoppingListObject* Eintrag = Liste->GetShoppingListObjectFromClass(Rezept, bGefunden))
	{
		if (Eintrag->GetAmount() != Soll)
		{
			if (Soll > 0)
			{
				Eintrag->SetAmount(Soll);
			}
			else
			{
				Eintrag->ClearAmount();
			}
		}
		return;
	}
	if (Soll <= 0)
	{
		return;
	}
	// Noch kein Eintrag: die (private) Datenliste ergaenzen und wie nach der Replikation neu aufbauen lassen
	static bool bGemeldet = false;
	FArrayProperty* Daten = FindFProperty<FArrayProperty>(UFGShoppingListComponent::StaticClass(), TEXT("mShoppingListClassEntries"));
	UFunction* Neuaufbau = Liste->FindFunction(TEXT("OnRep_ShoppingListClassEntries"));
	if (Daten && Neuaufbau)
	{
		Daten->ContainerPtrToValuePtr<TArray<FShoppingListClassEntry>>(Liste)->Add(FShoppingListClassEntry(Rezept, Soll));
		Liste->ProcessEvent(Neuaufbau, nullptr);
	}
	if (!bGemeldet)
	{
		bGemeldet = true;
		UE_LOG(LogFicsitTD, Log, TEXT("To-do: Eintrag %s ueber %s angelegt"), *Rezept->GetName(),
			Daten && Neuaufbau ? TEXT("Datenliste + OnRep") : TEXT("NICHTS (Feld/Funktion fehlt)"));
	}
}

void UTDEinsatzSubsystem::AktualisiereToDo()
{
	// neu freigeschaltete Logistik-Rezepte (z. B. Band Mk2 aus dem MAM) ebenfalls kostenlos machen
	AktualisiereGratisRezepte();
	SchalteScannerFrei();
	PruefeForschungsbaeume();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	const ATDDefenseCore* Kern = mKern.Get();
	const bool bAn = mAktiv && Kern;

	// 1) Einkaufsliste: was das Kontingent noch erlaubt (bei freiem Bauen nichts)
	TMap<FString, int32> Offen;
	TArray<FString> OffenText;
	if (bAn)
	{
		for (const FTDKontingentZeile& Z : KontingentZeilen())
		{
			if (Z.Grenze > Z.Belegt)
			{
				Offen.Add(Z.Key, Z.Grenze - Z.Belegt);
				OffenText.Add(FString::Printf(TEXT("%s %d"), *TDAnzeige(Z.Key), Z.Grenze - Z.Belegt));
			}
		}
	}
	if (UFGShoppingListComponent* Liste = UFGShoppingListComponent::GetShoppingListComponent(PC))
	{
		TSet<FString> Schluessel = mToDoGesetzt;
		for (const TPair<FString, int32>& P : Offen)
		{
			Schluessel.Add(P.Key);
		}
		for (const FString& Key : Schluessel)
		{
			UClass* Rezept = BauRezeptFuer(Key);
			if (!Rezept)
			{
				continue;
			}
			const int32 Soll = mFrei ? 0 : Offen.FindRef(Key);
			SetzeEinkauf(Liste, Rezept, Soll);
			if (Soll > 0)
			{
				mToDoGesetzt.Add(Key);
			}
			else
			{
				mToDoGesetzt.Remove(Key);
			}
		}
	}

	// 2) Private Notiz: eigener Block oben, Text des Spielers bleibt darunter
	AFGPlayerState* PS = PC->GetPlayerState<AFGPlayerState>();
	if (!PS)
	{
		return;
	}
	static const FString Anfang = TEXT("=== FICSIT TD ===");
	static const FString Ende = TEXT("=== Ende TD ===");
	const FString Bisher = PS->GetPrivateTodoList();
	FString Eigen = Bisher;
	const int32 A = Eigen.Find(Anfang, ESearchCase::CaseSensitive);
	if (A != INDEX_NONE)
	{
		const int32 E = Eigen.Find(Ende, ESearchCase::CaseSensitive, ESearchDir::FromStart, A);
		Eigen = Eigen.Left(A) + (E != INDEX_NONE ? Eigen.Mid(E + Ende.Len()) : FString());
		Eigen.TrimStartAndEndInline();
	}
	FString Neu = Eigen;
	if (bAn)
	{
		FString Welle = TDT(TEXT("kein Lauf"), TEXT("no run"));
		if (Waves && Waves->LaeuftWelle())
		{
			Welle = Waves->GetLaufText();
		}
		else if (Waves && Waves->LaeuftKernVerteidigung())
		{
			Welle = TDF(TEXT("Welle %d/%d bereit"), TEXT("Wave %d/%d ready"), Waves->GetWelle() + 1, Waves->mWellenGesamt);
		}
		TArray<FString> Block;
		Block.Add(Anfang);
		Block.Add(Welle);
		Block.Add(TDF(TEXT("Kern %.0f %%  |  Konto %d Coupons"), TEXT("Core %.0f %%  |  Balance %d Coupons"), 100.f * Kern->GetTDHealth() / FMath::Max(Kern->GetTDMaxHealth(), 1.f), Konto()));
		Block.Add(mFrei ? TDT(TEXT("Freies Bauen: keine Kontingente"), TEXT("Free Build: no quotas"))
			: OffenText.Num() ? TDT(TEXT("Noch baubar: "), TEXT("Still buildable: ")) + FString::Join(OffenText, TEXT(", "))
			: TDT(TEXT("Kontingent ausgeschöpft - mehr im Laden"), TEXT("Quota used up - more in the Shop")));
		Block.Add(TDT(TEXT("Laden und Welle: E am Abwehrkern"), TEXT("Shop and wave: E at the Defense Core")));
		Block.Add(Ende);
		Neu = FString::Join(Block, TEXT("\n")) + (Eigen.IsEmpty() ? FString() : FString(TEXT("\n\n")) + Eigen);
	}
	if (Neu != Bisher)
	{
		PS->SetPrivateTodoList(Neu);
	}
}

UTexture2D* UTDEinsatzSubsystem::Symbol(const FString& Key)
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> Zwischenspeicher;
	if (const TWeakObjectPtr<UTexture2D>* Gefunden = Zwischenspeicher.Find(Key))
	{
		if (Gefunden->IsValid())
		{
			return Gefunden->Get();
		}
	}
	UTexture2D* Bild = nullptr;
	if (const FTDSpielerPosten* SP = SpielerFuer(Key))
	{
		if (UClass* Item = LoadClass<UFGItemDescriptor>(nullptr, SP->Item))
		{
			Bild = UFGItemDescriptor::GetBigIcon(Item);
		}
	}
	else if (UClass* Rezept = BauRezeptFuer(Key))
	{
		const TArray<FItemAmount> Produkte = UFGRecipe::GetProducts(Rezept);
		if (Produkte.Num() > 0 && Produkte[0].ItemClass)
		{
			Bild = UFGItemDescriptor::GetBigIcon(Produkte[0].ItemClass);
		}
	}
	Zwischenspeicher.Add(Key, Bild);
	return Bild;
}

int32 UTDEinsatzSubsystem::Paket(const FString& Key)
{
	const FTDLadenEintrag* E = LadenFuer(Key);
	return E ? LadenPaket(*E) : 1;
}

void UTDEinsatzSubsystem::ZaehleNeu()
{
	mBelegt.Reset();
	const ATDDefenseCore* Kern = mKern.Get();
	for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
	{
		// Kontingente gelten auf der ganzen Karte (0.56; vorher nur 400 m um den Kern)
		const FString Key = Schluessel(It->GetClass());
		if (!Key.IsEmpty())
		{
			mBelegt.FindOrAdd(Key)++;
		}
	}
	mBelegtZeit = GetWorld()->GetTimeSeconds();
}

int32 UTDEinsatzSubsystem::Belegt(const FString& Key)
{
	// Hologramme pruefen jedes Bild - Zaehlung hoechstens alle 0,5 s
	if (mBelegtZeit < 0.f || GetWorld()->GetTimeSeconds() - mBelegtZeit > 0.5f)
	{
		ZaehleNeu();
	}
	const int32* Anzahl = mBelegt.Find(Key);
	return Anzahl ? *Anzahl : 0;
}

void UTDEinsatzSubsystem::PruefeHologramm(AFGHologram* Hologramm)
{
	if (!mAktiv || mFrei || !Hologramm)
	{
		return;
	}
	TDAktualisiereSperrTexte();
	// 0.9.6 (Gesamtreview 12.11.: Blaupausen gingen an Kontingent und Sink-Sperre vorbei; Marius: in der ersten Version keine)
	if (Hologramm->IsA<AFGBlueprintHologram>())
	{
		Hologramm->AddConstructDisqualifier(UTDCDBlaupause::StaticClass());
		return;
	}
	// Mehrere auf einmal (Mauern ziehen): alle muessen ins Kontingent passen (Test 0.19.1: "Mauer 6/5")
	const FString Grund = SperrGrund(Hologramm->GetBuildClass(), Hologramm->GetActorLocation(), FMath::Max(1, Hologramm->GetBaseCostMultiplier()));
	if (Grund == TEXT("Welle"))
	{
		Hologramm->AddConstructDisqualifier(UTDCDWellenSperre::StaticClass());
	}
	else if (Grund == TEXT("Sink"))
	{
		Hologramm->AddConstructDisqualifier(UTDCDSinkGesperrt::StaticClass());
	}
	else if (Grund == TEXT("Kontingent"))
	{
		Hologramm->AddConstructDisqualifier(UTDCDKontingent::StaticClass());
	}
}

FString UTDEinsatzSubsystem::SperrGrund(UClass* BauKlasse, const FVector& Ort, int32 Anzahl)
{
	if (!mAktiv || mFrei || !BauKlasse)
	{
		return FString();
	}
	const FString Key = Schluessel(BauKlasse);
	if (Key.IsEmpty())
	{
		return FString();
	}
	// Bausperre im Verteidigungsfeld waehrend einer Welle
	const ATDDefenseCore* Kern = mKern.Get();
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (Waves && Waves->LaeuftWelle() && Kern && FVector::Dist2D(Ort, Kern->GetActorLocation()) < EinsatzFeldRadius)
	{
		return TEXT("Welle");
	}
	if (Key == TEXT("AWESOME-Sink"))
	{
		return TEXT("Sink");
	}
	return Belegt(Key) + Anzahl > Grenze(Key) ? TEXT("Kontingent") : FString();
}

void UTDEinsatzSubsystem::Melde(const FString& Text) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: %s"), *Text);
	if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct Nachricht;
		Nachricht.MessageText = FText::FromString(Text);
		Nachricht.MessageType = EFGChatMessageType::CMT_CustomMessage;
		Nachricht.MessageSender = TDTX(TEXT("Einsatz"), TEXT("Operation"));
		Nachricht.MessageSenderColor = FLinearColor(0.2f, 0.7f, 1.f);
		Chat->BroadcastChatMessage(Nachricht);
	}
}

void UTDEinsatzSubsystem::Verbinde()
{
	if (AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(GetWorld()))
	{
		Bau->BuildableConstructedGlobalDelegate.AddUniqueDynamic(this, &UTDEinsatzSubsystem::GebaeudeGebaut);
	}
}

void UTDEinsatzSubsystem::GebaeudeGebaut(AFGBuildable* Gebaeude)
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!mAktiv || mFrei || !Gebaeude || !Kern)
	{
		return;
	}
	const FString Key = Schluessel(Gebaeude->GetClass());
	if (Key.IsEmpty())
	{
		return;
	}
	ZaehleNeu();
	const int32 Jetzt = Belegt(Key), Max = Grenze(Key);
	Melde(FString::Printf(TEXT("%s %d/%d%s"), *TDAnzeige(Key), Jetzt, Max, Jetzt >= Max ? *TDT(TEXT(" - Kontingent voll"), TEXT(" - quota full")) : TEXT("")));
}

ATDDefenseCore* UTDEinsatzSubsystem::SetzeKern(APawn* Spieler)
{
	UClass* Klasse = LoadClass<AFGBuildable>(nullptr, KernKlassePfad);
	AFGBuildableSubsystem* Bau = AFGBuildableSubsystem::Get(GetWorld());
	if (!Spieler || !Klasse || !Bau)
	{
		return nullptr;
	}
	// 10 m vor den Spieler auf den Boden, Blickrichtung zum Spieler
	const FVector Vorne = Spieler->GetActorForwardVector().GetSafeNormal2D();
	FVector Ort = Spieler->GetActorLocation() + Vorne * 1000.f;
	FHitResult Hit;
	FCollisionQueryParams Query(TEXT("TDEinsatzKern"), false, Spieler);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Ort + FVector(0, 0, 5000.f), Ort - FVector(0, 0, 20000.f), ECC_Visibility, Query))
	{
		Ort = Hit.ImpactPoint;
	}
	const FTransform Transform(FRotator(0.f, (-Vorne).Rotation().Yaw, 0.f), Ort);
	AFGBuildable* Gebaut = Bau->BeginSpawnBuildable(Klasse, Transform);
	if (!Gebaut)
	{
		return nullptr;
	}
	Gebaut->SetBuiltWithRecipe(LoadClass<UFGRecipe>(nullptr, KernRezeptPfad));
	Gebaut->FinishSpawning(Transform);
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: Abwehrkern gesetzt bei %s"), *Ort.ToCompactString());
	return Cast<ATDDefenseCore>(Gebaut);
}

void UTDEinsatzSubsystem::SchalteStartFrei()
{
	AFGRecipeManager* Rezepte = AFGRecipeManager::Get(GetWorld());
	if (!Rezepte)
	{
		return;
	}
	for (const TCHAR* Pfad : StartRezepte)
	{
		if (UClass* Rezept = LoadClass<UFGRecipe>(nullptr, Pfad))
		{
			Rezepte->AddAvailableRecipe(Rezept);
		}
		else
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Einsatz: Rezept fehlt %s"), Pfad);
		}
	}
}

void UTDEinsatzSubsystem::Starte(ATDDefenseCore* Kern, int32 Stufe, int32 Wellen, APawn* Spieler, TArray<FString>& Meldungen)
{
	if (!Kern)
	{
		Kern = SetzeKern(Spieler);
		if (!Kern)
		{
			Meldungen.Add(TDT(TEXT("Abwehrkern konnte nicht gesetzt werden."), TEXT("Could not place the Defense Core.")));
			return;
		}
		Meldungen.Add(TDT(TEXT("Abwehrkern 10 m vor dir gesetzt."), TEXT("Defense Core placed 10 m in front of you.")));
	}
	SchalteStartFrei();
	Verbinde();
	mKern = Kern;
	mAktiv = true;
	mFrei = false;
	Kern->mEinsatzAktiv = true;
	Kern->mEinsatzFrei = false;
	// Startkit nur einmal je Einsatz (sonst liesse es sich durch Neustart beliebig oft holen)
	if (!Kern->mEinsatzStartkit)
	{
		// Startkit = Baumaterial fuer das ganze Startkontingent (aus den Rezepten) + Brennstoff
		TMap<UClass*, int32> Kit;
		for (const TPair<const TCHAR*, int32>& Eintrag : StartKontingent)
		{
			const FString Key = Eintrag.Key;
			if (Key != TEXT("Abwehrkern") && Key != TEXT("MAM"))   // Kern setzt der Einsatz, MAM ist kostenlos
			{
				SammleBaumaterial(Key, StartMenge(Eintrag.Key, Eintrag.Value), Kit);
			}
		}
		for (const TPair<const TCHAR*, int32>& R : StartRessourcen)
		{
			if (UClass* Klasse = LoadClass<UFGItemDescriptor>(nullptr, R.Key))
			{
				Kit.FindOrAdd(Klasse) += R.Value;
			}
		}
		Kern->mEinsatzStartkit = true;
		Meldungen.Add(TDT(TEXT("Startkit: "), TEXT("Starter kit: ")) + GibMaterial(Kit));
		// 0.84: Wellen je Einsatz verschieden (Option, Standard aus); einmal je Einsatz gewuerfelt und im Kern gespeichert
		Kern->mLaufSaat = TDBalance::Wert(TEXT("Wellen.VariationJeEinsatz"), 0.f) > 0.f ? (FMath::Rand() | 1) : 0;
		UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: Wellensaat %d (%s)"), Kern->mLaufSaat, Kern->mLaufSaat ? TEXT("Wellen je Einsatz verschieden") : TEXT("Wellen je Kernplatz fest"));
	}
	if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
	{
		Waves->SetManuell(true);
		if (!Waves->StarteKernVerteidigung(Kern, Wellen, -1.f, -1.f, Stufe))
		{
			Meldungen.Add(TDT(TEXT("Es laeuft schon eine Kern-Verteidigung - erst /td kern stop."), TEXT("A Core defense is already running - use /td kern stop first.")));
		}
	}
	mBelegtZeit = -1.f;
	AktualisiereGratisRezepte();
	ZeigeHud();
	Meldungen.Append(StatusText());
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz gestartet (Stufe %d, %d Wellen)"), Stufe, Wellen);
}

void UTDEinsatzSubsystem::Beende()
{
	VersteckeHud();
	mAktiv = false;
	mFrei = false;
	if (ATDDefenseCore* Kern = mKern.Get())
	{
		Kern->mEinsatzAktiv = false;
		Kern->mEinsatzFrei = false;
		Kern->mEinsatzStartkit = false;
		Kern->mEinsatzPunkte = 0;
		Kern->mKontingentGekauft.Reset();
	}
	if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
	{
		Waves->SetManuell(false);
		Waves->StoppeLauf(TDT(TEXT("Einsatz beendet."), TEXT("Operation ended.")));
	}
	AktualisiereToDo();
	StelleRezepteWiederHer();
}

int32 UTDEinsatzSubsystem::LaufEnde() const
{
	const ATDDefenseCore* Kern = mKern.Get();
	return mAktiv && Kern ? Kern->mLaufEnde : 0;
}

FString UTDEinsatzSubsystem::WiederholeWelle(APlayerController* Spieler)
{
	const ATDDefenseCore* Kern = mKern.Get();
	if (!Kern || Kern->mLaufEnde != 2)
	{
		return TDT(TEXT("Wiederholen geht nur nach einer Niederlage."), TEXT("Retry is only possible after a defeat."));
	}
	const FString Name = FString::Printf(TEXT("FicsitTD_W%02d_Start"), Kern->mLaufEndWelle);
	UFGSaveSystem* Speicher = UFGSaveSystem::Get(GetWorld());
	FSaveHeader Kopf;
	if (!Speicher || !Speicher->LoadSaveGameHeaderSync(Name, Kopf))
	{
		return TDF(TEXT("Kein Spielstand vom Start der Welle %d gefunden (%s)."), TEXT("No save from the start of wave %d found (%s)."), Kern->mLaufEndWelle, *Name);
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: Welle %d wiederholen, lade %s"), Kern->mLaufEndWelle, *Name);
	FLoadSaveFileParameters Parameter;
	Parameter.bEnableAdvancedGameSettings = Kopf.IsCreativeModeEnabled;
	Speicher->LoadSaveFile(Kopf, Parameter, Spieler);
	return TDF(TEXT("Lade den Stand vom Start der Welle %d ..."), TEXT("Loading the state from the start of wave %d ..."), Kern->mLaufEndWelle);
}

FString UTDEinsatzSubsystem::StarteNeu(APawn* Spieler)
{
	ATDDefenseCore* Kern = mKern.Get();
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!Kern || !Waves)
	{
		return TDT(TEXT("Kein Abwehrkern."), TEXT("No Defense Core."));
	}
	const int32 Stufe = Waves->GetLaufStufe();
	const int32 Wellen = Waves->mWellenGesamt;
	// Startkit nur einmal je Einsatz, auch ueber den Neustart hinweg (sonst Material durch Verlieren)
	const bool bKit = Kern->mEinsatzStartkit;
	Beende();
	Kern->Aufladen();
	Kern->mEinsatzStartkit = bKit;
	TArray<FString> Meldungen;
	Starte(Kern, Stufe, Wellen, Spieler, Meldungen);
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz neu gestartet (Stufe %d, %d Wellen): %s"), Stufe, Wellen, *FString::Join(Meldungen, TEXT(" | ")));
	return TDT(TEXT("Neuer Durchgang ab Welle 1. Fabrik und Türme bleiben stehen, Coupons und Käufe beginnen von vorn."),
		TEXT("New run from wave 1. Factory and turrets stay, Coupons and purchases start over."));
}

FString UTDEinsatzSubsystem::BeendeNachEnde()
{
	ATDDefenseCore* Kern = mKern.Get();
	Beende();
	if (Kern)
	{
		Kern->Aufladen();
		Kern->mLaufEnde = 0;
	}
	return TDT(TEXT("Einsatz beendet: keine Kontingente und Sperren mehr. Der Kern lässt sich jetzt abreißen."),
		TEXT("Operation ended: no more quotas or locks. The Core can now be dismantled."));
}

void UTDEinsatzSubsystem::Lade(ATDDefenseCore* Kern)
{
	mKern = Kern;
	mAktiv = Kern && Kern->mEinsatzAktiv;
	mFrei = Kern && Kern->mEinsatzFrei;
	mBelegtZeit = -1.f;
	if (mAktiv)
	{
		Verbinde();
		// Auch beim Laden: Spielstaende aus aelteren Versionen bekommen neu hinzugekommene Startgebaeude (Test 0.20.0: MAM fehlte)
		SchalteStartFrei();
		AktualisiereGratisRezepte();
		ZeigeHud();
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz geladen: aktiv %d, freies Bauen %d, Konto %d EP, gekauft %d Posten"), mAktiv, mFrei,
		Konto(), Kern ? Kern->mKontingentGekauft.Num() : 0);
	if (mAktiv)
	{
		// G3: Kontingente nach dem Laden (Zaehlung braucht die geladenen Gebaeude, deshalb kurz spaeter)
		FTimerHandle Handle;
		GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			TArray<FString> Teile;
			for (const FTDKontingentZeile& Z : KontingentZeilen())
			{
				Teile.Add(FString::Printf(TEXT("%s %d/%d"), *Z.Key, Z.Belegt, Z.Grenze));
			}
			UE_LOG(LogFicsitTD, Log, TEXT("Einsatz nach dem Laden: %s"), *FString::Join(Teile, TEXT(", ")));
		}), 5.f, false);
		// 0.10.0: Umstellung alter Staende, sobald das Spielerinventar da ist (Test 14.11.: der Spieler kam erst ~20 s nach dem Laden)
		mUmstellVersuche = 0;
		GetWorld()->GetTimerManager().SetTimer(mUmstellTimer, FTimerDelegate::CreateUObject(this, &UTDEinsatzSubsystem::UmstellungNachLaden), 2.f, true, 2.f);
	}
}

void UTDEinsatzSubsystem::UmstellungNachLaden()
{
	if (!SpielerInventar(GetWorld()) && ++mUmstellVersuche < 90)
	{
		return;   // noch kein Spieler: bis zu 3 min weiter warten
	}
	GetWorld()->GetTimerManager().ClearTimer(mUmstellTimer);
	if (!SpielerInventar(GetWorld()))
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Umstellung 0.10: nach 3 min kein Spielerinventar - Kampfdaten und wartende Coupons bleiben bis zum naechsten Nachreichen"));
		return;
	}
	const int32 Kampfdaten = TauscheAlteKampfdaten();
	const int32 Festplatten = GibOffeneFP();
	const int32 Coupons = GibOffeneCoupons();
	UE_LOG(LogFicsitTD, Log, TEXT("Umstellung 0.10 nach dem Laden (nach %d s): %d Kampfdaten getauscht, %d Festplatten und %d Coupons ins Inventar, Konto %d"),
		2 * (mUmstellVersuche + 1), Kampfdaten, Festplatten, Coupons, Konto());
}

void UTDEinsatzSubsystem::SetzeFrei(bool bFrei)
{
	mFrei = bFrei;
	if (ATDDefenseCore* Kern = mKern.Get())
	{
		Kern->mEinsatzFrei = bFrei;
	}
}

TArray<FString> UTDEinsatzSubsystem::StatusText()
{
	TArray<FString> Zeilen;
	if (!mAktiv)
	{
		Zeilen.Add(TDT(TEXT("Kein Einsatz aktiv (/td einsatz start [stufe] [wellen])."), TEXT("No operation active (/td einsatz start [stufe] [wellen]).")));
		return Zeilen;
	}
	Zeilen.Add(mFrei ? TDT(TEXT("Einsatz aktiv - FREIES BAUEN (Kontingente und Bausperre aus, /td einsatz frei aus)"),
			TEXT("Operation active - FREE BUILD (quotas and build lock off, /td einsatz frei aus)"))
		: TDT(TEXT("Einsatz aktiv - Kontingente gelten, Bauen waehrend der Welle gesperrt (/td einsatz frei an)"),
			TEXT("Operation active - quotas apply, building is locked during a wave (/td einsatz frei an)")));
	ZaehleNeu();
	TArray<FString> Teile;
	TSet<FString> Gezeigt;
	for (const TPair<const TCHAR*, int32>& Eintrag : StartKontingent)
	{
		Teile.Add(FString::Printf(TEXT("%s %d/%d"), *TDAnzeige(Eintrag.Key), Belegt(Eintrag.Key), Grenze(Eintrag.Key)));
		Gezeigt.Add(Eintrag.Key);
	}
	if (const ATDDefenseCore* Kern = mKern.Get())
	{
		for (const TPair<FString, int32>& Kauf : Kern->mKontingentGekauft)
		{
			if (!Gezeigt.Contains(Kauf.Key))
			{
				Teile.Add(FString::Printf(TEXT("%s %d/%d"), *TDAnzeige(Kauf.Key), Belegt(Kauf.Key), Grenze(Kauf.Key)));
			}
		}
		Zeilen.Add(TDF(TEXT("Konto: %d Coupons"), TEXT("Balance: %d Coupons"), Konto()));
	}
	Zeilen.Add(TDT(TEXT("Kontingent: "), TEXT("Quota: ")) + FString::Join(Teile, TEXT(", ")));
	return Zeilen;
}

void UTDEinsatzSubsystem::HeileVerteidigung()
{
	if (!mAktiv)
	{
		return;
	}
	if (ATDDefenseCore* Kern = mKern.Get())
	{
		Kern->LadeSchild();
	}
	if (UTDPionier* Pionier = UTDPionier::Get(this))
	{
		Pionier->WellenStart();
	}
	int32 Geheilt = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		ITDDamageable* Ziel = Cast<ITDDamageable>(*It);
		if (Ziel && !It->IsA<ATDDefenseCore>() && Ziel->IsTDIntact() && Ziel->GetTDHealth() < Ziel->GetTDMaxHealth())
		{
			Ziel->HealTD(Ziel->GetTDMaxHealth());
			++Geheilt;
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Einsatz: %d Verteidigungsanlagen zu Wellenbeginn auf 100 %%"), Geheilt);
}
