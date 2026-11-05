#include "TDGunTurret.h"
#include "TDBalance.h"
#include "Components/StaticMeshComponent.h"
#include "Creature/FGCreature.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

// Masse aus Modelle\TD_GunTurret_Stufen.blend (0.62, flacher Sockel nach Marius' Stufenbildern), Unreal-cm
static const FVector GewehrLaufNabe(150.f, 0.f, 0.f);       // Drehachse des Laufbuendels in der Kanzel
static const FVector GewehrMuendung(345.f, 0.f, 0.f);       // vor den Laufenden
static constexpr float GewehrLaufMax = 1440.f;              // Grad/s bei voller Drehzahl

/** Farbe der Leuchtspur und des Muendungsfeuers je Patrone (0.64: Munition sichtbar) */
static FLinearColor PatronenFarbe(const UClass* Munition)
{
	const FString Name = Munition ? Munition->GetName() : FString();
	if (Name.Contains(TEXT("SmartProjectile"))) return FLinearColor(0.3f, 0.6f, 1.f);    // Zielsuch: blau
	if (Name.Contains(TEXT("Chaos")))           return FLinearColor(1.f, 0.22f, 0.1f);   // Turbo: rot
	if (Name.Contains(TEXT("PatronePanzer")))   return FLinearColor(0.85f, 0.9f, 0.95f); // Panzerbrechend: weiss
	if (Name.Contains(TEXT("PatroneBrand")))    return FLinearColor(1.f, 0.45f, 0.05f);  // Brand: orange
	return FLinearColor(1.f, 0.8f, 0.3f);                                                  // Standard: gelb
}

ATDGunTurret::ATDGunTurret()
{
	mTurmName = TEXT("Gewehrturm");
	mSchadensart = ETDSchadensart::Kugel;
	mDrehpunkt = FVector(0.f, 0.f, 145.f);
	mNeigeachse = FVector(0.f, 0.f, 90.f);
	mLampenOrt = FVector(-90.f, -150.f, 128.f);   // neben dem Faltenbalg hinten links auf dem Deck
	mMuendung = GewehrMuendung;
	mRange = 4200.f;   // Test 0.24.1: Spucker blieben ausserhalb von 35 m, Gewehr kam in Welle 4 nicht zum Schuss
	mTurnSpeed = 220.f;
	mZielToleranz = 6.f;
	mYawModell = TEXT("/FicsitTD/Buildings/GunTurret/Mesh/SM_TD_GunTurret_Yaw.SM_TD_GunTurret_Yaw");
	mHeadModell = TEXT("/FicsitTD/Buildings/GunTurret/Mesh/SM_TD_GunTurret_Head.SM_TD_GunTurret_Head");
	mStandardRezept = TEXT("/FicsitTD/Buildings/GunTurret/Recipe_TD_Gun_Standard.Recipe_TD_Gun_Standard_C");

	mLaeufe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Laufbuendel"));
	mLaeufe->SetupAttachment(mHeadMesh);
	mLaeufe->SetRelativeLocation(GewehrLaufNabe);

	mMuendungsfeuer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Muendungsfeuer"));
	mMuendungsfeuer->SetupAttachment(mHeadMesh);
	mMuendungsfeuer->SetRelativeLocation(GewehrMuendung);
	mMuendungsfeuer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mMuendungsfeuer->SetCastShadow(false);
	mMuendungsfeuer->SetVisibility(false);
}

USceneComponent* ATDGunTurret::StufenZiel(const FString& Teil) const
{
	return Teil == TEXT("Barrels") ? static_cast<USceneComponent*>(mLaeufe) : Super::StufenZiel(Teil);
}

