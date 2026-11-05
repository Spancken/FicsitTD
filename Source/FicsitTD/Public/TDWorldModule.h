#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "TDWorldModule.generated.h"

/** Root-Modul der Spielwelt: registriert die Chat-Befehle. SML findet native Root-Module automatisch. */
UCLASS()
class FICSITTD_API UTDWorldModule : public UGameWorldModule
{
	GENERATED_BODY()
public:
	UTDWorldModule();
	/** Vor der Registrierung (INITIALIZATION): MAM-Baeume und Schematics aus TDForschungListe.inl eintragen */
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
