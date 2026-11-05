#include "TDWorldModule.h"
#include "TDBalance.h"
#include "Commands/MauerCommand.h"
#include "Commands/TDDebugCommand.h"
#include "Commands/WelleCommand.h"
#include "FicsitTDModule.h"
#include "FGResearchTree.h"
#include "FGSchematic.h"
#include "TDForschungListe.inl"

UTDWorldModule::UTDWorldModule()
{
	bRootModule = true;
	mChatCommands.Add(AWelleCommand::StaticClass());
	mChatCommands.Add(AMauerCommand::StaticClass());
	mChatCommands.Add(ATDDebugCommand::StaticClass());
}

void UTDWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		// Balance-Datei bei jedem Spielstart frisch lesen (legt sie beim ersten Mal an)
		TDBalance::Lade();
	}
	if (Phase == ELifecyclePhase::CONSTRUCTION && mResearchTrees.Num() == 0)
	{
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
