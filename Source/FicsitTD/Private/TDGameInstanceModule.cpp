#include "TDGameInstanceModule.h"
#include "FicsitTDModule.h"
#include "TDSprache.h"
#include "SessionSettings/SessionSetting.h"
#include "Settings/FGUserSetting.h"
#include "Settings/FGUserSettingCategory.h"   // 16.11.: kam bisher nur zufaellig ueber die Unity-Gruppe mit
#include "UObject/ConstructorHelpers.h"

const TCHAR* UTDGameInstanceModule::TowerDefenseId = TEXT("FicsitTD.TowerDefense");
const TCHAR* UTDGameInstanceModule::ModusId = TEXT("FicsitTD.ModusInt");
const TCHAR* UTDGameInstanceModule::StufeId = TEXT("FicsitTD.StufeInt");

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
	// 0.11 (Gegencheck Einsatz, Marius 16.11.): Stufe waehlbar (Scripts/create_session_stufe.py)
	static ConstructorHelpers::FObjectFinder<USMLSessionSetting> Stufe(TEXT("/FicsitTD/SessionSettings/SessionSetting_Stufe.SessionSetting_Stufe"));
	if (Stufe.Succeeded())
	{
		mStufe = Stufe.Object;
		SessionSettings.Add(Stufe.Object);
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
			"geforscht wird im MAM mit Festplatten, HUB-Meilensteine und Blaupausen sind gesperrt. Die Schwierigkeit stellst du unter „Stufe“ ein, "
			"Standard ist der Startort: Grasfelder leicht, Felsenwüste normal, Nördlicher Wald schwer, Dünenwüste Albtraum."),
		TEXT("Choose how FICSIT Tower Defense changes this game. "
			"OFF: regular Satisfactory, the mod stays out of the way. "
			"OPERATION: you land at a fixed core site of your start location, the platform and the Defense Core are already built. Creatures attack the core in 50 waves, "
			"in five phases with bosses. Your factory supplies the ammo for the turrets. Buildings are limited by quotas (FICSIT Coupons buy more), "
			"research happens in the MAM with Hard Drives, HUB milestones and blueprints are locked. Set the difficulty under \"Difficulty\", "
			"by default it follows the start location: Grass Fields easy, Rocky Desert normal, Northern Forest hard, Dune Desert nightmare.")));
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
			"hält deine Verteidigung, gibt es Erz, beim ersten Erfolg je Rohstoff und Stufe auch Festplatten, auf Schwer zusätzlich einen Talentpunkt."),
		TEXT(" FREE PLAY WITH DEFENSE: regular Satisfactory with HUB and milestones, plus turrets, walls and defense research in the MAM (paid with Coupons). "
			"Nothing attacks unless you switch it on: place a Defense Core as a deep drill next to a resource node and start a drilling there - "
			"if your defense holds, you get ore, and on the first success per resource and level also Hard Drives, plus a Talent Point on Hard.")));
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
	if (mStufe)
	{
		mStufe->DisplayName = FText::FromString(TDT(TEXT("Stufe"), TEXT("Difficulty")));
		mStufe->ToolTip = FText::FromString(TDT(
			TEXT("Schwierigkeit für Einsatz und Endlos-Wellen. NACH STARTORT: Grasfelder Erkundung, Felsenwüste Normal, Nördlicher Wald Hart, Dünenwüste Albtraum. "
				"Erkundung ist ohne Druck zum Kennenlernen, Normal ist die vorgesehene Stufe. Hart und Albtraum sind noch ungetestet: Sie sind nur in der Simulation eingestellt, "
				"ein ganzer Lauf im Spiel steht aus."),
			TEXT("Difficulty for Operation and Endless waves. BY START LOCATION: Grass Fields Exploration, Rocky Desert Normal, Northern Forest Hard, Dune Desert Nightmare. "
				"Exploration has no pressure and is for getting to know the mod, Normal is the intended difficulty. Hard and Nightmare are still untested: they are only tuned "
				"in the simulation, a full run in the game is still missing.")));
		if (UFGUserSetting_IntSelector* Wahl = Cast<UFGUserSetting_IntSelector>(mStufe->ValueSelector))
		{
			for (FIntegerSelection& W : Wahl->IntegerSelectionValues)
			{
				switch (W.Value)
				{
				case 0: W.Name = FText::FromString(TDT(TEXT("Nach Startort"), TEXT("By start location"))); break;
				case 1: W.Name = FText::FromString(TDT(TEXT("Erkundung – ohne Druck"), TEXT("Exploration – no pressure"))); break;
				case 2: W.Name = FText::FromString(TDT(TEXT("Normal"), TEXT("Normal"))); break;
				case 3: W.Name = FText::FromString(TDT(TEXT("Hart (ungetestet)"), TEXT("Hard (untested)"))); break;
				default: W.Name = FText::FromString(TDT(TEXT("Albtraum (ungetestet)"), TEXT("Nightmare (untested)"))); break;
				}
			}
		}
	}
	else
	{
		UE_LOG(LogFicsitTD, Warning, TEXT("Stufe: Einstellung SessionSetting_Stufe nicht gefunden - Stufe folgt dem Startort"));
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Spielmodus: Auswahl %s (%s) mit Texten in %s"), *mModus->StrId,
		mModus->ValueSelector ? *mModus->ValueSelector->GetClass()->GetName() : TEXT("ohne Auswahl"), TDSprache::Englisch() ? TEXT("Englisch") : TEXT("Deutsch"));
}
