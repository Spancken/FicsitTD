#include "TDPionier.h"
#include "TDGegnerHirn.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDWaveSubsystem.h"
#include "TDTurretBase.h"
#include "Creature/FGCreature.h"
#include "Equipment/FGWeapon.h"
#include "EngineUtils.h"
#include "FGCharacterMovementComponent.h"
#include "FGCharacterPlayer.h"
#include "FGChatManager.h"
#include "FGHealthComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PointLightComponent.h"
#include "TimerManager.h"

namespace TDPionierWerte
{
	// Zweig-Indizes
	static constexpr int32 Armierpistole = 0, Gewehr = 1, Nobelisk = 2, Ueberleben = 3, Mobilitaet = 4;
}

/** 0.85 (L1): Talenttabelle je Sprache. Key und Braucht (Laden-Schluessel) sind in beiden gleich und bleiben deutsch,
 *  nur Name und Stufen-Texte sind uebersetzt. Das Log nimmt immer die deutsche Tabelle. */
static const TArray<FTDTalentZweig>& TDZweigeDe()
{
	static const TArray<FTDTalentZweig> Liste = {
		{ TEXT("Armierpistole"), TEXT("Armierpistole"), TEXT("Armierpistole"),
			{ TEXT("Schaden +25 %"), TEXT("Schaden +25 %"), TEXT("Nachladen 30 % schneller"), TEXT("Schaden +25 %"),
			  TEXT("Durchschlag: jeder Treffer trifft den nächsten Gegner dahinter mit halbem Schaden") } },
		{ TEXT("Gewehr"), TEXT("Gewehr"), TEXT("Gewehr"),
			{ TEXT("Schaden +20 %"), TEXT("Schaden +20 %"), TEXT("Nachladen 30 % schneller"), TEXT("Schaden +20 %"),
			  TEXT("Sprengmantel: jeder 5. Treffer explodiert (120 Schaden in 4 m)") } },
		{ TEXT("Nobelisk"), TEXT("Nobelisk"), TEXT("Nobelisken-Zünder"),
			{ TEXT("Schaden +25 %"), TEXT("Schaden +25 %"), TEXT("Schaden +25 %"), TEXT("Schaden +25 %"),
			  TEXT("Kettenexplosion: Gegner in 8 m um das Ziel nehmen halben Schaden") } },
		{ TEXT("Ueberleben"), TEXT("Überleben"), nullptr,
			{ TEXT("+25 Lebenspunkte"), TEXT("+25 Lebenspunkte"), TEXT("Regeneration: 1 LP je Sekunde"), TEXT("Rüstung: 25 % weniger Schaden von Kreaturen"),
			  TEXT("Zweite Chance: einmal je Welle bei unter 25 % LP voll geheilt") } },
		{ TEXT("Mobilitaet"), TEXT("Mobilität"), nullptr,
			{ TEXT("Lauftempo +10 %"), TEXT("Lauftempo +10 %"), TEXT("Sprunghöhe +25 %"), TEXT("Sprinttempo +20 %"),
			  TEXT("Sprinter: Sprinttempo +20 %, Sprunghöhe +25 %") } },
	};
	return Liste;
}

static const TArray<FTDTalentZweig>& TDZweigeEn()
{
	static const TArray<FTDTalentZweig> Liste = {
		{ TEXT("Armierpistole"), TEXT("Rebar Gun"), TEXT("Armierpistole"),
			{ TEXT("Damage +25 %"), TEXT("Damage +25 %"), TEXT("Reload 30 % faster"), TEXT("Damage +25 %"),
			  TEXT("Piercing: every hit also strikes the next enemy behind the target for half damage") } },
		{ TEXT("Gewehr"), TEXT("Rifle"), TEXT("Gewehr"),
			{ TEXT("Damage +20 %"), TEXT("Damage +20 %"), TEXT("Reload 30 % faster"), TEXT("Damage +20 %"),
			  TEXT("Explosive rounds: every 5th hit explodes (120 damage in 4 m)") } },
		{ TEXT("Nobelisk"), TEXT("Nobelisk"), TEXT("Nobelisken-Zünder"),
			{ TEXT("Damage +25 %"), TEXT("Damage +25 %"), TEXT("Damage +25 %"), TEXT("Damage +25 %"),
			  TEXT("Chain explosion: enemies within 8 m of the target take half damage") } },
		{ TEXT("Ueberleben"), TEXT("Survival"), nullptr,
			{ TEXT("+25 health"), TEXT("+25 health"), TEXT("Regeneration: 1 HP per second"), TEXT("Armor: 25 % less damage from creatures"),
			  TEXT("Second chance: fully healed once per wave when below 25 % HP") } },
		{ TEXT("Mobilitaet"), TEXT("Mobility"), nullptr,
			{ TEXT("Walk speed +10 %"), TEXT("Walk speed +10 %"), TEXT("Jump height +25 %"), TEXT("Sprint speed +20 %"),
			  TEXT("Sprinter: sprint speed +20 %, jump height +25 %") } },
	};
	return Liste;
}

