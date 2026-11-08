#include "TDTeslaCoil.h"
#include "TDBalance.h"
#include "TDForschung.h"
#include "TDGegnerHirn.h"
#include "Components/StaticMeshComponent.h"
#include "Creature/FGCreature.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TDSprache.h"

/** Blitz-Modelle im Vorrat: begrenzt die Spruenge aus der Balance-Datei */
static constexpr int32 TeslaBlitzVorrat = 8;
/** Ziele je Entladung: Hauptziel + Munition.Tesla.Spruenge */
static int32 TeslaZiele() { return FMath::Clamp(1 + TDBalance::Ganz(TEXT("Munition.Tesla.Spruenge"), 3), 1, TeslaBlitzVorrat); }
static float TeslaSprungweite() { return TDBalance::Wert(TEXT("Munition.Tesla.Sprungweite"), 8.f) * 100.f; }
static float TeslaSchaden() { return TDBalance::Wert(TEXT("Munition.Tesla.Schaden"), 300.f); }
/** 0.73 Gabelung Kettenblitz | Ueberlast: Ziele je Entladung und Faktor am Hauptziel */
static int32 TeslaZieleMit(const UObject* Kontext, int32 Grund)
{
	if (TDForschung::Hat(Kontext, TEXT("Ueberlast")))
	{
		return 1;
	}
	return FMath::Clamp(Grund + (TDForschung::Hat(Kontext, TEXT("Kettenblitz")) ? TDBalance::Ganz(TEXT("Forschung.Kettenblitz.Spruenge"), 3) : 0), 1, TeslaBlitzVorrat);
}
static float TeslaUeberlast(const UObject* Kontext)
{
	return TDForschung::Hat(Kontext, TEXT("Ueberlast")) ? TDBalance::Wert(TEXT("Forschung.Ueberlast.Faktor"), 2.5f) : 1.f;
}
static constexpr float TeslaBlitzDauer = 0.25f;

ATDTeslaCoil::ATDTeslaCoil()
{
	mTurmName = TEXT("Teslaspule");
	mSchadensart = ETDSchadensart::Strom;
	mDreht = false;
	// Masse aus Modelle\TD_TeslaCoil_Stufen.blend (0.62, nach Marius' Stufenbildern), Unreal-cm
	mDrehpunkt = FVector(0.f, 0.f, 90.f);           // Spule steht fest auf dem Deck (liegt in mYawMesh)
	mMuendung = FVector(0.f, 0.f, 594.f);           // Kugelmitte im Spulen-Raum
	mLampenOrt = FVector(60.f, -120.f, 90.f);
	mRange = 3000.f;
	mKlarraumHoehe = 900.f;
	mYawModell = TEXT("/FicsitTD/Buildings/TeslaCoil/Mesh/SM_TD_TeslaCoil_Coil.SM_TD_TeslaCoil_Coil");
	mStandardRezept = TEXT("/FicsitTD/Buildings/TeslaCoil/Recipe_TD_Tesla_Discharge.Recipe_TD_Tesla_Discharge_C");

	for (int32 i = 0; i < TeslaBlitzVorrat; ++i)
	{
		UStaticMeshComponent* Blitz = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Blitz%d"), i));
		Blitz->SetupAttachment(RootComponent);
		Blitz->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Blitz->SetCastShadow(false);
		Blitz->SetVisibility(false);
		mBlitze.Add(Blitz);
	}
}

void ATDTeslaCoil::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* Blitz : mBlitze)
	{
		SetzeModell(Blitz, TEXT("/FicsitTD/Effects/SM_TD_Lightning.SM_TD_Lightning"));
		Blitz->SetVisibility(false);
		mBlitzMaterial.Add(Blitz->CreateDynamicMaterialInstance(0));
	}
	// Leuchtkanten der Ringe: Materialslot M_TD_TeslaGlow (Parameter Leuchtkraft)
	if (mYawMesh && mYawMesh->GetStaticMesh())
	{
		const int32 Slot = mYawMesh->GetStaticMesh()->GetMaterialIndex(TEXT("M_TD_TeslaGlow"));
		if (Slot != INDEX_NONE)
		{
			mGluehMaterial = mYawMesh->CreateDynamicMaterialInstance(Slot);
		}
	}
}

void ATDTeslaCoil::ZeigeBlitz(int32 Index, const FVector& Von, const FVector& Nach)
{
	if (!mBlitze.IsValidIndex(Index))
	{
		return;
	}
	UStaticMeshComponent* Blitz = mBlitze[Index];
	const FVector Weg = Nach - Von;
	// Modell: 1 m lang entlang +X -> auf die Strecke strecken
	Blitz->SetWorldLocationAndRotation(Von, Weg.Rotation());
	Blitz->SetWorldScale3D(FVector(Weg.Size() / 100.f, 1.f, 1.f));
	Blitz->SetVisibility(true);
}

