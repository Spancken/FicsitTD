#include "TDTurretBase.h"
#include "TDForschung.h"
#include "FGSchematicManager.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "TDEinsatz.h"
#include "FicsitTDModule.h"
#include "TDDecoy.h"
#include "TDBalance.h"
#include "TDEinsatz.h"
#include "TDKernTerminal.h"
#include "TDDefenseCore.h"
#include "FGCharacterPlayer.h"
#include "TDWaveSubsystem.h"
#include "TDLeistung.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Creature/FGCreature.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FGFactoryColoringTypes.h"
#include "FGFactoryConnectionComponent.h"
#include "FGInventoryComponent.h"
#include "FGHealthComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGRecipe.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

static const TCHAR* TurmSwatchPfad = TEXT("/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/SwatchDesc_Custom.SwatchDesc_Custom_C");
static constexpr float TurmSchrittIntervall = 0.1f;
static constexpr float TurmZielIntervall = 1.f / 30.f;

ATDTurretBase::ATDTurretBase()
{
	SetCanBeDamaged(true);

	// Modelle setzt die Blueprint (Scripts/setup_turret_blueprint.py); BeginPlay laedt sie notfalls nach.
	// Lage aus mDrehpunkt/mNeigeachse wird in BeginPlay gesetzt (Unterklassen-Konstruktor laeuft erst danach).
	mYawMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretYaw"));
	mYawMesh->SetupAttachment(RootComponent);
	mYawMesh->SetRelativeLocation(mDrehpunkt);

	mHeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretHead"));
	mHeadMesh->SetupAttachment(mYawMesh);
	mHeadMesh->SetRelativeLocation(mNeigeachse);

	// Statuslampe vorne rechts auf dem Sockeldeck (88 cm hoch), wie die Leuchte beim Konstruktor
	mStatusLampe = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StatusLampe"));
	mStatusLampe->SetupAttachment(RootComponent);
	mStatusLampe->SetRelativeLocation(FVector(150.f, -150.f, 225.f));
	mStatusLampe->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	mRangeRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RangeRing"));
	mRangeRing->SetupAttachment(RootComponent);
	mRangeRing->SetRelativeLocation(FVector(0.f, 0.f, 15.f));
	mRangeRing->SetRelativeScale3D(FVector(mRange / 100.f, mRange / 100.f, 1.f));
	mRangeRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mRangeRing->SetCastShadow(false);
	mRangeRing->SetCanEverAffectNavigation(false);
}

void ATDTurretBase::SetzeModell(UStaticMeshComponent* Teil, const FString& Pfad)
{
	if (Teil && !Teil->GetStaticMesh() && !Pfad.IsEmpty())
	{
		Teil->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, *Pfad));
	}
}

void ATDTurretBase::ZeigeReichweite(bool bAn)
{
	if (!mRangeRing)
	{
		return;
	}
	const float R = GetReichweite() / 100.f;
	mRangeRing->SetRelativeScale3D(FVector(R, R, 1.f));
	mRangeRing->SetVisibility(bAn);
}

void ATDTurretBase::GetClearanceData_Implementation(TArray<FFGClearanceData>& out_data) const
{
	FFGClearanceData Data;
	Data.ClearanceBox = FBox(FVector(-205.f, -205.f, 0.f), FVector(205.f, 205.f, mKlarraumHoehe));
	out_data.Add(Data);
}

float ATDTurretBase::SchadenBonus() const
{
	return (1.f + LevelWert(TEXT("Schaden"))) * (HatDoktrin() ? 1.f + TDBalance::Wert(TEXT("Doktrin.Schaden"), 0.25f) : 1.f);
}

/** Doktrin-Knoten je Turmtyp (Scripts/create_research.py, Baum Festung) */
static const TCHAR* DoktrinKnoten(const FString& Turm)
{
	if (Turm == TEXT("Bolzenturm")) return TEXT("DoktrinBolzen");
	if (Turm == TEXT("Gewehrturm")) return TEXT("DoktrinGewehr");
	if (Turm == TEXT("Teslaspule")) return TEXT("DoktrinTesla");
	if (Turm == TEXT("Flammenwerfer")) return TEXT("DoktrinFlamme");
	if (Turm == TEXT("Nobelisk-Werfer")) return TEXT("DoktrinNobelisk");
	return nullptr;
}

float ATDTurretBase::TestWert(const FString& Name) const
{
	if (Name == TEXT("SchadenBonus")) return SchadenBonus();
	if (Name == TEXT("TaktBonus")) return TaktBonus();
	if (Name == TEXT("Uebertakt")) return GetCurrentPotential();
	if (Name == TEXT("NennDPS")) return NennDPS();
	if (Name == TEXT("LevelPreis")) return LevelPreis();
	if (Name == TEXT("LevelMax")) return LevelMax();
	if (Name == TEXT("Level")) return GetLevel();
	if (Name == TEXT("Doktrin")) return HatDoktrin() ? 1.f : 0.f;
	if (Name == TEXT("KannUebertakten")) return mCanChangePotential ? 1.f : 0.f;
	if (Name == TEXT("SplitterPlaetze")) return mPotentialShardSlots;
	if (Name == TEXT("StromLaden")) return GetProducingPowerConsumptionBase();
	return -1.f;
}

bool ATDTurretBase::HatDoktrin() const
{
	const TCHAR* Knoten = DoktrinKnoten(mTurmName);
	return Knoten && TDForschung::Hat(this, Knoten);
}

float ATDTurretBase::TaktBonus() const
{
	return 1.f + LevelWert(TEXT("Takt"));
}

float ATDTurretBase::GetReichweite() const
{
	return TDBalance::Wert(BalanceTurm() + TEXT("Reichweite"), mRange / 100.f) * 100.f * (1.f + LevelWert(TEXT("Reichweite")));
}

// ---------------- Turmlevel (0.63, Marius 04.11.2026) ----------------
// Level 1-5 nur per Upgrade im Turmfenster (EP), keine Stufen mehr durch Abschuesse. Level 2 mit dem MAM-Knoten des Turmtyps,
// 3/4 mit Turmfertigung II/III, 5 = Meisterstufe (0.64). Jedes Level ist ein festes Paket passend zum Modell (Anbauteile _S<n>).

struct FTDLevelStandard { const TCHAR* Turm; int32 Level; const TCHAR* Wert; float Betrag; };
static const FTDLevelStandard TDLevelStandards[] = {
	{ TEXT("Gewehrturm"), 2, TEXT("Schaden"), 0.2f }, { TEXT("Gewehrturm"), 3, TEXT("Takt"), 0.3f },
	{ TEXT("Gewehrturm"), 4, TEXT("Magazin"), 1.f }, { TEXT("Gewehrturm"), 4, TEXT("Reichweite"), 0.15f },
	{ TEXT("Bolzenturm"), 2, TEXT("Schaden"), 0.2f }, { TEXT("Bolzenturm"), 3, TEXT("Takt"), 0.25f },
	{ TEXT("Bolzenturm"), 4, TEXT("Reichweite"), 0.15f }, { TEXT("Bolzenturm"), 4, TEXT("Durchschlag"), 1.f },
	{ TEXT("Nobelisk-Werfer"), 2, TEXT("Schaden"), 0.2f }, { TEXT("Nobelisk-Werfer"), 3, TEXT("Takt"), 0.25f },
	{ TEXT("Nobelisk-Werfer"), 4, TEXT("Reichweite"), 0.15f }, { TEXT("Nobelisk-Werfer"), 4, TEXT("Radius"), 0.2f },
	{ TEXT("Teslaspule"), 2, TEXT("Reichweite"), 0.1f }, { TEXT("Teslaspule"), 3, TEXT("Spruenge"), 1.f },
	{ TEXT("Teslaspule"), 3, TEXT("Schaden"), 0.2f }, { TEXT("Teslaspule"), 4, TEXT("Takt"), 0.3f },
	{ TEXT("Flammenwerfer"), 2, TEXT("Schaden"), 0.2f }, { TEXT("Flammenwerfer"), 3, TEXT("Kegel"), 0.5f },
	{ TEXT("Flammenwerfer"), 4, TEXT("Reichweite"), 0.15f }, { TEXT("Flammenwerfer"), 4, TEXT("Nachbrennen"), 0.5f },
};
static const TCHAR* TDLevelWerte[] = { TEXT("Schaden"), TEXT("Takt"), TEXT("Reichweite"), TEXT("Magazin"), TEXT("Durchschlag"),
	TEXT("Spruenge"), TEXT("Radius"), TEXT("Kegel"), TEXT("Nachbrennen") };

float ATDTurretBase::LevelWertBis(int32 Level, const TCHAR* Wert) const
{
	float Summe = 0.f;
	for (int32 n = 2; n <= FMath::Min(Level, TurmMaxLevel); ++n)
	{
		// nur Werte mit Standard abfragen (sonst warnt die Balance-Datei bei jedem fehlenden Schluessel, 0.64.1)
		for (const FTDLevelStandard& S : TDLevelStandards)
		{
			if (S.Level == n && mTurmName == S.Turm && FCString::Strcmp(S.Wert, Wert) == 0)
			{
				Summe += TDBalance::Wert(FString::Printf(TEXT("Tuerme.Level.%s.L%d.%s"), *mTurmName, n, Wert), S.Betrag);
				break;
			}
		}
	}
	return Summe;
}

