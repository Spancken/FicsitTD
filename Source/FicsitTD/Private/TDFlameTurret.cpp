#include "TDFlameTurret.h"
#include "TDSlickPlate.h"
#include "TDNobeliskLauncher.h"
#include "TDBalance.h"
#include "FicsitTDModule.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Creature/FGCreature.h"
#include "FGInventoryComponent.h"
#include "FGPipeConnectionFactory.h"
#include "FGRecipe.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TDSprache.h"

// Masse aus Modelle\TD_FlameTurret_Stufen.blend (0.62, nach Marius' Stufenbildern), Unreal-cm
static const FVector FlammeDuese(150.f, 0.f, 0.f);           // Duesenspitze in der Kanzel
static const FVector FlammeRohr(-175.f, 0.f, 120.f);         // 0.9.3: Mitte des Original-Rohreingangs (SM_Pipe_InPut_01) auf der Rueckwand
static constexpr float FlammeKegel = 15.f;                    // Grad zur Seite

ATDFlameTurret::ATDFlameTurret()
{
	mTurmName = TEXT("Flammenwerfer");
	mSchadensart = ETDSchadensart::Feuer;
	mDrehpunkt = FVector(0.f, 0.f, 315.f);   // 0.9.3: Sockel 165 cm hoeher
	mNeigeachse = FVector(0.f, 0.f, 95.f);
	mLampenOrt = FVector(-60.f, 180.f, 340.f);   // 0.9.3: Lampe oben auf dem Original-Statusmast
	mMuendung = FlammeDuese;
	mRange = 2000.f;   // Test G2: 15 m zu kurz (Balance-Datei: Tuerme.Flammenwerfer.Reichweite)
	mTurnSpeed = 120.f;
	mZielToleranz = 10.f;
	mNeigungMin = -30.f;
	mNeigungMax = 30.f;
	mYawModell = TEXT("/FicsitTD/Buildings/FlameTurret/Mesh/SM_TD_FlameTurret_Yaw.SM_TD_FlameTurret_Yaw");
	mHeadModell = TEXT("/FicsitTD/Buildings/FlameTurret/Mesh/SM_TD_FlameTurret_Head.SM_TD_FlameTurret_Head");
	mStandardRezept = TEXT("/FicsitTD/Buildings/FlameTurret/Recipe_TD_Flame_Fuel.Recipe_TD_Flame_Fuel_C");

	// Rohr-Eingang hinten mittig, Blickrichtung nach aussen (-X)
	mRohrEingang = CreateDefaultSubobject<UFGPipeConnectionFactory>(TEXT("PipeInput"));
	mRohrEingang->SetupAttachment(RootComponent);
	mRohrEingang->SetRelativeLocation(FlammeRohr);
	mRohrEingang->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	mRohrEingang->SetPipeConnectionType(EPipeConnectionType::PCT_CONSUMER);

	mFlamme = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flammenstrahl"));
	mFlamme->SetupAttachment(mHeadMesh);
	mFlamme->SetRelativeLocation(FlammeDuese);
	mFlamme->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mFlamme->SetCastShadow(false);
	mFlamme->SetVisibility(false);
	// 0.9.3 Original-Anschluesse (Lagen aus Modelle\Original_Anschluesse.json)
	OriginalTeil(TDOriginal::Rohr, FVector(-175.f, 0.f, 120.f), 90.f);
	OriginalTeil(TDOriginal::Strom, FVector(-45.f, -175.f, 82.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-60.f, 180.f, 0.f), 180.f);

}

void ATDFlameTurret::NachMaschinenStart()
{
	// Falls die Maschine den Anschluss nicht selbst eingesammelt hat: von Hand eintragen und selbst abpumpen
	if (mRohrEingang && !mPipeInputConnections.Contains(mRohrEingang))
	{
		mPipeInputConnections.Add(mRohrEingang);
		mSelbstPumpen = true;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Flammenwerfer %s: Rohr-Eingang %s (selbst pumpen: %d)"), *GetName(),
		mRohrEingang ? *mRohrEingang->GetName() : TEXT("fehlt"), mSelbstPumpen ? 1 : 0);
}

