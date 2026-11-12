#include "TDSlickPlate.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDGegnerHirn.h"
#include "Creature/FGCreature.h"
#include "FGPipeConnectionFactory.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TDSprache.h"

// Masse aus Modelle\td_oelschlick.py: Platte 8 x 8 m, Deck 0,3 m, Rohr auf +X (Flansch bei 5,15 m), Handrad bei (4,75 / -0,6 / 0,62)
static constexpr float SchlickHalb = 400.f;
static const FVector SchlickRohr(450.f, 0.f, 112.f);   // 0.9.3: Original-Rohrflansch am Pumpenhaus
static const FVector SchlickRad(400.f, -60.f, 235.f);   // 0.9.3: Ventil auf dem Dach des Pumpenhauses

ATDSlickPlate::ATDSlickPlate()
{
	mTurmName = TEXT("Oelschlick");
	mSchadensart = ETDSchadensart::Feuer;
	mDreht = false;
	mRange = 650.f;                     // Tuerme.Oelschlick.Reichweite: Film nur, wenn Gegner nah an der Platte sind
	mDrehpunkt = SchlickRad;            // Handrad (Pivot TD_Valve) dreht sich, solange Oel fliesst
	mMuendung = FVector::ZeroVector;
	mHalbmasse = FVector(SchlickHalb, SchlickHalb, 15.f);
	mLampenOrt = FVector(375.f, -140.f, 340.f);
	// 0.9.3 Original-Anschluesse (Modelle\Original_Anschluesse.json, Gebaeude-Review)
	OriginalTeil(TDOriginal::Rohr, FVector(450.f, 0.f, 112.f), -90.f);
	OriginalTeil(TDOriginal::Strom, FVector(375.f, 115.f, 80.f), 90.f);
	OriginalTeil(TDOriginal::Mast, FVector(375.f, -140.f, 0.f), 0.f);

	mKlarraumHoehe = 60.f;
	mKlarraumHalb = FVector2D(SchlickHalb, SchlickHalb);
	mBaseMaxHealth = 3000.f;
	mYawModell = TEXT("/FicsitTD/Buildings/SlickPlate/Mesh/SM_TD_SlickPlate_Valve.SM_TD_SlickPlate_Valve");
	mStandardRezept = TEXT("/FicsitTD/Buildings/SlickPlate/Recipe_TD_Slick_HeavyOil.Recipe_TD_Slick_HeavyOil_C");

	// Rohr-Eingang aussen am Flansch, Blickrichtung +X (wie beim Flammenwerfer von der Maschine eingesammelt oder von Hand eingehaengt)
	mRohrEingang = CreateDefaultSubobject<UFGPipeConnectionFactory>(TEXT("PipeInput"));
	mRohrEingang->SetupAttachment(RootComponent);
	mRohrEingang->SetRelativeLocation(SchlickRohr);
	mRohrEingang->SetRelativeRotation(FRotator::ZeroRotator);
	mRohrEingang->SetPipeConnectionType(EPipeConnectionType::PCT_CONSUMER);
}

void ATDSlickPlate::NachMaschinenStart()
{
	if (mRohrEingang && !mPipeInputConnections.Contains(mRohrEingang))
	{
		mPipeInputConnections.Add(mRohrEingang);
		mSelbstPumpen = true;
	}
}

void ATDSlickPlate::BeginPlay()
{
	Super::BeginPlay();
	if (mSelbstPumpen && GetCurrentRecipe())
	{
		AssignInputAccessIndices(GetCurrentRecipe());
	}
	// 0.9.3: Flammen-Vorrat fuer den Oelbrand (nur Client-Optik)
	UMaterialInterface* Feuer = LoadObject<UMaterialInterface>(nullptr, TEXT("/FicsitTD/Effects/M_TD_FeuerWolke.M_TD_FeuerWolke"));
	if (Feuer && !IsRunningDedicatedServer())
	{
		const int32 Anzahl = FMath::Clamp(FMath::RoundToInt(TDBalance::Wert(TEXT("Effekte.Oelbrand.Flammen"), 36.f)), 4, 96);
		for (int32 n = 0; n < Anzahl; ++n)
		{
			UMaterialBillboardComponent* B = NewObject<UMaterialBillboardComponent>(this, NAME_None, RF_Transient);
			B->SetupAttachment(RootComponent);
			B->SetUsingAbsoluteLocation(true);
			B->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			B->SetCastShadow(false);
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Feuer, this);
			M->SetScalarParameterValue(TEXT("SpiegelU"), (n & 1) ? 1.f : 0.f);
			M->SetScalarParameterValue(TEXT("SpiegelV"), 0.f);
			M->SetVectorParameterValue(TEXT("Hell"), FLinearColor(1.f, 0.85f, 0.45f));
			M->SetVectorParameterValue(TEXT("Rot"), FLinearColor(1.f, 0.3f, 0.04f));
			B->AddElement(M, nullptr, false, 150.f, 150.f, nullptr);
			B->SetVisibility(false);
			B->RegisterComponent();
			mFlammen.Add(B); mFlammenMat.Add(M);
			mFlammeAlter.Add(1.f); mFlammeLeben.Add(0.8f); mFlammeOrt.Add(FVector::ZeroVector);
		}
		GetWorldTimerManager().SetTimer(mFlammenTimer, this, &ATDSlickPlate::FlammenTick, 1.f / 30.f, true);
	}
	SperreTakt();   // 0.9.6 (Gesamtreview 12.11., A1)
}

