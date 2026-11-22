// 0.13 K1 Kern-Zugang (Satisfactory/Notizen/Fahrplan-0.13-2026-11-21.md, Bericht A1 Abschnitt 5.1):
// Ist der Abwehrkern fuer Wellengegner erreichbar? Vor jeder Welle je aktiver Front einmal pruefen. Bei einem Teilweg die Sperrstelle
// suchen (fuenf Kapselfahrten vom Wegende Richtung Kern) und den sperrenden Bau bestimmen. Riegelt ein Bau des Spielers den Kern ab,
// startet die Welle nicht (Einsatz, Endlos, zwischen Bohrwellen), und eine Bohrung startet gar nicht erst. Nichts wird abgerissen
// (Marius 21.11.: Fabrikgebaeude werden nie zerstoert). TD-Hindernisse (Mauer, Sperre, Testmauer, Tor, Elektrozaun) duerfen den Weg
// schliessen - an ihnen greifen die Gegner an ("Durchbrechbar").

#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "TDSicherheitsnetz.h"
#include "TDEinsatz.h"
#include "TDGegnerHirn.h"
#include "TDNavFilter.h"
#include "TDDefenseCore.h"
#include "TDDefenseWall.h"
#include "TDBarrier.h"
#include "TDTestWall.h"
#include "TDGate.h"
#include "TDBautenErweitert.h"
#include "TDDecoy.h"
#include "FicsitTDModule.h"
#include "AbstractInstanceManager.h"
#include "InstanceData.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Buildables/FGBuildable.h"
#include "FGRecipe.h"
#include "FGMapManager.h"
#include "FGMapMarker.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

namespace
{
	/** Wie UTDGegnerHirn IstHindernis: Bauten, die Wellengegner angreifen, wenn sie den Weg sperren */
	bool TDKZIstTDHindernis(const UClass* Klasse)
	{
		return Klasse && (Klasse->IsChildOf(ATDDefenseWall::StaticClass()) || Klasse->IsChildOf(ATDBarrier::StaticClass())
			|| Klasse->IsChildOf(ATDTestWall::StaticClass()) || Klasse->IsChildOf(ATDGate::StaticClass()) || Klasse->IsChildOf(ATDConductorWall::StaticClass()));
	}

	/** Himmelsrichtung vom Kern aus (Karte: +X Osten, +Y Sueden), wie Himmelsrichtung() im Wellensystem */
	FString TDKZRichtung(const FVector& Kern, const FVector& Ort, bool bDeutsch)
	{
		const float Winkel = FMath::RadiansToDegrees(FMath::Atan2(Ort.Y - Kern.Y, Ort.X - Kern.X));
		const int32 i = FMath::RoundToInt(FRotator::NormalizeAxis(Winkel + 90.f) / 45.f + 8.f) % 8;
		static const TCHAR* De[] = { TEXT("Norden"), TEXT("Nordosten"), TEXT("Osten"), TEXT("Südosten"), TEXT("Süden"), TEXT("Südwesten"), TEXT("Westen"), TEXT("Nordwesten") };
		static const TCHAR* Lg[] = { TEXT("Norden"), TEXT("Nordosten"), TEXT("Osten"), TEXT("Suedosten"), TEXT("Sueden"), TEXT("Suedwesten"), TEXT("Westen"), TEXT("Nordwesten") };
		static const TCHAR* En[] = { TEXT("north"), TEXT("northeast"), TEXT("east"), TEXT("southeast"), TEXT("south"), TEXT("southwest"), TEXT("west"), TEXT("northwest") };
		return bDeutsch ? FString(Lg[i]) : TDT(De[i], En[i]);
	}

	/** Ein getroffener Bau (Actor oder Lightweight-Instanz) */
	struct FTDKZKandidat
	{
		const UClass* Klasse = nullptr;
		TSubclassOf<UFGRecipe> Rezept;
		const AActor* Actor = nullptr;
		FVector Ort = FVector::ZeroVector;      // Auftreffpunkt
		FVector BauOrt = FVector::ZeroVector;   // Lage des Baus (Schluessel fuer Doppelte)
		FVector Richtung = FVector::ForwardVector;
		float Abstand = 0.f;
		bool bTD = false;
	};

