#include "TDPionier.h"
#include "TDGegnerHirn.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "TDDefenseCore.h"
#include "TDDefenseWall.h"
#include "TDGate.h"
#include "TDBarrier.h"
#include "TDEinsatz.h"
#include "TDWaveSubsystem.h"
#include "TDTurretBase.h"
#include "Creature/FGCreature.h"
#include "Equipment/FGWeapon.h"
#include "Equipment/FGAmmoType.h"
#include "EngineUtils.h"
#include "FGCharacterMovementComponent.h"
#include "FGCharacterPlayer.h"
#include "FGChatManager.h"
#include "FGHealthComponent.h"
#include "FGInventoryComponent.h"
#include "FGRecipe.h"
#include "Resources/FGItemDescriptor.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PointLightComponent.h"
#include "TimerManager.h"

namespace TDPionierWerte
{
	// Waffen (nicht Zweige): Ergebnis von UTDPionier::Waffe
	static constexpr int32 Armierpistole = 0, Gewehr = 1, Nobelisk = 2;
	static const TCHAR* Keys[] = { TEXT("Schuetze"), TEXT("Kommandant"), TEXT("Techniker"), TEXT("Jaeger") };
}

/** 0.9.8 P3 (Analyse 3.3, Marius 12.11.): vier Rollen-Zweige, jede Stufe eine Faehigkeit. Key bleibt deutsch (Spielstand). */
static const TArray<FTDTalentZweig>& TDZweigeDe()
{
	static const TArray<FTDTalentZweig> Liste = {
		{ TEXT("Schuetze"), TEXT("Schütze"), TEXT("Deine Handwaffen"),
			{ TEXT("Kopftreffer ×1,5"), TEXT("Schnellladen: Nachladen doppelt so schnell"),
			  TEXT("Durchschlag: jeder Treffer trifft auch den nächsten Gegner dahinter (halber Schaden)"),
			  TEXT("Munition sparen: jeder vierte Treffer kostet nichts"),
			  TEXT("Sprengmantel: jeder fünfte Treffer explodiert mit vollem Trefferschaden in 4 m") } },
		{ TEXT("Kommandant"), TEXT("Kommandant"), TEXT("Türme lenken"),
			{ TEXT("Zielmarkierung: dein Treffer markiert den Gegner 6 s, Türme mit „Markierte zuerst“ nehmen ihn"),
			  TEXT("Markierte Gegner nehmen +15 % Turmschaden"),
			  TEXT("Stirbt ein markierter Gegner, springt die Markierung auf den nächsten (10 m)"),
			  TEXT("Sammelbefehl: Türme in 30 m um dich schießen 5 s auf dein Ziel"),
			  TEXT("Übersteuern im Turmfenster wirkt auf alle Türme in 20 m") } },
		{ TEXT("Techniker"), TEXT("Techniker"), TEXT("Bauten halten"),
			{ TEXT("Feldreparatur: Bauten in 8 m um dich heilen 5 % LP je Sekunde (kostet Eisenplatten)"),
			  TEXT("Feldreparatur doppelt so schnell"),
			  TEXT("Übersteuern: Knopf im Turmfenster, 10 s +50 % Takt, danach 60 s Pause"),
			  TEXT("Nachschub: Türme in 20 m ohne Munition bekommen Munition aus deinem Inventar"),
			  TEXT("Bollwerk: einmal je Welle fällt ein Bau nicht, sondern hält mit halben LP") } },
		{ TEXT("Jaeger"), TEXT("Jäger"), TEXT("Elite und Boss"),
			{ TEXT("Dein Schaden an Elite und Boss ×1,5"),
			  TEXT("Elite oder Boss selbst erlegt: Kampfdaten und Coupons fallen sicher"),
			  TEXT("Schwachstelle: ein Boss, den du triffst, nimmt 5 s +25 % von allen Türmen"),
			  TEXT("Boss-Balken zeigt Resistenzen und deinen Anteil"),
			  TEXT("Hinrichtung: Elite unter 15 % LP stirbt beim nächsten Treffer von dir") } },
	};
	return Liste;
}

static const TArray<FTDTalentZweig>& TDZweigeEn()
{
	static const TArray<FTDTalentZweig> Liste = {
		{ TEXT("Schuetze"), TEXT("Marksman"), TEXT("Your hand weapons"),
			{ TEXT("Headshots ×1.5"), TEXT("Quick reload: reload twice as fast"),
			  TEXT("Piercing: every hit also strikes the next enemy behind (half damage)"),
			  TEXT("Ammo saver: every fourth hit is free"),
			  TEXT("Explosive rounds: every fifth hit explodes for full hit damage in 4 m") } },
		{ TEXT("Kommandant"), TEXT("Commander"), TEXT("Direct your turrets"),
			{ TEXT("Target marking: your hit marks the enemy for 6 s; turrets set to “Marked first” go for it"),
			  TEXT("Marked enemies take +15 % turret damage"),
			  TEXT("When a marked enemy dies, the mark jumps to the next one (10 m)"),
			  TEXT("Rally order: turrets within 30 m of you fire at your target for 5 s"),
			  TEXT("Overdrive in the turret window affects all turrets within 20 m") } },
		{ TEXT("Techniker"), TEXT("Engineer"), TEXT("Keep buildings up"),
			{ TEXT("Field repair: buildings within 8 m of you heal 5 % HP per second (costs Iron Plates)"),
			  TEXT("Field repair twice as fast"),
			  TEXT("Overdrive: button in the turret window, 10 s +50 % clock, then 60 s cooldown"),
			  TEXT("Resupply: turrets within 20 m without ammo get ammo from your inventory"),
			  TEXT("Bulwark: once per wave a building does not fall but holds at half HP") } },
		{ TEXT("Jaeger"), TEXT("Hunter"), TEXT("Elites and bosses"),
			{ TEXT("Your damage to elites and bosses ×1.5"),
			  TEXT("Elite or boss killed by you: combat data and Coupons always drop"),
			  TEXT("Weak spot: a boss you hit takes +25 % from all turrets for 5 s"),
			  TEXT("Boss bar shows resistances and your share"),
			  TEXT("Execution: an elite below 15 % HP dies at your next hit") } },
	};
	return Liste;
}

