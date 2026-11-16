#include "TDGameInstanceModule.h"
#include "FicsitTDModule.h"
#include "TDSprache.h"
#include "SessionSettings/SessionSetting.h"
#include "Settings/FGUserSetting.h"
#include "Settings/FGUserSettingCategory.h"   // 16.11.: kam bisher nur zufaellig ueber die Unity-Gruppe mit
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
	// 0.11 (Marius 15.11.): dritter Modus "Endlos-Wellen" - Hilfetext ergaenzen
	mModus->ToolTip = FText::FromString(mModus->ToolTip.ToString() + TDT(
		TEXT(" ENDLOS-WELLEN: wie Einsatz, aber die Wellen kommen auf Zeit und ohne Ende aus einer einzigen Richtung. Anhalten geht nicht - "
			"baue weiter aus und halte die Türme ständig mit Munition versorgt. Coupons und Festplatten gibt es wie im Einsatz."),
		TEXT(" ENDLESS WAVES: like Operation, but waves arrive on a timer and never stop, always from one direction. You cannot pause them - "
			"keep expanding and keep your turrets supplied with ammo. Coupons and Hard Drives work as in Operation.")));
	// 0.12-dev (Marius 15.11.): vierter Modus "Freies Spiel mit Abwehr"
	mModus->ToolTip = FText::FromString(mModus->ToolTip.ToString() + TDT(
		TEXT(" FREIES SPIEL MIT ABWEHR: normales Satisfactory mit HUB und Meilensteinen, dazu Türme, Mauern und Abwehr-Forschung im MAM (mit Coupons). "
			"Nichts greift an, was du nicht einschaltest: Stell einen Abwehrkern als Tiefbohrer neben einen Rohstoffknoten und starte dort eine Bohrung - "
			"hält deine Verteidigung, gibt es Erz, beim ersten Erfolg auch Festplatten und einen Talentpunkt."),
		TEXT(" FREE PLAY WITH DEFENSE: regular Satisfactory with HUB and milestones, plus turrets, walls and defense research in the MAM (paid with Coupons). "
			"Nothing attacks unless you switch it on: place a Defense Core as a deep drill next to a resource node and start a drilling there - "
			"if your defense holds, you get ore, and on the first success also Hard Drives and a Talent Point.")));
	if (UFGUserSetting_IntSelector* Wahl = Cast<UFGUserSetting_IntSelector>(mModus->ValueSelector))
	{
		// 0.11: Eintrag fuer Wert 2 zur Laufzeit anlegen (das Asset kennt nur 0 und 1)
		if (!Wahl->IntegerSelectionValues.ContainsByPredicate([](const FIntegerSelection& W) { return W.Value == ETDModus::Endlos; }))
		{
			FIntegerSelection Neu;
			Neu.Value = ETDModus::Endlos;
			Wahl->IntegerSelectionValues.Add(Neu);
		}
		if (!Wahl->IntegerSelectionValues.ContainsByPredicate([](const FIntegerSelection& W) { return W.Value == ETDModus::Abwehr; }))
		{
			FIntegerSelection Neu;
			Neu.Value = ETDModus::Abwehr;
			Wahl->IntegerSelectionValues.Add(Neu);
		}
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
			else if (W.Value == ETDModus::Endlos)
			{
				W.Name = FText::FromString(TDT(TEXT("Endlos-Wellen – auf Zeit, eine Front"), TEXT("Endless waves – timed, one front")));
			}
			else if (W.Value == ETDModus::Abwehr)
			{
				W.Name = FText::FromString(TDT(TEXT("Freies Spiel mit Abwehr – HUB, Türme, Bohrung"), TEXT("Free play with defense – HUB, turrets, drilling")));
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