	/** Treffer in einen Bau aufloesen. false = kein Bau (Gelaende, Felsen, Baum), Logistik, Koeder, Kern */
	bool TDKZLoese(UWorld* World, const FHitResult& Hit, const ATDDefenseCore* Kern, FTDKZKandidat& Out, bool& bOutLogistik)
	{
		const AActor* Actor = Hit.GetActor();
		const UClass* Klasse = nullptr;
		TSubclassOf<UFGRecipe> Rezept;
		FVector BauOrt = Hit.ImpactPoint;
		// Instanzierte Modelle (Maschinen, Fundamente, Waende): der Treffer gehoert dem Instanz-Manager, nicht dem Bau
		AAbstractInstanceManager* Manager = AAbstractInstanceManager::GetInstanceManager(World);
		FInstanceHandle Handle;
		if (Manager && Manager->ResolveHit(Hit, Handle))
		{
			FLightweightBuildableInstanceRef Ref;
			if (const AActor* Besitzer = Handle.GetOwner(); Besitzer && Besitzer->IsA<AFGBuildable>())
			{
				Actor = Besitzer;
			}
			else if (AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Handle, Ref))
			{
				Actor = nullptr;
				Klasse = Ref.GetBuildableClass();
				Rezept = Ref.GetBuiltWithRecipe();
				BauOrt = Ref.GetBuildableTransform().GetLocation();
			}
		}
		if (Actor)
		{
			if (Actor == Kern || Actor->IsA<ATDDecoy>())
			{
				return false;
			}
			const AFGBuildable* Bau = Cast<AFGBuildable>(Actor);
			if (!Bau)
			{
				return false;   // Gelaende, Felsen, Pflanzen: das meldet die Pruefung als "Gelaende"
			}
			Klasse = Bau->GetClass();
			Rezept = Bau->GetBuiltWithRecipe();
			BauOrt = Bau->GetActorLocation();
		}
		if (!Klasse)
		{
			return false;
		}
		if (UTDEinsatzSubsystem::IstLogistik(Klasse))
		{
			bOutLogistik = true;   // Logistik ist fuer Wellengegner kein Hindernis (EntschaerfeLogistik beim Spawnen der Welle)
			return false;
		}
		Out.Klasse = Klasse;
		Out.Rezept = Rezept;
		Out.Actor = Actor;
		Out.Ort = Hit.ImpactPoint;
		Out.BauOrt = BauOrt;
		Out.bTD = TDKZIstTDHindernis(Klasse);
		return true;
	}

	FString TDKZName(const FTDKZKandidat& K)
	{
		if (K.Rezept)
		{
			const FString Name = UFGRecipe::GetRecipeName(K.Rezept).ToString();
			if (!Name.IsEmpty())
			{
				return Name;
			}
		}
		FString N = K.Klasse ? K.Klasse->GetName() : FString(TEXT("?"));
		N.RemoveFromStart(TEXT("Build_"));
		N.RemoveFromEnd(TEXT("_C"));
		return N;
	}
}

const TCHAR* UTDWaveSubsystem::KernZugangName(ETDKernZugang Zustand)
{
	switch (Zustand)
	{
	case ETDKernZugang::Frei:            return TEXT("Frei");
	case ETDKernZugang::Durchbrechbar:   return TEXT("Durchbrechbar");
	case ETDKernZugang::NetzNichtBereit: return TEXT("NetzNichtBereit");
	default:                             return TEXT("Unpassierbar");
	}
}

