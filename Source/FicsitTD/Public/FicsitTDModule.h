#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogFicsitTD, Log, All);

class FICSITTD_API FFicsitTDModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	/** Wie oft der Hook auf AFGCreature::SetEnabled das Abschalten eines Wellengegners verhindert hat */
	static int32 AbschaltenVerhindert;
	/** Wellengegner nach Adresse (Absturz 0.56 beim Speichern: der ShouldSave-Hook las die Tags eines ungueltigen Objekts).
	 *  Der Hook vergleicht nur noch die Adresse und prueft ueber den schwachen Zeiger, ohne das Objekt anzufassen. */
	static void MerkeWellenKreatur(class AActor* Kreatur);
	static bool IstGemerkteWellenKreatur(const void* Adresse);
};
