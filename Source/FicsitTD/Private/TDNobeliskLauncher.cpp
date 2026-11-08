#include "TDNobeliskLauncher.h"
#include "EngineUtils.h"
#include "TDBalance.h"
#include "Components/StaticMeshComponent.h"
#include "Creature/FGCreature.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "FicsitTDModule.h"
#include "FGProjectile.h"
#include "Equipment/FGAmmoTypeProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "Resources/FGItemDescriptor.h"

static constexpr int32 NobeliskPool = 9;   // 0.64: Level 5 wirft drei je Salve
/** Level 5 Salvenkopf: Rohre neben dem Hauptrohr (Kanzel-Raum) und Abstand der Nebenziele vom Hauptziel */
static const FVector NobeliskRohrLinks(0.f, 30.f, 68.f);
static const FVector NobeliskRohrRechts(0.f, -30.f, 68.f);
static constexpr float NobeliskStreuAbstand = 450.f;
static constexpr float NobeliskTempo = 2200.f;   // cm/s waagerecht

ATDNobeliskLauncher::ATDNobeliskLauncher()
{
	// Masse aus Modelle\TD_NobeliskLauncher_Stufen.blend (0.62, nach Marius' Stufenbildern), Unreal-cm
	mTurmName = TEXT("Nobelisk-Werfer");
	mSchadensart = ETDSchadensart::Explosion;
	mDrehpunkt = FVector(0.f, 0.f, 136.f);
	mNeigeachse = FVector(0.f, 0.f, 119.f);
	mMuendung = FVector(158.f, 0.f, 0.f);
	mLampenOrt = FVector(-90.f, -130.f, 123.f);
	mRange = 6000.f;
	mTurnSpeed = 90.f;
	mNeigungMin = 35.f;
	mNeigungMax = 70.f;
	mZielToleranz = 80.f;   // Rohr steht steil: es zaehlt die Seitenrichtung
	mYawModell = TEXT("/FicsitTD/Buildings/NobeliskLauncher/Mesh/SM_TD_NobeliskLauncher_Yaw.SM_TD_NobeliskLauncher_Yaw");
	mHeadModell = TEXT("/FicsitTD/Buildings/NobeliskLauncher/Mesh/SM_TD_NobeliskLauncher_Head.SM_TD_NobeliskLauncher_Head");
	mStandardRezept = TEXT("/FicsitTD/Buildings/NobeliskLauncher/Recipe_TD_Nobelisk_Explosive.Recipe_TD_Nobelisk_Explosive_C");

	for (int32 i = 0; i < NobeliskPool; ++i)
	{
		UStaticMeshComponent* G = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Geschoss%d"), i));
		G->SetupAttachment(RootComponent);
		G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		G->SetVisibility(false);
		mGeschosse.Add(G);
		UStaticMeshComponent* E = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Explosion%d"), i));
		E->SetupAttachment(RootComponent);
		E->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		E->SetCastShadow(false);
		E->SetVisibility(false);
		mExplosionen.Add(E);
	}
}

void ATDNobeliskLauncher::BeginPlay()
{
	Super::BeginPlay();
	for (UStaticMeshComponent* G : mGeschosse)
	{
		SetzeModell(G, TEXT("/FicsitTD/Effects/SM_TD_Effects_NobeliskShell.SM_TD_Effects_NobeliskShell"));
		G->SetVisibility(false);
		G->SetUsingAbsoluteLocation(true);
		G->SetUsingAbsoluteRotation(true);
	}
	for (UStaticMeshComponent* E : mExplosionen)
	{
		SetzeModell(E, TEXT("/FicsitTD/Effects/SM_TD_MuzzleFlash.SM_TD_MuzzleFlash"));
		E->SetVisibility(false);
		E->SetUsingAbsoluteLocation(true);
		E->SetUsingAbsoluteRotation(true);
		E->SetUsingAbsoluteScale(true);
		mExplosionsMaterial.Add(E->CreateDynamicMaterialInstance(0));
		mExplosionRest.Add(0.f);
	}
	mFluege.SetNum(NobeliskPool);
	if (mHeadMesh)
	{
		mHeadMesh->SetRelativeRotation(FRotator(50.f, 0.f, 0.f));   // Ruhelage wie im Modell
	}
}