FTDKernZugang UTDWaveSubsystem::PruefeZugangEinzeln(const ATDDefenseCore* Kern, const FVector& Start, int32 Front) const
{
	FTDKernZugang Z;
	Z.Front = Front;
	UWorld* World = GetWorld();
	UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Kern || !Nav)
	{
		return Z;
	}
	// 22.11. (Lauf Kern-Zugang-K1: im selben Bild wie die Ruecknahme von Klippensperren "Unpassierbar", 2 s spaeter "Frei"):
	// waehrend eines Netzumbaus nicht urteilen
	if (Nav->IsNavigationBuildInProgress())
	{
		Z.Zustand = ETDKernZugang::NetzNichtBereit;
		Z.SperreKlasse = TEXT("Netzumbau");
		return Z;
	}
	bool bVoll = false;
	const TArray<FVector> Weg = UTDGegnerHirn::BerechneWeg(World, Start, Kern, &bVoll);
	++Z.Wegsuchen;
	if (Weg.Num() == 0)
	{
		Z.Zustand = ETDKernZugang::NetzNichtBereit;   // wie PruefeFrontenVorWelle: 0 Punkte = Netz nicht bereit
		return Z;
	}
	if (bVoll)
	{
		Z.Zustand = ETDKernZugang::Frei;
		return Z;
	}
	// Teilweg: Sperrstelle suchen. Kapsel wie die Klippenprobe (Radius 70 cm), Mitte 1,2 m ueber dem Wegende, damit sie nicht im Boden streift.
	const FVector KernOrt = Kern->GetActorLocation();
	const FVector Ende = Weg.Last();
	FNavLocation ZielNav;
	if (!Nav->ProjectPointToNavigation(KernOrt, ZielNav, FVector(1200.f, 1200.f, 1000.f)))
	{
		Z.Zustand = ETDKernZugang::NetzNichtBereit;
		return Z;
	}
	const float Laenge = FMath::Min(TDBalance::Wert(TEXT("KernZugang.StrahlLaenge"), 12.f) * 100.f, FVector::Dist2D(Ende, KernOrt));
	const FVector Basis = (FVector(KernOrt.X, KernOrt.Y, Ende.Z) - Ende).GetSafeNormal2D();
	const FVector Anfang = Ende + FVector(0.f, 0.f, 120.f);
	const FCollisionShape Kapsel = FCollisionShape::MakeCapsule(70.f, 90.f);
	FCollisionQueryParams Query(TEXT("TDKernZugang"), false);
	FCollisionObjectQueryParams Objekte;
	Objekte.AddObjectTypesToQuery(ECC_WorldStatic);
	Objekte.AddObjectTypesToQuery(ECC_WorldDynamic);
	TArray<FTDKZKandidat> Kandidaten;
	bool bGelaende = false;
	bool bLogistik = false;
	for (const float Winkel : { 0.f, 20.f, -20.f, 40.f, -40.f })
	{
		const FVector Richtung = Basis.RotateAngleAxis(Winkel, FVector::UpVector);
		TArray<FHitResult> Treffer;
		World->SweepMultiByObjectType(Treffer, Anfang, Anfang + Richtung * Laenge, FQuat::Identity, Objekte, Kapsel, Query);
		for (const FHitResult& Hit : Treffer)
		{
			FTDKZKandidat K;
			if (!TDKZLoese(World, Hit, Kern, K, bLogistik))
			{
				bGelaende |= Hit.GetActor() == nullptr || !Hit.GetActor()->IsA<AFGBuildable>();
				continue;
			}
			K.Richtung = Richtung;
			K.Abstand = FVector::Dist(Anfang, Hit.ImpactPoint);
			FTDKZKandidat* Gleich = Kandidaten.FindByPredicate([&K](const FTDKZKandidat& A)
			{
				return A.Klasse == K.Klasse && (K.Actor ? A.Actor == K.Actor : FVector::DistSquared(A.BauOrt, K.BauOrt) < 100.f);
			});
			if (!Gleich)
			{
				Kandidaten.Add(K);
			}
			else if (K.Abstand < Gleich->Abstand)
			{
				*Gleich = K;
			}
		}
	}
	if (Kandidaten.Num() == 0 && bLogistik)
	{
		// nur Logistik vor dem Wegende: seit der letzten Welle gebaute Baender und Masten stehen noch im Netz, bis EntschaerfeLogistik
		// beim Spawnen sie herausnimmt. Das ist kein Abriegeln - wie "Netz nicht bereit" behandeln, der Start haelt nicht.
		Z.Zustand = ETDKernZugang::NetzNichtBereit;
		Z.Sperrstelle = Ende;
		Z.SperreKlasse = TEXT("Logistik (Netzumbau)");
		return Z;
	}
	if (Kandidaten.Num() == 0)
	{
		// kein Bau vor dem Wegende: Gelaende (Klippe, Wasser) oder ein Loch im Netz. Der Wellenstart haelt das nur kurz (KernZugang.WarteOhneBau).
		Z.Zustand = ETDKernZugang::Unpassierbar;
		Z.bBau = false;
		Z.Sperrstelle = Ende;
		Z.SperreKlasse = bGelaende ? TEXT("Gelaende") : TEXT("unbekannt");
		return Z;
	}
	Kandidaten.Sort([](const FTDKZKandidat& A, const FTDKZKandidat& B) { return A.Abstand < B.Abstand; });
	// Brueckentest: hinter dem Bau einen Punkt aufs Netz projizieren und von dort den Weg zum Kern rechnen. Der erste Bau, hinter dem
	// ein voller Weg beginnt, ist die Sperrstelle. Mehrere Abstaende, weil Maschinen tief sind (Fundament 8 m, Konstruktor 10 m).
	const int32 MaxSuchen = TDBalance::Ganz(TEXT("KernZugang.MaxSuchen"), 16);
	const FTDKZKandidat* Gewaehlt = nullptr;
	for (const FTDKZKandidat& K : Kandidaten)
	{
		if (Gewaehlt || Z.Wegsuchen >= MaxSuchen)
		{
			break;
		}
		for (const float Weiter : { 150.f, 400.f, 800.f, 1200.f })
		{
			if (Z.Wegsuchen >= MaxSuchen)
			{
				break;
			}
			FNavLocation Hinter;
			if (!Nav->ProjectPointToNavigation(K.Ort + K.Richtung * Weiter, Hinter, FVector(200.f, 200.f, 400.f)))
			{
				continue;
			}
			++Z.Wegsuchen;
			const UNavigationPath* Pfad = UNavigationSystemV1::FindPathToLocationSynchronously(World, Hinter.Location, ZielNav.Location, nullptr, UTDNavFilter::StaticClass());
			if (PfadAmZiel(Pfad, ZielNav.Location))
			{
				Gewaehlt = &K;
				break;
			}
		}
	}
	// Ohne Bruecke (zwei Lagen, Kern rundum eng zugebaut): der naechste Bau gilt als Sperre. Durchbrechbar nur, wenn alles TD-Hindernisse sind.
	bool bAlleTD = true;
	for (const FTDKZKandidat& K : Kandidaten)
	{
		bAlleTD &= K.bTD;
	}
	const FTDKZKandidat& S = Gewaehlt ? *Gewaehlt : Kandidaten[0];
	Z.bBau = true;
	Z.Sperrstelle = S.Ort;
	Z.Sperre = TDKZName(S);
	Z.SperreKlasse = S.Klasse ? S.Klasse->GetName() : TEXT("?");
	Z.Zustand = (Gewaehlt ? S.bTD : bAlleTD) ? ETDKernZugang::Durchbrechbar : ETDKernZugang::Unpassierbar;
	return Z;
}