const TArray<FTDTalentZweig>& UTDPionier::Zweige()
{
	return TDSprache::Englisch() ? TDZweigeEn() : TDZweigeDe();
}

/** Anzeigename eines Laden-Postens, den ein Waffen-Zweig braucht (der Schluessel selbst bleibt deutsch) */
static FString TDPostenAnzeige(const FString& Posten)
{
	if (!TDSprache::Englisch())
	{
		return Posten;
	}
	if (Posten == TEXT("Armierpistole")) return TEXT("Rebar Gun");
	if (Posten == TEXT("Gewehr")) return TEXT("Rifle");
	if (Posten == TEXT("Nobelisken-Zünder")) return TEXT("Nobelisk Detonator");
	return Posten;
}

UTDPionier* UTDPionier::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTDPionier>() : nullptr;
}

void UTDPionier::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(mTimer, FTimerDelegate::CreateUObject(this, &UTDPionier::Wirke), 0.5f, true, 2.f);
}

ATDDefenseCore* UTDPionier::Kern() const
{
	const UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	return Einsatz && Einsatz->IstAktiv() ? Einsatz->GetKern() : nullptr;
}

int32 UTDPionier::GetXP() const
{
	const ATDDefenseCore* K = Kern();
	return K ? K->mPionierXP : 0;
}

int32 UTDPionier::MaxRang()
{
	return FMath::Clamp(TDBalance::Ganz(TEXT("Pionier.MaxRang"), 30), 2, 99);
}

int32 UTDPionier::XPFuerRang(int32 Rang)
{
	// Schritt je Rang waechst: Rang 2 bei 100, 3 bei 250, 4 bei 450, 5 bei 700 (Pionier.XPGrund + Pionier.XPSchritt x Rang)
	const int32 Grund = TDBalance::Ganz(TEXT("Pionier.XPGrund"), 50);
	const int32 Schritt = TDBalance::Ganz(TEXT("Pionier.XPSchritt"), 50);
	int32 Summe = 0;
	for (int32 r = 1; r < Rang; ++r)
	{
		Summe += Grund + Schritt * r;
	}
	return Summe;
}

int32 UTDPionier::GetRang() const
{
	const int32 XP = GetXP();
	int32 Rang = 1;
	while (Rang < MaxRang() && XP >= XPFuerRang(Rang + 1))
	{
		++Rang;
	}
	return Rang;
}

int32 UTDPionier::GetStufe(int32 Zweig) const
{
	const ATDDefenseCore* K = Kern();
	return K && Zweige().IsValidIndex(Zweig) ? K->mTalente.FindRef(Zweige()[Zweig].Key) : 0;
}

int32 UTDPionier::FreiePunkte() const
{
	int32 Vergeben = 0;
	for (int32 i = 0; i < Zweige().Num(); ++i)
	{
		Vergeben += GetStufe(i);
	}
	return FMath::Max(0, GetRang() - 1 - Vergeben);
}

FString UTDPionier::FehltFuer(int32 Zweig) const
{
	const ATDDefenseCore* K = Kern();
	if (!K || !Zweige().IsValidIndex(Zweig) || !Zweige()[Zweig].Braucht)
	{
		return FString();
	}
	const FString Posten = Zweige()[Zweig].Braucht;
	return K->mKontingentGekauft.FindRef(Posten) > 0 ? FString() : Posten;
}

FString UTDPionier::Lerne(int32 Zweig)
{
	ATDDefenseCore* K = Kern();
	if (!K || !Zweige().IsValidIndex(Zweig))
	{
		return TDT(TEXT("Nur im Einsatz."), TEXT("Only during an operation."));
	}
	const FTDTalentZweig& Z = Zweige()[Zweig];
	const int32 Stufe = GetStufe(Zweig);
	if (Stufe >= MaxStufe)
	{
		return TDF(TEXT("%s ist schon voll ausgebaut."), TEXT("%s is already fully upgraded."), Z.Name);
	}
	const FString Fehlt = FehltFuer(Zweig);
	if (!Fehlt.IsEmpty())
	{
		return TDF(TEXT("%s: erst %s im Laden kaufen."), TEXT("%s: buy the %s in the Shop first."), Z.Name, *TDPostenAnzeige(Fehlt));
	}
	if (FreiePunkte() <= 0)
	{
		return TDT(TEXT("Kein Talentpunkt frei. Nächster Rang bringt einen."), TEXT("No Talent Point free. The next rank gives you one."));
	}
	K->mTalente.FindOrAdd(Z.Key) = Stufe + 1;
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Talent %s Stufe %d (%s), Rang %d"), Z.Key, Stufe + 1, TDZweigeDe()[Zweig].Stufen[Stufe], GetRang());
	Wirke();
	return TDF(TEXT("%s Stufe %d: %s"), TEXT("%s level %d: %s"), Z.Name, Stufe + 1, Z.Stufen[Stufe]);
}