bool ATDSlickPlate::BereitZumProduzieren(AFGCreature* Ziel) const
{
	// 0.9.6 (Gesamtreview 12.11., A6): Ausloesung ueber die Platte selbst (wie die Wirkung in AufPlatte), nicht ueber den Umkreis des Ventils
	if (!IsTDIntact())
	{
		return false;
	}
	const float Rand = TDBalance::Wert(TEXT("Tuerme.Oelschlick.Ausloserand"), 2.f) * 100.f;
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	for (const AFGCreature* F : Feinde)
	{
		if (!F || !F->IsAliveAndWell())
		{
			continue;
		}
		const FVector Lokal = GetActorTransform().InverseTransformPosition(F->GetActorLocation());
		if (FMath::Abs(Lokal.X) < SchlickHalb + Rand && FMath::Abs(Lokal.Y) < SchlickHalb + Rand && Lokal.Z > -150.f && Lokal.Z < 350.f)
		{
			return true;
		}
	}
	return false;
}

void ATDSlickPlate::Factory_Tick(float dt)
{
	Super::Factory_Tick(dt);
	if (mSelbstPumpen)
	{
		Factory_PullPipeInput_Implementation(dt);
	}
}

double ATDSlickPlate::Jetzt() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

bool ATDSlickPlate::IstNass() const
{
	return IsTDIntact() && (IsProducing() || Jetzt() < mNassBis);
}

bool ATDSlickPlate::AufPlatte(const FVector& Ort) const
{
	const FVector Lokal = GetActorTransform().InverseTransformPosition(Ort);
	return FMath::Abs(Lokal.X) < SchlickHalb + 30.f && FMath::Abs(Lokal.Y) < SchlickHalb + 30.f && Lokal.Z > -150.f && Lokal.Z < 350.f;
}

void ATDSlickPlate::Zuende(const ATDTurretBase* Zuender)
{
	if (Jetzt() >= mBrandBis)
	{
		++mBraende;
		UE_LOG(LogFicsitTD, Log, TEXT("Oelschlick %s: entzuendet von %s"), *GetName(), Zuender ? *Zuender->GetTurmName() : TEXT("?"));
		// 0.9.1: Oelbrand ist die vierte Kombo (Ansage beim ersten Mal, Zaehler im Turmfenster der Platte und des Zuenders)
		TDKombo::Melde(TEXT("Oelbrand"), this, 0.f);
		if (Zuender && Zuender != this)
		{
			TDKombo::Melde(TEXT("Oelbrand"), Zuender, 0.f);
		}
	}
	mBrandBis = Jetzt() + TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandDauer"), 5.f);
}

void ATDSlickPlate::Entzuende(UWorld* Welt, const FVector& Ort, const ATDTurretBase* Zuender)
{
	if (!Welt)
	{
		return;
	}
	for (TActorIterator<ATDSlickPlate> It(Welt); It; ++It)
	{
		if (It->IstNass() && It->AufPlatte(Ort))
		{
			It->Zuende(Zuender);
		}
	}
}

void ATDSlickPlate::EntzuendeUmkreis(UWorld* Welt, const FVector& Ort, float Radius, const ATDTurretBase* Zuender)
{
	if (!Welt)
	{
		return;
	}
	for (TActorIterator<ATDSlickPlate> It(Welt); It; ++It)
	{
		if (!It->IstNass())
		{
			continue;
		}
		// naechster Punkt der Platte zum Kreismittelpunkt (lokal, 2D)
		const FVector Lokal = It->GetActorTransform().InverseTransformPosition(Ort);
		const float DX = FMath::Max(FMath::Abs(Lokal.X) - SchlickHalb, 0.f), DY = FMath::Max(FMath::Abs(Lokal.Y) - SchlickHalb, 0.f);
		if (FMath::Sqrt(DX * DX + DY * DY) <= Radius && FMath::Abs(Lokal.Z) < 600.f)
		{
			It->Zuende(Zuender);
		}
	}
}