FTDKernZugang UTDWaveSubsystem::PruefeKernZugangVon(const ATDDefenseCore* Kern, const TArray<FVector>& Starts, bool bRingprobe) const
{
	const double T0 = FPlatformTime::Seconds();
	FTDKernZugang Gesamt;
	Gesamt.Zeit = T0;
	if (!Kern)
	{
		return Gesamt;
	}
	TArray<FVector> Orte = Starts;
	if (bRingprobe && Orte.Num() == 0)
	{
		// ohne Fronten (Bohrung vor dem Start): 8 Richtungen in zwei Abstaenden
		const FVector Mitte = Kern->GetActorLocation();
		for (const float Abstand : { TDBalance::Wert(TEXT("KernZugang.RingAbstand1"), 60.f), TDBalance::Wert(TEXT("KernZugang.RingAbstand2"), 120.f) })
		{
			for (int32 r = 0; r < 8; ++r)
			{
				Orte.Add(Mitte + FRotator(0.f, 45.f * r, 0.f).Vector() * Abstand * 100.f);
			}
		}
	}
	// Fronten: der schlechteste Zustand zaehlt (jede aktive Front muss durchkommen). Ringprobe: der beste (irgendein Zugang reicht).
	bool bErster = true;
	int32 Suchen = 0;
	for (int32 i = 0; i < Orte.Num(); ++i)
	{
		FTDKernZugang Z = PruefeZugangEinzeln(Kern, Orte[i], bRingprobe ? 0 : i + 1);
		Suchen += Z.Wegsuchen;
		// 22.11. (Lauf K1b, KZ-6: Ringprobe "NetzNichtBereit", obwohl 14-m-Ring geschlossen): ein Probenpunkt ohne Netz (Klippe,
		// Wasser) sagt nichts ueber den Zugang - in der Ringprobe nur zaehlen, wenn kein anderer Punkt urteilt
		if (bRingprobe && Z.Zustand == ETDKernZugang::NetzNichtBereit && !bErster && Gesamt.Zustand != ETDKernZugang::NetzNichtBereit)
		{
			continue;
		}
		const bool bNetzlos = bRingprobe && Gesamt.Zustand == ETDKernZugang::NetzNichtBereit && Z.Zustand != ETDKernZugang::NetzNichtBereit;
		const bool bBesser = bNetzlos || (bRingprobe ? static_cast<uint8>(Z.Zustand) < static_cast<uint8>(Gesamt.Zustand)
			: static_cast<uint8>(Z.Zustand) > static_cast<uint8>(Gesamt.Zustand));
		// Bei gleichem Zustand die Sperre mit erkanntem Bau nehmen (bessere Meldung)
		if (bErster || bBesser || (Z.Zustand == Gesamt.Zustand && Z.bBau && !Gesamt.bBau))
		{
			Gesamt = Z;
			bErster = false;
		}
		if (bRingprobe && Gesamt.Zustand == ETDKernZugang::Frei)
		{
			break;
		}
	}
	Gesamt.Wegsuchen = Suchen;
	Gesamt.Zeit = T0;
	Gesamt.Ms = (FPlatformTime::Seconds() - T0) * 1000.0;
	return Gesamt;
}