int32 UTDPionier::NeuVerteilenPreis()
{
	return TDBalance::Ganz(TEXT("Pionier.NeuVerteilenPreis"), 20);
}

FString UTDPionier::NeuVerteilen()
{
	ATDDefenseCore* K = Kern();
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (!K || !Einsatz)
	{
		return TDT(TEXT("Nur im Einsatz."), TEXT("Only during an operation."));
	}
	if (K->mTalente.Num() == 0)
	{
		return TDT(TEXT("Es sind noch keine Talente vergeben."), TEXT("No talents assigned yet."));
	}
	const FString Fehler = Einsatz->ZahleEP(NeuVerteilenPreis(), TEXT("Talente neu verteilen"), true);
	if (!Fehler.IsEmpty())
	{
		return Fehler;
	}
	K->mTalente.Reset();
	Wirke();
	return TDF(TEXT("Talente zurückgesetzt, %d Punkte frei."), TEXT("Talents reset, %d points free."), FreiePunkte());
}

void UTDPionier::Melde(const FString& Text) const
{
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: %s"), *Text);
	if (AFGChatManager* Chat = AFGChatManager::Get(GetWorld()))
	{
		FChatMessageStruct Nachricht;
		Nachricht.MessageText = FText::FromString(Text);
		Nachricht.MessageType = EFGChatMessageType::CMT_CustomMessage;
		Nachricht.MessageSender = TDTX(TEXT("PIONIER"), TEXT("PIONEER"));
		Nachricht.MessageSenderColor = FLinearColor(0.45f, 0.8f, 1.f);
		Chat->BroadcastChatMessage(Nachricht);
	}
}

void UTDPionier::GibXP(int32 XP, const FString& Grund)
{
	ATDDefenseCore* K = Kern();
	if (!K || XP <= 0)
	{
		return;
	}
	const int32 Vorher = GetRang();
	K->mPionierXP += XP;
	const int32 Nachher = GetRang();
	if (Nachher > Vorher)
	{
		Melde(TDF(TEXT("Rang %d erreicht! %d Talentpunkt%s frei: Kern-Menü, Reiter Pionier."), TEXT("Rank %d reached! %d Talent Point%s free: Core Menu, Pioneer tab."),
			Nachher, FreiePunkte(), FreiePunkte() == 1 ? TEXT("") : *TDT(TEXT("e"), TEXT("s"))));
	}
	UE_LOG(LogFicsitTD, Verbose, TEXT("Pionier: +%d XP (%s), %d XP, Rang %d"), XP, *Grund, K->mPionierXP, Nachher);
}

void UTDPionier::WelleBeendet(int32 Welle, float KernAnteil)
{
	int32 XP = TDBalance::Ganz(TEXT("Pionier.XPWelle"), 40) + TDBalance::Ganz(TEXT("Pionier.XPJeWelle"), 6) * Welle
		+ (KernAnteil >= 99.5f ? TDBalance::Ganz(TEXT("Pionier.XPKernVoll"), 20) : 0);
	if (Welle % 10 == 0)
	{
		XP *= 2;
	}
	GibXP(XP, FString::Printf(TEXT("Welle %d"), Welle));
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Welle %d +%d XP, jetzt %d XP, Rang %d, %d Punkte frei"), Welle, XP, GetXP(), GetRang(), FreiePunkte());
}

void UTDPionier::WellenStart()
{
	mZweiteChanceGenutzt = false;
	mGewehrTreffer = 0;
	mBilanz = FSpielerBilanz();
	RaeumeMarken(true);
}

// ---------------- 0.9.7 P2: Zielmarkierung und Jaeger (Analyse-Spieler-und-Pionier 3.2) ----------------