void ATDFlameTurret::BeginPlay()
{
	Super::BeginPlay();
	if (mSelbstPumpen && GetCurrentRecipe())
	{
		AssignInputAccessIndices(GetCurrentRecipe());
	}
	SetzeModell(mFlamme, TEXT("/FicsitTD/Effects/SM_TD_FlameJet.SM_TD_FlameJet"));
	if (UMaterialInterface* Farbe = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_FlameJetFarbe.M_TD_FlameJetFarbe")))
	{
		mFlamme->SetMaterial(0, Farbe);
	}
	mFlamme->SetVisibility(false);
	mFlammenMaterial = mFlamme->CreateDynamicMaterialInstance(0);
	ErzeugeWolken();
}

void ATDFlameTurret::ErzeugeWolken()
{
	// 0.9.3: Vorrat an Sprites, nur auf dem Client sichtbar (keine Kollision, kein Schatten). Fehlt das Material, bleibt der alte Kegel.
	UMaterialInterface* Basis = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_FeuerWolke.M_TD_FeuerWolke"));
	if (!Basis || IsRunningDedicatedServer())
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Flammenwerfer %s: Feuerwolken %s"), *GetName(), Basis ? TEXT("aus (Server)") : TEXT("FEHLEN, Kegel bleibt"));
		return;
	}
	const int32 Anzahl = FMath::Clamp(FMath::RoundToInt(TDBalance::Wert(TEXT("Effekte.Flamme.Wolken"), 28.f)), 4, 64);
	for (int32 i = 0; i < Anzahl; ++i)
	{
		UMaterialBillboardComponent* S = NewObject<UMaterialBillboardComponent>(this, NAME_None, RF_Transient);
		S->SetupAttachment(RootComponent);
		S->SetUsingAbsoluteLocation(true);
		S->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		S->SetCastShadow(false);
		S->bReceivesDecals = false;
		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Basis, this);
		M->SetScalarParameterValue(TEXT("SpiegelU"), (i & 1) ? 1.f : 0.f);
		M->SetScalarParameterValue(TEXT("SpiegelV"), (i & 2) ? 1.f : 0.f);
		S->AddElement(M, nullptr, false, 100.f, 100.f, nullptr);
		S->SetVisibility(false);
		S->RegisterComponent();
		mWolkenSprites.Add(S);
		mWolkenMaterial.Add(M);
		mWolken.AddDefaulted();
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Flammenwerfer %s: %d Feuerwolken"), *GetName(), Anzahl);
}