FString ATDTurretBase::LevelDiffText(int32 Von, int32 Bis) const
{
	TArray<FString> Teile;
	for (const TCHAR* Wert : TDLevelWerte)
	{
		const float D = LevelWertBis(Bis, Wert) - LevelWertBis(Von, Wert);
		if (FMath::Abs(D) < 0.001f)
		{
			continue;
		}
		const FString W = Wert;
		if (W == TEXT("Durchschlag"))
		{
			Teile.Add(FString::Printf(TEXT("+%d Durchschlag"), FMath::RoundToInt(D)));
		}
		else if (W == TEXT("Spruenge"))
		{
			Teile.Add(FString::Printf(TEXT("+%d Sprung"), FMath::RoundToInt(D)));
		}
		else
		{
			FString Name = W;
			if (W == TEXT("Takt"))
			{
				Name = mTurmName == TEXT("Teslaspule") ? TEXT("Aufladen") : mTurmName == TEXT("Nobelisk-Werfer") ? TEXT("Wurftakt") : TEXT("Feuerrate");
			}
			else if (W == TEXT("Radius"))
			{
				Name = TEXT("Sprengradius");
			}
			else if (W == TEXT("Kegel"))
			{
				Name = TEXT("Kegelbreite");
			}
			Teile.Add(W == TEXT("Magazin") && FMath::IsNearlyEqual(D, 1.f) ? FString(TEXT("Magazin x2"))
				: FString::Printf(TEXT("%s +%d %%"), *Name, FMath::RoundToInt(100.f * D)));
		}
	}
	// Level 5 = Meisterstufe mit eigener Faehigkeit je Turm (0.64, Marius: nicht ueberall ein zweiter Lauf)
	if (Von < TurmMaxLevel && Bis >= TurmMaxLevel)
	{
		Teile.Add(mTurmName == TEXT("Gewehrturm") ? TEXT("Meisterstufe Gurttrommel: Dauerfeuer ohne Nachladen")
			: mTurmName == TEXT("Bolzenturm") ? TEXT("Meisterstufe Doppellauf: zwei Bolzen je Schuss")
			: mTurmName == TEXT("Nobelisk-Werfer") ? TEXT("Meisterstufe Salvenkopf: drei Nobelisken je Salve")
			: mTurmName == TEXT("Teslaspule") ? TEXT("Meisterstufe Spannungsfeld: bremst Gegner bis 10 m")
			: mTurmName == TEXT("Flammenwerfer") ? TEXT("Meisterstufe Feuerteppich: der Boden brennt 6 s")
			: TEXT("Meisterstufe"));
	}
	if (Teile.Num() == 0)
	{
		return FString(TEXT("-"));
	}
	return FString::Join(Teile, TEXT(", "));
}

/** 0.72 (Forschung 2.0): Level 2 gibt es mit dem Turm selbst. Knoten, der den Turm freischaltet; leer = Starterturm */
static const TCHAR* TurmKnoten(const FString& Turm, bool& bHatLevel)
{
	bHatLevel = true;
	if (Turm == TEXT("Bolzenturm")) return TEXT("");
	if (Turm == TEXT("Gewehrturm")) return TEXT("Gewehrturm");
	if (Turm == TEXT("Teslaspule")) return TEXT("Teslaspule");
	if (Turm == TEXT("Flammenwerfer")) return TEXT("Flammenwerfer");
	if (Turm == TEXT("Nobelisk-Werfer")) return TEXT("NobeliskWerfer");
	bHatLevel = false;
	return nullptr;
}

bool ATDTurretBase::HatUpgrades() const
{
	bool bHatLevel = false;
	TurmKnoten(mTurmName, bHatLevel);
	return bHatLevel;
}

bool ATDTurretBase::UpgradeFreigeschaltet() const
{
	bool bHatLevel = false;
	const TCHAR* Knoten = TurmKnoten(mTurmName, bHatLevel);
	return bHatLevel && (FCString::Strlen(Knoten) == 0 || TDForschung::Hat(this, Knoten));
}

int32 ATDTurretBase::LevelMax() const
{
	if (!HatUpgrades() || !UpgradeFreigeschaltet())
	{
		return 1;
	}
	if (!TDForschung::Hat(this, TEXT("Turmfertigung2")))
	{
		return 2;
	}
	if (!TDForschung::Hat(this, TEXT("Turmfertigung3")))
	{
		return 3;
	}
	// 0.73 Doktrin: die Hauptlinie bekommt Level 5 schon mit Turmfertigung III
	return TDForschung::Hat(this, TEXT("Turmfertigung4")) || HatDoktrin() ? 5 : 4;
}

int32 ATDTurretBase::LevelPreis() const
{
	const int32 Naechstes = GetLevel() + 1;
	if (Naechstes > TurmMaxLevel)
	{
		return 0;
	}
	static const int32 Standard[] = { 0, 0, 8, 15, 25, 40 };
	const int32 Preis = TDBalance::Ganz(FString::Printf(TEXT("Tuerme.Level.Preis%d"), Naechstes), Standard[Naechstes]);
	// 0.73 Doktrin: Level der Hauptlinie guenstiger
	return HatDoktrin() ? FMath::Max(1, FMath::RoundToInt(Preis * (1.f - TDBalance::Wert(TEXT("Doktrin.LevelRabatt"), 0.25f)))) : Preis;
}

FString ATDTurretBase::LevelSperre() const
{
	const int32 Naechstes = GetLevel() + 1;
	if (!HatUpgrades())
	{
		return TEXT("Dieser Turm hat keine Level.");
	}
	if (Naechstes > TurmMaxLevel)
	{
		return TEXT("Höchstes Level erreicht.");
	}
	if (Naechstes <= LevelMax())
	{
		return FString();
	}
	if (Naechstes == 2)
	{
		return FString::Printf(TEXT("Erst im MAM erforschen: %s."), *mTurmName);
	}
	if (Naechstes == 3)
	{
		return TEXT("Erst im MAM erforschen: Turmfertigung II.");
	}
	if (Naechstes == 4)
	{
		return TEXT("Erst im MAM erforschen: Turmfertigung III.");
	}
	return TEXT("Erst im MAM erforschen: Turmfertigung IV (Meisterstufe).");
}

FString ATDTurretBase::KaufeLevel()
{
	const FString Sperre = LevelSperre();
	if (!Sperre.IsEmpty())
	{
		return Sperre;
	}
	const int32 Neu = GetLevel() + 1;
	const int32 Preis = LevelPreis();
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	const FString Fehler = Einsatz ? Einsatz->ZahleEP(Preis, FString::Printf(TEXT("%s auf Level %d"), *mTurmName, Neu), true) : TEXT("Nur im Einsatz.");
	if (!Fehler.IsEmpty())
	{
		return Fehler;
	}
	mLevel = Neu;
	if (mRangeRing)
	{
		ZeigeReichweite(mRangeRing->IsVisible());
	}
	ZeigeStufenTeile();
	UE_LOG(LogFicsitTD, Log, TEXT("Level %s %s: Level %d fuer %d EP (%s)"), *mTurmName, *GetName(), mLevel, Preis, *LevelText(mLevel));
	return FString::Printf(TEXT("Level %d für %d EP: %s. Konto %d EP."), mLevel, Preis, *LevelText(mLevel), Einsatz->Konto());
}

void ATDTurretBase::SetzeLevelTest(int32 Level)
{
	mLevel = FMath::Clamp(Level, 1, TurmMaxLevel);
	if (mRangeRing)
	{
		ZeigeReichweite(mRangeRing->IsVisible());
	}
	ZeigeStufenTeile();
}

float ATDTurretBase::GetTDMaxHealth() const
{
	return TDBalance::Wert(BalanceTurm() + TEXT("LP"), mBaseMaxHealth);
}

float ATDTurretBase::GetProductionCycleTime() const
{
	return Super::GetProductionCycleTime() / TaktBonus();
}

FText ATDTurretBase::GetLookAtDecription_Implementation(AFGCharacterPlayer* byCharacter, const FUseState& state) const
{
	const int32 Stufe = GetLevel();
	const int32 MaxStufe = TurmMaxLevel;
	const FString Sperre = LevelSperre();
	const FString Fortschritt = Stufe >= MaxStufe ? FString(TEXT("Höchstes Level"))
		: Sperre.IsEmpty() ? FString::Printf(TEXT("Level %d für %d EP im Turmfenster"), Stufe + 1, LevelPreis()) : Sperre;
	const FString Boni = Stufe > 1 ? TEXT("   |   ") + LevelSumme() : FString();
	const FString Zusatz = FString::Printf(TEXT("Schadensart %s%s%s"), SchadensartName(mSchadensart),
		ZusatzInfo().IsEmpty() ? TEXT("") : TEXT("   |   "), *ZusatzInfo());
	return FText::FromString(FString::Printf(TEXT("%s\nLevel %d von %d   |   %s\nLebenspunkte %.0f / %.0f   |   Reichweite %.0f m%s%s%s"),
		*Super::GetLookAtDecription_Implementation(byCharacter, state).ToString(), Stufe, MaxStufe, *Fortschritt,
		mHealth, GetTDMaxHealth(), GetReichweite() / 100.f, *Boni, Zusatz.IsEmpty() ? TEXT("") : TEXT("\n"), *Zusatz));
}