void ATDSlickPlate::Feuern(AFGCreature* Ziel)
{
	// Ein Zyklus Oel verteilt: der Film haelt noch ein paar Sekunden nach
	mNassBis = Jetzt() + TDBalance::Wert(TEXT("Tuerme.Oelschlick.Nachlauf"), 3.f);
	++mSchuesse;
}

void ATDSlickPlate::Animieren(float DeltaSeconds, AFGCreature* Ziel)
{
	const bool bNass = IstNass();
	mDiagTakt -= DeltaSeconds;
	if (mDiagTakt <= 0.f)
	{
		mDiagTakt = 0.5f;
		mDiagZiel += Ziel ? 1 : 0;
		mDiagNass += bNass ? 1 : 0;
		const FVector Lokal = Ziel ? GetActorTransform().InverseTransformPosition(Ziel->GetActorLocation()) : FVector(1e6f);
		mDiagNah += FMath::Abs(Lokal.X) < SchlickHalb + 200.f && FMath::Abs(Lokal.Y) < SchlickHalb + 200.f ? 1 : 0;
	}
	if (mYawMesh && IsProducing())
	{
		mYawMesh->AddLocalRotation(FRotator(0.f, 90.f * DeltaSeconds, 0.f));
	}
	mWirkTakt -= DeltaSeconds;
	if (mWirkTakt > 0.f || (!bNass && mNachbrand.Num() == 0))
	{
		return;
	}
	mWirkTakt = 0.5f;
	const bool bBrennt = bNass && Jetzt() < mBrandBis;
	const float Bremse = TDBalance::Wert(TEXT("Tuerme.Oelschlick.Bremse"), 0.65f);
	const float Brand = TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandSchaden"), 20.f) * 0.5f;   // je Takt von 0,5 s
	const float Nach = TDBalance::Wert(TEXT("Tuerme.Oelschlick.Nachbrennen"), 10.f) * 0.5f;
	const double NachBis = Jetzt() + TDBalance::Wert(TEXT("Tuerme.Oelschlick.NachbrennDauer"), 3.f);
	TSet<AFGCreature*> AufDerPlatte;
	if (bNass)
	{
		TArray<AFGCreature*> Feinde;
		SammleFeinde(Feinde);
		for (AFGCreature* F : Feinde)
		{
			if (!F || !F->IsAliveAndWell() || !AufPlatte(F->GetActorLocation()))
			{
				continue;
			}
			AufDerPlatte.Add(F);
			if (UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(F))
			{
				Hirn->Bremse(F, Bremse, 0.8f);
				++mGebremst;
			}
			if (bBrennt)
			{
				Treffer(F, Brand, FVector::UpVector, 0.f);
				mBrandSchadenSumme += Brand;
				mNachbrand.Add(F, NachBis);   // wer hier brennt, traegt das Feuer aus dem Feld hinaus
			}
		}
	}
	// Nachbrennen ausserhalb der Platte (auf der Platte wirkt schon der Brand)
	for (auto It = mNachbrand.CreateIterator(); It; ++It)
	{
		AFGCreature* F = It.Key().Get();
		if (!F || !F->IsAliveAndWell() || Jetzt() >= It.Value())
		{
			It.RemoveCurrent();
			continue;
		}
		if (!AufDerPlatte.Contains(F))
		{
			Treffer(F, Nach, FVector::UpVector, 0.f);
			mBrandSchadenSumme += Nach;
		}
	}
	if (bBrennt)
	{
		mFunkenTakt -= 0.5f;
		if (mFunkenTakt <= 0.f)
		{
			mFunkenTakt = 1.f;
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector Lokal(FMath::FRandRange(-330.f, 330.f), FMath::FRandRange(-330.f, 330.f), 30.f);
				ZeigeLeuchten(GetActorTransform().TransformPosition(Lokal), FLinearColor(1.f, 0.3f, 0.03f), 4.f, 1.1f, 0.f, true, 20.f);
			}
		}
	}
}

