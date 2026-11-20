#include "TDGameInstanceModule.h"
#include "FicsitTDModule.h"
#include "TDSprache.h"
#include "TDBalance.h"
#include "TDKernTerminal.h"
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
	// Hilfetext unten im Menue (keine Zeilenumbrueche moeglich, das Spiel bricht selbst um).
	// 18.11. (Marius, Screenshot): der lange Text aller vier Modi nahm dem Menue fast die ganze Hoehe, die Einstellungen waren nur noch
	// ein schmaler Streifen. Kurz wie die Texte des Spiels; die Auswahl nennt jeden Modus schon, Einzelheiten stehen im Handbuch.
	mModus->ToolTip = FText::FromString(TDT(
		TEXT("Wie FICSIT Tower Defense dieses Spiel verändert: Aus, Einsatz (50 Wellen), Endlos-Wellen oder Freies Spiel mit Abwehr. "
			"Einzelheiten im Handbuch: spancken.github.io/FicsitTD"),
		TEXT("How FICSIT Tower Defense changes this game: Off, Operation (50 waves), Endless waves or Free play with defense. "
			"Details in the player guide: spancken.github.io/FicsitTD")));
	if (UFGUserSetting_IntSelector* Wahl = Cast<UFGUserSetting_IntSelector>(mModus->ValueSelector))
	{
		// 18.11. (Marius: Tester finden die Einstellung kaum): neue Spiele stehen auf Einsatz. Bestehende Welten ohne gespeicherten Modus
		// bekaemen damit auch Einsatz - davor schuetzt UTDKernplatz::PruefeTDStart (eigene Bauten, kein Kern -> Modus Aus).
		Wahl->DefaultValue = ETDModus::Einsatz;
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
	// 18.11.: eigene, gut lesbare Auswahl statt der schmalen Aufklappliste (Schalter Spielmodus.EigeneAuswahl)
	if (TDBalance::Wert(TEXT("Spielmodus.EigeneAuswahl"), 1.f) > 0.f)
	{
		mModus->CustomValueSelectorWidget = TSoftClassPtr<UFGOptionsValueController>(UTDAuswahlKarten::StaticClass());
		if (mStufe)
		{
			mStufe->CustomValueSelectorWidget = TSoftClassPtr<UFGOptionsValueController>(UTDAuswahlKarten::StaticClass());
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
			TEXT("Schwierigkeit für Einsatz und Endlos-Wellen; Standard nach Startort (Grasfelder Erkundung bis Dünenwüste Albtraum). "
				"Hart und Albtraum sind noch nicht fertig eingestellt."),
			TEXT("Difficulty for Operation and Endless waves; by default it follows the start location (Grass Fields Exploration to Dune Desert Nightmare). "
				"Hard and Nightmare are not fully balanced yet.")));
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