float ATDTurretBase::Zykluszeit() const
{
	return GetCurrentRecipe() ? GetProductionCycleTime() : 0.f;
}

void ATDTurretBase::Kennwerte(TArray<FTDKennwert>& Out) const
{
	const int32 Stufe = GetLevel();
	Out.Add({ TEXT("Level"), FString::Printf(TEXT("%d von %d"), Stufe, TurmMaxLevel) });
	Out.Add({ TEXT("Lebenspunkte"), FString::Printf(TEXT("%.0f / %.0f"), FMath::Max(mHealth, 0.f), GetTDMaxHealth()) });
	Out.Add({ TEXT("Reichweite"), FString::Printf(TEXT("%.0f m"), GetReichweite() / 100.f) });
	Out.Add({ TEXT("Schadensart"), SchadensartName(mSchadensart) });
	Kampfwerte(Out);
	if (Stufe > 1)
	{
		Out.Add({ TEXT("Levelbonus"), LevelSumme() });
	}
	Out.Add({ TEXT("Munition"), MunitionsText() });
	const FString Zusatz = ZusatzImPanel() ? ZusatzInfo() : FString();
	if (!Zusatz.IsEmpty())
	{
		Out.Add({ TEXT("Status"), Zusatz });
	}
	Out.Add({ TEXT("Strom"), RunsOnPower()
		? FString::Printf(TEXT("%s · %.0f MW"), HasPower() ? TEXT("ja") : TEXT("NEIN"), GetDefaultProducingPowerConsumption())
		: FString(TEXT("nicht nötig")) });
	Out.Add({ TEXT("Schüsse/Abschüsse"), FString::Printf(TEXT("%d / %d"), mSchuesse, mKills) });
}

FString ATDTurretBase::MunitionsText() const
{
	return FString::Printf(TEXT("x%d %s"), GetAmmo(), *GetMunitionsName());
}

void ATDTurretBase::RegisterInteractingPlayer_Implementation(AFGCharacterPlayer* player)
{
	Super::RegisterInteractingPlayer_Implementation(player);
	APlayerController* PC = player && player->IsLocallyControlled() ? Cast<APlayerController>(player->GetController()) : nullptr;
	if (!PC || mInfo)
	{
		return;
	}
	mInfo = CreateWidget<UTDTurmInfo>(PC, UTDTurmInfo::StaticClass());
	if (mInfo)
	{
		mInfo->SetzeTurm(this);
		mInfo->AddToViewport(500);
		UE_LOG(LogFicsitTD, Log, TEXT("Turm-Info: %s geoeffnet"), *GetName());
	}
}

void ATDTurretBase::UnregisterInteractingPlayer_Implementation(AFGCharacterPlayer* player)
{
	Super::UnregisterInteractingPlayer_Implementation(player);
	if (mInfo && player && player->IsLocallyControlled())
	{
		mInfo->RemoveFromParent();
		mInfo = nullptr;
		UE_LOG(LogFicsitTD, Log, TEXT("Turm-Info: %s geschlossen"), *GetName());
	}
}

void ATDTurretBase::BeginPlay()
{
	// 0.73 Energiesplitter: Uebertakten muss am Turm moeglich sein (Maschinenfenster des Spiels); einmal je Turmtyp melden
	static TSet<FString> UebertaktGemeldet;
	if (!UebertaktGemeldet.Contains(mTurmName))
	{
		UebertaktGemeldet.Add(mTurmName);
		UE_LOG(LogFicsitTD, Log, TEXT("Uebertakten %s: moeglich %d, Splitter-Plaetze %d, hoechstens %.0f %%"), *mTurmName,
			mCanChangePotential ? 1 : 0, mPotentialShardSlots, mMaxPotential * 100.f);
	}
	// Lage der beweglichen Teile aus der Konfiguration der Unterklasse
	if (mYawMesh)
	{
		mYawMesh->SetRelativeLocation(mDrehpunkt);
	}
	if (mHeadMesh)
	{
		mHeadMesh->SetRelativeLocation(mNeigeachse);
	}
	Super::BeginPlay();
	NachMaschinenStart();
	SetzeModell(mYawMesh, mYawModell);
	SetzeModell(mHeadMesh, mHeadModell);
	SetzeModell(mStatusLampe, TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_StatusLight.SM_TD_StatusLight"));
	SetzeModell(mRangeRing, TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_RangeRing.SM_TD_RangeRing"));
	if (mStatusLampe)
	{
		mStatusLampe->SetRelativeLocation(mLampenOrt);
	}
	BaueStufenTeile();
	// Gegnertakt-Recherche 16: bewegte Teile (Drehteil, Kopf, Laeufe, Effekte) duerfen die Navigation nicht beeinflussen,
	// sonst baut das NavMesh bei jeder Drehung die Kachel neu (65-80 ms je Ruckel-Bild). Der Sockel (Instanz-Proxy) bleibt Hindernis.
	int32 OhneNavigation = 0;
	TArray<UPrimitiveComponent*> Teile;
	GetComponents<UPrimitiveComponent>(Teile);
	for (UPrimitiveComponent* Teil : Teile)
	{
		if (Teil && Teil->GetClass() == UStaticMeshComponent::StaticClass() && Teil->CreationMethod == EComponentCreationMethod::Native
			&& Teil->CanEverAffectNavigation())
		{
			Teil->SetCanEverAffectNavigation(false);
			++OhneNavigation;
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("%s: %d bewegte Teile aus dem Navigationsnetz genommen"), *GetName(), OhneNavigation);
	if (mStatusLampe)
	{
		// Slot 1 = Leuchtkoerper (M_TD_StatusLight, Parameter Farbe/Leuchtkraft)
		mLampenMaterial = mStatusLampe->CreateDynamicMaterialInstance(1);
	}
	// Der Kreis ist nur fuers Hologramm sichtbar; am fertigen Turm per /td reichweite
	ZeigeReichweite(false);
	if (!HasAuthority())
	{
		return;
	}
	if (mHealth < 0.f)
	{
		mHealth = GetTDMaxHealth();
	}
	// Spielstand vor 0.63: Einzel-Upgrades in ein Level umrechnen (hoechste Stufe + 1), Spezialisierung entfaellt
	if (mLevel <= 0)
	{
		mLevel = FMath::Clamp(1 + FMath::Max3(mUpgReichweite, mUpgTakt, mUpgSchaden), 1, 4);
		if (mLevel > 1 || mSpezial > 0)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("%s: Spielstand vor 0.63, Upgrades R%d/F%d/S%d Spezialisierung %d -> Level %d"), *GetName(), mUpgReichweite, mUpgTakt, mUpgSchaden, mSpezial, mLevel);
		}
		mUpgReichweite = mUpgTakt = mUpgSchaden = mSpezial = 0;
	}
	// Neu gebaut: gleich das Standardrezept setzen
	if (!GetCurrentRecipe() && !mStandardRezept.IsEmpty())
	{
		if (UClass* Rezept = LoadClass<UFGRecipe>(nullptr, *mStandardRezept))
		{
			SetRecipe(Rezept);
			// Startmunition (Test 0.55: bis die Produktion die ersten Bolzen bringt, stand der Turm zu lange)
			const int32 Start = TDBalance::Ganz(BalanceTurm() + TEXT("Startmunition"), 0);
			const TArray<FItemAmount> Zutaten = UFGRecipe::GetIngredients(this, Rezept);
			if (Start > 0 && Zutaten.Num() > 0 && Zutaten[0].ItemClass && GetInputInventory())
			{
				const int32 Rein = GetInputInventory()->AddStack(FInventoryStack(Start, Zutaten[0].ItemClass), true);
				UE_LOG(LogFicsitTD, Log, TEXT("%s: Startmunition %d %s"), *GetName(), Rein, *UFGItemDescriptor::GetItemName(Zutaten[0].ItemClass).ToString());
			}
		}
		// Neubau nach einem Abriss: Level uebernehmen (Marius 03.11.2026: abreissen und wieder bei 1 anfangen ergab keinen Sinn)
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			int32 Kills = 0, Level = 1;
			if (Einsatz->HoleTurmErbe(mTurmName, Kills, Level))
			{
				mKills = FMath::Max(mKills, Kills);
				mLevel = FMath::Clamp(Level, 1, TurmMaxLevel);
				mHealth = GetTDMaxHealth();
				UE_LOG(LogFicsitTD, Log, TEXT("%s: uebernimmt Erbe eines abgerissenen %s (%d Abschuesse, Level %d)"), *GetName(), *mTurmName, mKills, mLevel);
			}
		}
	}

	// Koeder stehen im Sockel (Y = +-(40+70) = +-110 < 180): verdeckt, fuer Kreaturen nah genug an der Aussenwand
	ATDDecoy::SpawnForBox(this, FVector(0.f, 0.f, mHalbmasse.Z), FVector(mHalbmasse.X, 40.f, mHalbmasse.Z), 1, mDecoys);
	GetWorldTimerManager().SetTimer(mSpielTimer, this, &ATDTurretBase::Spielschritt, TurmSchrittIntervall, true);
	GetWorldTimerManager().SetTimer(mZielTimer, this, &ATDTurretBase::Zielen, TurmZielIntervall, true);
	// erst nach dem Laden der Umgebung (Instanzen der Spielgebaeude entstehen verzoegert)
	GetWorldTimerManager().SetTimer(mDiagnoseTimer, this, &ATDTurretBase::DiagnoseUmgebung, 8.f, false);
	if (!mFarbeGesetzt)
	{
		GetWorldTimerManager().SetTimer(mFarbTimer, this, &ATDTurretBase::SetzeErkennungsfarbe, 0.5f, false);
	}
	int32 Rohre = 0;
	TInlineComponentArray<UFGPipeConnectionComponent*> RohrAnschluesse(this);
	for (const UFGPipeConnectionComponent* R : RohrAnschluesse)
	{
		Rohre += R ? 1 : 0;
	}
	TArray<TSubclassOf<UFGRecipe>> Verfuegbar;
	GetAvailableRecipes(Verfuegbar);
	FString RezeptListe;
	for (const TSubclassOf<UFGRecipe>& R : Verfuegbar)
	{
		RezeptListe += R ? R->GetName() + TEXT(" ") : FString();
	}
	UE_LOG(LogFicsitTD, Log, TEXT("%s %s: %d waehlbare Rezepte: %s| Band-Anschluesse %d, Rohr-Anschluesse %d (davon Eingang %d)"), *mTurmName, *GetName(),
		Verfuegbar.Num(), *RezeptListe, GetConnectionComponents().Num(), Rohre, mPipeInputConnections.Num());
	UE_LOG(LogFicsitTD, Log, TEXT("%s %s: Rezept %s, Strom %s, Level %d (%d Abschuesse)"), *mTurmName, *GetName(),
		GetCurrentRecipe() ? *GetCurrentRecipe()->GetName() : TEXT("keins"), *GetStromText(), GetLevel(), mKills);
}