const TArray<FTDTalentZweig>& UTDPionier::Zweige()
{
	return TDSprache::Englisch() ? TDZweigeEn() : TDZweigeDe();
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
	// 0.9.8 P3: 20 Plaetze, hoechstens 15 Punkte (Rang 16) - zwei Zweige voll, einer halb
	return FMath::Clamp(TDBalance::Ganz(TEXT("Pionier.MaxRang"), 16), 2, 99);
}

int32 UTDPionier::XPFuerRang(int32 Rang)
{
	// Schwelle je Rang: Pionier.XPGrund + Pionier.XPSchritt x Rang (0.9.8: 5 + 20 r -> Rang 2 bei 25 XP, also nach Welle 1)
	const int32 Grund = TDBalance::Ganz(TEXT("Pionier.XPGrund"), 5);
	const int32 Schritt = TDBalance::Ganz(TEXT("Pionier.XPSchritt"), 20);
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

int32 UTDPionier::RangBoni() const
{
	return GetRang() / FMath::Max(TDBalance::Ganz(TEXT("Pionier.RangBonusAlle"), 3), 1);
}

int32 UTDPionier::GetStufe(int32 Zweig) const
{
	const ATDDefenseCore* K = Kern();
	return K && Zweig >= 0 && Zweig < 4 ? FMath::Clamp(K->mTalente.FindRef(TDPionierWerte::Keys[Zweig]), 0, MaxStufe) : 0;
}

int32 UTDPionier::Stufe(const UObject* Kontext, int32 Zweig)
{
	const UTDPionier* P = Kontext ? Get(Kontext) : nullptr;
	return P ? P->GetStufe(Zweig) : 0;
}

int32 UTDPionier::FreiePunkte() const
{
	int32 Vergeben = 0;
	for (int32 i = 0; i < 4; ++i)
	{
		Vergeben += GetStufe(i);
	}
	return FMath::Max(0, GetRang() - 1 - Vergeben);
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
	if (FreiePunkte() <= 0)
	{
		return TDT(TEXT("Kein Talentpunkt frei. Nächster Rang bringt einen."), TEXT("No Talent Point free. The next rank gives you one."));
	}
	K->mTalente.FindOrAdd(Z.Key) = Stufe + 1;
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Talent %s Stufe %d (%s), Rang %d"), Z.Key, Stufe + 1, TDZweigeDe()[Zweig].Stufen[Stufe], GetRang());
	Wirke();
	return TDF(TEXT("%s Stufe %d: %s"), TEXT("%s level %d: %s"), Z.Name, Stufe + 1, Z.Stufen[Stufe]);
}

void UTDPionier::SetzeStufe(const FString& Key, int32 Neu)
{
	if (ATDDefenseCore* K = Kern())
	{
		K->mTalente.FindOrAdd(Key) = FMath::Clamp(Neu, 0, MaxStufe);
		UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Talent %s auf Stufe %d gesetzt (Selbsttest)"), *Key, Neu);
		Wirke();
	}
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

void UTDPionier::PruefeAlteTalente()
{
	ATDDefenseCore* K = Kern();
	if (mAlteGeprueft || !K)
	{
		return;
	}
	mAlteGeprueft = true;
	TArray<FString> Alt;
	for (const TPair<FString, int32>& T : K->mTalente)
	{
		bool bNeu = false;
		for (const TCHAR* Key : TDPionierWerte::Keys)
		{
			bNeu |= T.Key == Key;
		}
		if (!bNeu)
		{
			Alt.Add(T.Key);
		}
	}
	for (const FString& Key : Alt)
	{
		K->mTalente.Remove(Key);
	}
	if (Alt.Num() > 0)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Pionier: alter Talentbaum entfernt (%s), %d Punkte frei"), *FString::Join(Alt, TEXT(", ")), FreiePunkte());
		Melde(TDF(TEXT("Der Talentbaum ist neu (Schütze, Kommandant, Techniker, Jäger). Deine %d Punkte sind wieder frei: Kern-Menü, Reiter Pionier."),
			TEXT("The talent tree is new (Marksman, Commander, Engineer, Hunter). Your %d points are free again: Core Menu, Pioneer tab."), FreiePunkte()));
	}
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
	// 0.9.8 P3: Wellen-XP halbiert (Standard 20 + 3 x N, +10 bei vollem Kern), dazu XP fuer eigene Leistung
	int32 XP = TDBalance::Ganz(TEXT("Pionier.XPWelle"), 20) + TDBalance::Ganz(TEXT("Pionier.XPJeWelle"), 3) * Welle
		+ (KernAnteil >= 99.5f ? TDBalance::Ganz(TEXT("Pionier.XPKernVoll"), 10) : 0);
	if (Welle % 10 == 0)
	{
		XP *= 2;
	}
	GibXP(XP, FString::Printf(TEXT("Welle %d"), Welle));
	const int32 Leistung = FMath::RoundToInt(mBilanz.Anteil * TDBalance::Wert(TEXT("Pionier.XPJeProzent"), 2.f))
		+ FMath::FloorToInt(mBilanz.Repariert / FMath::Max(TDBalance::Wert(TEXT("Pionier.XPJeReparatur"), 200.f), 1.f));
	mBilanz.LeistungsXP = Leistung;
	mBilanz.XP += Leistung;
	GibXP(Leistung, TEXT("Leistung"));
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Welle %d +%d XP, Leistung +%d XP (Anteil %.1f %%, repariert %.0f LP), jetzt %d XP, Rang %d, %d Punkte frei"),
		Welle, XP, Leistung, mBilanz.Anteil, mBilanz.Repariert, GetXP(), GetRang(), FreiePunkte());
	mLetzte = mBilanz;
}