void UTDPionier::Markiere(AActor* Gegner)
{
	if (!Gegner || !GetWorld() || TDBalance::Wert(TEXT("Pionier.Markierung.An"), 1.f) < 0.5f)
	{
		return;
	}
	const float Jetzt = GetWorld()->GetTimeSeconds();
	FMarke& M = mMarken.FindOrAdd(Gegner);
	if (M.Bis < Jetzt)
	{
		++mBilanz.Markiert;
		if (mBilanz.Markiert == 1)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Zielmarkierung auf %s (%.0f s, Turmschaden x%.2f)"), *Gegner->GetName(),
				TDBalance::Wert(TEXT("Pionier.Markierung.Dauer"), 6.f), TDBalance::Wert(TEXT("Pionier.Markierung.Bonus"), 1.15f));
		}
	}
	M.Bis = Jetzt + TDBalance::Wert(TEXT("Pionier.Markierung.Dauer"), 6.f);
	// Sichtbar: blaues Licht ueber dem Gegner (Spielschrift hat keine Symbole, ein Licht sieht man auch im Gedraenge); hoechstens 24 gleichzeitig
	if (!M.Licht.IsValid() && TDBalance::Wert(TEXT("Pionier.Markierung.Licht"), 1.f) > 0.5f && Gegner->GetRootComponent())
	{
		int32 Lichter = 0;
		for (const TPair<TWeakObjectPtr<AActor>, FMarke>& Je : mMarken)
		{
			Lichter += Je.Value.Licht.IsValid() ? 1 : 0;
		}
		if (Lichter < 24)
		{
			UPointLightComponent* Licht = NewObject<UPointLightComponent>(Gegner);
			Licht->SetupAttachment(Gegner->GetRootComponent());
			Licht->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
			Licht->SetLightColor(FLinearColor(0.45f, 0.8f, 1.f));
			Licht->SetIntensity(8000.f);
			Licht->SetAttenuationRadius(350.f);
			Licht->SetCastShadows(false);
			Licht->RegisterComponent();
			M.Licht = Licht;
		}
	}
}

void UTDPionier::RaeumeMarken(bool bAlle)
{
	const float Jetzt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	for (auto It = mMarken.CreateIterator(); It; ++It)
	{
		if (bAlle || !It.Key().IsValid() || It.Value().Bis < Jetzt)
		{
			if (UPointLightComponent* Licht = It.Value().Licht.Get())
			{
				Licht->DestroyComponent();
			}
			It.RemoveCurrent();
		}
	}
}

bool UTDPionier::IstMarkiert(const AActor* Gegner)
{
	const UTDPionier* P = Gegner ? Get(Gegner) : nullptr;
	if (!P || P->mMarken.Num() == 0)
	{
		return false;
	}
	const FMarke* M = P->mMarken.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Gegner)));
	return M && M->Bis >= Gegner->GetWorld()->GetTimeSeconds();
}

float UTDPionier::MarkierungsFaktor(const AActor* Gegner, float Schaden)
{
	if (!IstMarkiert(Gegner))
	{
		return 1.f;
	}
	const float Faktor = FMath::Max(TDBalance::Wert(TEXT("Pionier.Markierung.Bonus"), 1.15f), 1.f);
	if (UTDPionier* P = Get(Gegner))
	{
		P->mBilanz.MarkBonus += Schaden * (Faktor - 1.f);
	}
	return Faktor;
}

void UTDPionier::EntferneMarke(const AActor* Gegner)
{
	const TWeakObjectPtr<AActor> Schluessel(const_cast<AActor*>(Gegner));
	if (FMarke* M = mMarken.Find(Schluessel))
	{
		if (UPointLightComponent* Licht = M->Licht.Get())
		{
			Licht->DestroyComponent();
		}
		mMarken.Remove(Schluessel);
	}
}

bool UTDPionier::VomSpielerErlegt(const AActor* Gegner)
{
	const UTDPionier* P = Gegner ? Get(Gegner) : nullptr;
	const double* Zeit = P ? P->mSpielerTreffer.Find(Gegner) : nullptr;
	return Zeit && FPlatformTime::Seconds() - *Zeit <= 5.0;
}

FString UTDPionier::WellenZeile(float TurmSchaden) const
{
	if (mBilanz.Treffer == 0)
	{
		return FString();
	}
	const float Anteil = 100.f * mBilanz.Schaden / FMath::Max(mBilanz.Schaden + TurmSchaden, 1.f);
	FString Text = TDF(TEXT(" Du: %.0f Schaden (%.0f %%), %d Abschüsse"), TEXT(" You: %.0f damage (%.0f %%), %d kills"), mBilanz.Schaden, Anteil, mBilanz.Abschuesse);
	if (mBilanz.Eliten + mBilanz.Bosse > 0)
	{
		Text += TDF(TEXT(", davon %d Elite/Boss"), TEXT(", %d of them elite/boss"), mBilanz.Eliten + mBilanz.Bosse);
	}
	if (mBilanz.Assists > 0 || mBilanz.MarkBonus >= 1.f)
	{
		Text += TDF(TEXT(", %d Assists (Markierung +%.0f Turmschaden)"), TEXT(", %d assists (marking +%.0f turret damage)"), mBilanz.Assists, mBilanz.MarkBonus);
	}
	if (mBilanz.XP > 0)
	{
		Text += FString::Printf(TEXT(", +%d XP"), mBilanz.XP);
	}
	return Text + TEXT(".");
}

