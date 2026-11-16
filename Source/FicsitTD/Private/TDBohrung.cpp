// 0.12-dev Spielmodus 3 "Freies Spiel mit Abwehr" (Gegencheck Normalspiel, Marius 15.11.2026):
// Bohrung am Kern-Ableger. Der Abwehrkern steht als Tiefbohrer neben einem Rohstoffknoten; auf Knopfdruck greifen einige Wellen
// den Kern an. Je abgewehrter Welle gibt es Erz des Knotens (wiederholbar), beim ersten Erfolg je Rohstoff und Stufe Festplatten,
// auf Schwer einen Talentpunkt. Abbruch jederzeit ohne Strafe, bei Kernverlust endet nur der Lauf - der Kern repariert sich.
// Nichts greift an, was der Spieler nicht eingeschaltet hat.
#include "TDWaveSubsystem.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDKernplatz.h"
#include "TDPionier.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "FicsitTDModule.h"
#include "Resources/FGResourceNode.h"
#include "Resources/FGResourceDeposit.h"
#include "Resources/FGResourceDescriptor.h"
#include "Resources/FGItemDescriptor.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
	struct FTDBohrStufe
	{
		const TCHAR* Key;
		int32 VonWelle;
		int32 BisWelle;
		float LP;
		int32 ErzJeWelle;
		int32 Festplatten;
		int32 Talentpunkte;
		int32 XP;
	};
	// Leicht: Phase 1 (Hogs, Spucker), eine Front. Mittel: Phase 2 mit Elite, zwei Fronten. Schwer: Phase 3 bis zum Boss der Welle 30.
	const FTDBohrStufe TDBohrStufen[] = {
		{ TEXT("Leicht"), 1, 4, 8000.f, 60, 2, 0, 60 },
		{ TEXT("Mittel"), 15, 19, 18000.f, 120, 4, 0, 120 },   // 16.11. (P3): LP x1,5
		{ TEXT("Schwer"), 25, 30, 24000.f, 200, 6, 1, 200 },
	};

	int32 BohrWert(int32 Stufe, const TCHAR* Feld, int32 Standard)
	{
		return TDBalance::Ganz(FString::Printf(TEXT("Bohrung.%s.%s"), TDBohrStufen[FMath::Clamp(Stufe, 0, 2)].Key, Feld), Standard);
	}

	float Reinheit(const AFGResourceNode* Knoten)
	{
		switch (Knoten ? Knoten->GetResourcePurity() : RP_Normal)
		{
		case RP_Inpure: return TDBalance::Wert(TEXT("Bohrung.Reinheit.Unrein"), 0.5f);
		case RP_Pure:   return TDBalance::Wert(TEXT("Bohrung.Reinheit.Rein"), 2.f);
		default:        return 1.f;
		}
	}

	FString ErfolgKey(const FString& Rohstoff, int32 Stufe)
	{
		return FString::Printf(TEXT("%s|%d"), *Rohstoff, Stufe);
	}
}

FString UTDWaveSubsystem::BohrStufenName(int32 Stufe)
{
	switch (FMath::Clamp(Stufe, 0, 2))
	{
	case 0: return TDT(TEXT("Leicht"), TEXT("Easy"));
	case 1: return TDT(TEXT("Mittel"), TEXT("Medium"));
	default: return TDT(TEXT("Schwer"), TEXT("Hard"));
	}
}

bool UTDWaveSubsystem::BohrStufeFrei(const ATDDefenseCore* Kern, int32 Stufe)
{
	if (Stufe <= 0)
	{
		return true;
	}
	if (!Kern)
	{
		return false;
	}
	const FString Ende = FString::Printf(TEXT("|%d"), Stufe - 1);
	return Kern->mBohrErfolge.ContainsByPredicate([&Ende](const FString& E) { return E.EndsWith(Ende); });
}