void UTDPionier::WellenStart()
{
	mTrefferZaehler = 0;
	mBilanz = FSpielerBilanz();
	mBollwerkGenutzt = false;
	mPlattenFehltGemeldet = false;
	mSammelZiel.Reset();
	RaeumeMarken(true);
}

FString UTDPionier::WellenZeile(float TurmSchaden)
{
	mBilanz.Anteil = 100.f * mBilanz.Schaden / FMath::Max(mBilanz.Schaden + TurmSchaden, 1.f);
	if (mBilanz.Treffer == 0 && mBilanz.Repariert < 1.f)
	{
		return FString();
	}
	FString Text = TDF(TEXT(" Du: %.0f Schaden (%.0f %%), %d Abschüsse"), TEXT(" You: %.0f damage (%.0f %%), %d kills"), mBilanz.Schaden, mBilanz.Anteil, mBilanz.Abschuesse);
	if (mBilanz.Eliten + mBilanz.Bosse > 0)
	{
		Text += TDF(TEXT(", davon %d Elite/Boss"), TEXT(", %d of them elite/boss"), mBilanz.Eliten + mBilanz.Bosse);
	}
	if (mBilanz.Assists > 0 || mBilanz.MarkBonus >= 1.f)
	{
		Text += TDF(TEXT(", %d Assists (Markierung +%.0f Turmschaden)"), TEXT(", %d assists (marking +%.0f turret damage)"), mBilanz.Assists, mBilanz.MarkBonus);
	}
	if (mBilanz.Repariert >= 1.f)
	{
		Text += TDF(TEXT(", %.0f LP repariert"), TEXT(", %.0f HP repaired"), mBilanz.Repariert);
	}
	if (mBilanz.XP > 0)
	{
		Text += FString::Printf(TEXT(", +%d XP"), mBilanz.XP);
	}
	return Text + TEXT(".");
}

FString UTDPionier::IstWerte(int32 Zweig) const
{
	const FSpielerBilanz& B = mLetzte;
	switch (Zweig)
	{
	case Schuetze:
		return B.Kopftreffer + B.Durchschlaege + B.MunitionGespart == 0 ? FString()
			: TDF(TEXT("%d Kopftreffer, %d Durchschläge, %d Munition gespart"), TEXT("%d headshots, %d pierces, %d ammo saved"), B.Kopftreffer, B.Durchschlaege, B.MunitionGespart);
	case Kommandant:
		return B.Markiert + B.Assists == 0 ? FString()
			: TDF(TEXT("%d markiert, +%.0f Turmschaden, %d Assists, %d Sammelbefehle"), TEXT("%d marked, +%.0f turret damage, %d assists, %d rally orders"), B.Markiert, B.MarkBonus, B.Assists, B.Sammelbefehle);
	case Techniker:
		return B.Repariert < 1.f && B.Uebersteuert + B.Nachschub + B.Gerettet == 0 ? FString()
			: TDF(TEXT("%.0f LP repariert, %d übersteuert, %d Munition nachgeliefert, %d gerettet"), TEXT("%.0f HP repaired, %d overdrives, %d ammo resupplied, %d saved"), B.Repariert, B.Uebersteuert, B.Nachschub, B.Gerettet);
	case Jaeger:
		return B.JagdSchaden < 1.f && B.Hinrichtungen + B.JagdBeute == 0 ? FString()
			: TDF(TEXT("%.0f Schaden an Elite/Boss, %d Hinrichtungen, %d sichere Beute"), TEXT("%.0f damage to elites/bosses, %d executions, %d sure drops"), B.JagdSchaden, B.Hinrichtungen, B.JagdBeute);
	default:
		return FString();
	}
}

