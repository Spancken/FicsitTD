#include "TDWellenStatistik.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "EngineUtils.h"
#include "FGPowerCircuit.h"
#include "FGPowerInfoComponent.h"
#include "FGRecipe.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableManufacturer.h"

UTDWellenStatistik* UTDWellenStatistik::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDWellenStatistik>() : nullptr;
}

FLinearColor UTDWellenStatistik::Farbe(const FString& Art)
{
	if (Art.Contains(TEXT("Bolzen"))) return FLinearColor(FColor(250, 149, 73));
	if (Art.Contains(TEXT("Gewehr"))) return FLinearColor(FColor(235, 205, 80));
	if (Art.Contains(TEXT("Tesla"))) return FLinearColor(FColor(110, 170, 250));
	if (Art.Contains(TEXT("Flamm"))) return FLinearColor(FColor(235, 90, 70));
	if (Art.Contains(TEXT("Nobelisk"))) return FLinearColor(FColor(150, 215, 125));
	return FLinearColor(FColor(170, 170, 170));
}

static float KernProzent(const UObject* Ctx)
{
	const UTDEinsatzSubsystem* Einsatz = Ctx->GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	const ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	return Kern && Kern->GetTDMaxHealth() > 0.f ? 100.f * Kern->GetTDHealth() / Kern->GetTDMaxHealth() : 0.f;
}

void UTDWellenStatistik::Sammle(TMap<FString, FTDTurmStat>& Out) const
{
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		ATDTurretBase* T = *It;
		const FString Art = T->GetTurmName().IsEmpty() ? UTDEinsatzSubsystem::Schluessel(T->GetClass()) : T->GetTurmName();
		FTDTurmStat& S = Out.FindOrAdd(Art);
		S.Art = Art;
		++S.Anzahl;
		const ATDTurretBase::FBilanz& B = T->GetBilanz();
		S.Schaden += FMath::Max(0.f, B.Schaden - B.Overkill);
		S.Schuesse += B.Schuesse + T->GetShotsFired();
		const int32* Vorher = mKillsStart.Find(T);
		S.Kills += FMath::Max(0, T->GetKills() - (Vorher ? *Vorher : T->GetKills()));
		S.VorratEnde += T->GetAmmo();
		S.Hungrig += B.ZeitHungrig;
		S.OhneStrom += B.ZeitOhneStrom;
	}
}

void UTDWellenStatistik::Produktion(TMap<FString, FTDTurmStat>& Out) const
{
	// Munition je Turmart = erste Zutat des eingestellten Rezepts der Tuerme
	TMap<UClass*, FString> MunitionZuArt;
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		if (const TSubclassOf<UFGRecipe> R = It->GetCurrentRecipe())
		{
			const TArray<FItemAmount> Zutaten = UFGRecipe::GetIngredients(*It, R);
			if (Zutaten.Num() > 0 && Zutaten[0].ItemClass)
			{
				const FString Art = It->GetTurmName().IsEmpty() ? UTDEinsatzSubsystem::Schluessel(It->GetClass()) : It->GetTurmName();
				MunitionZuArt.Add(Zutaten[0].ItemClass.Get(), Art);
			}
		}
	}
	if (MunitionZuArt.Num() == 0)
	{
		return;
	}
	for (TActorIterator<AFGBuildableManufacturer> It(GetWorld()); It; ++It)
	{
		if (It->IsA<ATDTurretBase>())
		{
			continue;
		}
		const TSubclassOf<UFGRecipe> R = It->GetCurrentRecipe();
		if (!R)
		{
			continue;
		}
		const float Dauer = UFGRecipe::GetManufacturingDuration(R);
		if (Dauer <= 0.f)
		{
			continue;
		}
		for (const FItemAmount& P : UFGRecipe::GetProducts(R))
		{
			if (const FString* Art = MunitionZuArt.Find(P.ItemClass.Get()))
			{
				FTDTurmStat& S = Out.FindOrAdd(*Art);
				S.Art = *Art;
				S.ProduktionMin += P.Amount * 60.f / Dauer * FMath::Max(0.01f, It->GetCurrentPotential());
			}
		}
	}
}

void UTDWellenStatistik::MesseStrom(float& Kapazitaet, float& Verbrauch, bool& bUeberlast) const
{
	TSet<UFGPowerCircuit*> Netze;
	for (TActorIterator<AFGBuildableGenerator> It(GetWorld()); It; ++It)
	{
		if (UFGPowerInfoComponent* Info = It->GetPowerInfo())
		{
			if (UFGPowerCircuit* Netz = Info->GetPowerCircuit())
			{
				Netze.Add(Netz);
			}
		}
	}
	Kapazitaet = Verbrauch = 0.f;
	bUeberlast = false;
	for (UFGPowerCircuit* Netz : Netze)
	{
		FPowerCircuitStats Stat;
		Netz->GetStats(Stat);
		Kapazitaet += Stat.PowerProductionCapacity;
		Verbrauch += Stat.PowerConsumed;
		bUeberlast |= Netz->IsFuseTriggered() || Stat.PowerConsumed > Stat.PowerProductionCapacity + 0.1f;
	}
}

