#include "TDGameInstanceModule.h"
#include "SessionSettings/SessionSetting.h"
#include "UObject/ConstructorHelpers.h"

const TCHAR* UTDGameInstanceModule::TowerDefenseId = TEXT("FicsitTD.TowerDefense");

UTDGameInstanceModule::UTDGameInstanceModule()
{
	bRootModule = true;
	// Asset aus Scripts/create_session_setting.py (Kopie des Schalters "Force Allow Cheats" von SML)
	static ConstructorHelpers::FObjectFinder<USMLSessionSetting> Schalter(TEXT("/FicsitTD/SessionSettings/SessionSetting_TowerDefense.SessionSetting_TowerDefense"));
	if (Schalter.Succeeded())
	{
		SessionSettings.Add(Schalter.Object);
	}
}
