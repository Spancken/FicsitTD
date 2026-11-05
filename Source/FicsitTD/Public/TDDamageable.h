#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Creature/FGCreature.h"
#include "GameFramework/Controller.h"
#include "TDDamageable.generated.h"

UINTERFACE(MinimalAPI)
class UTDDamageable : public UInterface
{
	GENERATED_BODY()
};

/** Kommt der Schaden von einem Gegner (Kreatur selbst, ihr Geschoss oder ihr Controller)?
 *  Test 0.28.0: Giftwolken der Umgebung zerstoerten Mauern - Umgebung, Spielerwaffen usw. zaehlen nicht.
 *  Die Wellengegner greifen ohnehin direkt ueber ApplyTDDamage an. */
namespace TDSchaden
{
	inline bool VonGegner(const AActor* Verursacher, const AController* Ausloeser)
	{
		auto IstKreatur = [](const AActor* A) { return A && A->IsA<AFGCreature>(); };
		if (IstKreatur(Verursacher))
		{
			return true;
		}
		if (Verursacher && (IstKreatur(Verursacher->GetInstigator()) || IstKreatur(Verursacher->GetOwner())))
		{
			return true;
		}
		return Ausloeser && IstKreatur(Ausloeser->GetPawn());
	}
}

/** Alles, was Lebenspunkte hat und von Koedern Schaden weitergereicht bekommt (Testmauer, Verteidigungsmauer, spaeter Tuerme/Kern). */
class FICSITTD_API ITDDamageable
{
	GENERATED_BODY()
public:
	virtual void ApplyTDDamage(float Amount, const AActor* Causer) = 0;
	virtual bool IsTDIntact() const = 0;
	virtual float GetTDHealth() const = 0;
	virtual float GetTDMaxHealth() const = 0;
	/** Reparatur (Reparaturstation); Standard: nicht reparierbar */
	virtual float HealTD(float Amount) { return 0.f; }
};