void ATDFlameTurret::BewegeWolken(float DeltaSeconds, bool bNeue, const FTDTreibstoffWerte& W, float Kegel)
{
	// Neue Wolken an der Duese fliegen in Rohrrichtung (Streuung im Kegel) bis ans Strahlende, wachsen, spielen das Flipbook
	// einmal ab und kuehlen ab (Material: Alter). Laufende Wolken leben nach dem Abschalten zu Ende.
	const float Leben = TDBalance::Wert(TEXT("Effekte.Flamme.Lebensdauer"), 0.6f);
	const float Takt = Leben / FMath::Max(mWolkenSprites.Num() * 0.85f, 1.f);
	if (bNeue)
	{
		mWolkenTakt += DeltaSeconds;
		const FVector Duese = GetMuzzleLocation();
		const FVector Vorne = mHeadMesh ? mHeadMesh->GetForwardVector() : GetActorForwardVector();
		const float Halb = FMath::DegreesToRadians(FlammeKegel * Kegel * 0.6f);
		while (mWolkenTakt >= Takt)
		{
			mWolkenTakt -= Takt;
			int32 Frei = INDEX_NONE;
			float Aeltestes = -1.f;
			for (int32 i = 0; i < mWolken.Num(); ++i)
			{
				if (mWolken[i].Alter >= 1.f) { Frei = i; break; }
				if (mWolken[i].Alter > Aeltestes) { Aeltestes = mWolken[i].Alter; Frei = i; }
			}
			if (Frei == INDEX_NONE)
			{
				break;
			}
			FTDFeuerWolke& N = mWolken[Frei];
			N.Alter = FMath::FRandRange(0.f, 0.06f);
			N.Lebensdauer = Leben * FMath::FRandRange(0.85f, 1.15f);
			N.Start = Duese;
			N.Weg = FMath::VRandCone(Vorne, Halb) * mFlammenLaenge * FMath::FRandRange(0.85f, 1.05f);
			N.GroesseStart = FMath::FRandRange(35.f, 60.f);
			N.GroesseEnde = FMath::Clamp(mFlammenLaenge * 0.42f * Kegel, 150.f, 700.f) * FMath::FRandRange(0.85f, 1.15f);
			N.Steigen = FMath::FRandRange(30.f, 90.f);
		}
	}
	else
	{
		mWolkenTakt = 0.f;
	}
	const float Staerke = TDBalance::Wert(TEXT("Effekte.Flamme.Staerke"), 8.f);
	for (int32 i = 0; i < mWolken.Num(); ++i)
	{
		FTDFeuerWolke& N = mWolken[i];
		UMaterialBillboardComponent* S = mWolkenSprites[i];
		if (!S)
		{
			continue;
		}
		if (N.Alter >= 1.f)
		{
			if (S->IsVisible()) { S->SetVisibility(false); }
			continue;
		}
		N.Alter = FMath::Min(1.f, N.Alter + DeltaSeconds / FMath::Max(N.Lebensdauer, 0.05f));
		const float A = N.Alter;
		const float Flug = 1.f - FMath::Square(1.f - A);   // schnell aus der Duese, am Ende langsamer
		S->SetWorldLocation(N.Start + N.Weg * Flug + FVector(0.f, 0.f, N.Steigen * A * A));
		const float G = FMath::Lerp(N.GroesseStart, N.GroesseEnde, FMath::Pow(A, 0.8f));
		TArray<FMaterialSpriteElement> E = S->Elements;
		if (E.Num() > 0)
		{
			E[0].BaseSizeX = E[0].BaseSizeY = G;
			S->SetElements(E);
		}
		if (UMaterialInstanceDynamic* M = mWolkenMaterial[i])
		{
			M->SetScalarParameterValue(TEXT("Bild"), A);
			M->SetScalarParameterValue(TEXT("Alter"), A);
			M->SetScalarParameterValue(TEXT("Staerke"), Staerke);
			M->SetVectorParameterValue(TEXT("Hell"), W.Hell);
			M->SetVectorParameterValue(TEXT("Rot"), W.Rot);
		}
		if (!S->IsVisible()) { S->SetVisibility(true); }
	}
}

void ATDFlameTurret::Factory_Tick(float dt)
{
	Super::Factory_Tick(dt);
	if (mSelbstPumpen)
	{
		Factory_PullPipeInput_Implementation(dt);
	}
}

int32 ATDFlameTurret::GetAmmo() const
{
	return Super::GetAmmo();   // Fluessigkeit in Litern (1000 = 1 m³)
}