FString UTDWaveSubsystem::BohrPruefung(ATDDefenseCore* Kern, AFGResourceNode** OutKnoten)
{
	if (!Kern)
	{
		return TDT(TEXT("Kein Abwehrkern."), TEXT("No Defense Core."));
	}
	if (!UTDKernplatz::IstAbwehr(this))
	{
		return TDT(TEXT("Bohrungen gibt es nur im Spielmodus \"Freies Spiel mit Abwehr\"."), TEXT("Drilling is only available in Game Mode \"Free play with defense\"."));
	}
	if (mLauf != ETDLauf::Aus)
	{
		return TDT(TEXT("Es läuft schon eine Bohrung."), TEXT("A drilling is already running."));
	}
	// Rohstoffknoten in der Naehe (fest, kein Oel/Wasser/Geysir)
	const float Radius = TDBalance::Wert(TEXT("Bohrung.KnotenRadius"), 30.f) * 100.f;
	AFGResourceNode* Bester = nullptr;
	float BesterAbstand = Radius;
	for (TActorIterator<AFGResourceNode> It(GetWorld()); It; ++It)
	{
		const float D = FVector::Dist(It->GetActorLocation(), Kern->GetActorLocation());
		const TSubclassOf<UFGResourceDescriptor> Art = It->GetResourceClass();
		// 16.11. (Nachpruefung F5, Marius): nur echte Knoten, an denen ein Miner steht - keine Rohstoffbrocken (AFGResourceDeposit)
		if (D < BesterAbstand && Art && UFGItemDescriptor::GetForm(Art) == EResourceForm::RF_SOLID && !It->IsA<AFGResourceDeposit>())
		{
			Bester = *It;
			BesterAbstand = D;
		}
	}
	if (!Bester)
	{
		return TDF(TEXT("Kein fester Rohstoffknoten im Umkreis von %.0f m. Stell den Abwehrkern direkt neben einen Erz-, Kohle- oder Kalkknoten."),
			TEXT("No solid resource node within %.0f m. Place the Defense Core right next to an ore, coal or limestone node."), Radius / 100.f);
	}
	// Wege wie beim Kernplatz pruefen und im Klartext sagen, was fehlt
	FrontenFuer(Kern);
	if (mGueltigeRichtungen <= 0)
	{
		return TDT(TEXT("Kein begehbarer Weg zum Bohrer: ringsum Klippen, Wasser oder Gebäude. Stell den Kern in offeneres Gelände oder räume einen Zugang frei (Wegvorschau: \"Wegvorschau an\")."),
			TEXT("No walkable path to the drill: cliffs, water or buildings all around. Move the Core to more open ground or clear an approach (\"Path preview on\")."));
	}
	if (OutKnoten)
	{
		*OutKnoten = Bester;
	}
	return FString();
}

TArray<FString> UTDWaveSubsystem::BohrInfo(ATDDefenseCore* Kern)
{
	TArray<FString> Zeilen;
	if (IstBohrung())
	{
		Zeilen.Add(TDF(TEXT("Bohrung %s läuft: %s, bisher %d Erz. Abbrechen geht jederzeit ohne Strafe."), TEXT("%s drilling running: %s, %d ore so far. You can abort at any time without penalty."),
			*BohrStufenName(mBohrStufe), *mBohrRohstoff, mBohrErz));
		return Zeilen;
	}
	AFGResourceNode* Knoten = nullptr;
	const FString Fehler = BohrPruefung(Kern, &Knoten);
	if (!Fehler.IsEmpty())
	{
		Zeilen.Add(Fehler);
		return Zeilen;
	}
	const TSubclassOf<UFGResourceDescriptor> Art = Knoten->GetResourceClass();
	const FString Name = UFGItemDescriptor::GetItemName(Art).ToString();
	Zeilen.Add(TDF(TEXT("Knoten: %s (%s), %d von 16 Richtungen begehbar."), TEXT("Node: %s (%s), %d of 16 directions walkable."),
		*Name, *Knoten->GetResourcePurityText().ToString(), mGueltigeRichtungen));
	for (int32 S = 0; S < 3; ++S)
	{
		const bool bErst = !Kern->mBohrErfolge.Contains(ErfolgKey(Art->GetPathName(), S));
		const int32 Erz = FMath::RoundToInt(BohrWert(S, TEXT("ErzJeWelle"), TDBohrStufen[S].ErzJeWelle) * Reinheit(Knoten));
		const int32 Wellen = TDBohrStufen[S].BisWelle - TDBohrStufen[S].VonWelle + 1;
		FString Lohn = TDF(TEXT("%d Wellen, je Welle %d %s"), TEXT("%d waves, %d %s per wave"), Wellen, Erz, *Name);
		if (bErst)
		{
			Lohn += TDF(TEXT(", Erstlohn %d Festplatten"), TEXT(", first-time reward %d Hard Drives"), BohrWert(S, TEXT("Festplatten"), TDBohrStufen[S].Festplatten));
			if (BohrWert(S, TEXT("Talentpunkte"), TDBohrStufen[S].Talentpunkte) > 0)
			{
				Lohn += TDT(TEXT(" und 1 Talentpunkt"), TEXT(" and 1 Talent Point"));
			}
		}
		Zeilen.Add(FString::Printf(TEXT("%s: %s%s"), *BohrStufenName(S), *Lohn,
			BohrStufeFrei(Kern, S) ? TEXT("") : *TDF(TEXT(" (erst nach einem Erfolg auf %s)"), TEXT(" (after a success on %s)"), *BohrStufenName(S - 1))));
	}
	return Zeilen;
}