// ---------------- Kommandant: Zielmarkierung, Sammelbefehl (P2, Talent seit 0.9.8) ----------------

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
			UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Zielmarkierung auf %s (%.0f s, Turmschaden x%.2f ab Kommandant 2)"), *Gegner->GetName(),
				TDBalance::Wert(TEXT("Pionier.Markierung.Dauer"), 6.f), TDBalance::Wert(TEXT("Pionier.Markierung.Bonus"), 1.15f));
		}
	}
	M.Bis = Jetzt + TDBalance::Wert(TEXT("Pionier.Markierung.Dauer"), 6.f);
	// Sichtbar: blaues Licht ueber dem Gegner; hoechstens 24 gleichzeitig
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
		const bool bAbgelaufen = It.Value().Bis < Jetzt && It.Value().SchwachBis < Jetzt;
		if (It.Value().Bis < Jetzt)
		{
			if (UPointLightComponent* Licht = It.Value().Licht.Get())
			{
				Licht->DestroyComponent();
			}
		}
		if (bAlle || !It.Key().IsValid() || bAbgelaufen)
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
	UTDPionier* P = Gegner ? Get(Gegner) : nullptr;
	if (!P || P->mMarken.Num() == 0)
	{
		return 1.f;
	}
	const FMarke* M = P->mMarken.Find(TWeakObjectPtr<AActor>(const_cast<AActor*>(Gegner)));
	const float Jetzt = Gegner->GetWorld()->GetTimeSeconds();
	float Faktor = 1.f;
	if (M && M->Bis >= Jetzt && P->GetStufe(Kommandant) >= 2)
	{
		Faktor *= FMath::Max(TDBalance::Wert(TEXT("Pionier.Markierung.Bonus"), 1.15f), 1.f);
	}
	if (M && M->SchwachBis >= Jetzt)
	{
		Faktor *= FMath::Max(TDBalance::Wert(TEXT("Pionier.Jaeger.Schwachstelle"), 1.25f), 1.f);
	}
	P->mBilanz.MarkBonus += Schaden * (Faktor - 1.f);
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

void UTDPionier::SpringeMarke(const AFGCreature* Tot)
{
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	AFGCreature* Bester = nullptr;
	float Abstand = TDBalance::Wert(TEXT("Pionier.Markierung.Sprung"), 10.f) * 100.f;
	for (AFGCreature* C : Waves ? Waves->GetLivingWaveCreatures() : TArray<AFGCreature*>())
	{
		const float D = FVector::Dist(C->GetActorLocation(), Tot->GetActorLocation());
		if (C != Tot && C->IsAliveAndWell() && D < Abstand && !IstMarkiert(C))
		{
			Abstand = D;
			Bester = C;
		}
	}
	if (Bester)
	{
		Markiere(Bester);
	}
}

AFGCreature* UTDPionier::Sammelziel(const ATDTurretBase* Turm)
{
	UTDPionier* P = Turm ? Get(Turm) : nullptr;
	AFGCreature* Ziel = P ? P->mSammelZiel.Get() : nullptr;
	if (!Ziel || !Ziel->IsAliveAndWell() || Turm->GetWorld()->GetTimeSeconds() > P->mSammelBis || P->GetStufe(Kommandant) < 4)
	{
		return nullptr;
	}
	const APlayerController* PC = Turm->GetWorld()->GetFirstPlayerController();
	const APawn* Spieler = PC ? PC->GetPawn() : nullptr;
	const float Radius = TDBalance::Wert(TEXT("Pionier.Sammelbefehl.Radius"), 30.f) * 100.f;
	return Spieler && FVector::Dist(Spieler->GetActorLocation(), Turm->GetActorLocation()) <= Radius ? Ziel : nullptr;
}

// ---------------- Jaeger ----------------

bool UTDPionier::VomSpielerErlegt(const AActor* Gegner)
{
	const UTDPionier* P = Gegner ? Get(Gegner) : nullptr;
	const double* Zeit = P ? P->mSpielerTreffer.Find(Gegner) : nullptr;
	return Zeit && FPlatformTime::Seconds() - *Zeit <= 5.0;
}

bool UTDPionier::JagdBeute(const AActor* Gegner)
{
	UTDPionier* P = Gegner ? Get(Gegner) : nullptr;
	if (!P || P->GetStufe(Jaeger) < 2 || !VomSpielerErlegt(Gegner))
	{
		return false;
	}
	++P->mBilanz.JagdBeute;
	return true;
}

// ---------------- Techniker: Bollwerk, Uebersteuern, Feldreparatur, Nachschub (P4) ----------------

bool UTDPionier::Bollwerk(const AActor* Bau)
{
	UTDPionier* P = Bau ? Get(Bau) : nullptr;
	if (!P || P->mBollwerkGenutzt || P->GetStufe(Techniker) < 5)
	{
		return false;
	}
	P->mBollwerkGenutzt = true;
	++P->mBilanz.Gerettet;
	const ATDTurretBase* Turm = Cast<ATDTurretBase>(Bau);
	const FString Name = Turm ? Turm->GetTurmAnzeige() : TDT(TEXT("Ein Bau"), TEXT("A building"));
	P->Melde(TDF(TEXT("Bollwerk: %s hält mit halben LP (wieder bereit in der nächsten Welle)."), TEXT("Bulwark: %s holds at half HP (ready again next wave)."), *Name));
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Bollwerk rettet %s"), *Bau->GetName());
	return true;
}