float ATDNobeliskLauncher::EntzuendeWolke(UWorld* Welt, const FVector& Ort, const ATDTurretBase* Zuender)
{
	if (!Welt || !TDKombo::Aktiv(Welt, TEXT("Verpuffung")))
	{
		return 0.f;
	}
	float Summe = 0.f;
	for (TActorIterator<ATDNobeliskLauncher> It(Welt); It; ++It)
	{
		ATDNobeliskLauncher* L = *It;
		for (int32 i = L->mWolken.Num() - 1; i >= 0; --i)
		{
			const FTDGasWolke G = L->mWolken[i];
			if (FVector::Dist(G.Ort, Ort) > G.Radius)
			{
				continue;
			}
			// das ganze restliche Gas verbrennt auf einmal: Schaden der Restdauer an alle in der Wolke.
			// 0.84.3 (Review 08.11.): als Explosion (Verpuffung = Gasexplosion) und Faktor 1,5 - sonst war es nur ein Zeitraffer der Wolke,
			// netto eher weniger Schaden, weil spaeter eintretende Gegner das Gas verpassen
			const float Schaden = G.Schaden * FMath::Max(G.Rest, 1.f) * TDBalance::Wert(TEXT("Kombo.Verpuffung.Faktor"), 1.5f);
			L->mWolken.RemoveAtSwap(i);
			TArray<AFGCreature*> Feinde;
			L->SammleFeinde(Feinde);
			int32 Getroffen = 0;
			for (AFGCreature* F : Feinde)
			{
				const FVector Zu = F->GetActorLocation() - G.Ort;
				if (Zu.Size() <= G.Radius * 1.2f)
				{
					L->TrefferArt(F, Schaden, Zu.GetSafeNormal(), 0.f, 0.f, ETDSchadensart::Explosion);
					++Getroffen;
				}
			}
			// Stichflamme: grosses oranges Leuchten ueber der Wolke, kurz und hell
			L->ZeigeLeuchten(G.Ort + FVector(0.f, 0.f, 150.f), FLinearColor(1.f, 0.5f, 0.1f), G.Radius / 18.f, 0.6f, 0.f, false, 60.f);
			L->ZeigeLeuchten(G.Ort + FVector(0.f, 0.f, 350.f), FLinearColor(1.f, 0.8f, 0.3f), G.Radius / 30.f, 0.4f, 0.1f, false, 40.f);
			TDKombo::Melde(TEXT("Verpuffung"), Zuender, Schaden * Getroffen);
			Summe += Schaden * Getroffen;
		}
	}
	return Summe;
}

FTDNobeliskWerte ATDNobeliskLauncher::WerteFuer(const UClass* Munition)
{
	// Werte aus der Balance-Datei (Munition.Nobelisk.<Art>.*); die Explosionsgroesse ist nur Optik
	FTDNobeliskWerte W;
	const FString Name = Munition ? Munition->GetName() : FString();
	FString Art = TEXT("Nobelisk");
	if (Name.Contains(TEXT("Gas")))            { Art = TEXT("Gas"); W.ExplosionsGroesse = 1.2f; }
	else if (Name.Contains(TEXT("Shockwave"))) { Art = TEXT("Puls"); W.ExplosionsGroesse = 1.4f; }
	else if (Name.Contains(TEXT("Cluster")))   { Art = TEXT("Streu"); }
	else if (Name.Contains(TEXT("Nuke")))      { Art = TEXT("Nuke"); W.ExplosionsGroesse = 5.f; }
	const FString V = TEXT("Munition.Nobelisk.") + Art + TEXT(".");
	W.Schaden = TDBalance::Wert(V + TEXT("Schaden"), W.Schaden);
	W.Radius = TDBalance::Wert(V + TEXT("Radius"), W.Radius / 100.f) * 100.f;
	if (Art == TEXT("Gas"))
	{
		W.GasSchaden = TDBalance::Wert(V + TEXT("GasSchaden"), 20.f);
		W.GasDauer = TDBalance::Wert(V + TEXT("GasDauer"), 10.f);
	}
	else if (Art == TEXT("Puls"))
	{
		W.Betaeubung = TDBalance::Wert(V + TEXT("Betaeubung"), 2.f);
	}
	else if (Art == TEXT("Streu"))
	{
		W.Streu = TDBalance::Ganz(V + TEXT("Streu"), 5);
	}
	return W;
}