float UTDPionier::HandFaktor(int32 W, const AActor* Verursacher, const AActor* Getroffen, FString& OutMunition, float& OutSkalierung, float& OutResistenz) const
{
	// Geschoss- und Waffenklassen bis zum Spieler: daran erkennt man die Munition (Spielklassen, im Log je Treffer gemeldet)
	FString Klassen;
	for (const AActor* A = Verursacher; A; A = A->GetOwner())
	{
		Klassen += A->GetClass()->GetName() + TEXT(" ");
		if (A->IsA<APawn>())
		{
			break;
		}
	}
	const TCHAR* Waffe = TEXT("Armierpistole");
	const TCHAR* Schluessel = TEXT("Munition.Bolzen.Armierungsbolzen.Schaden");
	ETDSchadensart Art = ETDSchadensart::Durchschlag;
	float Spielwert = 15.f;
	if (W == TDPionierWerte::Armierpistole)
	{
		if (Klassen.Contains(TEXT("Stun")))                                        { Schluessel = TEXT("Munition.Bolzen.Betaeubungsbolzen.Schaden"); Art = ETDSchadensart::Strom; }
		else if (Klassen.Contains(TEXT("Spread")) || Klassen.Contains(TEXT("Shatter"))) { Schluessel = TEXT("Munition.Bolzen.Streubolzen.Schaden"); }
		else if (Klassen.Contains(TEXT("Explosive")))                              { Schluessel = TEXT("Munition.Bolzen.Explosivbolzen.Schaden"); Art = ETDSchadensart::Explosion; }
	}
	else if (W == TDPionierWerte::Gewehr)
	{
		Waffe = TEXT("Gewehr");
		Spielwert = 5.f;
		Art = ETDSchadensart::Kugel;
		Schluessel = TEXT("Munition.Patronen.Standard.Schaden");
		if (Klassen.Contains(TEXT("Smart")) || Klassen.Contains(TEXT("Homing")))  { Schluessel = TEXT("Munition.Patronen.Zielsuch.Schaden"); }
		else if (Klassen.Contains(TEXT("Chaos")) || Klassen.Contains(TEXT("Turbo"))) { Schluessel = TEXT("Munition.Patronen.Turbo.Schaden"); }
	}
	else
	{
		Waffe = TEXT("Nobelisk");
		Spielwert = 50.f;
		Art = ETDSchadensart::Explosion;
		Schluessel = TEXT("Munition.Nobelisk.Nobelisk.Schaden");
		if (Klassen.Contains(TEXT("Gas")))                                         { Schluessel = TEXT("Munition.Nobelisk.Gas.Schaden"); Art = ETDSchadensart::Gift; }
		else if (Klassen.Contains(TEXT("Cluster")))                                { Schluessel = TEXT("Munition.Nobelisk.Streu.Schaden"); }
		else if (Klassen.Contains(TEXT("Shockwave")) || Klassen.Contains(TEXT("Pulse"))) { Schluessel = TEXT("Munition.Nobelisk.Puls.Schaden"); }
		else if (Klassen.Contains(TEXT("Nuke")))                                   { Schluessel = TEXT("Munition.Nobelisk.Nuke.Schaden"); }
	}
	OutMunition = FString(Schluessel).Replace(TEXT("Munition."), TEXT("")).Replace(TEXT(".Schaden"), TEXT(""));
	const float Tabelle = TDBalance::Wert(Schluessel, 0.f);
	// Spielwert je Waffe (Wiki, im Log je Treffer nachpruefen): Verhaeltnis statt Ersatz, damit Kopftreffer und Fallabfall erhalten bleiben
	Spielwert = TDBalance::Wert(FString::Printf(TEXT("Pionier.Spielwert.%s"), Waffe), Spielwert);
	const UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const float Wachstum = Waves && Waves->LaeuftWelle() ? Waves->GetWellenWachstum() : 1.f;
	OutSkalierung = 1.f + TDBalance::Wert(TEXT("Pionier.WellenAnteil"), 0.5f) * FMath::Max(0.f, Wachstum - 1.f);
	OutResistenz = ATDTurretBase::Resistenz(Art, Cast<AFGCreature>(Getroffen));
	if (Tabelle <= 0.f || Spielwert <= 0.f)
	{
		return OutSkalierung * OutResistenz;
	}
	return Tabelle / Spielwert * TDBalance::Wert(FString::Printf(TEXT("Pionier.HandFaktor.%s"), Waffe), 1.f) * OutSkalierung * OutResistenz;
}