bool UTDPionier::KannUebersteuern() const
{
	return GetStufe(Techniker) >= 3 || GetStufe(Kommandant) >= 5;
}

FString UTDPionier::Uebersteuere(ATDTurretBase* Turm)
{
	if (!Turm || !KannUebersteuern())
	{
		return TDT(TEXT("Übersteuern braucht Techniker 3 oder Kommandant 5."), TEXT("Overdrive needs Engineer 3 or Commander 5."));
	}
	TArray<ATDTurretBase*> Tuerme;
	if (GetStufe(Kommandant) >= 5)
	{
		const float Radius = TDBalance::Wert(TEXT("Pionier.Uebersteuern.Radius"), 20.f) * 100.f;
		for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist(It->GetActorLocation(), Turm->GetActorLocation()) <= Radius)
			{
				Tuerme.Add(*It);
			}
		}
	}
	else
	{
		Tuerme.Add(Turm);
	}
	const float Dauer = TDBalance::Wert(TEXT("Pionier.Uebersteuern.Dauer"), 10.f);
	const float Pause = TDBalance::Wert(TEXT("Pionier.Uebersteuern.Pause"), 60.f);
	int32 Gestartet = 0;
	for (ATDTurretBase* T : Tuerme)
	{
		Gestartet += T->StarteUebersteuern(Dauer, Pause) ? 1 : 0;
	}
	mBilanz.Uebersteuert += Gestartet;
	UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Uebersteuern an %s: %d von %d Tuermen fuer %.0f s"), *Turm->GetName(), Gestartet, Tuerme.Num(), Dauer);
	if (Gestartet == 0)
	{
		return TDF(TEXT("Übersteuern noch in der Pause (%.0f s)."), TEXT("Overdrive still cooling down (%.0f s)."), Turm->UebersteuernPause());
	}
	return Gestartet == 1 ? TDF(TEXT("Übersteuert: %.0f s +50 %% Takt."), TEXT("Overdrive: %.0f s +50 %% clock."), Dauer)
		: TDF(TEXT("%d Türme übersteuert: %.0f s +50 %% Takt."), TEXT("%d turrets overdriven: %.0f s +50 %% clock."), Gestartet, Dauer);
}

void UTDPionier::Feldreparatur(AFGCharacterPlayer* Spieler, float Sekunden)
{
	const int32 T = GetStufe(Techniker);
	if (T < 1 || !Spieler)
	{
		return;
	}
	static UClass* Platte = LoadClass<UFGItemDescriptor>(nullptr, TEXT("/Game/FactoryGame/Resource/Parts/IronPlate/Desc_IronPlate.Desc_IronPlate_C"));
	UFGInventoryComponent* Inventar = Spieler->GetInventory();
	const float Rate = TDBalance::Wert(TEXT("Pionier.Reparatur.Rate"), 0.05f) * (T >= 2 ? 2.f : 1.f);
	const float Radius = TDBalance::Wert(TEXT("Pionier.Reparatur.Radius"), 8.f) * 100.f;
	const float LPJePlatte = FMath::Max(TDBalance::Wert(TEXT("Pionier.Reparatur.LPJePlatte"), 300.f), 1.f);
	const FVector Ort = Spieler->GetActorLocation();
	auto Repariere = [&](AActor* Bau)
	{
		ITDDamageable* D = Cast<ITDDamageable>(Bau);
		if (!D || !D->IsTDIntact() || D->GetTDHealth() >= D->GetTDMaxHealth() || FVector::Dist(Bau->GetActorLocation(), Ort) > Radius)
		{
			return;
		}
		const float Menge = FMath::Min(D->GetTDMaxHealth() * Rate * Sekunden, D->GetTDMaxHealth() - D->GetTDHealth());
		mPlattenKonto += Menge / LPJePlatte;
		while (mPlattenKonto >= 1.f)
		{
			if (!Platte || !Inventar || !Inventar->HasItems(Platte, 1))
			{
				mPlattenKonto = 0.f;
				if (!mPlattenFehltGemeldet)
				{
					mPlattenFehltGemeldet = true;
					Melde(TDT(TEXT("Feldreparatur braucht Eisenplatten im Inventar."), TEXT("Field repair needs Iron Plates in your inventory.")));
				}
				return;
			}
			Inventar->Remove(Platte, 1);
			mPlattenKonto -= 1.f;
		}
		const float Vorher = D->GetTDHealth();
		D->HealTD(Menge);
		mBilanz.Repariert += FMath::Max(0.f, D->GetTDHealth() - Vorher);
	};
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It) { Repariere(*It); }
	for (TActorIterator<ATDDefenseWall> It(GetWorld()); It; ++It) { Repariere(*It); }
	for (TActorIterator<ATDGate> It(GetWorld()); It; ++It) { Repariere(*It); }
	for (TActorIterator<ATDBarrier> It(GetWorld()); It; ++It) { Repariere(*It); }
}