void ATDNobeliskLauncher::Feuern(AFGCreature* Ziel)
{
	if (!Ziel)
	{
		return;
	}
	++mSchuesse;
	mRueckstoss = 1.f;
	const FTransform Kanzel = mHeadMesh ? mHeadMesh->GetComponentTransform() : GetActorTransform();
	StarteFlug(GetMuzzleLocation(), Ziel, FVector::ZeroVector);
	// Level 5 Streuschuss (0.64): zwei weitere Nobelisken aus den Nebenrohren, im Dreieck um das Ziel
	if (GetLevel() >= 5)
	{
		const float Abstand = TDBalance::Wert(TEXT("Tuerme.Nobelisk-Werfer.StreuAbstand"), NobeliskStreuAbstand / 100.f) * 100.f;
		StarteFlug(Kanzel.TransformPosition(mMuendung + NobeliskRohrLinks), Ziel, FRotator(0.f, 60.f, 0.f).Vector() * Abstand);
		StarteFlug(Kanzel.TransformPosition(mMuendung + NobeliskRohrRechts), Ziel, FRotator(0.f, -60.f, 0.f).Vector() * Abstand);
	}
}

void ATDNobeliskLauncher::StarteFlug(const FVector& Start, AFGCreature* Ziel, const FVector& Versatz)
{
	for (int32 i = 0; i < mFluege.Num(); ++i)
	{
		FTDNobeliskFlug& F = mFluege[i];
		if (F.bAktiv || !mGeschosse.IsValidIndex(i))
		{
			continue;
		}
		F.Modell = mGeschosse[i];
		F.Start = Start;
		// Vorhalten: Flugzeit haengt vom Ziel ab, deshalb zweimal schaetzen
		F.Ziel = Ziel->GetActorLocation();
		for (int32 n = 0; n < 2; ++n)
		{
			F.Dauer = FMath::Clamp(FVector::Dist2D(F.Start, F.Ziel) / NobeliskTempo, 0.8f, 3.5f);
			F.Ziel = Ziel->GetActorLocation() + Ziel->GetVelocity() * F.Dauer;
		}
		// Nebenziele (Streuschuss): fester Versatz in Schussrichtung gedreht, ohne Nachfuehren
		const FRotator Blick = FRotator(0.f, (F.Ziel - F.Start).Rotation().Yaw, 0.f);
		F.Ziel += Blick.RotateVector(Versatz);
		const float Weit = FVector::Dist2D(F.Start, F.Ziel);
		F.Gegner = Versatz.IsNearlyZero() ? Ziel : nullptr;
		F.Bogen = FMath::Max(400.f, 0.3f * Weit);
		F.Zeit = 0.f;
		F.bAktiv = true;
		mGeschosse[i]->SetVisibility(true);
		return;
	}
	// alle Geschosse unterwegs: sofort einschlagen lassen
	Einschlag(Ziel->GetActorLocation() + Versatz, WerteFuer(AktuelleMunition()), SchadenBonus());
}