bool UTDWaveSubsystem::StarteBohrung(ATDDefenseCore* Kern, int32 Stufe, FString& OutMeldung)
{
	Stufe = FMath::Clamp(Stufe, 0, 2);
	AFGResourceNode* Knoten = nullptr;
	OutMeldung = BohrPruefung(Kern, &Knoten);
	if (!OutMeldung.IsEmpty())
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Bohrung: Start abgelehnt - %s"), *OutMeldung);
		return false;
	}
	if (!BohrStufeFrei(Kern, Stufe))
	{
		OutMeldung = TDF(TEXT("%s ist erst nach einem Erfolg auf %s frei."), TEXT("%s unlocks after a success on %s."), *BohrStufenName(Stufe), *BohrStufenName(Stufe - 1));
		return false;
	}
	const FTDBohrStufe& B = TDBohrStufen[Stufe];
	mBohrung = true;
	mBohrStufe = Stufe;
	mBohrKnoten = Knoten;
	mBohrRohstoff = UFGItemDescriptor::GetItemName(Knoten->GetResourceClass()).ToString();
	mBohrReinheit = Reinheit(Knoten);
	mBohrErz = 0;
	mBohrEndWelle = BohrWert(Stufe, TEXT("BisWelle"), B.BisWelle);
	SetManuell(false);
	SetEndlos(false);
	if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
	{
		Einsatz->SetzeAbwehrKern(Kern);
	}
	if (!StarteKernVerteidigung(Kern, mBohrEndWelle, TDBalance::Wert(TEXT("Bohrung.Countdown"), 60.f), TDBalance::Wert(TEXT("Bohrung.Abstand"), 75.f), 1))
	{
		mBohrung = false;
		OutMeldung = TDT(TEXT("Die Bohrung konnte nicht starten."), TEXT("The drilling could not start."));
		return false;
	}
	// Wellen aus der passenden Phase: die Zaehlung beginnt bei VonWelle
	mWelle = FMath::Max(0, BohrWert(Stufe, TEXT("VonWelle"), B.VonWelle) - 1);
	Kern->SetzeMaxLP(static_cast<float>(BohrWert(Stufe, TEXT("LP"), FMath::RoundToInt(B.LP))));
	Kern->Aufladen();
	SpeichereLauf();
	MarkiereFronten();
	UE_LOG(LogFicsitTD, Log, TEXT("Bohrung gestartet: %s auf %s (Reinheit x%.1f), Wellen %d bis %d, Kern %d LP"), B.Key, *mBohrRohstoff, mBohrReinheit, mWelle + 1, mBohrEndWelle,
		BohrWert(Stufe, TEXT("LP"), FMath::RoundToInt(B.LP)));
	OutMeldung = TDF(TEXT("Bohrung %s auf %s gestartet: %d Wellen, die erste in %.0f s. Abbrechen geht jederzeit ohne Strafe."),
		TEXT("%s drilling on %s started: %d waves, the first in %.0f s. You can abort at any time without penalty."),
		*BohrStufenName(Stufe), *mBohrRohstoff, mBohrEndWelle - mWelle, mCountdown);
	return true;
}