USceneComponent* ATDTurretBase::StufenZiel(const FString& Teil) const
{
	return Teil == TEXT("Base") ? GetRootComponent() : Teil == TEXT("Yaw") ? static_cast<USceneComponent*>(mYawMesh)
		: Teil == TEXT("Head") ? static_cast<USceneComponent*>(mHeadMesh) : nullptr;
}

void ATDTurretBase::BaueStufenTeile()
{
	// Namensschema aus dem Modell des Drehteils: <Ordner>/SM_TD_<Turm>_Yaw (Teslaspule: _Coil) -> <Ordner>/SM_TD_<Turm>_<Teil>_S<n>
	FString Ordner, Name;
	if (!mYawModell.Split(TEXT("/"), &Ordner, &Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return;
	}
	Name.Split(TEXT("."), &Name, nullptr);
	FString Praefix;
	if (!Name.Split(TEXT("_"), &Praefix, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return;
	}
	static const TCHAR* Teile[] = { TEXT("Base"), TEXT("Yaw"), TEXT("Head"), TEXT("Barrels") };
	for (const TCHAR* Teil : Teile)
	{
		USceneComponent* Ziel = StufenZiel(Teil);
		if (!Ziel)
		{
			continue;
		}
		for (int32 n = 2; n <= TurmMaxLevel; ++n)
		{
			const FString Asset = FString::Printf(TEXT("%s_%s_S%d"), *Praefix, Teil, n);
			UStaticMesh* Modell = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Ordner, *Asset, *Asset), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Modell)
			{
				continue;
			}
			UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Stufe_%s"), *Asset));
			K->SetStaticMesh(Modell);
			K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			K->SetCanEverAffectNavigation(false);
			K->SetupAttachment(Ziel);
			K->SetVisibility(false);
			K->RegisterComponent();
			mStufenTeile.Add(K);
			mStufenTeilStufe.Add(n);
		}
	}
	if (mStufenTeile.Num() > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("%s: %d Stufen-Anbauteile (%s_*_S2..S%d)"), *GetName(), mStufenTeile.Num(), *Praefix, TurmMaxLevel);
		mGezeigteStufe = -1;
		ZeigeStufenTeile();
		GetWorldTimerManager().SetTimer(mLackTimer, this, &ATDTurretBase::GleicheLackAb, 2.f, true, 0.3f);
	}
}

void ATDTurretBase::ZeigeStufenTeile()
{
	const int32 Stufe = GetLevel();
	if (Stufe == mGezeigteStufe || mStufenTeile.Num() == 0)
	{
		return;
	}
	mGezeigteStufe = Stufe;
	for (int32 i = 0; i < mStufenTeile.Num(); ++i)
	{
		if (mStufenTeile[i])
		{
			mStufenTeile[i]->SetVisibility(Stufe >= mStufenTeilStufe[i]);
		}
	}
	GleicheLackAb();
}

void ATDTurretBase::GleicheLackAb()
{
	// Lack kommt als Primitivdaten (0-2 Primaer, 3-5 Sekundaer, 6 Strom) an die Komponenten des Gebaeudes; spaeter erzeugte Teile kopieren ihn
	const UPrimitiveComponent* Vorbild = mYawMesh;
	if (!Vorbild)
	{
		return;
	}
	const TArray<float>& Daten = Vorbild->GetCustomPrimitiveData().Data;
	for (UStaticMeshComponent* K : mStufenTeile)
	{
		if (K && K->GetCustomPrimitiveData().Data != Daten)
		{
			for (int32 i = 0; i < Daten.Num(); ++i)
			{
				K->SetCustomPrimitiveDataFloat(i, Daten[i]);
			}
		}
	}
}

void ATDTurretBase::Dismantle_Implementation()
{
	if (HasAuthority() && (mKills > 0 || GetLevel() > 1))
	{
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->MerkeTurmErbe(mTurmName, mKills, GetLevel());
		}
	}
	Super::Dismantle_Implementation();
}

void ATDTurretBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(mSpielTimer);
	GetWorldTimerManager().ClearTimer(mZielTimer);
	GetWorldTimerManager().ClearTimer(mFarbTimer);
	GetWorldTimerManager().ClearTimer(mDiagnoseTimer);
	GetWorldTimerManager().ClearTimer(mLackTimer);
	ATDDecoy::DestroyAll(mDecoys);
	if (mInfo)
	{
		mInfo->RemoveFromParent();
		mInfo = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ATDTurretBase::SetzeErkennungsfarbe()
{
	if (UClass* CustomSwatch = LoadClass<UFGFactoryCustomizationDescriptor_Swatch>(nullptr, TurmSwatchPfad))
	{
		FFactoryCustomizationData Data = GetCustomizationData_Native();
		Data.SwatchDesc = CustomSwatch;
		Data.OverrideColorData = FFactoryCustomizationColorSlot(FLinearColor(0.22f, 0.f, 0.f), FLinearColor(0.005f, 0.005f, 0.005f));
		SetCustomizationData_Native(Data);
		mFarbeGesetzt = true;
	}
}

TSubclassOf<UFGItemDescriptor> ATDTurretBase::AktuelleMunition() const
{
	const TSubclassOf<UFGRecipe> Rezept = GetCurrentRecipe();
	if (Rezept)
	{
		const TArray<FItemAmount> Zutaten = UFGRecipe::GetIngredients(this, Rezept);
		if (Zutaten.Num() > 0)
		{
			return Zutaten[0].ItemClass;
		}
	}
	return nullptr;
}

FString ATDTurretBase::GetMunitionsName() const
{
	const TSubclassOf<UFGItemDescriptor> Munition = AktuelleMunition();
	return Munition ? UFGItemDescriptor::GetItemName(Munition).ToString() : TEXT("keine");
}

int32 ATDTurretBase::GetAmmo() const
{
	const TSubclassOf<UFGItemDescriptor> Munition = AktuelleMunition();
	const UFGInventoryComponent* Inventar = GetInputInventory();
	return Inventar && Munition ? Inventar->GetNumItems(Munition) : 0;
}

FString ATDTurretBase::GetStromText() const
{
	if (!RunsOnPower() || !GetPowerInfo())
	{
		return TEXT("nicht noetig");
	}
	return HasPower() ? TEXT("ja") : TEXT("NEIN (kein Netz mit Generator)");
}

static bool TDSichtFrei(const ATDTurretBase* Turm, const FVector& Von, const FVector& Nach, const AActor* Ziel, FString& OutHindernis);

FString ATDTurretBase::KeinZielGrund() const
{
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!Waves)
	{
		return FString();
	}
	const FVector Achse = mDreht && mHeadMesh ? mHeadMesh->GetComponentLocation() : GetMuzzleLocation();
	AFGCreature* Naechster = nullptr;
	float Best = TNumericLimits<float>::Max();
	for (AFGCreature* C : Waves->GetLivingWaveCreatures())
	{
		const float D = FVector::DistSquared(C->GetActorLocation(), Achse);
		if (D < Best)
		{
			Best = D;
			Naechster = C;
		}
	}
	if (!Naechster)
	{
		return FString();
	}
	const float Abstand = FMath::Sqrt(Best);
	FString Grund;
	if (Abstand > GetReichweite())
	{
		Grund = FString::Printf(TEXT("ausser Reichweite (%.0f m)"), GetReichweite() / 100.f);
	}
	else if (!ZielErlaubt(Naechster, Abstand))
	{
		Grund = TEXT("nicht erlaubt (z. B. Mindestabstand)");
	}
	else
	{
		FString Hindernis;
		if (!TDSichtFrei(this, Achse, Naechster->GetActorLocation(), Naechster, Hindernis))
		{
			Grund = FString::Printf(TEXT("Sicht verdeckt durch %s"), *Hindernis);
		}
		else
		{
			Grund = TEXT("Sicht frei - eigentlich Ziel");
		}
	}
	return FString::Printf(TEXT(" (naechster Gegner %s in %.0f m: %s)"), *Naechster->GetClass()->GetName(), Abstand / 100.f, *Grund);
}