void UTDWellenStatistik::WellenStart(int32 Welle)
{
	mLaeuft = true;
	mWelle = Welle;
	mStart = FPlatformTime::Seconds();
	mKernStart = KernProzent(this);
	mKillsStart.Reset();
	mVorratStart.Reset();
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		mKillsStart.Add(*It, It->GetKills());
		const FString Art = It->GetTurmName().IsEmpty() ? UTDEinsatzSubsystem::Schluessel(It->GetClass()) : It->GetTurmName();
		mVorratStart.FindOrAdd(Art) += It->GetAmmo();
	}
	mTakt = 0.f;
	mProben = 0;
	mStromSumme = mStromSpitze = mStromKap = mUeberlast = 0.f;
}

void UTDWellenStatistik::Tick(float DeltaTime)
{
	mTakt -= DeltaTime;
	if (mTakt > 0.f)
	{
		return;
	}
	mTakt = 0.5f;
	float Kap = 0.f, Verbrauch = 0.f;
	bool bUeber = false;
	MesseStrom(Kap, Verbrauch, bUeber);
	++mProben;
	mStromSumme += Verbrauch;
	mStromSpitze = FMath::Max(mStromSpitze, Verbrauch);
	mStromKap = FMath::Max(mStromKap, Kap);
	mUeberlast += bUeber ? 0.5f : 0.f;
}

TArray<FTDTurmStat> UTDWellenStatistik::Live() const
{
	TMap<FString, FTDTurmStat> Je;
	Sammle(Je);
	TArray<FTDTurmStat> Liste;
	Je.GenerateValueArray(Liste);
	Liste.Sort([](const FTDTurmStat& A, const FTDTurmStat& B) { return A.Schaden > B.Schaden; });
	return Liste;
}

const TArray<FTDWellenStat>& UTDWellenStatistik::Verlauf() const
{
	static const TArray<FTDWellenStat> Leer;
	const UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	const ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	return Kern ? Kern->mStatistik : Leer;
}

void UTDWellenStatistik::WellenEnde(int32 Welle, int32 EP, int32 FP, float KernAnteil)
{
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	ATDDefenseCore* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	FTDWellenStat W;
	W.Welle = Welle;
	W.Dauer = mLaeuft && mWelle == Welle ? float(FPlatformTime::Seconds() - mStart) : 0.f;
	if (const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
	{
		W.Gegner = Waves->GetWellenStatus().Groesse;
	}
	W.KernStart = mWelle == Welle ? mKernStart : KernAnteil;
	W.KernEnde = KernAnteil;
	W.EP = EP;
	W.FP = FP;
	W.StromKapazitaet = mStromKap;
	W.StromMittel = mProben > 0 ? mStromSumme / mProben : 0.f;
	W.StromSpitze = mStromSpitze;
	W.StromUeberlast = mUeberlast;
	TMap<FString, FTDTurmStat> Je;
	Sammle(Je);
	Produktion(Je);
	for (TPair<FString, FTDTurmStat>& P : Je)
	{
		P.Value.VorratStart = mVorratStart.FindRef(P.Key);
		W.Tuerme.Add(P.Value);
	}
	W.Tuerme.Sort([](const FTDTurmStat& A, const FTDTurmStat& B) { return A.Schaden > B.Schaden; });
	if (W.Gegner <= 0)
	{
		for (const FTDTurmStat& T : W.Tuerme)
		{
			W.Gegner += T.Kills;
		}
	}
	mLaeuft = false;
	if (!Kern)
	{
		return;
	}
	// Nach dem Laden eines aelteren Spielstands: spaetere Wellen dieses Laufs sind ueberholt
	Kern->mStatistik.RemoveAll([Welle](const FTDWellenStat& A) { return A.Welle >= Welle; });
	Kern->mStatistik.Add(W);
	float Summe = 0.f;
	for (const FTDTurmStat& T : W.Tuerme)
	{
		Summe += T.Schaden;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Statistik Welle %d: %.0f s, Kern %.0f -> %.0f %%, Schaden %.0f in %d Turmarten, Strom %.0f/%.0f MW (Spitze %.0f, Ueberlast %.0f s)"),
		Welle, W.Dauer, W.KernStart, W.KernEnde, Summe, W.Tuerme.Num(), W.StromMittel, W.StromKapazitaet, W.StromSpitze, W.StromUeberlast);
}

TMap<FString, FTDTurmStat> UTDWellenStatistik::Momentaufnahme() const
{
	TMap<FString, FTDTurmStat> Je;
	Sammle(Je);
	Produktion(Je);
	return Je;
}

float UTDWellenStatistik::StromKapazitaetJetzt() const
{
	float Kap = 0.f, Verbrauch = 0.f;
	bool bUeber = false;
	MesseStrom(Kap, Verbrauch, bUeber);
	return Kap;
}