const FTDKernZugang& UTDWaveSubsystem::PruefeKernZugang(bool bErzwingen)
{
	const double Jetzt = FPlatformTime::Seconds();
	if (!bErzwingen && Jetzt - mKernZugang.Zeit < TDBalance::Wert(TEXT("KernZugang.Haltezeit"), 5.f))
	{
		return mKernZugang;
	}
	const ATDDefenseCore* Kern = mKern.Get();
	TArray<FVector> Starts;
	const int32 Naechste = mLauf == ETDLauf::Welle ? FMath::Max(1, mWelle) : mWelle + 1;
	for (int32 i = 0; i < AktiveFrontenFuer(Naechste) && mFronten.IsValidIndex(i); ++i)
	{
		Starts.Add(mFronten[i].Ort);
	}
	const FTDKernZugang Alt = mKernZugang;
	mKernZugang = PruefeKernZugangVon(Kern, Starts, Starts.Num() == 0);
	if (mKernZugang.Zustand != Alt.Zustand || mKernZugang.Sperre != Alt.Sperre || mZugangWarte <= 0.f)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("KernZugang: Zustand %s, %d Wegsuchen, %.2f ms, Front %d, Sperre %s (%s)%s"), KernZugangName(mKernZugang.Zustand),
			mKernZugang.Wegsuchen, mKernZugang.Ms, mKernZugang.Front, mKernZugang.Sperre.IsEmpty() ? TEXT("-") : *mKernZugang.Sperre, *mKernZugang.SperreKlasse,
			mKernZugang.Sperre.IsEmpty() || !Kern ? TEXT("")
			: *FString::Printf(TEXT(" bei %.0f m %s"), FVector::Dist2D(mKernZugang.Sperrstelle, Kern->GetActorLocation()) / 100.f,
				*TDKZRichtung(Kern->GetActorLocation(), mKernZugang.Sperrstelle, true)));
	}
	return mKernZugang;
}