FString ATDTurretBase::GetZustandText() const
{
	int32 Verbunden = 0;
	for (const UFGFactoryConnectionComponent* K : GetConnectionComponents())
	{
		Verbunden += K && K->IsConnected() ? 1 : 0;
	}
	FString Zustand;
	if (!GetCurrentRecipe())                Zustand = TEXT("kein Rezept gewaehlt");
	else if (!HatMunition())                Zustand = TEXT("keine Munition");
	else if (RunsOnPower() && !HasPower())  Zustand = TEXT("kein Strom");
	else if (IsProductionPaused())          Zustand = TEXT("Standby");
	else if (!mAktuellesZiel.IsValid())     Zustand = TEXT("bereit, kein Ziel") + KeinZielGrund();
	else                                    Zustand = TEXT("kaempft");
	return FString::Printf(TEXT("%s, Band verbunden %d, Munition %s x%d"), *Zustand, Verbunden, *GetMunitionsName(), GetAmmo());
}

bool ATDTurretBase::CanProduce_Implementation() const
{
	// Laeuft auf dem Fabrik-Thread: nur den vom Spiel-Thread gesetzten Zustand lesen
	return mBereit.load() && Super::CanProduce_Implementation();
}

void ATDTurretBase::Factory_ProductionCycleCompleted(float overProductionRate)
{
	Super::Factory_ProductionCycleCompleted(overProductionRate);
	// Das Produkt "Bolzenschuss" gibt es nur, damit die Rezeptauswahl die Rezepte zeigt - gleich wieder wegwerfen
	if (UFGInventoryComponent* Ausgabe = GetOutputInventory())
	{
		Ausgabe->Empty();
	}
	++mOffeneZyklen;
}

FVector ATDTurretBase::GetMuzzleLocation() const
{
	const USceneComponent* Bezug = mDreht && mHeadMesh ? static_cast<const USceneComponent*>(mHeadMesh) : static_cast<const USceneComponent*>(mYawMesh);
	return Bezug ? Bezug->GetComponentTransform().TransformPosition(mMuendung) : GetActorLocation() + FVector(0.f, 0.f, 420.f);
}

bool ATDTurretBase::IstAusgerichtet(const AActor* Ziel) const
{
	if (!Ziel)
	{
		return false;
	}
	if (!mDreht || !mHeadMesh || UTDWaveSubsystem::bStressDrehenAus)
	{
		return true;
	}
	const FVector Zu = Ziel->GetActorLocation() - mHeadMesh->GetComponentLocation();
	const FVector Vorne = mHeadMesh->GetForwardVector();
	const float Toleranz = FMath::Cos(FMath::DegreesToRadians(mZielToleranz));
	if (FVector::DotProduct(Vorne, Zu.GetSafeNormal()) > Toleranz)
	{
		return true;
	}
	// 0.64.1 (Marius: Turm auf dem Felsen schoss nicht auf Hogs direkt darunter): liegt das Ziel ausserhalb des Neigungsbereichs
	// und steht die Kanzel am Anschlag, genuegt die Seitenrichtung. Der Schaden geht trotzdem auf das Ziel.
	const float Neigung = FMath::RadiansToDegrees(FMath::Atan2(Zu.Z, Zu.Size2D()));
	const float Kopf = mHeadMesh->GetRelativeRotation().Pitch;
	const bool bAmAnschlag = (Neigung < mNeigungMin && Kopf <= mNeigungMin + 1.f) || (Neigung > mNeigungMax && Kopf >= mNeigungMax - 1.f);
	return bAmAnschlag && FVector::DotProduct(Vorne.GetSafeNormal2D(), Zu.GetSafeNormal2D()) > Toleranz;
}

bool ATDTurretBase::BereitZumProduzieren(AFGCreature* Ziel) const
{
	return Ziel != nullptr && IstAusgerichtet(Ziel);
}

void ATDTurretBase::Zielen()
{
	TDLeistung::FUhr Uhr(TDLeistung::TurmZyklen);
	AFGCreature* Ziel = mAktuellesZiel.Get();
	// T2e: Stresstest schaltet den Schatten von Drehteil und Kopf ab und danach wieder an
	if (mYawMesh && mYawMesh->CastShadow == UTDWaveSubsystem::bStressSchattenAus)
	{
		mYawMesh->SetCastShadow(!UTDWaveSubsystem::bStressSchattenAus);
		if (mHeadMesh)
		{
			mHeadMesh->SetCastShadow(!UTDWaveSubsystem::bStressSchattenAus);
		}
	}
	// T2f: Distanzfeld-Beleuchtung von Drehteil und Kopf im Stresstest ab- und danach wieder einschalten
	if (mYawMesh && mYawMesh->bAffectDistanceFieldLighting == UTDWaveSubsystem::bStressDfAus)
	{
		for (UStaticMeshComponent* Teil : { mYawMesh.Get(), mHeadMesh.Get() })
		{
			if (Teil)
			{
				Teil->SetAffectDistanceFieldLighting(!UTDWaveSubsystem::bStressDfAus);
				Teil->SetAffectDynamicIndirectLighting(!UTDWaveSubsystem::bStressDfAus);
			}
		}
	}
	// T2f: Drehtakt - nur N-mal je Sekunde drehen, dafuer mit groesseren Schritten
	mDrehZeit += TurmZielIntervall;
	const bool bDrehtakt = UTDWaveSubsystem::StressDrehtakt <= 0.f || mDrehZeit >= 1.f / UTDWaveSubsystem::StressDrehtakt - KINDA_SMALL_NUMBER;
	if (Ziel && mDreht && mYawMesh && mHeadMesh && !UTDWaveSubsystem::bStressDrehenAus && bDrehtakt)
	{
		const float MaxSchritt = mTurnSpeed * FMath::Min(mDrehZeit, 0.5f);
		mDrehZeit = 0.f;
		const FVector Lokal = GetActorTransform().InverseTransformPosition(Ziel->GetActorLocation());
		FRotator Gier = mYawMesh->GetRelativeRotation();
		Gier.Yaw = FMath::FixedTurn(Gier.Yaw, FMath::RadiansToDegrees(FMath::Atan2(Lokal.Y, Lokal.X)), MaxSchritt);
		mYawMesh->SetRelativeRotation(Gier);
		const FVector ImDrehteil = mYawMesh->GetComponentTransform().InverseTransformPosition(Ziel->GetActorLocation()) - mNeigeachse;
		const float SollNeigung = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(ImDrehteil.Z, FVector2D(ImDrehteil.X, ImDrehteil.Y).Size())), mNeigungMin, mNeigungMax);
		FRotator Neigung = mHeadMesh->GetRelativeRotation();
		Neigung.Pitch = FMath::FixedTurn(Neigung.Pitch, SollNeigung, MaxSchritt);
		mHeadMesh->SetRelativeRotation(Neigung);
	}
	else if (!Ziel)
	{
		mDrehZeit = 0.f;
	}
	if (!UTDWaveSubsystem::bStressEffekteAus)
	{
		Animieren(TurmZielIntervall, Ziel);
		TickeEffekte(TurmZielIntervall);
	}
}