void ATDTeslaCoil::Feuern(AFGCreature* Ziel)
{
	if (!Ziel)
	{
		return;   // aufgeladen, aber das Ziel ist weg: die Ladung verpufft
	}
	++mSchuesse;
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	TArray<AFGCreature*> Getroffen;
	FVector Von = GetMuzzleLocation();
	AFGCreature* Aktuell = Ziel;
	float Schaden = TeslaSchaden() * SchadenBonus() * TeslaUeberlast(this);
	const int32 Ziele = TeslaZieleMit(this, TeslaZiele() + FMath::RoundToInt(LevelWert(TEXT("Spruenge"))));
	const bool bBlitzableiter = TDKombo::Aktiv(this, TEXT("Blitzableiter"));
	int32 ZusatzZiele = 0;   // je Blitzableiter ein Sprung mehr, hoechstens TeslaBlitzVorrat Blitze
	for (int32 i = 0; i < FMath::Min(Ziele + ZusatzZiele, TeslaBlitzVorrat) && Aktuell; ++i)
	{
		const FVector Nach = Aktuell->GetActorLocation();
		ZeigeBlitz(i, Von, Nach);
		Treffer(Aktuell, Schaden, (Nach - Von).GetSafeNormal(), 1.f);
		Getroffen.Add(Aktuell);
		// naechstes Sprungziel: naechste noch nicht getroffene Kreatur im Umkreis
		AFGCreature* Naechstes = nullptr;
		float Best = FMath::Square(TeslaSprungweite());
		for (AFGCreature* F : Feinde)
		{
			const float D = FVector::DistSquared(F->GetActorLocation(), Nach);
			if (!Getroffen.Contains(F) && D < Best)
			{
				Best = D;
				Naechstes = F;
			}
		}
		// 0.84 Blitzableiter: steckende Bolzen sind Metall im Gegner - der Blitz springt weiter und bevorzugt dorthin, ohne Schwaechung
		bool bLeiter = false;
		if (bBlitzableiter)
		{
			float BestLeiter = FMath::Square(TeslaSprungweite() * TDBalance::Wert(TEXT("Kombo.Blitzableiter.Reichweite"), 1.5f));
			for (AFGCreature* F : Feinde)
			{
				const float D = FVector::DistSquared(F->GetActorLocation(), Nach);
				if (!Getroffen.Contains(F) && D < BestLeiter && TDKombo::HatBolzen(this, F))
				{
					BestLeiter = D;
					Naechstes = F;
					bLeiter = true;
				}
			}
		}
		Von = Nach;
		Aktuell = Naechstes;
		if (bLeiter)
		{
			TDKombo::Melde(TEXT("Blitzableiter"), this, Schaden);
			ZeigeLeuchten(Naechstes->GetActorLocation() + FVector(0.f, 0.f, 80.f), FLinearColor(0.5f, 0.8f, 1.f), 0.8f, 0.3f);
			++ZusatzZiele;
		}
		else
		{
			Schaden *= TDBalance::Wert(TEXT("Munition.Tesla.SprungFaktor"), 0.5f);
		}
	}
	mBlitzRest = TeslaBlitzDauer;
	mNachgluehen = 1.f;
}

void ATDTeslaCoil::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	// Blitz flackert: jede Bild andere Drehung um die Laengsachse und Helligkeit
	mBlitzRest -= DeltaSeconds;
	for (int32 i = 0; i < mBlitze.Num(); ++i)
	{
		UStaticMeshComponent* Blitz = mBlitze[i];
		if (!Blitz->IsVisible())
		{
			continue;
		}
		if (mBlitzRest <= 0.f)
		{
			Blitz->SetVisibility(false);
			continue;
		}
		FRotator R = Blitz->GetComponentRotation();
		R.Roll = FMath::FRandRange(0.f, 360.f);
		Blitz->SetWorldRotation(R);
		if (mBlitzMaterial.IsValidIndex(i) && mBlitzMaterial[i])
		{
			mBlitzMaterial[i]->SetScalarParameterValue(TEXT("Staerke"), FMath::FRandRange(15.f, 40.f));
		}
	}
	// Level 5 Spannungsfeld (0.64): Gegner im Umkreis werden gebremst und nehmen leichten Dauerschaden, kleine Blitze zum Boden
	mFeldTakt -= DeltaSeconds;
	if (GetLevel() >= 5 && mFeldTakt <= 0.f && IsTDIntact() && (!GetPowerInfo() || HasPower()))
	{
		mFeldTakt = 0.5f;
		const float Radius = TDBalance::Wert(TEXT("Tuerme.Teslaspule.FeldRadius"), 10.f) * 100.f;
		const float Bremse = TDBalance::Wert(TEXT("Tuerme.Teslaspule.FeldTempo"), 0.6f);
		const float Schaden = TDBalance::Wert(TEXT("Tuerme.Teslaspule.FeldSchaden"), 15.f) * SchadenBonus() * 0.5f;   // je Takt von 0,5 s
		const FVector Mitte = GetActorLocation() + FVector(0.f, 0.f, mDrehpunkt.Z + 30.f);
		TArray<AFGCreature*> Feinde;
		SammleFeinde(Feinde);
		int32 Funken = 0;
		for (AFGCreature* F : Feinde)
		{
			const FVector Ort = F ? F->GetActorLocation() : FVector::ZeroVector;
			if (!F || !F->IsAliveAndWell() || FVector::Dist2D(Ort, Mitte) > Radius)
			{
				continue;
			}
			if (UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(F))
			{
				Hirn->Bremse(F, Bremse, 0.8f);
			}
			Treffer(F, Schaden, (Ort - Mitte).GetSafeNormal(), 0.f);
			if (Funken++ < 4)
			{
				ZeigeSpur(Mitte + (Ort - Mitte).GetSafeNormal2D() * 90.f, Ort, FLinearColor(0.35f, 0.6f, 1.f), 0.15f, 0.5f);
			}
		}
		if (Funken > 0)
		{
			for (int32 k = 0; k < 2; ++k)
			{
				const FVector Richtung = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector();
				ZeigeSpur(Mitte + Richtung * 90.f, GetActorLocation() + Richtung * FMath::FRandRange(300.f, Radius * 0.8f) + FVector(0.f, 0.f, 10.f), FLinearColor(0.4f, 0.65f, 1.f), 0.12f, 0.4f);
			}
		}
	}
	// Leuchtkanten: gluehen mit dem Ladefortschritt auf, nach der Entladung kurz grell
	mNachgluehen = FMath::Max(0.f, mNachgluehen - DeltaSeconds * 3.f);
	if (mGluehMaterial)
	{
		const float Ladung = IsProducing() ? GetProductionProgress() : 0.f;
		mGluehMaterial->SetScalarParameterValue(TEXT("Leuchtkraft"), 2.f + 30.f * Ladung * Ladung + 60.f * mNachgluehen
			+ (Ladung > 0.8f ? FMath::FRandRange(0.f, 15.f) : 0.f));
	}
}

