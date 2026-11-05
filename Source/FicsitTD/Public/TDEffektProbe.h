#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TDEffektProbe.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Vorfuehrmodell fuer Animationen und Effekte kuenftiger Tuerme (/td probe gewehr|flamme).
 * Kein Gebaeude: nur Modelle aus Modelle\td_bau.py, Bewegung im Tick. Wechselt alle 3 s zwischen Feuern und Pause.
 * Gewehrturm: Laufbuendel laeuft hoch/aus, Muendungsfeuer. Flammenwerfer: Flammenstrahl waechst, flackert, erlischt.
 */
UCLASS()
class FICSITTD_API ATDEffektProbe : public AActor
{
	GENERATED_BODY()
public:
	ATDEffektProbe();

	/** bFlamme = Flammenwerfer, sonst Gewehrturm */
	void Einrichten(bool bFlamme);
	virtual void Tick(float DeltaSeconds) override;

private:
	UStaticMeshComponent* Teil(const TCHAR* Name, USceneComponent* Parent, const FString& Modell, const FVector& Ort);

	UPROPERTY() TObjectPtr<USceneComponent> mWurzel;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> mYaw;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> mHead;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> mLaeufe;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> mEffekt;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> mEffektMaterial;

	bool mFlamme = false;
	float mZeit = 0.f;
	float mLaufTempo = 0.f;        // Grad/s
	float mNaechsterBlitz = 0.f;
	float mBlitzBis = 0.f;
	float mFlammenLaenge = 0.f;    // cm
};