void ATDSlickPlate::FlammenTick()
{
	// Solange die Platte brennt: Flammen entstehen an zufaelligen Stellen der Platte, steigen 1,5 m, wachsen und verglimmen
	const float Dt = 1.f / 30.f;
	const bool bBrennt = IstNass() && Jetzt() < mBrandBis;
	if (bBrennt)
	{
		const float Leben = TDBalance::Wert(TEXT("Effekte.Oelbrand.Lebensdauer"), 0.9f);
		int32 Neu = FMath::Max(1, FMath::RoundToInt(mFlammen.Num() * Dt / Leben));
		for (int32 n = 0; n < mFlammen.Num() && Neu > 0; ++n)
		{
			if (mFlammeAlter[n] < 1.f)
			{
				continue;
			}
			--Neu;
			mFlammeAlter[n] = 0.f;
			mFlammeLeben[n] = Leben * FMath::FRandRange(0.75f, 1.25f);
			mFlammeOrt[n] = GetActorTransform().TransformPosition(FVector(FMath::FRandRange(-360.f, 360.f), FMath::FRandRange(-360.f, 360.f), 20.f));
		}
	}
	for (int32 n = 0; n < mFlammen.Num(); ++n)
	{
		UMaterialBillboardComponent* B = mFlammen[n];
		if (!B)
		{
			continue;
		}
		if (mFlammeAlter[n] >= 1.f)
		{
			if (B->IsVisible()) { B->SetVisibility(false); }
			continue;
		}
		mFlammeAlter[n] = FMath::Min(1.f, mFlammeAlter[n] + Dt / FMath::Max(mFlammeLeben[n], 0.1f));
		const float A = mFlammeAlter[n];
		B->SetWorldLocation(mFlammeOrt[n] + FVector(0.f, 0.f, 50.f + 150.f * A));
		TArray<FMaterialSpriteElement> E = B->Elements;
		if (E.Num() > 0)
		{
			E[0].BaseSizeX = E[0].BaseSizeY = FMath::Lerp(110.f, 300.f, FMath::Pow(A, 0.7f));
			B->SetElements(E);
		}
		if (UMaterialInstanceDynamic* M = mFlammenMat[n])
		{
			M->SetScalarParameterValue(TEXT("Bild"), A);
			M->SetScalarParameterValue(TEXT("Alter"), A * 0.8f);
			M->SetScalarParameterValue(TEXT("Staerke"), TDBalance::Wert(TEXT("Effekte.Oelbrand.Staerke"), 7.f));
		}
		if (!B->IsVisible()) { B->SetVisibility(true); }
	}
}

FString ATDSlickPlate::MunitionsText() const
{
	return FString::Printf(TEXT("%.1f m³ %s"), GetAmmo() / 1000.f, *GetMunitionsName());
}

FString ATDSlickPlate::ZusatzInfo() const
{
	if (Jetzt() < mBrandBis)
	{
		return TDT(TEXT("brennt"), TEXT("burning"));
	}
	return IstNass() ? TDT(TEXT("Ölfilm liegt, Gegner werden gebremst"), TEXT("oil film down, enemies are slowed")) : TDT(TEXT("trocken, wartet auf Gegner"), TEXT("dry, waiting for enemies"));
}

void ATDSlickPlate::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	Out.Add({ TDT(TEXT("Bremse"), TEXT("Slow")), FString::Printf(TEXT("%.0f %%"), (1.f - TDBalance::Wert(TEXT("Tuerme.Oelschlick.Bremse"), 0.65f)) * 100.f), true });
	Out.Add({ TDT(TEXT("Brand"), TEXT("Fire")), TDF(TEXT("%.0f Schaden/s für %.0f s"), TEXT("%.0f damage/s for %.0f s"), TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandSchaden"), 20.f),
		TDBalance::Wert(TEXT("Tuerme.Oelschlick.BrandDauer"), 5.f)), true });
	Out.Add({ TDT(TEXT("Nachbrennen"), TEXT("Afterburn")), TDF(TEXT("%.0f Schaden/s für %.0f s"), TEXT("%.0f damage/s for %.0f s"), TDBalance::Wert(TEXT("Tuerme.Oelschlick.Nachbrennen"), 10.f),
		TDBalance::Wert(TEXT("Tuerme.Oelschlick.NachbrennDauer"), 3.f)), true });
	Out.Add({ TDT(TEXT("Entzündet"), TEXT("Ignited")), TDF(TEXT("%d-mal, %.0f Brandschaden"), TEXT("%d times, %.0f fire damage"), mBraende, mBrandSchadenSumme), true });
}
