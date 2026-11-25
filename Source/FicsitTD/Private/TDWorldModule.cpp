#include "TDWorldModule.h"
#include "TDBalance.h"
#include "TDSpielstand.h"
#include "TDSprache.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Commands/MauerCommand.h"
#include "Commands/TDDebugCommand.h"
#include "Commands/WelleCommand.h"
#include "FicsitTDModule.h"
#include "FGResearchTree.h"
#include "FGSchematic.h"
#include "TDShop.h"
#include "TDForschungListe.inl"

UTDWorldModule::UTDWorldModule()
{
	bRootModule = true;
	// 0.9.0 Alpha: alte Testbefehle /welle und /mauer nur im Entwicklermodus (Saved/FicsitTD/Entwickler.txt oder -TDEntwickler)
	if (FParse::Param(FCommandLine::Get(), TEXT("TDEntwickler")) || FPaths::FileExists(FPaths::ProjectSavedDir() / TEXT("FicsitTD") / TEXT("Entwickler.txt")))
	{
		mChatCommands.Add(AWelleCommand::StaticClass());
		mChatCommands.Add(AMauerCommand::StaticClass());
	}
	mChatCommands.Add(ATDDebugCommand::StaticClass());
	// 0.12-dev: Fortschritt je Spielstand (Freies Spiel mit Abwehr)
	ModSubsystems.Add(ATDSpielstand::StaticClass());
}

void UTDWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		// Balance-Datei bei jedem Spielstart frisch lesen (legt sie beim ersten Mal an)
		TDBalance::Lade();
		// 0.85 L1: Asset-Texte bei englischer Spielsprache ersetzen
		TDSprache::UebersetzeAssets();
		// 0.12 E4: Preise im CDO leben laenger als eine Welt -> je Welt auf den Grundpreis (Lade/Starte setzen die Staffel)
		TDShop::SetzeZurueck();
		// Hooks auf Shop und Schematic-Verwaltung: Spiel-Artikel im Einsatz sperren, Posten des Mods pruefen und wirken
		TDShop::RichteSperreEin();
	}
	if (Phase == ELifecyclePhase::CONSTRUCTION && mResearchTrees.Num() == 0)
	{
		// 0.12 E4: Posten fuer den AWESOME-Shop (Scripts/create_shop.py, je Eintrag der Ladenliste). Sichtbar nur im Einsatz
		// (UTDShopAbhaengigkeit), gekauft und gewirkt ueber die Hooks in TDShop.cpp.
		int32 ShopPosten = 0;
		for (const FString& Pfad : TDShop::PostenPfade())
		{
			if (UClass* Klasse = LoadClass<UFGSchematic>(nullptr, *Pfad))
			{
				mSchematics.Add(Klasse);
				++ShopPosten;
			}
		}
		UE_LOG(LogFicsitTD, Log, TEXT("Shop: %d von %d Posten eingetragen"), ShopPosten, TDShop::PostenPfade().Num());
		// Einsatz-Konzept Abschnitt 5: MAM-Baeume (erzeugt von Scripts/create_research.py)
		for (const TCHAR* Pfad : TDForschungsSchemata)
		{
			if (UClass* Klasse = LoadClass<UFGSchematic>(nullptr, Pfad))
			{
				mSchematics.Add(Klasse);
			}
		}
		for (const TCHAR* Pfad : TDForschungsBaeume)
		{
			if (UClass* Klasse = LoadClass<UFGResearchTree>(nullptr, Pfad))
			{
				mResearchTrees.Add(Klasse);
			}
			else
			{
				UE_LOG(LogFicsitTD, Warning, TEXT("Forschungsbaum fehlt: %s"), Pfad);
			}
		}
		UE_LOG(LogFicsitTD, Log, TEXT("MAM: %d Baeume, %d Forschungen eingetragen"), mResearchTrees.Num(), mSchematics.Num());
	}
	Super::DispatchLifecycleEvent(Phase);
}