FString ATDTeslaCoil::ZusatzInfo() const
{
	return TDF(TEXT("Ladung %.0f %%"), TEXT("Charge %.0f %%"), (IsProducing() ? GetProductionProgress() : 0.f) * 100.f);
}

float ATDTeslaCoil::GetIdlePowerConsumption() const
{
	return FMath::Max(0.f, TDBalance::Wert(TEXT("Tuerme.Teslaspule.Grundverbrauch"), 15.f));
}

float ATDTeslaCoil::GetProducingPowerConsumptionBase() const
{
	// 0.80: Ladestrom aus der Balance (Tuerme.Teslaspule.Ladeverbrauch), Asset-Wert nur noch Ersatz
	const float Laden = TDBalance::Wert(TEXT("Tuerme.Teslaspule.Ladeverbrauch"), Super::GetProducingPowerConsumptionBase());
	return FMath::Max(0.f, Laden) * (TDForschung::Hat(this, TEXT("Akkubank")) ? TDBalance::Wert(TEXT("Forschung.Akkubank.Faktor"), 0.5f) : 1.f);
}

float ATDTeslaCoil::NennDPS() const
{
	const float Takt = Zykluszeit();
	return Takt > 0.f ? TeslaSchaden() * SchadenBonus() * TeslaUeberlast(this) / Takt : 0.f;
}

void ATDTeslaCoil::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	const float Schaden = TeslaSchaden() * SchadenBonus() * TeslaUeberlast(this);
	const float Takt = Zykluszeit();
	Out.Add({ TDT(TEXT("Schaden je Treffer"), TEXT("Damage per hit")), FString::Printf(TEXT("%.0f"), Schaden), true });
	if (Takt > 0.f)
	{
		Out.Add({ TDT(TEXT("Feuerrate"), TEXT("Fire rate")), TDF(TEXT("%.2f Schuss/s (%.1f s Laden)"), TEXT("%.2f shots/s (%.1f s charging)"), 1.f / Takt, Takt), true });
		Out.Add({ TDT(TEXT("Schaden/s"), TEXT("Damage/s")), TDF(TEXT("%.0f (ein Ziel)"), TEXT("%.0f (single target)"), Schaden / Takt), true });
	}
	Out.Add({ TDT(TEXT("Sprünge"), TEXT("Jumps")), TDF(TEXT("+%d Gegner in %.0f m, je x%.2f"), TEXT("+%d enemies within %.0f m, x%.2f each"), TeslaZieleMit(this, TeslaZiele() + FMath::RoundToInt(LevelWert(TEXT("Spruenge")))) - 1, TeslaSprungweite() / 100.f, TDBalance::Wert(TEXT("Munition.Tesla.SprungFaktor"), 0.5f)), true });
	Out.Add({ TDT(TEXT("Strombedarf"), TEXT("Power use")), TDF(TEXT("%.0f MW Grundlast, %.0f MW beim Aufladen"), TEXT("%.0f MW idle, %.0f MW while charging"), GetIdlePowerConsumption(), GetDefaultProducingPowerConsumption()), true });
}