void UTDPionier::Nachschub(AFGCharacterPlayer* Spieler)
{
	if (GetStufe(Techniker) < 4 || !Spieler || !Spieler->GetInventory())
	{
		return;
	}
	const float Radius = TDBalance::Wert(TEXT("Pionier.Nachschub.Radius"), 20.f) * 100.f;
	const int32 Menge = FMath::Max(TDBalance::Ganz(TEXT("Pionier.Nachschub.Menge"), 10), 1);
	UFGInventoryComponent* Inventar = Spieler->GetInventory();
	for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
	{
		ATDTurretBase* Turm = *It;
		if (FVector::Dist(Turm->GetActorLocation(), Spieler->GetActorLocation()) > Radius || !Turm->GetCurrentRecipe() || !Turm->GetInputInventory())
		{
			continue;
		}
		const TArray<FItemAmount> Zutaten = UFGRecipe::GetIngredients(Turm, Turm->GetCurrentRecipe());
		UClass* Sorte = Zutaten.Num() > 0 ? Zutaten[0].ItemClass.Get() : nullptr;
		if (!Sorte || UFGItemDescriptor::GetForm(Sorte) != EResourceForm::RF_SOLID || Turm->GetInputInventory()->GetNumItems(Sorte) >= Zutaten[0].Amount)
		{
			continue;
		}
		const int32 Hat = FMath::Min(Inventar->GetNumItems(Sorte), Menge);
		if (Hat <= 0)
		{
			continue;
		}
		const int32 Rein = Turm->GetInputInventory()->AddStack(FInventoryStack(Hat, Sorte), true);
		if (Rein > 0)
		{
			Inventar->Remove(Sorte, Rein);
			mBilanz.Nachschub += Rein;
			if (mBilanz.Nachschub == Rein)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Nachschub %d x %s an %s"), Rein, *UFGItemDescriptor::GetItemName(Sorte).ToString(), *Turm->GetName());
			}
		}
	}
}

// ---------------- Schuetze ----------------

void UTDPionier::SpareMunition(AActor* Verursacher, AFGCharacterPlayer* Spieler)
{
	AFGWeapon* Waffe = nullptr;
	for (AActor* A = Verursacher; A && !Waffe; A = A->GetOwner())
	{
		Waffe = Cast<AFGWeapon>(A);
		if (A->IsA<APawn>())
		{
			break;
		}
	}
	const FClassProperty* Feld = FindFProperty<FClassProperty>(AFGWeapon::StaticClass(), TEXT("mCurrentAmmunitionClass"));
	UClass* Munition = Waffe && Feld ? Cast<UClass>(Feld->GetObjectPropertyValue_InContainer(Waffe)) : nullptr;
	if (!Munition || !Spieler || !Spieler->GetInventory())
	{
		return;
	}
	// jeder vierte Treffer gratis: ein Viertel Schuss je Treffer, ein Gegenstand = ein Magazin
	const UFGAmmoType* Typ = Cast<UFGAmmoType>(Munition->GetDefaultObject());
	const int32 Magazin = Typ ? FMath::Max(Typ->GetMagazineSize(), 1) : 1;
	mMunitionKonto += TDBalance::Wert(TEXT("Pionier.Munition.Anteil"), 0.25f) / Magazin;
	if (mMunitionKonto >= 1.f)
	{
		mMunitionKonto -= 1.f;
		if (Spieler->GetInventory()->AddStack(FInventoryStack(1, Munition), true) > 0)
		{
			++mBilanz.MunitionGespart;
		}
	}
}