void ATDTurretBase::DiagnoseUmgebung()
{
	const float Radius = 600.f;
	const FTransform Turm = GetActorTransform();
	int32 Anzahl = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A == this)
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Teile(A);
		for (const UPrimitiveComponent* P : Teile)
		{
			if (!P->IsRegistered() || !P->IsVisible() || P->bHiddenInGame)
			{
				continue;
			}
			const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(P);
			const FString Modell = SM && SM->GetStaticMesh() ? SM->GetStaticMesh()->GetName() : TEXT("-");
			if (Modell.StartsWith(TEXT("SM_DryGrass")) || Modell.StartsWith(TEXT("SM_CoverGround")) || Modell.StartsWith(TEXT("SM_Roots")))
			{
				continue;   // Bewuchs
			}
			if (const UInstancedStaticMeshComponent* ISM = Cast<UInstancedStaticMeshComponent>(P))
			{
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
				{
					FTransform T;
					if (ISM->GetInstanceTransform(i, T, true) && FVector::Dist(T.GetLocation(), Turm.GetLocation()) < Radius)
					{
						UE_LOG(LogFicsitTD, Log, TEXT("Umfeld %s: Instanz %s #%d (%s) lokal %s Drehung %s Skala %s"), *GetName(), *Modell, i, *A->GetClass()->GetName(),
							*Turm.InverseTransformPosition(T.GetLocation()).ToCompactString(), *Turm.InverseTransformRotation(T.GetRotation()).Rotator().ToCompactString(),
							*T.GetScale3D().ToCompactString());
						++Anzahl;
					}
				}
			}
			else if (FVector::Dist(P->Bounds.Origin, Turm.GetLocation()) < Radius + P->Bounds.SphereRadius && P->Bounds.SphereRadius < 2000.f)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Umfeld %s: %s.%s (%s) %s lokal %s Halbmass %s"), *GetName(), *A->GetClass()->GetName(), *P->GetName(),
					*P->GetClass()->GetName(), *Modell, *Turm.InverseTransformPosition(P->Bounds.Origin).ToCompactString(), *P->Bounds.BoxExtent.ToCompactString());
				++Anzahl;
			}
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Umfeld %s: %d sichtbare Teile im Umkreis von 6 m"), *GetName(), Anzahl);
}

void ATDTurretBase::AktualisiereLampe()
{
	// 0 aus (Standby), 1 rot (kein Strom), 2 gelb (kein Rezept/keine Munition), 3 gruen (bereit), 4 hellgruen (kaempft)
	int32 Zustand;
	if (IsProductionPaused())                         Zustand = 0;
	else if (RunsOnPower() && !HasPower())            Zustand = 1;
	else if (!GetCurrentRecipe() || !HatMunition())   Zustand = 2;
	else                                              Zustand = mAktuellesZiel.IsValid() ? 4 : 3;
	if (Zustand == mLampenZustand || !mLampenMaterial)
	{
		return;
	}
	mLampenZustand = Zustand;
	static const FLinearColor Farben[] = { FLinearColor(0.02f, 0.02f, 0.02f), FLinearColor(1.f, 0.03f, 0.01f),
		FLinearColor(1.f, 0.55f, 0.02f), FLinearColor(0.1f, 1.f, 0.1f), FLinearColor(0.4f, 1.f, 0.4f) };
	static const float Leuchtkraft[] = { 0.f, 20.f, 20.f, 15.f, 40.f };
	mLampenMaterial->SetVectorParameterValue(TEXT("Farbe"), Farben[Zustand]);
	mLampenMaterial->SetScalarParameterValue(TEXT("Leuchtkraft"), Leuchtkraft[Zustand]);
}

void ATDTurretBase::Spielschritt()
{
	TDLeistung::FUhr Uhr(TDLeistung::TurmZyklen);
	ZeigeStufenTeile();
	// Forschung Munitionspuffer: Eingang fasst die doppelte Stapelgroesse (einmal je Munitionssorte setzen)
	if (UFGInventoryComponent* Eingang = GetInputInventory())
	{
		const TSubclassOf<UFGRecipe> Rezept = GetCurrentRecipe();
		const TArray<FItemAmount> Zutaten = Rezept ? UFGRecipe::GetIngredients(this, Rezept) : TArray<FItemAmount>();
		UClass* Sorte = Zutaten.Num() > 0 ? Zutaten[0].ItemClass.Get() : nullptr;
		if (Sorte && mPufferFuer.Get() != Sorte && Eingang->GetSizeLinear() > 0 && UFGItemDescriptor::GetForm(Sorte) == EResourceForm::RF_SOLID
			&& TDForschung::Hat(this, TEXT("Munitionspuffer")))
		{
			mPufferFuer = Sorte;
			const int32 Groesse = UFGItemDescriptor::GetStackSize(Sorte) * FMath::Max(1, TDBalance::Ganz(TEXT("Forschung.Munitionspuffer.Faktor"), 2));
			Eingang->AddArbitrarySlotSize(0, Groesse);
			UE_LOG(LogFicsitTD, Log, TEXT("%s: Munitionspuffer %d %s"), *GetName(), Groesse, *UFGItemDescriptor::GetItemName(Sorte).ToString());
		}
	}
	const FVector Achse = mDreht && mHeadMesh ? mHeadMesh->GetComponentLocation() : GetMuzzleLocation();
	AFGCreature* Ziel = IsTDIntact() && GetCurrentRecipe() && !UTDWaveSubsystem::bStressTuermeAus ? SucheZiel(Achse) : nullptr;
	mAktuellesZiel = Ziel;
	mBereit.store(BereitZumProduzieren(Ziel));
	// Messwerte je Welle (Recherche 3.8): Uptime, Munitionsmangel, Stromausfall
	mBilanz.Messzeit += TurmSchrittIntervall;
	const bool bMunition = HatMunition();
	mBilanz.ZeitMitZiel += Ziel ? TurmSchrittIntervall : 0.f;
	mBilanz.ZeitOhneMunition += bMunition ? 0.f : TurmSchrittIntervall;
	mBilanz.ZeitHungrig += Ziel && !bMunition ? TurmSchrittIntervall : 0.f;
	mBilanz.ZeitOhneStrom += GetPowerInfo() && !HasPower() ? TurmSchrittIntervall : 0.f;
	AktualisiereLampe();

	// Abgeschlossene Produktionszyklen umsetzen (Zutat wurde dafuer bereits verbraucht)
	int32 Offen = mOffeneZyklen.exchange(0);
	while (Offen-- > 0)
	{
		Feuern(Ziel);
	}
}

void ATDTurretBase::SammleFeinde(TArray<AFGCreature*>& OutFeinde) const
{
	if (const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
	{
		OutFeinde.Append(Waves->GetLivingWaveCreatures());
	}
	// Auch wilde Kreaturen, die angreifen koennen (alle nicht-passiven AFGCreature)
	const FVector Mitte = GetActorLocation();
	const float Reichweite = FMath::Square(GetReichweite() + 1000.f);
	for (TActorIterator<AFGCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsPassiveCreature() && It->IsAliveAndWell() && FVector::DistSquared(It->GetActorLocation(), Mitte) < Reichweite)
		{
			OutFeinde.AddUnique(*It);
		}
	}
}

const TCHAR* ATDTurretBase::ZielModusName(int32 Modus)
{
	switch (Modus)
	{
	case 1:  return TEXT("Nächster zum Kern");
	case 2:  return TEXT("Wenigste LP");
	case 3:  return TEXT("Meiste LP");
	case 4:  return TEXT("Elite und Boss zuerst");
	default: return TEXT("Nächster zum Turm");
	}
}

const TArray<FString>& ATDTurretBase::ZielFamilien()
{
	static const TArray<FString> Liste = { TEXT(""), TEXT("Hog"), TEXT("Stinger"), TEXT("Spucker"), TEXT("Krabbe") };
	return Liste;
}

FString ATDTurretBase::FamilienName(const FString& Familie)
{
	if (Familie.IsEmpty()) return TEXT("Alle gleich");
	if (Familie == TEXT("Hog")) return TEXT("Hogs");
	if (Familie == TEXT("Krabbe")) return TEXT("Krabben");
	return Familie;
}

FString ATDTurretBase::FamilieVon(const AActor* Ziel)
{
	const FString Name = Ziel ? Ziel->GetClass()->GetName() : FString();
	if (Name.Contains(TEXT("Spitter"))) return TEXT("Spucker");
	if (Name.Contains(TEXT("Stinger"))) return TEXT("Stinger");
	if (Name.Contains(TEXT("Crab"))) return TEXT("Krabbe");
	if (Name.Contains(TEXT("Hog")) || Name.Contains(TEXT("Johnny"))) return TEXT("Hog");
	return FString();
}

void ATDTurretBase::SetzeZielwahl(int32 Modus, const FString& Familie)
{
	mZielModus = FMath::Clamp(Modus, 0, ZielModusAnzahl - 1);
	mZielFamilie = Familie;
	UE_LOG(LogFicsitTD, Log, TEXT("Zielwahl %s: %s, bevorzugt %s"), *GetName(), ZielModusName(mZielModus), *FamilienName(mZielFamilie));
}