FTDTreibstoffWerte ATDFlameTurret::WerteFuer(const UClass* Treibstoff)
{
	// Werte aus der Balance-Datei (Munition.Treibstoff.<Art>.*)
	FTDTreibstoffWerte W;
	const FString Name = Treibstoff ? Treibstoff->GetName() : FString();
	FString Art = TEXT("Treibstoff");
	if (Name.Contains(TEXT("Biofuel")))          Art = TEXT("Biotreibstoff");
	else if (Name.Contains(TEXT("TurboFuel")))   Art = TEXT("Turbotreibstoff");
	else if (Name.Contains(TEXT("RocketFuel")))  Art = TEXT("Raketentreibstoff");
	else if (Name.Contains(TEXT("IonizedFuel"))) Art = TEXT("Ionisiert");
	const FString V = TEXT("Munition.Treibstoff.") + Art + TEXT(".");
	W.SchadenProSekunde = TDBalance::Wert(V + TEXT("SchadenProSekunde"), W.SchadenProSekunde);
	W.BrennSchaden = TDBalance::Wert(V + TEXT("Nachbrennen"), W.BrennSchaden);
	W.BrennDauer = TDBalance::Wert(V + TEXT("NachbrennDauer"), W.BrennDauer);
	// 0.64 (Marius: normaler Treibstoff rote kleine Flamme, Biotreibstoff gruen und breiter): Aussehen und Kegel je Treibstoff
	if (Art == TEXT("Biotreibstoff"))
	{
		W.Hell = FLinearColor(0.75f, 1.f, 0.35f); W.Rot = FLinearColor(0.08f, 0.7f, 0.04f); W.Kegel = 1.4f; W.Flackern = 3.f;
	}
	else if (Art == TEXT("Turbotreibstoff"))
	{
		W.Hell = FLinearColor(1.f, 1.f, 0.85f); W.Rot = FLinearColor(1.f, 0.55f, 0.08f); W.Kegel = 0.9f; W.Flackern = 8.f;
	}
	else if (Art == TEXT("Raketentreibstoff"))
	{
		W.Hell = FLinearColor(0.9f, 0.95f, 1.f); W.Rot = FLinearColor(0.12f, 0.3f, 1.f); W.Kegel = 0.75f; W.Flackern = 7.f;
	}
	else if (Art == TEXT("Ionisiert"))
	{
		W.Hell = FLinearColor(1.f, 0.85f, 1.f); W.Rot = FLinearColor(0.6f, 0.1f, 1.f); W.Betaeubung = 0.3f; W.Flackern = 6.f;
	}
	if (Art == TEXT("Ionisiert") && TDBalance::Wert(TEXT("Munition.Treibstoff.Ionisiert.AlsStrom"), 1.f) > 0.f)
	{
		W.Art = ETDSchadensart::Strom;
	}
	W.Kegel = TDBalance::Wert(V + TEXT("Kegel"), W.Kegel);
	W.Betaeubung = TDBalance::Wert(V + TEXT("Betaeubung"), W.Betaeubung);
	return W;
}

