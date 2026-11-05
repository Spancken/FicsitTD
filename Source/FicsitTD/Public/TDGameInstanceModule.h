#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "TDGameInstanceModule.generated.h"

/** 0.69.1 (Konzept-Kernplaetze S4): Spieleinstellung "FICSIT Tower Defense" fuer neue Spielstaende (SML Session Setting) */
UCLASS()
class FICSITTD_API UTDGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()
public:
	UTDGameInstanceModule();
	/** Schluessel der Einstellung */
	static const TCHAR* TowerDefenseId;
};
