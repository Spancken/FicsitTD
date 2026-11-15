#include "TDGameInstanceModule.h"
#include "FicsitTDModule.h"
#include "TDSprache.h"
#include "SessionSettings/SessionSetting.h"
#include "Settings/FGUserSetting.h"
#include "UObject/ConstructorHelpers.h"

const TCHAR* UTDGameInstanceModule::TowerDefenseId = TEXT("FicsitTD.TowerDefense");
const TCHAR* UTDGameInstanceModule::ModusId = TEXT("FicsitTD.ModusInt");

UTDGameInstanceModule::UTDGameInstanceModule()
{
	bRootModule = true;
	// 0.10.1: Auswahl "Spielmodus" in eigener Kategorie (Scripts/create_session_modus.py). Die alte Ankreuzbox
	// (SessionSetting_TowerDefense, bei den SML-Cheats) wird nicht mehr angezeigt.
	static ConstructorHelpers::FObjectFinder<USMLSessionSetting> Modus(TEXT("/FicsitTD/SessionSettings/SessionSetting_Modus.SessionSetting_Modus"));
	if (Modus.Succeeded())
	{
		mModus = Modus.Object;
		SessionSettings.Add(Modus.Object);
	}
}

void UTDGameInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase == ELifecyclePhase::CONSTRUCTION)
	{
		SetzeTexte();
	}
	Super::DispatchLifecycleEvent(Phase);
}

/** Anzeigename einer Kategorie (Blueprint-Klasse) zur Laufzeit setzen - mDisplayName ist geschuetzt, deshalb ueber Reflexion */
static void SetzeKategorieName(UClass* Kategorie, const FText& Name)
{
	UObject* Standard = Kategorie ? Kategorie->GetDefaultObject() : nullptr;
	FTextProperty* Feld = Standard ? FindFProperty<FTextProperty>(Standard->GetClass(), TEXT("mDisplayName")) : nullptr;
	if (Feld)
	{
		Feld->SetPropertyValue_InContainer(Standard, Name);
	}
}

void UTDGameInstanceModule::SetzeTexte()
{
	if (!mModus)
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Spielmodus: Einstellung SessionSetting_Modus nicht gefunden"));
		return;
	}
	mModus->DisplayName = FText::FromString(TDT(TEXT("Spielmodus"), TEXT("Game Mode")));
	// Hilfetext unten im Menue (keine Zeilenumbrueche moeglich, das Spiel bricht selbst um)
	mModus->ToolTip = FText::FromString(TDT(
		TEXT("Wähle, wie FICSIT Tower Defense dieses Spiel verändert. "
			"AUS: normales Satisfactory, die Mod greift nicht ein. "
			"EINSATZ: Du landest an einem festen Kernplatz deines Startorts, Plattform und Abwehrkern stehen schon. Kreaturen greifen den Kern in 50 Wellen an, "
			"in fünf Phasen mit Bossen. Deine Fabrik liefert die Munition für die Türme. Gebäude sind über Kontingente begrenzt (mehr gibt es für FICSIT-Coupons), "
			"geforscht wird im MAM mit Festplatten, HUB-Meilensteine und Blaupausen sind gesperrt. Die Schwierigkeit folgt dem Startort: Grasfelder leicht, "
			"Felsenwüste normal, Nördlicher Wald schwer, Dünenwüste Albtraum. Weitere Modi folgen."),
		TEXT("Choose how FICSIT Tower Defense changes this game. "
			"OFF: regular Satisfactory, the mod stays out of the way. "
			"OPERATION: you land at a fixed core site of your start location, the platform and the Defense Core are already built. Creatures attack the core in 50 waves, "
			"in five phases with bosses. Your factory supplies the ammo for the turrets. Buildings are limited by quotas (FICSIT Coupons buy more), "
			"research happens in the MAM with Hard Drives, HUB milestones and blueprints are locked. Difficulty follows the start location: Grass Fields easy, "
			"Rocky Desert normal, Northern Forest hard, Dune Desert nightmare. More modes will follow.")));
	if (UFGUserSetting_IntSelector* Wahl = Cast<UFGUserSetting_IntSelector>(mModus->ValueSelector))
	{
		for (FIntegerSelection& W : Wahl->IntegerSelectionValues)
		{
			if (W.Value == ETDModus::Aus)
			{
				W.Name = FText::FromString(TDT(TEXT("Aus – normales Satisfactory"), TEXT("Off – regular Satisfactory")));
			}
			else if (W.Value == ETDModus::Einsatz)
			{
				W.Name = FText::FromString(TDT(TEXT("Einsatz – Abwehrkern, 50 Wellen"), TEXT("Operation – Defense Core, 50 waves")));
			}
		}
	}
	for (const FSettingsWidgetLocationDescriptor& Ort : mModus->WidgetsToCreate)
	{
		SetzeKategorieName(Ort.CategoryClass, FText::FromString(TEXT("FICSIT Tower Defense")));
		SetzeKategorieName(Ort.SubCategoryClass, FText::FromString(TDT(TEXT("Spielmodus"), TEXT("Game Mode"))));
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Spielmodus: Auswahl %s (%s) mit Texten in %s"), *mModus->StrId,
		mModus->ValueSelector ? *mModus->ValueSelector->GetClass()->GetName() : TEXT("ohne Auswahl"), TDSprache::Englisch() ? TEXT("Englisch") : TEXT("Deutsch"));
}