AFGCreature* ATDTurretBase::SucheZiel(const FVector& Achse) const
{
	// Zielwahl (Marius 03.11.2026): Kandidaten in Reichweite bewerten, dann in dieser Reihenfolge die Sicht pruefen
	const float MaxDist = FMath::Square(GetReichweite());
	TArray<AFGCreature*> Feinde;
	SammleFeinde(Feinde);
	const UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	const AActor* Kern = Einsatz ? Einsatz->GetKern() : nullptr;
	TArray<TPair<double, AFGCreature*>> Kandidaten;
	for (AFGCreature* Creature : Feinde)
	{
		const float Dist = FVector::DistSquared(Creature->GetActorLocation(), Achse);
		if (Dist >= MaxDist || !ZielErlaubt(Creature, FMath::Sqrt(Dist)))
		{
			continue;
		}
		const UFGHealthComponent* Leben = Creature->GetHealthComponent();
		const double LP = Leben ? Leben->GetCurrentHealth() : 0.0;
		double Wert = Dist;
		switch (mZielModus)
		{
		case 1: Wert = Kern ? FVector::DistSquared(Creature->GetActorLocation(), Kern->GetActorLocation()) : Dist; break;
		case 2: Wert = LP * 1.0e8 + Dist; break;
		case 3: Wert = -LP * 1.0e8 + Dist; break;
		case 4: Wert = (Creature->Tags.Contains(TEXT("FicsitTDBoss")) ? 0.0 : Creature->Tags.Contains(TEXT("FicsitTDElite")) ? 1.0e12 : 2.0e12) + Dist; break;
		default: break;
		}
		if (!mZielFamilie.IsEmpty() && FamilieVon(Creature) != mZielFamilie)
		{
			Wert += 1.0e18;   // bevorzugte Familie immer zuerst, die anderen nur ohne sie
		}
		Kandidaten.Emplace(Wert, Creature);
	}
	Kandidaten.Sort([](const TPair<double, AFGCreature*>& A, const TPair<double, AFGCreature*>& B) { return A.Key < B.Key; });
	// Sichtpruefung ist ein Linientest: hoechstens 12 Kandidaten
	for (int32 i = 0; i < Kandidaten.Num() && i < 12; ++i)
	{
		if (FreieSicht(Achse, Kandidaten[i].Value))
		{
			return Kandidaten[i].Value;
		}
	}
	return nullptr;
}

/** 0.77 (Bewertung 05.11., Beobachtung T2: "Sicht verdeckt durch AbstractInstanceManager"): Lightweight-Bauteile (Fundamente,
 *  Rampen, Treppen, Traeger) liegen alle in einem Actor. Nur Mauern sollen die Sicht nehmen; der Rest wird uebergangen.
 *  Liefert true, wenn das Hindernis zaehlt; OutName = Name fuer Meldungen */
static bool TDHindernisZaehlt(const FHitResult& Hit, FString& OutName)
{
	const AActor* Getroffen = Hit.GetActor();
	OutName = Getroffen ? Getroffen->GetClass()->GetName() : FString(TEXT("Landschaft"));
	if (Getroffen && OutName.Contains(TEXT("AbstractInstanceManager")))
	{
		const UStaticMeshComponent* Teil = Cast<UStaticMeshComponent>(Hit.GetComponent());
		const FString Modell = Teil && Teil->GetStaticMesh() ? Teil->GetStaticMesh()->GetName() : FString();
		OutName = Modell.IsEmpty() ? OutName : Modell;
		return Modell.Contains(TEXT("Wall"));
	}
	return true;
}

static bool TDSichtFrei(const ATDTurretBase* Turm, const FVector& Von, const FVector& Nach, const AActor* Ziel, FString& OutHindernis)
{
	FCollisionQueryParams Query(TEXT("FicsitTDTurmSicht"), false, Turm);
	// Bis zu 8 Durchlaeufe: Hindernisse, die nicht zaehlen, werden uebergangen und die Linie weiter verfolgt
	for (int32 Versuch = 0; Versuch < 8; ++Versuch)
	{
		FHitResult Hit;
		if (!Turm->GetWorld()->LineTraceSingleByChannel(Hit, Von, Nach, ECC_Visibility, Query))
		{
			return true;
		}
		const AActor* Getroffen = Hit.GetActor();
		// Eigene Verteidigung (Mauern, Tuerme, Koeder) blockiert nicht
		if ((Ziel && Getroffen == Ziel) || Cast<ITDDamageable>(Getroffen) || Cast<ATDDecoy>(Getroffen))
		{
			return true;
		}
		// Test 0.27.1: andere Kreaturen (auch tote) und Logistik verdeckten das Ziel - der Turm hatte dann gar keins
		if (Getroffen && (Getroffen->IsA<AFGCreature>() || UTDEinsatzSubsystem::IstLogistik(Getroffen->GetClass())))
		{
			Query.AddIgnoredActor(Getroffen);
			continue;
		}
		if (!TDHindernisZaehlt(Hit, OutHindernis))
		{
			if (Hit.GetComponent())
			{
				Query.AddIgnoredComponent(Hit.GetComponent());
			}
			continue;
		}
		return false;
	}
	return true;
}

bool ATDTurretBase::FreieSicht(const FVector& Von, const AActor* Ziel) const
{
	FString Hindernis;
	return Ziel && TDSichtFrei(this, Von, Ziel->GetActorLocation(), Ziel, Hindernis);
}

bool ATDTurretBase::FreieSichtZuPunkt(const FVector& Nach, FString& OutHindernis) const
{
	const FVector Von = mDreht && mHeadMesh ? mHeadMesh->GetComponentLocation() : GetMuzzleLocation();
	return TDSichtFrei(this, Von, Nach, nullptr, OutHindernis);
}

const TCHAR* ATDTurretBase::SchadensartName(ETDSchadensart Art)
{
	switch (Art)
	{
	case ETDSchadensart::Kugel:     return TEXT("Kugel");
	case ETDSchadensart::Feuer:     return TEXT("Feuer");
	case ETDSchadensart::Strom:     return TEXT("Strom");
	case ETDSchadensart::Explosion: return TEXT("Explosion");
	case ETDSchadensart::Gift:      return TEXT("Gift");
	default:                        return TEXT("Durchschlag");
	}
}

float ATDTurretBase::Resistenz(ETDSchadensart Art, const AFGCreature* Ziel)
{
	if (!Ziel)
	{
		return 1.f;
	}
	// Faktor je Gegnerfamilie und Schadensart aus der Balance-Datei (Resistenzen.<Familie>.<Art>)
	const FString Name = Ziel->GetClass()->GetName();
	const TCHAR* Familie = nullptr;
	if (Name.Contains(TEXT("Aquatic")))                                   Familie = TEXT("WasserSpitter");
	else if (Name.Contains(TEXT("Desert")))                               Familie = TEXT("WuestenSpitter");
	else if (Name.Contains(TEXT("Spitter")))                              Familie = TEXT("Spitter");
	else if (Name.Contains(TEXT("Stinger")))                              Familie = TEXT("Stinger");
	else if (Name.Contains(TEXT("Crab")))                                 Familie = TEXT("Krabbe");
	else if (Name.Contains(TEXT("Hog")) || Name.Contains(TEXT("Johnny"))) Familie = TEXT("Hog");
	return Familie ? TDBalance::Wert(FString::Printf(TEXT("Resistenzen.%s.%s"), Familie, SchadensartName(Art)), 1.f) : 1.f;
}

FString ATDTurretBase::BilanzText(float Sekunden) const
{
	TArray<FString> Arten;
	for (const TPair<FString, FVector>& Je : mBilanz.JeGegner)
	{
		Arten.Add(FString::Printf(TEXT("%s %.0f (x%.2f, %.0f Treffer)"), *Je.Key, Je.Value.Y, Je.Value.X > 0.f ? Je.Value.Y / Je.Value.X : 1.f, Je.Value.Z));
	}
	const float Mess = FMath::Max(mBilanz.Messzeit, 0.1f);
	return FString::Printf(TEXT("%s [%s] Level %d, Munition %s: %d Schuss, %d Treffer, %.0f Schaden, %.1f Schaden/s ueber %.0f s, %d Abschuesse gesamt; je Gegner: %s; ")
		TEXT("Nenn %.0f DPS, Uptime %.0f %%, Overkill %.0f %%, Nebenziele %d, ohne Munition %.0f s (davon mit Ziel %.0f s), ohne Strom %.0f s"),
		*mTurmName, *GetName(), GetLevel(), *GetMunitionsName(), mBilanz.Schuesse + mSchuesse, mBilanz.Treffer, mBilanz.Schaden,
		Sekunden > 0.f ? mBilanz.Schaden / Sekunden : 0.f, Sekunden, mKills, Arten.Num() ? *FString::Join(Arten, TEXT(", ")) : TEXT("-"),
		NennDPS(), 100.f * mBilanz.ZeitMitZiel / Mess, mBilanz.Schaden > 0.f ? 100.f * mBilanz.Overkill / mBilanz.Schaden : 0.f, mBilanz.Nebentreffer,
		mBilanz.ZeitOhneMunition, mBilanz.ZeitHungrig, mBilanz.ZeitOhneStrom);
}

void ATDTurretBase::Treffer(AFGCreature* Ziel, float Schaden, const FVector& Richtung, float Betaeubung, float Verzoegerung)
{
	TrefferArt(Ziel, Schaden, Richtung, Betaeubung, Verzoegerung, mSchadensart);
}