void ATDFlameTurret::Feuern(AFGCreature* Ziel)
{
	// 1 s Flamme: alle Kreaturen im Kegel
	const FTDTreibstoffWerte W = WerteFuer(AktuelleMunition());
	const FVector Duese = GetMuzzleLocation();
	// Kegel zeigt auf das Ziel, auch wenn die Kanzel am Neigungsanschlag steht (0.64.1)
	const FVector Richtung = Ziel ? (Ziel->GetActorLocation() - Duese).GetSafeNormal() : mHeadMesh ? mHeadMesh->GetForwardVector() : GetActorForwardVector();
	const float MinCos = FMath::Cos(FMath::DegreesToRadians(FlammeKegel * (1.f + LevelWert(TEXT("Kegel"))) * W.Kegel));   // Level 3: Kegel breiter
	const float Nach = 1.f + LevelWert(TEXT("Nachbrennen"));   // Level 4
	const float Faktor = SchadenBonus();
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	int32 Getroffen = 0;
	for (AFGCreature* F : Feinde)
	{
		const FVector Zu = F->GetActorLocation() - Duese;
		if (Zu.Size() > GetReichweite() + 200.f || FVector::DotProduct(Zu.GetSafeNormal(), Richtung) < MinCos)
		{
			continue;
		}
		++Getroffen;
		ATDSlickPlate::Entzuende(GetWorld(), F->GetActorLocation(), this);   // 0.81: nasses Oelschlick-Feld unter dem Ziel brennt (0.9.1 Kombo Oelbrand)
		ATDNobeliskLauncher::EntzuendeWolke(GetWorld(), F->GetActorLocation(), this);   // 0.84 Verpuffung: Gaswolke um das Ziel brennt
		for (int32 t = 0; t < 4; ++t)   // Schaden ueber die Sekunde verteilt
		{
			TrefferArt(F, W.SchadenProSekunde * Faktor / 4.f, Zu.GetSafeNormal(), t == 0 ? W.Betaeubung : 0.f, 0.25f * t, W.Art);
		}
		for (int32 s = 1; s <= FMath::RoundToInt(W.BrennDauer * Nach); ++s)
		{
			Treffer(F, W.BrennSchaden * Nach * Faktor, Zu.GetSafeNormal(), 0.f, 1.f + s);
		}
	}
	mSchuesse += Getroffen > 0 ? 1 : 0;
	mFlammeRest = 1.15f;
	// Level 5 Feuerteppich (0.64): wo die Flamme auftrifft, brennt der Boden weiter
	if (GetLevel() >= 5 && Ziel)
	{
		FTDBrand Brand;
		Brand.Ort = Ziel->GetActorLocation() - FVector(0.f, 0.f, 60.f);
		Brand.Rest = TDBalance::Wert(TEXT("Tuerme.Flammenwerfer.TeppichDauer"), 6.f);
		Brand.Schaden = W.SchadenProSekunde * Faktor * TDBalance::Wert(TEXT("Tuerme.Flammenwerfer.TeppichAnteil"), 0.4f);
		Brand.Hell = W.Hell;
		if (mBraende.Num() >= 6)
		{
			mBraende.RemoveAt(0);
		}
		mBraende.Add(Brand);
		ZeigeLeuchten(Brand.Ort, W.Rot, 7.f, Brand.Rest, 0.f, true, 10.f);
	}
}