void UTDPionier::Abschuss(const AFGCreature* Kreatur)
{
	const double* Zeit = mSpielerTreffer.Find(Kreatur);
	if (!Zeit || FPlatformTime::Seconds() - *Zeit > 5.0)
	{
		// 0.9.7 P2: ein Turm hat einen markierten Gegner erledigt -> Assist
		if (IstMarkiert(Kreatur))
		{
			const int32 XP = TDBalance::Ganz(TEXT("Pionier.XPAssist"), 1);
			++mBilanz.Assists;
			mBilanz.XP += FMath::Max(XP, 0);
			GibXP(XP, TEXT("Assist"));
			if (mBilanz.Assists == 1)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Pionier: erster Assist der Welle (%s, markiert, Turm-Abschuss)"), *Kreatur->GetName());
			}
		}
		EntferneMarke(Kreatur);
		return;
	}
	mSpielerTreffer.Remove(Kreatur);
	EntferneMarke(Kreatur);
	const bool bBoss = Kreatur->Tags.Contains(TEXT("FicsitTDBoss"));
	const bool bElite = Kreatur->Tags.Contains(TEXT("FicsitTDElite"));
	const int32 XP = bBoss ? TDBalance::Ganz(TEXT("Pionier.XPAbschussBoss"), 50) : bElite ? TDBalance::Ganz(TEXT("Pionier.XPAbschussElite"), 10)
		: TDBalance::Ganz(TEXT("Pionier.XPAbschuss"), 3);
	GibXP(XP, TEXT("Abschuss"));
	++mBilanz.Abschuesse;
	mBilanz.Eliten += bElite ? 1 : 0;
	mBilanz.Bosse += bBoss ? 1 : 0;
	mBilanz.XP += XP;
}

int32 UTDPionier::Waffe(const AActor* Verursacher)
{
	for (const AActor* A = Verursacher; A; A = A->GetOwner())
	{
		const FString Name = A->GetClass()->GetName();
		const int32 W = Name.Contains(TEXT("Rebar")) ? 0 : (Name.Contains(TEXT("Rifle")) || Name.Contains(TEXT("Cartridge"))) ? 1 : Name.Contains(TEXT("Nobelisk")) ? 2 : -1;
		if (!mGemeldeteKlassen.Contains(Name))
		{
			mGemeldeteKlassen.Add(Name);
			UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Spielertreffer durch %s -> Waffe %d"), *Name, W);
		}
		if (W >= 0)
		{
			return W;
		}
		if (A->IsA<APawn>())
		{
			break;
		}
	}
	return -1;
}