FString UTDWaveSubsystem::KernZugangText(const FTDKernZugang& Z, const ATDDefenseCore* FuerKern) const
{
	// 22.11. (KZ-6): vor dem Bohrungsstart ist mKern noch nicht gesetzt - dann den gepruefen Kern nehmen
	const ATDDefenseCore* Kern = FuerKern ? FuerKern : mKern.Get();
	switch (Z.Zustand)
	{
	case ETDKernZugang::Frei:
		return TDT(TEXT("Kern-Zugang: frei."), TEXT("Core access: open."));
	case ETDKernZugang::Durchbrechbar:
		return TDT(TEXT("Kern-Zugang: nur durch Abwehrmauern. Die Gegner greifen sie an."), TEXT("Core access: only through defense walls. The enemies will attack them."));
	case ETDKernZugang::NetzNichtBereit:
		return TDT(TEXT("Kern-Zugang: Navigationsnetz noch im Aufbau."), TEXT("Core access: navigation mesh still building."));
	default:
		break;
	}
	if (!Z.bBau || !Kern)
	{
		return TDT(TEXT("Kern-Zugang: kein Weg zum Kern (Gelände)."), TEXT("Core access: no path to the Core (terrain)."));
	}
	const float Meter = FVector::Dist2D(Z.Sperrstelle, Kern->GetActorLocation()) / 100.f;
	return TDF(TEXT("Kern-Zugang: abgeriegelt durch %s, %.0f m %s vom Kern. Lass dort eine Lücke oder sperre mit einer Abwehrmauer. Bis dahin startet keine Welle."),
		TEXT("Core access: sealed off by %s, %.0f m %s of the Core. Leave a gap there or block it with a defense wall. Until then no wave will start."),
		*Z.Sperre, Meter, *TDKZRichtung(Kern->GetActorLocation(), Z.Sperrstelle, false));
}

bool UTDWaveSubsystem::HalteStartFuerKernZugang(float DeltaTime)
{
	if (TDBalance::Wert(TEXT("KernZugang.Aktiv"), 1.f) <= 0.f || TDBalance::Wert(TEXT("KernZugang.StartWarten"), 1.f) <= 0.f)
	{
		return false;
	}
	const bool bErst = mZugangWarte <= 0.f;
	mZugangPruefUhr -= DeltaTime;
	bool bGeprueft = false;
	if (bErst || mZugangPruefUhr <= 0.f)
	{
		mZugangPruefUhr = TDBalance::Wert(TEXT("KernZugang.PruefTakt"), 2.f);
		PruefeKernZugang(true);
		bGeprueft = true;
	}
	const FTDKernZugang& Z = mKernZugang;
	// Ein erkannter Bau haelt den Start, bis der Weg frei ist. Ohne erkannten Bau (Gelaende, Loch im Netz) nur kurz - sonst sperrte
	// ein Netzfehler den Einsatz fuer immer; danach laeuft der Start wie bisher (Nachzuegler-Netz). Ein laufender Netzumbau haelt
	// den Start hoechstens KernZugang.WarteNetz s (danach urteilt die Pruefung wieder).
	const bool bNetzumbau = Z.Zustand == ETDKernZugang::NetzNichtBereit && Z.SperreKlasse == TEXT("Netzumbau");
	mZugangNetzWarte = bNetzumbau ? mZugangNetzWarte + DeltaTime : 0.f;
	const bool bHalten = (Z.IstGesperrt() && (Z.bBau || mZugangWarte < TDBalance::Wert(TEXT("KernZugang.WarteOhneBau"), 20.f)))
		|| (bNetzumbau && mZugangNetzWarte < TDBalance::Wert(TEXT("KernZugang.WarteNetz"), 10.f));
	if (bHalten)
	{
		if (bErst)
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Wellenstart %d haelt: KernZugang %s (Sperre %s, %s) - pruefe in %.0f s erneut"), mWelle + 1, KernZugangName(Z.Zustand),
				Z.Sperre.IsEmpty() ? TEXT("-") : *Z.Sperre, *Z.SperreKlasse, mZugangPruefUhr);
			mZugangWarte = KINDA_SMALL_NUMBER;
			mZugangGemeldet = false;
		}
		mZugangWarte += DeltaTime;
		// Melden erst, wenn eine zweite Pruefung die Abriegelung bestaetigt (keine Ansage fuer einen Zwischenstand des Netzes)
		if (!mZugangGemeldet && !bErst && bGeprueft && Z.IstGesperrt() && Z.bBau)
		{
			mZugangGemeldet = true;
			UE_LOG(LogFicsitTD, Warning, TEXT("Wellenstart %d wartet: Kern abgeriegelt (Front %d, Sperre %s, %s)"), mWelle + 1, Z.Front, *Z.Sperre, *Z.SperreKlasse);
			TDNetz::Melde(TDNetz::EArt::Abriegelung);
			Ansage(KernZugangText(Z));
			MarkiereSperrstelle(true);
			mZugangAnsageUhr = TDBalance::Wert(TEXT("KernZugang.Wiederholung"), 30.f);
		}
		else if (mZugangGemeldet && Z.bBau)
		{
			mZugangAnsageUhr -= DeltaTime;
			if (mZugangAnsageUhr <= 0.f)
			{
				mZugangAnsageUhr = TDBalance::Wert(TEXT("KernZugang.Wiederholung"), 30.f);
				Ansage(KernZugangText(Z));
				MarkiereSperrstelle(true);
			}
		}
		mCountdown = 0.f;
		return true;
	}
	if (mZugangWarte > 0.f)
	{
		if (!Z.IstGesperrt())
		{
			UE_LOG(LogFicsitTD, Log, TEXT("Wellenstart %d nach %.1f s: Kern wieder erreichbar (%s)"), mWelle + 1, mZugangWarte, KernZugangName(Z.Zustand));
			if (mZugangGemeldet)
			{
				Ansage(TDT(TEXT("Kern-Zugang wieder frei. Die Welle startet."), TEXT("Core access open again. The wave is starting.")));
			}
		}
		else
		{
			UE_LOG(LogFicsitTD, Warning, TEXT("Wellenstart %d nach %.1f s ohne erkannten Bau vor der Sperre (%s) - Start wie bisher"), mWelle + 1, mZugangWarte, *Z.SperreKlasse);
		}
		MarkiereSperrstelle(false);
		mZugangWarte = 0.f;
		mZugangPruefUhr = 0.f;
		mZugangGemeldet = false;
	}
	return false;
}