void ATDTurretBase::TrefferArt(AFGCreature* Ziel, float Schaden, const FVector& Richtung, float Betaeubung, float Verzoegerung, ETDSchadensart Art)
{
	const float Roh = Schaden;
	Schaden *= Resistenz(Art, Ziel);
	TWeakObjectPtr<AFGCreature> WeakZiel = Ziel;
	TWeakObjectPtr<ATDTurretBase> WeakSelf = this;
	auto Wirkung = [WeakZiel, WeakSelf, Richtung, Schaden, Roh, Betaeubung]()
	{
		AFGCreature* Z = WeakZiel.Get();
		ATDTurretBase* T = WeakSelf.Get();
		if (!Z || !Z->IsAliveAndWell())
		{
			return;
		}
		if (T)
		{
			// Bilanz: Gegnerart = Klassenname ohne Char_/_C
			FString Art = Z->GetClass()->GetName();
			Art.RemoveFromStart(TEXT("Char_"));
			Art.RemoveFromEnd(TEXT("_C"));
			FVector& Je = T->mBilanz.JeGegner.FindOrAdd(Art);
			Je.X += Roh;
			Je.Y += Schaden;
			Je.Z += 1.f;
			T->mBilanz.Schaden += Schaden;
			++T->mBilanz.Treffer;
			T->mBilanz.Nebentreffer += Z != T->mAktuellesZiel.Get() ? 1 : 0;
		}
		const UFGHealthComponent* Leben = Z->GetHealthComponent();
		const float RestLP = Leben ? Leben->GetCurrentHealth() : Schaden;
		{
			TDLeistung::FUhr Uhr(TDLeistung::SchadenZyklen);
			FHitResult Hit;
			Hit.ImpactPoint = Z->GetActorLocation();
			UGameplayStatics::ApplyPointDamage(Z, Schaden, Richtung, Hit, nullptr, T, UDamageType::StaticClass());
			if (Betaeubung > 0.f && Z->IsAliveAndWell() && Z->CanBeStunned())
			{
				Z->BeginStun(Betaeubung);
			}
		}
		if (T)
		{
			T->mBilanz.Overkill += FMath::Max(0.f, Schaden - RestLP);
		}
		if (T && !Z->IsAliveAndWell() && !UTDWaveSubsystem::bStressSchutz)   // Stresstest-Gegner zaehlen nicht fuer die Turmstufe
		{
			++T->mKills;
		}
	};
	if (Verzoegerung <= 0.001f)
	{
		Wirkung();
		return;
	}
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Wirkung), Verzoegerung, false);
}

float ATDTurretBase::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!TDSchaden::VonGegner(DamageCauser, EventInstigator))
	{
		return 0.f;   // Giftwolken, Umgebung, eigene Waffen
	}
	ApplyTDDamage(DamageAmount, DamageCauser);
	return DamageAmount;
}

void ATDTurretBase::ApplyTDDamage(float Amount, const AActor* Causer)
{
	if (!HasAuthority() || !IsTDIntact() || Amount <= 0.f || Causer == this)
	{
		return;
	}
	mHealth = FMath::Max(0.f, mHealth - Amount);
	if (mHealth <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("%s %s zerstoert (Level %d, %d Abschuesse)"), *mTurmName, *GetName(), GetLevel(), mKills);
		if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
		{
			Einsatz->GebaeudeZerstoert(GetClass());
		}
		ATDDecoy::DestroyAll(mDecoys);
		TurnOffAndDestroy();
	}
}

// ---------------- Effekte (0.64) ----------------

ATDTurretBase::FTDEffekt* ATDTurretBase::HoleEffekt(bool bSpur)
{
	FTDEffekt* Aeltester = nullptr;
	int32 Anzahl = 0;
	for (FTDEffekt& E : mEffekte)
	{
		if (E.bSpur != bSpur || !E.K.IsValid())
		{
			continue;
		}
		++Anzahl;
		if (E.Rest <= 0.f && E.Warten <= 0.f)
		{
			return &E;
		}
		if (!Aeltester || E.Rest < Aeltester->Rest)
		{
			Aeltester = &E;
		}
	}
	if (Anzahl >= 24 && Aeltester)
	{
		return Aeltester;
	}
	UStaticMeshComponent* K = NewObject<UStaticMeshComponent>(this);
	K->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, bSpur ? TEXT("/FicsitTD/Effects/SM_TD_Lightning.SM_TD_Lightning") : TEXT("/FicsitTD/Effects/SM_TD_MuzzleFlash.SM_TD_MuzzleFlash")));
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, bSpur ? TEXT("/FicsitTD/Effects/M_TD_SpurFarbe.M_TD_SpurFarbe") : TEXT("/FicsitTD/Effects/M_TD_EffektFarbe.M_TD_EffektFarbe")))
	{
		K->SetMaterial(0, Mat);
	}
	K->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	K->SetCanEverAffectNavigation(false);
	K->SetCastShadow(false);
	K->SetUsingAbsoluteLocation(true);
	K->SetUsingAbsoluteRotation(true);
	K->SetUsingAbsoluteScale(true);
	K->SetupAttachment(GetRootComponent());
	K->RegisterComponent();
	K->SetVisibility(false);
	UMaterialInstanceDynamic* M = K->CreateDynamicMaterialInstance(0);
	mEffektObjekte.Add(K);
	mEffektObjekte.Add(M);
	FTDEffekt& Neu = mEffekte.AddDefaulted_GetRef();
	Neu.K = K;
	Neu.M = M;
	Neu.bSpur = bSpur;
	return &Neu;
}

void ATDTurretBase::ZeigeSpur(const FVector& Von, const FVector& Nach, const FLinearColor& Farbe, float Dauer, float Dicke, float Verzoegerung)
{
	if (UTDWaveSubsystem::bStressEffekteAus || FVector::DistSquared(Von, Nach) < 1.f)
	{
		return;
	}
	FTDEffekt* E = HoleEffekt(true);
	UStaticMeshComponent* K = E ? E->K.Get() : nullptr;
	if (!K)
	{
		return;
	}
	// Modell: 1 m lang entlang +X -> auf die Strecke strecken
	K->SetWorldLocationAndRotation(Von, (Nach - Von).Rotation());
	K->SetWorldScale3D(FVector(FVector::Dist(Von, Nach) / 100.f, Dicke, Dicke));
	if (UMaterialInstanceDynamic* M = E->M.Get())
	{
		M->SetVectorParameterValue(TEXT("Farbe"), Farbe);
	}
	E->Warten = Verzoegerung;
	E->Rest = FMath::Max(Dauer, 0.02f);
	E->Dauer = E->Rest;
	E->Staerke = 30.f;
	E->bPuls = false;
	K->SetVisibility(Verzoegerung <= 0.f);
}

void ATDTurretBase::ZeigeLeuchten(const FVector& Ort, const FLinearColor& Farbe, float Groesse, float Dauer, float Verzoegerung, bool bPuls, float Staerke)
{
	if (UTDWaveSubsystem::bStressEffekteAus)
	{
		return;
	}
	FTDEffekt* E = HoleEffekt(false);
	UStaticMeshComponent* K = E ? E->K.Get() : nullptr;
	if (!K)
	{
		return;
	}
	K->SetWorldLocationAndRotation(Ort, FRotator(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
	K->SetWorldScale3D(FVector(Groesse));
	if (UMaterialInstanceDynamic* M = E->M.Get())
	{
		M->SetVectorParameterValue(TEXT("Farbe"), Farbe);
		M->SetScalarParameterValue(TEXT("Staerke"), Staerke);
	}
	E->Warten = Verzoegerung;
	E->Rest = FMath::Max(Dauer, 0.02f);
	E->Dauer = E->Rest;
	E->Staerke = Staerke;
	E->bPuls = bPuls;
	K->SetVisibility(Verzoegerung <= 0.f);
}

void ATDTurretBase::TickeEffekte(float DeltaSeconds)
{
	const float Zeit = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (FTDEffekt& E : mEffekte)
	{
		UStaticMeshComponent* K = E.K.Get();
		if (!K || (E.Rest <= 0.f && E.Warten <= 0.f))
		{
			continue;
		}
		if (E.Warten > 0.f)
		{
			E.Warten -= DeltaSeconds;
			if (E.Warten > 0.f)
			{
				continue;
			}
			K->SetVisibility(true);
		}
		E.Rest -= DeltaSeconds;
		if (E.Rest <= 0.f)
		{
			K->SetVisibility(false);
			continue;
		}
		const float Anteil = FMath::Clamp(E.Rest / FMath::Max(E.Dauer, 0.01f), 0.f, 1.f);
		float Staerke = E.Staerke * (E.bPuls ? (0.65f + 0.35f * FMath::Sin(Zeit * 3.f + K->GetComponentLocation().X * 0.01f)) * FMath::Min(1.f, Anteil * 4.f) : Anteil);
		if (E.bSpur)
		{
			FRotator R = K->GetComponentRotation();
			R.Roll = FMath::FRandRange(0.f, 360.f);
			K->SetWorldRotation(R);
			Staerke *= FMath::FRandRange(0.7f, 1.2f);
		}
		else if (E.bPuls)
		{
			K->AddWorldRotation(FRotator(0.f, 20.f * DeltaSeconds, 0.f));
		}
		else
		{
			K->SetWorldRotation(FRotator(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f)));
		}
		if (UMaterialInstanceDynamic* M = E.M.Get())
		{
			M->SetScalarParameterValue(TEXT("Staerke"), Staerke);
		}
	}
}