void ATDFlameTurret::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	// Feuerteppich: jede Sekunde Schaden an allen darin, Funken in der Farbe des Treibstoffs
	if (mBraende.Num() > 0)
	{
		const float Radius = TDBalance::Wert(TEXT("Tuerme.Flammenwerfer.TeppichRadius"), 3.f) * 100.f;
		TArray<AFGCreature*> Feinde;
		for (int32 i = mBraende.Num() - 1; i >= 0; --i)
		{
			FTDBrand& B = mBraende[i];
			B.Rest -= DeltaSeconds;
			B.Takt -= DeltaSeconds;
			B.Funken -= DeltaSeconds;
			if (B.Takt <= 0.f)
			{
				B.Takt += 1.f;
				if (Feinde.Num() == 0)
				{
					SammleFeinde(Feinde);
				}
				ATDSlickPlate::EntzuendeUmkreis(GetWorld(), B.Ort, Radius, this);   // 0.9.1 Oelbrand: der Feuerteppich zuendet nasse Platten
				for (AFGCreature* F : Feinde)
				{
					if (F && F->IsAliveAndWell() && FVector::Dist2D(F->GetActorLocation(), B.Ort) <= Radius)
					{
						Treffer(F, B.Schaden, FVector::UpVector, 0.f);
					}
				}
			}
			if (B.Funken <= 0.f)
			{
				B.Funken = 0.2f;
				ZeigeLeuchten(B.Ort + FVector(FMath::FRandRange(-0.8f, 0.8f) * Radius, FMath::FRandRange(-0.8f, 0.8f) * Radius, 40.f), B.Hell, 2.5f, 0.35f, 0.f, false, 20.f);
			}
			if (B.Rest <= 0.f)
			{
				mBraende.RemoveAt(i);
			}
		}
	}
	mFlammeRest -= DeltaSeconds;
	const bool bBrennt = mFlammeRest > 0.f || (Ziel && IsProducing());
	const float SollLaenge = bBrennt ? (Ziel ? FMath::Min(GetReichweite(), FVector::Dist(GetMuzzleLocation(), Ziel->GetActorLocation()) + 250.f) : GetReichweite()) : 0.f;
	mFlammenLaenge = FMath::FInterpConstantTo(mFlammenLaenge, SollLaenge, DeltaSeconds, bBrennt ? 5000.f : 8000.f);
	const bool bSichtbar = mFlammenLaenge > 20.f;
	const FTDTreibstoffWerte W = WerteFuer(AktuelleMunition());
	const float Kegel = (1.f + LevelWert(TEXT("Kegel"))) * W.Kegel;
	if (mWolkenSprites.Num() > 0)
	{
		// 0.9.3: Feuerwolken statt Kegel
		mFlamme->SetVisibility(false);
		BewegeWolken(DeltaSeconds, bSichtbar, W, Kegel);
		return;
	}
	mFlamme->SetVisibility(bSichtbar);
	if (!bSichtbar)
	{
		return;
	}
	// Kegel ist 1 m lang und am Ende 1 m breit; flackert in Laenge, Breite, Drehung und Helligkeit
	const float Breite = (0.3f + 0.5f * mFlammenLaenge / FMath::Max(GetReichweite(), 1.f)) * FMath::FRandRange(0.93f, 1.08f);
	mFlamme->SetRelativeScale3D(FVector(mFlammenLaenge / 100.f * FMath::FRandRange(0.92f, 1.05f), 2.5f * Breite * Kegel, 2.5f * Breite * Kegel));
	mFlamme->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
	if (mFlammenMaterial)
	{
		mFlammenMaterial->SetScalarParameterValue(TEXT("Staerke"), FMath::FRandRange(2.5f, 4.f));
		mFlammenMaterial->SetVectorParameterValue(TEXT("Hell"), W.Hell);
		mFlammenMaterial->SetVectorParameterValue(TEXT("Rot"), W.Rot);
		mFlammenMaterial->SetScalarParameterValue(TEXT("Tempo"), W.Flackern);
	}
}

FString ATDFlameTurret::ZusatzInfo() const
{
	return TDF(TEXT("Treibstoff %.1f m³ (%s)"), TEXT("Fuel %.1f m³ (%s)"), GetAmmo() / 1000.f, *GetMunitionsName());
}

float ATDFlameTurret::NennDPS() const
{
	return WerteFuer(AktuelleMunition()).SchadenProSekunde * SchadenBonus();
}

void ATDFlameTurret::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	const FTDTreibstoffWerte W = WerteFuer(AktuelleMunition());
	Out.Add({ TDT(TEXT("Feuerrate"), TEXT("Fire rate")), TDT(TEXT("Dauerstrahl"), TEXT("Continuous stream")), true });
	Out.Add({ TDT(TEXT("Schaden/s"), TEXT("Damage/s")), TDF(TEXT("%.0f (je Gegner im Kegel)"), TEXT("%.0f (per enemy in the cone)"), W.SchadenProSekunde * SchadenBonus()), true });
	Out.Add({ TDT(TEXT("Nachbrennen"), TEXT("Afterburn")), TDF(TEXT("%.0f Schaden/s für %.0f s"), TEXT("%.0f damage/s for %.0f s"), W.BrennSchaden * SchadenBonus() * (1.f + LevelWert(TEXT("Nachbrennen"))), W.BrennDauer * (1.f + LevelWert(TEXT("Nachbrennen")))), true });
	Out.Add({ TDT(TEXT("Kegel"), TEXT("Cone")), TDF(TEXT("%.0f°, alle darin"), TEXT("%.0f°, hits all inside"), FlammeKegel * (1.f + LevelWert(TEXT("Kegel"))) * W.Kegel), true });
}