void UTDPionier::Zusatzschaden(AActor* Getroffen, AActor* Verursacher, int32 W, float Schaden)
{
	const int32 S = GetStufe(Schuetze);
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	const TArray<AFGCreature*> Lebend = Waves ? Waves->GetLivingWaveCreatures() : TArray<AFGCreature*>();
	const FVector Ort = Getroffen->GetActorLocation();
	TArray<TPair<TWeakObjectPtr<AFGCreature>, float>> Opfer;
	// Stufe 3 Durchschlag: der naechste Gegner hinter dem Ziel (Schussrichtung, bis 6 m), halber Schaden
	if (S >= 3 && W != TDPionierWerte::Nobelisk)
	{
		const APawn* Spieler = Verursacher ? Verursacher->GetInstigator() : nullptr;
		const FVector Von = Spieler ? Spieler->GetActorLocation() : Verursacher ? Verursacher->GetActorLocation() : Ort;
		const FVector Richtung = (Ort - Von).GetSafeNormal();
		AFGCreature* Bester = nullptr;
		float BestAbstand = 600.f;
		for (AFGCreature* C : Lebend)
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
			Opfer.Emplace(Bester, 0.5f * Schaden);
			++mBilanz.Durchschlaege;
		}
	}
	// Stufe 5 Sprengmantel: jeder fuenfte Treffer explodiert mit dem Trefferschaden in 4 m
	if (S >= 5 && ++mTrefferZaehler % 5 == 0)
	{
		const float Radius = TDBalance::Wert(TEXT("Pionier.Sprengmantel.Radius"), 4.f) * 100.f;
		for (AFGCreature* C : Lebend)
		{
			if (C != Getroffen && FVector::Dist(C->GetActorLocation(), Ort) < Radius)
			{
				Opfer.Emplace(C, Schaden);
			}
		}
	}
	if (Opfer.Num() == 0)
	{
		return;
	}
	for (const TPair<TWeakObjectPtr<AFGCreature>, float>& O : Opfer)
	{
		mBilanz.Schaden += O.Value;
	}
	// im naechsten Bild und ohne Ausloeser -> keine Schleife ueber den Schadens-Hook
	GetWorld()->GetTimerManager().SetTimerForNextTick([Opfer]()
	{
		for (const TPair<TWeakObjectPtr<AFGCreature>, float>& O : Opfer)
		{
			if (AFGCreature* C = O.Key.Get(); C && C->IsAliveAndWell())
			{
				UGameplayStatics::ApplyDamage(C, O.Value, nullptr, nullptr, UDamageType::StaticClass());
				UTDGegnerHirn::RestToeten(C);
			}
		}
	});
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
	// Kommandant 3: Markierung springt auf den naechsten Gegner
	if (GetStufe(Kommandant) >= 3 && IstMarkiert(Kreatur))
	{
		SpringeMarke(Kreatur);
	}
	const double* Zeit = mSpielerTreffer.Find(Kreatur);
	if (!Zeit || FPlatformTime::Seconds() - *Zeit > 5.0)
	{
		// Turm hat einen markierten Gegner erledigt -> Assist
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

float UTDPionier::SpielerTreffer(AActor* Getroffen, AActor* Verursacher, float Menge, FName Knochen)
{
	mSpielerTreffer.Add(Getroffen, FPlatformTime::Seconds());
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
	const float Jetzt = GetWorld()->GetTimeSeconds();
	const int32 S = GetStufe(Schuetze);
	const int32 K = GetStufe(Kommandant);
	const int32 J = GetStufe(Jaeger);
	AFGCreature* Kreatur = Cast<AFGCreature>(Getroffen);
	const bool bBoss = Getroffen->Tags.Contains(TEXT("FicsitTDBoss"));
	const bool bElite = Getroffen->Tags.Contains(TEXT("FicsitTDElite"));
	// Kommandant 1 Markierung, 4 Sammelbefehl
	if (K >= 1)
	{
		Markiere(Getroffen);
	}
	if (K >= 4 && Kreatur)
	{
		if (mSammelZiel.Get() != Kreatur || Jetzt > mSammelBis)
		{
			++mBilanz.Sammelbefehle;
		}
		mSammelZiel = Kreatur;
		mSammelBis = Jetzt + TDBalance::Wert(TEXT("Pionier.Sammelbefehl.Dauer"), 5.f);
	}
	// Jaeger 3 Schwachstelle am Boss
	if (J >= 3 && bBoss)
	{
		mMarken.FindOrAdd(Getroffen).SchwachBis = Jetzt + TDBalance::Wert(TEXT("Pionier.Jaeger.SchwachDauer"), 5.f);
	}
	const float Jagd = J >= 1 && (bBoss || bElite) ? FMath::Max(TDBalance::Wert(TEXT("Pionier.Jaeger.Faktor"), 1.5f), 0.f) : 1.f;
	// Schuetze 1 Kopftreffer (nur Punktschaden kennt den Knochen)
	const bool bKopf = S >= 1 && Knochen != NAME_None && Knochen.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase);
	const float Kopf = bKopf ? FMath::Max(TDBalance::Wert(TEXT("Pionier.Kopftreffer"), 1.5f), 1.f) : 1.f;
	mBilanz.Kopftreffer += bKopf ? 1 : 0;
	const int32 W = Waffe(Verursacher);
	FString Munition = TEXT("-");
	float Skalierung = 1.f, Resistenz = 1.f;
	const float Hand = W >= 0 ? HandFaktor(W, Verursacher, Getroffen, Munition, Skalierung, Resistenz) : 1.f;
	float Faktor = Hand * Jagd * Kopf;
	// Jaeger 5 Hinrichtung: Elite unter 15 % LP stirbt beim naechsten Treffer
	const UFGHealthComponent* Leben = Kreatur ? Kreatur->GetHealthComponent() : nullptr;
	if (J >= 5 && bElite && !bBoss && Leben && Menge > 0.f && Leben->GetCurrentHealth() < TDBalance::Wert(TEXT("Pionier.Jaeger.Hinrichtung"), 0.15f) * Leben->GetMaxHealth())
	{
		Faktor = FMath::Max(Faktor, (Leben->GetCurrentHealth() + 1.f) / Menge * 1.05f);
		++mBilanz.Hinrichtungen;
		UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Hinrichtung %s (%.0f LP)"), *Getroffen->GetName(), Leben->GetCurrentHealth());
	}
	const float Schaden = Menge * Faktor;
	mBilanz.Schaden += Schaden;
	++mBilanz.Treffer;
	if (bBoss || bElite)
	{
		mBilanz.JagdSchaden += Schaden;
	}
	// je Geschoss und Munition einmal die Rechnung ins Log
	{
		const FString Klasse = Verursacher ? Verursacher->GetClass()->GetName() : FString(TEXT("-"));
		if (!mGemeldeteTreffer.Contains(Klasse + Munition))
		{
			mGemeldeteTreffer.Add(Klasse + Munition);
			UE_LOG(LogFicsitTD, Log, TEXT("Pionier: Handtreffer %s (%s): Spielschaden %.1f -> %.1f, Faktor %.2f (Munition/Spielwert %.2f, Welle x%.2f, Resistenz %.2f, Jaeger %.2f, Kopf %.2f)"),
				*Klasse, *Munition, Menge, Schaden, Faktor, Hand / FMath::Max(Skalierung * Resistenz, 0.01f), Skalierung, Resistenz, Jagd, Kopf);
		}
	}
	if (W >= 0)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
		if (S >= 4)
		{
			SpareMunition(Verursacher, Spieler);
		}
		if (S >= 3)
		{
			Zusatzschaden(Getroffen, Verursacher, W, Schaden);
		}
	}
	return Faktor;
}