void ATDGunTurret::BeginPlay()
{
	Super::BeginPlay();
	SetzeModell(mLaeufe, TEXT("/FicsitTD/Buildings/GunTurret/Mesh/SM_TD_GunTurret_Barrels.SM_TD_GunTurret_Barrels"));
	SetzeModell(mMuendungsfeuer, TEXT("/FicsitTD/Effects/SM_TD_MuzzleFlash.SM_TD_MuzzleFlash"));
	if (UMaterialInterface* Farbe = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_EffektFarbe.M_TD_EffektFarbe")))
	{
		mMuendungsfeuer->SetMaterial(0, Farbe);
	}
	mMuendungsfeuer->SetVisibility(false);
	mFeuerMaterial = mMuendungsfeuer->CreateDynamicMaterialInstance(0);
}

FTDPatronenWerte ATDGunTurret::WerteFuer(const UClass* MunitionsKlasse)
{
	// Werte aus der Balance-Datei (Munition.Patronen.<Art>.*); Magazingroessen wie im Spiel (mMagazineSize)
	FTDPatronenWerte W;
	const FString Name = MunitionsKlasse ? MunitionsKlasse->GetName() : FString();
	FString Art = TEXT("Standard");
	if (Name.Contains(TEXT("SmartProjectile"))) Art = TEXT("Zielsuch");   // trifft immer
	else if (Name.Contains(TEXT("Chaos")))      Art = TEXT("Turbo");
	else if (Name.Contains(TEXT("PatronePanzer"))) Art = TEXT("Panzerbrechend");
	else if (Name.Contains(TEXT("PatroneBrand")))  Art = TEXT("Brand");
	const FString V = TEXT("Munition.Patronen.") + Art + TEXT(".");
	W.Schaden = TDBalance::Wert(V + TEXT("Schaden"), W.Schaden);
	W.SchussProSekunde = TDBalance::Wert(V + TEXT("SchussProSekunde"), W.SchussProSekunde);
	W.Magazin = FMath::Max(1, TDBalance::Ganz(V + TEXT("Magazin"), W.Magazin));
	if (Art == TEXT("Panzerbrechend"))
	{
		W.Art = ETDSchadensart::Durchschlag;
	}
	else if (Art == TEXT("Brand"))
	{
		W.Brand = TDBalance::Wert(V + TEXT("BrandSchaden"), 6.f);
	}
	return W;
}

bool ATDGunTurret::BereitZumProduzieren(AFGCreature* Ziel) const
{
	// Nachladen erst bei leerem Magazin (ein Zyklus = ein Magazin). Level 5 Gurttrommel (0.64): schon ab halbem Magazin
	// nachladen, das Magazin laeuft nie leer -> Dauerfeuer
	const int32 Schwelle = GetLevel() >= 5 ? FMath::RoundToInt(WerteFuer(AktuelleMunition()).Magazin * (1.f + LevelWert(TEXT("Magazin"))) * 0.5f) : 0;
	return mMagazin <= Schwelle && Super::BereitZumProduzieren(Ziel);
}

void ATDGunTurret::Feuern(AFGCreature* Ziel)
{
	mMagazin += FMath::RoundToInt(WerteFuer(AktuelleMunition()).Magazin * (1.f + LevelWert(TEXT("Magazin"))));   // Level 4: Magazin x2
}

void ATDGunTurret::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	const FTDPatronenWerte W = WerteFuer(AktuelleMunition());
	const bool bFeuerbereit = Ziel && mMagazin > 0;
	// Laufbuendel: zuegig hochdrehen, langsam auslaufen
	const float Rampe = bFeuerbereit ? 2.f * GewehrLaufMax : 0.45f * GewehrLaufMax;
	mLaufTempo = FMath::FInterpConstantTo(mLaufTempo, bFeuerbereit ? GewehrLaufMax : 0.f, DeltaSeconds, Rampe);
	if (mLaeufe && mLaufTempo > 0.f)
	{
		mLaeufe->AddLocalRotation(FRotator(0.f, 0.f, mLaufTempo * DeltaSeconds));
	}

	if (bFeuerbereit && mLaufTempo > 0.6f * GewehrLaufMax && IstAusgerichtet(Ziel))
	{
		mFeuerTakt += DeltaSeconds;
		const float Abstand = 1.f / (W.SchussProSekunde * TaktBonus() * Uebertakt());
		const FLinearColor Farbe = PatronenFarbe(AktuelleMunition());
		const bool bTurbo = Farbe.G < 0.5f;
		const int32 SpurJede = Farbe.B > 0.9f || bTurbo ? 2 : 3;
		while (mFeuerTakt >= Abstand && mMagazin > 0)
		{
			mFeuerTakt -= Abstand;
			--mMagazin;
			++mSchuesse;
			const FVector Muendung = GetMuzzleLocation();
			const FVector Richtung = (Ziel->GetActorLocation() - Muendung).GetSafeNormal();
			TrefferArt(Ziel, W.Schaden * SchadenBonus(), Richtung, 0.f, 0.02f, W.Art);
			if (W.Brand > 0.f)
			{
				TrefferArt(Ziel, W.Brand * SchadenBonus(), Richtung, 0.f, 0.5f, ETDSchadensart::Feuer);
			}
			if (mSchuesse % SpurJede == 0)
			{
				ZeigeSpur(Muendung, Ziel->GetActorLocation() + FVector(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(-30.f, 30.f), 40.f), Farbe, 0.05f, 0.22f);
			}
			mBlitzRest = 0.035f;
			const float G = FMath::FRandRange(0.8f, 1.3f) * (bTurbo ? 1.5f : 1.f);
			mMuendungsfeuer->SetRelativeScale3D(FVector(G * FMath::FRandRange(0.9f, 1.4f), G, G));
			mMuendungsfeuer->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
			if (mFeuerMaterial)
			{
				mFeuerMaterial->SetScalarParameterValue(TEXT("Staerke"), FMath::FRandRange(18.f, 32.f));
				mFeuerMaterial->SetVectorParameterValue(TEXT("Farbe"), Farbe);
			}
		}
	}
	else
	{
		mFeuerTakt = 0.f;
	}
	mBlitzRest -= DeltaSeconds;
	mMuendungsfeuer->SetVisibility(mBlitzRest > 0.f);
}

FString ATDGunTurret::ZusatzInfo() const
{
	return FString::Printf(TEXT("Magazin %d Schuss (%s)"), mMagazin, *GetMunitionsName());
}

float ATDGunTurret::NennDPS() const
{
	const FTDPatronenWerte W = WerteFuer(AktuelleMunition());
	return (W.Schaden + W.Brand) * SchadenBonus() * W.SchussProSekunde * TaktBonus() * Uebertakt();   // Feuerabstand wie in Animieren
}

void ATDGunTurret::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	const FTDPatronenWerte W = WerteFuer(AktuelleMunition());
	const float Schaden = W.Schaden * SchadenBonus();
	const float Rate = W.SchussProSekunde * TaktBonus() * Uebertakt();
	Out.Add({ TEXT("Schaden je Treffer"), W.Brand > 0.f ? FString::Printf(TEXT("%.0f + %.0f Feuer"), Schaden, W.Brand * SchadenBonus())
		: FString::Printf(TEXT("%.0f (%s)"), Schaden, SchadensartName(W.Art)), true });
	Out.Add({ TEXT("Feuerrate"), FString::Printf(TEXT("%.1f Schuss/s"), Rate), true });
	Out.Add({ TEXT("Schaden/s"), FString::Printf(TEXT("%.0f (ein Ziel, Dauerfeuer)"), Schaden * Rate), true });
	Out.Add({ TEXT("Magazin"), FString::Printf(TEXT("%d / %d Schuss"), mMagazin, FMath::RoundToInt(W.Magazin * (1.f + LevelWert(TEXT("Magazin"))))), true });
}