void ATDNobeliskLauncher::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	// Rueckstoss: Rohr ruckt zurueck und federt
	mRueckstoss = FMath::Max(0.f, mRueckstoss - DeltaSeconds * 3.f);
	if (mHeadMesh)
	{
		mHeadMesh->SetRelativeLocation(mNeigeachse + mHeadMesh->GetRelativeRotation().Vector() * (-30.f * mRueckstoss));
	}
	const FTDNobeliskWerte W = WerteFuer(AktuelleMunition());
	// Gaswolken: jede Sekunde alle Gegner darin
	if (mWolken.Num() > 0)
	{
		TArray<AFGCreature*> Feinde;
		for (int32 i = mWolken.Num() - 1; i >= 0; --i)
		{
			FTDGasWolke& G = mWolken[i];
			G.Rest -= DeltaSeconds;
			G.Takt -= DeltaSeconds;
			if (G.Takt <= 0.f)
			{
				G.Takt += 1.f;
				if (Feinde.Num() == 0)
				{
					SammleFeinde(Feinde);
				}
				for (AFGCreature* F : Feinde)
				{
					const FVector Zu = F->GetActorLocation() - G.Ort;
					if (Zu.Size() <= G.Radius)
					{
						TrefferArt(F, G.Schaden, Zu.GetSafeNormal(), 0.f, 0.f, ETDSchadensart::Gift);
					}
				}
			}
			if (G.Rest <= 0.f)
			{
				mWolken.RemoveAtSwap(i);
			}
		}
	}
	for (FTDNobeliskFlug& F : mFluege)
	{
		UStaticMeshComponent* M = F.Modell.Get();
		if (!F.bAktiv || !M)
		{
			continue;
		}
		F.Zeit += DeltaSeconds;
		// Nachfuehren: Einschlagpunkt folgt dem Gegner (begrenzt, sieht noch wie ein Wurf aus)
		if (const AFGCreature* G = F.Gegner.Get())
		{
			if (G->IsAliveAndWell())
			{
				const float Rest = FMath::Max(F.Dauer - F.Zeit, 0.f);
				F.Ziel = FMath::VInterpTo(F.Ziel, G->GetActorLocation() + G->GetVelocity() * Rest, DeltaSeconds, 4.f);
			}
		}
		const float T = FMath::Clamp(F.Zeit / F.Dauer, 0.f, 1.f);
		const FVector Ort = FMath::Lerp(F.Start, F.Ziel, T) + FVector(0.f, 0.f, 4.f * F.Bogen * T * (1.f - T));
		const FVector Weiter = FMath::Lerp(F.Start, F.Ziel, FMath::Min(T + 0.02f, 1.f)) + FVector(0.f, 0.f, 4.f * F.Bogen * (T + 0.02f) * (1.f - T - 0.02f));
		M->SetWorldLocationAndRotation(Ort, (Weiter - Ort).Rotation());
		if (T >= 1.f)
		{
			F.bAktiv = false;
			M->SetVisibility(false);
			Einschlag(F.Ziel, W, SchadenBonus());
		}
	}
	for (int32 i = 0; i < mExplosionen.Num(); ++i)
	{
		if (mExplosionRest[i] <= 0.f)
		{
			continue;
		}
		mExplosionRest[i] -= DeltaSeconds;
		UStaticMeshComponent* E = mExplosionen[i];
		E->SetVisibility(mExplosionRest[i] > 0.f);
		E->SetWorldRotation(FRotator(FMath::FRandRange(-90.f, 90.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
		if (mExplosionsMaterial.IsValidIndex(i) && mExplosionsMaterial[i])
		{
			mExplosionsMaterial[i]->SetScalarParameterValue(TEXT("Staerke"), 60.f * mExplosionRest[i]);
		}
	}
}

bool ATDNobeliskLauncher::SpielExplosion(const FVector& Ort) const
{
	// Optik und Klang immer vom normalen Nobelisk (Gas/Streu/Nuke des Spiels wuerden eigenen Schaden machen)
	static TSubclassOf<AFGProjectile> Klasse = nullptr;
	static bool bGeprueft = false;
	if (!bGeprueft)
	{
		bGeprueft = true;
		UClass* Desc = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/NobeliskExplosive/Desc_NobeliskExplosive.Desc_NobeliskExplosive_C"));
		const UFGAmmoTypeProjectile* Typ = Cast<UFGAmmoTypeProjectile>(Desc ? Desc->GetDefaultObject() : nullptr);
		Klasse = Typ ? Typ->GetProjectileClass() : nullptr;
		UE_LOG(LogFicsitTD, Log, TEXT("Nobelisk: Spiel-Explosion %s"), Klasse ? *Klasse->GetName() : TEXT("NICHT gefunden (eigene Optik)"));
	}
	if (!Klasse)
	{
		return false;
	}
	const FTransform Wo(FRotator::ZeroRotator, Ort + FVector(0.f, 0.f, 30.f));
	AFGProjectile* Bombe = GetWorld()->SpawnActorDeferred<AFGProjectile>(Klasse, Wo, const_cast<ATDNobeliskLauncher*>(this), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Bombe)
	{
		return false;
	}
	// Kein Schaden vom Spiel-Nobelisk: den rechnet der Werfer selbst (Resistenzen, Bilanz)
	Bombe->SetImpactDamageTypes({});
	Bombe->SetEndOfLifeDamageTypes({});
	UGameplayStatics::FinishSpawningActor(Bombe, Wo);
	Bombe->SetActorEnableCollision(false);
	Bombe->SetInitialVelocity(FVector::ZeroVector);
	Bombe->TriggerSecondary();
	Bombe->SetLifeSpan(6.f);
	return true;
}

void ATDNobeliskLauncher::ZeigeExplosion(const FVector& Ort, float Groesse)
{
	if (SpielExplosion(Ort))
	{
		return;
	}
	int32 Frei = 0;
	for (int32 i = 0; i < mExplosionRest.Num(); ++i)
	{
		if (mExplosionRest[i] <= 0.f)
		{
			Frei = i;
			break;
		}
	}
	if (!mExplosionen.IsValidIndex(Frei))
	{
		return;
	}
	UStaticMeshComponent* E = mExplosionen[Frei];
	// Muendungsfeuer-Ebenen (0,6 m) als Explosionsblitz, mittig auf den Einschlag gesetzt
	const float S = 8.f * Groesse;
	E->SetWorldScale3D(FVector(S, S, S));
	E->SetWorldLocation(Ort + FVector(0.f, 0.f, 100.f));
	E->SetVisibility(true);
	mExplosionRest[Frei] = 0.4f;
}

void ATDNobeliskLauncher::Einschlag(const FVector& Ort, const FTDNobeliskWerte& WRoh, float Faktor)
{
	FTDNobeliskWerte W = WRoh;
	W.Radius *= 1.f + LevelWert(TEXT("Radius"));   // Level 4: Sprengradius (auch Gaswolke)
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	TArray<FVector> Orte = { Ort };
	for (int32 i = 0; i < W.Streu; ++i)
	{
		Orte.Add(Ort + FVector(FMath::FRandRange(-500.f, 500.f), FMath::FRandRange(-500.f, 500.f), 0.f));
	}
	for (int32 k = 0; k < Orte.Num(); ++k)
	{
		const FVector& O = Orte[k];
		ZeigeExplosion(O, W.ExplosionsGroesse * (k == 0 ? 1.f : 0.6f));
		for (AFGCreature* F : Feinde)
		{
			const FVector Zu = F->GetActorLocation() - O;
			if (Zu.Size() > W.Radius)
			{
				continue;
			}
			// Streu-Explosionen nach der ersten mit kurzer Verzoegerung
			Treffer(F, W.Schaden * Faktor, Zu.GetSafeNormal(), W.Betaeubung, 0.15f * k);
		}
	}
	// 0.64 Munition sichtbar: Gas gruene Wolke (bleibt liegen), Puls blaue Druckwelle, Nuke grosser weisser Blitz, Streu gelbe Blitze
	const FVector Hoch(0.f, 0.f, 80.f);
	if (W.GasSchaden > 0.f && W.GasDauer > 0.f)
	{
		ZeigeLeuchten(Ort + Hoch, FLinearColor(0.35f, 1.f, 0.2f), W.Radius / 22.f, W.GasDauer, 0.f, true, 5.f);
		ZeigeLeuchten(Ort + Hoch * 2.f, FLinearColor(0.5f, 1.f, 0.3f), W.Radius / 35.f, W.GasDauer * 0.85f, 0.3f, true, 4.f);
	}
	else if (W.Betaeubung > 0.f)
	{
		ZeigeLeuchten(Ort + Hoch, FLinearColor(0.3f, 0.6f, 1.f), W.Radius / 30.f, 0.45f, 0.f, false, 40.f);
		for (int32 k = 0; k < 12; ++k)
		{
			const FVector Richtung = FRotator(0.f, 30.f * k, 0.f).Vector();
			ZeigeSpur(Ort + Hoch + Richtung * 60.f, Ort + Hoch + Richtung * W.Radius, FLinearColor(0.3f, 0.6f, 1.f), 0.3f, 0.6f);
		}
	}
	else if (W.ExplosionsGroesse >= 3.f)
	{
		ZeigeLeuchten(Ort + Hoch * 4.f, FLinearColor(1.f, 0.95f, 0.85f), W.Radius / 18.f, 1.2f, 0.f, false, 60.f);
	}
	for (int32 k = 1; k < Orte.Num(); ++k)
	{
		ZeigeLeuchten(Orte[k] + Hoch, FLinearColor(1.f, 0.85f, 0.3f), 3.f, 0.3f, 0.15f * k, false, 40.f);
	}
	// Gas bleibt liegen (Test 0.24.1: nur Sofortschaden auf die beim Einschlag Getroffenen, 80 Schaden in einer Welle)
	if (W.GasSchaden > 0.f && W.GasDauer > 0.f)
	{
		FTDGasWolke Wolke;
		Wolke.Ort = Ort;
		Wolke.Radius = W.Radius;
		Wolke.Schaden = W.GasSchaden * Faktor;
		Wolke.Rest = W.GasDauer;
		Wolke.Takt = 1.f;
		mWolken.Add(Wolke);
	}
}

float ATDNobeliskLauncher::NennDPS() const
{
	const float Takt = Zykluszeit();
	return Takt > 0.f ? WerteFuer(AktuelleMunition()).Schaden * SchadenBonus() / Takt : 0.f;
}

void ATDNobeliskLauncher::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	const FTDNobeliskWerte W = WerteFuer(AktuelleMunition());
	const float Takt = Zykluszeit();
	Out.Add({ TEXT("Schaden je Treffer"), FString::Printf(TEXT("%.0f (Umkreis %.1f m)"), W.Schaden * SchadenBonus(), W.Radius * (1.f + LevelWert(TEXT("Radius"))) / 100.f), true });
	if (Takt > 0.f)
	{
		Out.Add({ TEXT("Feuerrate"), FString::Printf(TEXT("%.2f Schuss/s"), 1.f / Takt), true });
		Out.Add({ TEXT("Schaden/s"), FString::Printf(TEXT("%.0f (je Gegner im Umkreis)"), W.Schaden * SchadenBonus() / Takt), true });
	}
	if (W.GasSchaden > 0.f)
	{
		Out.Add({ TEXT("Gaswolke"), FString::Printf(TEXT("%.0f Schaden/s für %.0f s"), W.GasSchaden * SchadenBonus(), W.GasDauer), true });
	}
	if (W.Betaeubung > 0.f)
	{
		Out.Add({ TEXT("Betäubung"), FString::Printf(TEXT("%.0f s"), W.Betaeubung), true });
	}
	if (W.Streu > 0)
	{
		Out.Add({ TEXT("Streuung"), FString::Printf(TEXT("%d Zusatz-Explosionen"), W.Streu), true });
	}
	Out.Add({ TEXT("Mindestabstand"), TEXT("10 m"), true });
}