void UTDWaveSubsystem::MarkiereSperrstelle(bool bZeigen)
{
	static const FString Kategorie = TEXT("FicsitTD Kern-Zugang");
	AFGMapManager* Karte = AFGMapManager::Get(GetWorld());
	if (!Karte || !GetWorld()->GetAuthGameMode())
	{
		return;
	}
	TArray<FMapMarker> Alle;
	Karte->GetMapMarkers(Alle);
	for (const FMapMarker& M : Alle)
	{
		if (M.CategoryName == Kategorie)
		{
			// gleiche Stelle schon markiert: nichts tun (das Kern-Menue fragt die Bohrungspruefung oft ab)
			if (bZeigen && FVector::DistSquared(M.Location, mKernZugang.Sperrstelle) < 10000.f)
			{
				return;
			}
			Karte->RemoveMapMarker(M);
		}
	}
	if (!bZeigen || !mKernZugang.bBau)
	{
		return;
	}
	FMapMarker M;
	M.Location = mKernZugang.Sperrstelle;
	M.Name = TDF(TEXT("Kern abgeriegelt: %s"), TEXT("Core sealed off: %s"), *mKernZugang.Sperre);
	M.CategoryName = Kategorie;
	M.MapMarkerType = ERepresentationType::RT_MapMarker;
	M.IconID = 0;
	M.Color = FLinearColor(1.f, 0.55f, 0.f);
	M.Scale = 1.f;
	M.CompassViewDistance = ECompassViewDistance::CVD_Always;
	FMapMarker Neu;
	const bool bGesetzt = Karte->CanAddNewMapMarker() && Karte->AddNewMapMarker(M, Neu);
	UE_LOG(LogFicsitTD, Log, TEXT("KernZugang: Kartenmarke %s bei %s (%s)"), bGesetzt ? TEXT("gesetzt") : TEXT("nicht gesetzt"),
		*mKernZugang.Sperrstelle.ToCompactString(), *mKernZugang.SperreKlasse);
}