float UTDPionier::Schadensfaktor(AActor* Getroffen, AController* Ausloeser, AActor* Verursacher, float Menge, bool bWellenKreatur, bool bVonKreatur, FName Knochen)
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
		return P->SpielerTreffer(Getroffen, Verursacher, Menge, Knochen);
	}
	return 1.f;
}

void UTDPionier::Wirke()
{
	RaeumeMarken(false);
	const ATDDefenseCore* K = Kern();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFGCharacterPlayer* Spieler = PC ? Cast<AFGCharacterPlayer>(PC->GetPawn()) : nullptr;
	if (!K || !Spieler)
	{
		return;
	}
	PruefeAlteTalente();
	const AFGCharacterPlayer* Vorlage = Spieler->GetClass()->GetDefaultObject<AFGCharacterPlayer>();
	// Rangboni ohne Punkte (0.9.8 P3, ersetzt Ueberleben und Mobilitaet): je 3 Raenge +10 LP und +3 % Lauftempo
	const int32 Boni = RangBoni();
	if (UFGHealthComponent* G = Spieler->GetHealthComponent())
	{
		const float Grund = Vorlage && Vorlage->GetHealthComponent() ? Vorlage->GetHealthComponent()->GetMaxHealth() : 100.f;
		const float Soll = Grund + TDBalance::Wert(TEXT("Pionier.RangBonus.LP"), 10.f) * Boni;
		if (!FMath::IsNearlyEqual(G->GetMaxHealth(), Soll))
		{
			G->SetMaxHealth(Soll);
		}
	}
	UFGCharacterMovementComponent* Bewegung = Spieler->GetFGMovementComponent();
	const UFGCharacterMovementComponent* GrundBewegung = Vorlage ? Cast<UFGCharacterMovementComponent>(Vorlage->GetCharacterMovement()) : nullptr;
	if (Bewegung && GrundBewegung)
	{
		const float Lauf = GrundBewegung->MaxWalkSpeed * (1.f + TDBalance::Wert(TEXT("Pionier.RangBonus.Tempo"), 0.03f) * Boni);
		if (!FMath::IsNearlyEqual(Bewegung->MaxWalkSpeed, Lauf)) Bewegung->MaxWalkSpeed = Lauf;
		// alte Talente (bis 0.9.7) hatten Sprung und Sprint erhoeht: auf den Grundwert zurueck
		if (!FMath::IsNearlyEqual(Bewegung->JumpZVelocity, GrundBewegung->JumpZVelocity)) Bewegung->JumpZVelocity = GrundBewegung->JumpZVelocity;
		if (FFloatProperty* Sprint = FindFProperty<FFloatProperty>(UFGCharacterMovementComponent::StaticClass(), TEXT("mMaxSprintSpeed")))
		{
			const float Soll = Sprint->GetPropertyValue_InContainer(GrundBewegung);
			if (!FMath::IsNearlyEqual(Sprint->GetPropertyValue_InContainer(Bewegung), Soll))
			{
				Sprint->SetPropertyValue_InContainer(Bewegung, Soll);
			}
		}
	}
	// Schuetze 2 Schnellladen: Armierpistole und Gewehr laden doppelt so schnell
	if (FFloatProperty* Laden = FindFProperty<FFloatProperty>(AFGWeapon::StaticClass(), TEXT("mReloadTime")))
	{
		const float Faktor = GetStufe(Schuetze) >= 2 ? TDBalance::Wert(TEXT("Pionier.Schnellladen"), 0.5f) : 1.f;
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
			const float Soll = Laden->GetPropertyValue_InContainer(W->GetClass()->GetDefaultObject()) * Faktor;
			if (!FMath::IsNearlyEqual(Laden->GetPropertyValue_InContainer(W), Soll))
			{
				Laden->SetPropertyValue_InContainer(W, Soll);
			}
		}
	}
	// Techniker 1/2 Feldreparatur, 4 Nachschub (alle 2 s)
	Feldreparatur(Spieler, 0.5f);
	mNachschubTakt -= 0.5f;
	if (mNachschubTakt <= 0.f)
	{
		mNachschubTakt = 2.f;
		Nachschub(Spieler);
	}
}