void UTDWaveSubsystem::BohrWelleBeendet(float KernAnteil)
{
	// wiederholbarer Ertrag: Erz des Knotens je abgewehrter Welle (Reinheit), ins Inventar oder wartend am Kern
	const AFGResourceNode* Knoten = mBohrKnoten.Get();
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (!Knoten || !Einsatz)
	{
		return;
	}
	const int32 Erz = FMath::RoundToInt(BohrWert(mBohrStufe, TEXT("ErzJeWelle"), TDBohrStufen[mBohrStufe].ErzJeWelle) * mBohrReinheit);
	mBohrErz += Erz;
	const FString Text = Einsatz->GibMaterial({ { Knoten->GetResourceClass().Get(), Erz } });
	UE_LOG(LogFicsitTD, Log, TEXT("Bohrung: Welle %d abgewehrt, +%d %s (gesamt %d), Kern %.0f %%"), mWelle, Erz, *mBohrRohstoff, mBohrErz, KernAnteil);
	// 16.11. (Fahrplan P3, Simulation): der Bohrer repariert sich zwischen den Wellen (sonst summierte sich der Verlust, Mittel/Schwer
	// brauchten 33/57 Bolzentuerme). Bohrung.Reparatur = Anteil der Kern-LP je abgewehrter Welle.
	if (ATDDefenseCore* Kern = mKern.Get())
	{
		Kern->HealTD(Kern->GetTDMaxHealth() * TDBalance::Wert(TEXT("Bohrung.Reparatur"), 0.1f));
	}
	Ansage(TDF(TEXT("Bohrung: Welle %d abgewehrt - %s."), TEXT("Drilling: wave %d repelled - %s."), mWelle, *Text));
}

void UTDWaveSubsystem::BohrungEnde(bool bErfolg, const FString& Grund)
{
	ATDDefenseCore* Kern = mKern.Get();
	const int32 Stufe = mBohrStufe;
	const FTDBohrStufe& B = TDBohrStufen[Stufe];
	mBohrung = false;
	TArray<FString> Teile;
	Teile.Add(Grund);
	if (bErfolg && Kern && mBohrKnoten.IsValid())
	{
		const FString Key = ErfolgKey(mBohrKnoten->GetResourceClass()->GetPathName(), Stufe);
		if (!Kern->mBohrErfolge.Contains(Key))
		{
			Kern->mBohrErfolge.Add(Key);
			const int32 FP = BohrWert(Stufe, TEXT("Festplatten"), B.Festplatten);
			Kern->mFPOffen += FMath::Max(0, FP);
			if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
			{
				Einsatz->GibOffeneFP();
			}
			Teile.Add(TDF(TEXT("Erstlohn: %d Festplatten"), TEXT("First-time reward: %d Hard Drives"), FP));
			const int32 TP = BohrWert(Stufe, TEXT("Talentpunkte"), B.Talentpunkte);
			if (TP > 0)
			{
				Kern->mPionierBonusPunkte += TP;
				Teile.Add(TDF(TEXT("+%d Talentpunkt (Reiter Pionier)"), TEXT("+%d Talent Point (Pioneer tab)"), TP));
			}
			UE_LOG(LogFicsitTD, Log, TEXT("Bohrung: Erstlohn %s - %d Festplatten, %d Talentpunkte"), *Key, FP, TP);
		}
		const int32 XP = BohrWert(Stufe, TEXT("XP"), B.XP);
		Kern->mPionierXP += XP;
		Teile.Add(TDF(TEXT("+%d Pionier-XP"), TEXT("+%d Pioneer XP"), XP));
	}
	Teile.Add(TDF(TEXT("Ertrag %d %s"), TEXT("yield %d %s"), mBohrErz, *mBohrRohstoff));
	UE_LOG(LogFicsitTD, Log, TEXT("Bohrung beendet (%s, %s): %d Erz"), B.Key, bErfolg ? TEXT("Erfolg") : TEXT("ohne Erfolg"), mBohrErz);
	StoppeLauf(FString::Join(Teile, TEXT(". ")) + TEXT("."));
	if (Kern)
	{
		// der Bohrer geht nie verloren: Lauf zu Ende, Kern voll, kein Ende-Menue
		Kern->mLaufEnde = 0;
		Kern->mLaufBohrung = false;
		Kern->Aufladen();
	}
}

FString UTDWaveSubsystem::BrecheBohrungAb()
{
	if (!IstBohrung())
	{
		return TDT(TEXT("Es läuft keine Bohrung."), TEXT("No drilling is running."));
	}
	BohrungEnde(false, TDT(TEXT("Bohrung abgebrochen - ohne Strafe"), TEXT("Drilling aborted - no penalty")));
	return TDT(TEXT("Bohrung abgebrochen. Der Bohrer ist wieder voll."), TEXT("Drilling aborted. The drill is fully repaired."));
}