float UTDPionier::SpielerTreffer(AActor* Getroffen, AActor* Verursacher, float Menge)
{
	mSpielerTreffer.Add(Getroffen, FPlatformTime::Seconds());
	// 0.9.7 P2: jeder Spielertreffer markiert das Ziel fuer die Tuerme
	Markiere(Getroffen);
	// 0.9.7 P2 Jaeger: Spielerschaden an Eliten und Bossen hoeher
	const bool bJagd = Getroffen && (Getroffen->Tags.Contains(TEXT("FicsitTDElite")) || Getroffen->Tags.Contains(TEXT("FicsitTDBoss")));
	const float Jaeger = bJagd ? FMath::Max(TDBalance::Wert(TEXT("Pionier.Jaeger.Faktor"), 1.5f), 0.f) : 1.f;
	if (mSpielerTreffer.Num() > 300)
	{
		for (auto It = mSpielerTreffer.CreateIterator(); It; ++It)
		{
			if (!It->Key.IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	const int32 W = Waffe(Verursacher);
	if (W < 0)
	{
		// unbekannte Waffe (z. B. Xeno-Basher): nur zaehlen
		mBilanz.Schaden += Menge * Jaeger;
		++mBilanz.Treffer;
		return Jaeger;
	}
	const int32 Stufe = GetStufe(W);
	// 0.9.6 P1: Munitionswert der Tuerme statt Spielwert, mit Wellenzuwachs und Resistenz
	FString Munition;
	float Skalierung = 1.f, Resistenz = 1.f;
	const float Hand = HandFaktor(W, Verursacher, Getroffen, Munition, Skalierung, Resistenz);
	float Faktor = 1.f;
	if (W == TDPionierWerte::Armierpistole)
	{
		Faktor = 1.f + 0.25f * (FMath::Min(Stufe, 2) + (Stufe >= 4 ? 1 : 0));
	}
	else if (W == TDPionierWerte::Gewehr)
	{
		Faktor = 1.f + 0.2f * (FMath::Min(Stufe, 2) + (Stufe >= 4 ? 1 : 0));
	}
	else
	{
		Faktor = 1.f + 0.25f * FMath::Min(Stufe, 4);
	}
	const float Talent = Faktor;
	Faktor *= Hand * Jaeger;
	mBilanz.Schaden += Menge * Faktor;
	++mBilanz.Treffer;
	// 0.9.6 P0: je Geschoss und Munition einmal die Rechnung ins Log (bestaetigt oder widerlegt die Wiki-Werte 15 / 5 / 50)
	{
		const FString Klasse = Verursacher ? Verursacher->GetClass()->GetName() : FString(TEXT("-"));
		if (!mGemeldeteTreffer.Contains(Klasse + Munition))
		{
			mGemeldeteTreffer.Add(Klasse + Munition);
			UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Handtreffer %s (%s): Spielschaden %.1f -> %.1f, Faktor %.2f (Munition/Spielwert %.2f, Welle x%.2f, Resistenz %.2f, Talent %.2f, Jaeger %.2f)"),
				*Klasse, *Munition, Menge, Menge * Faktor, Faktor, Hand / FMath::Max(Skalierung * Resistenz, 0.01f), Skalierung, Resistenz, Talent, Jaeger);
		}
	}
	// Abschluss-Talente: Zusatzschaden an Nachbarn im naechsten Bild (nicht im laufenden Schadensaufruf), ohne Ausloeser -> keine Schleife
	if (Stufe >= MaxStufe)
	{
		const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
		const FVector Ort = Getroffen->GetActorLocation();
		TArray<TWeakObjectPtr<AFGCreature>> Opfer;
		float Zusatz = 0.f;
		if (W == TDPionierWerte::Armierpistole)
		{
			// der naechste Gegner hinter dem Ziel (in Schussrichtung, bis 6 m)
			const FVector Richtung = Verursacher ? (Ort - Verursacher->GetActorLocation()).GetSafeNormal() : FVector::ZeroVector;
			AFGCreature* Bester = nullptr;
			float BestAbstand = 600.f;
			for (AFGCreature* C : Waves ? Waves->GetLivingWaveCreatures() : TArray<AFGCreature*>())
			{
				const FVector Zu = C->GetActorLocation() - Ort;
				if (C != Getroffen && Zu.Size() < BestAbstand && FVector::DotProduct(Zu.GetSafeNormal(), Richtung) > 0.3f)
				{
					BestAbstand = Zu.Size();
					Bester = C;
				}
			}
			if (Bester)
			{
				Opfer.Add(Bester);
				Zusatz = 0.5f * Menge * Faktor;
			}
		}
		else if (W == TDPionierWerte::Gewehr ? (++mGewehrTreffer % 5 == 0) : true)
		{
			const float Radius = W == TDPionierWerte::Gewehr ? 400.f : 800.f;
			// 0.9.6 P1: Sprengmantel waechst mit der Welle (vorher fest 120, in Welle 30 ein Kratzer)
			Zusatz = W == TDPionierWerte::Gewehr ? TDBalance::Wert(TEXT("Pionier.Sprengmantel"), 120.f) * Skalierung : 0.5f * Menge * Faktor;
			for (AFGCreature* C : Waves ? Waves->GetLivingWaveCreatures() : TArray<AFGCreature*>())
			{
				if (C != Getroffen && FVector::Dist(C->GetActorLocation(), Ort) < Radius)
				{
					Opfer.Add(C);
				}
			}
		}
		if (Opfer.Num() > 0 && Zusatz > 0.f)
		{
			mBilanz.Schaden += Zusatz * Opfer.Num();   // 0.9.6 P0: Zusatzschaden zaehlt mit (laeuft ohne Ausloeser)
			GetWorld()->GetTimerManager().SetTimerForNextTick([Opfer, Zusatz]()
			{
				for (const TWeakObjectPtr<AFGCreature>& C : Opfer)
				{
					if (C.IsValid() && C->IsAliveAndWell())
					{
						UGameplayStatics::ApplyDamage(C.Get(), Zusatz, nullptr, nullptr, UDamageType::StaticClass());
						UTDGegnerHirn::RestToeten(C.Get());
					}
				}
			});
		}
	}
	return Faktor;
}

float UTDPionier::Schadensfaktor(AActor* Getroffen, AController* Ausloeser, AActor* Verursacher, float Menge, bool bWellenKreatur, bool bVonKreatur)
{
	UTDPionier* P = Get(Getroffen);
	if (!P || !P->Kern())
	{
		return 1.f;
	}
	// Spieler trifft einen Wellengegner
	const APawn* SpielerPawn = Ausloeser && Ausloeser->IsPlayerController() ? static_cast<const APawn*>(Ausloeser->GetPawn()) : Verursacher ? static_cast<const APawn*>(Verursacher->GetInstigator()) : nullptr;
	if (bWellenKreatur && SpielerPawn && SpielerPawn->IsA<AFGCharacterPlayer>())
	{
		return P->SpielerTreffer(Getroffen, Verursacher, Menge);
	}
	// Kreatur trifft den Spieler: Ruestung
	if (bVonKreatur && Getroffen && Getroffen->IsA<AFGCharacterPlayer>() && P->GetStufe(TDPionierWerte::Ueberleben) >= 4)
	{
		return 0.75f;
	}
	return 1.f;
}

void UTDPionier::Wirke()
{
	// 0.9.7 P2: abgelaufene Markierungen und ihre Lichter entfernen
	RaeumeMarken(false);
	const ATDDefenseCore* K = Kern();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	if (!K || !Spieler)
	{
		return;
	}
	const AFGCharacterPlayer* Vorlage = Spieler->GetClass()->GetDefaultObject<AFGCharacterPlayer>();
	// Ueberleben: LP, Regeneration, zweite Chance
	const int32 Leben = GetStufe(TDPionierWerte::Ueberleben);
	if (UFGHealthComponent* G = Spieler->GetHealthComponent())
	{
		const float Grund = Vorlage && Vorlage->GetHealthComponent() ? Vorlage->GetHealthComponent()->GetMaxHealth() : 100.f;
		const float Soll = Grund + 25.f * FMath::Min(Leben, 2);
		if (!FMath::IsNearlyEqual(G->GetMaxHealth(), Soll))
		{
			G->SetMaxHealth(Soll);
		}
		if (!G->IsDead())
		{
			if (Leben >= 3 && G->GetCurrentHealth() < G->GetMaxHealth())
			{
				G->Heal(0.5f);
			}
			if (Leben >= 5 && !mZweiteChanceGenutzt && G->GetCurrentHealth() < 0.25f * G->GetMaxHealth())
			{
				mZweiteChanceGenutzt = true;
				G->Heal(G->GetMaxHealth());
				Melde(TDT(TEXT("Zweite Chance: voll geheilt (wieder bereit in der nächsten Welle)."), TEXT("Second chance: fully healed (ready again next wave).")));
			}
		}
	}
	// Mobilitaet: Lauftempo, Sprung, Sprint (Grundwerte aus der Klassenvorlage, damit sich nichts aufschaukelt)
	const int32 Tempo = GetStufe(TDPionierWerte::Mobilitaet);
	UFGCharacterMovementComponent* Bewegung = Spieler->GetFGMovementComponent();
	const UFGCharacterMovementComponent* GrundBewegung = Vorlage ? Cast<UFGCharacterMovementComponent>(Vorlage->GetCharacterMovement()) : nullptr;
	if (Bewegung && GrundBewegung)
	{
		const float Lauf = GrundBewegung->MaxWalkSpeed * (1.f + 0.1f * FMath::Min(Tempo, 2));
		const float Sprung = GrundBewegung->JumpZVelocity * (1.f + 0.25f * ((Tempo >= 3 ? 1 : 0) + (Tempo >= 5 ? 1 : 0)));
		if (!FMath::IsNearlyEqual(Bewegung->MaxWalkSpeed, Lauf)) Bewegung->MaxWalkSpeed = Lauf;
		if (!FMath::IsNearlyEqual(Bewegung->JumpZVelocity, Sprung)) Bewegung->JumpZVelocity = Sprung;
		if (FFloatProperty* Sprint = FindFProperty<FFloatProperty>(UFGCharacterMovementComponent::StaticClass(), TEXT("mMaxSprintSpeed")))
		{
			const float Soll = Sprint->GetPropertyValue_InContainer(GrundBewegung) * (1.f + 0.2f * ((Tempo >= 4 ? 1 : 0) + (Tempo >= 5 ? 1 : 0)));
			if (!FMath::IsNearlyEqual(Sprint->GetPropertyValue_InContainer(Bewegung), Soll))
			{
				Sprint->SetPropertyValue_InContainer(Bewegung, Soll);
			}
		}
	}
	// Nachladen der Spielerwaffen (Stufe 3 der Waffen-Zweige)
	if (FFloatProperty* Laden = FindFProperty<FFloatProperty>(AFGWeapon::StaticClass(), TEXT("mReloadTime")))
	{
		for (TActorIterator<AFGWeapon> It(GetWorld()); It; ++It)
		{
			AFGWeapon* W = *It;
			if (W->GetInstigator() != Spieler && W->GetOwner() != Spieler)
			{
				continue;
			}
			const int32 Art = Waffe(W);
			if (Art != TDPionierWerte::Armierpistole && Art != TDPionierWerte::Gewehr)
			{
				continue;
			}
			const float Soll = Laden->GetPropertyValue_InContainer(W->GetClass()->GetDefaultObject()) * (GetStufe(Art) >= 3 ? 0.7f : 1.f);
			if (!FMath::IsNearlyEqual(Laden->GetPropertyValue_InContainer(W), Soll))
			{
				Laden->SetPropertyValue_InContainer(W, Soll);
			}
		}
	}
}
