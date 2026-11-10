#include "TDBoltTurret.h"
#include "TDLeistung.h"
#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "Creature/FGCreature.h"
#include "Engine/World.h"
#include "Equipment/FGAmmoTypeProjectile.h"
#include "FGProjectile.h"
#include "FGProjectileMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TDSprache.h"
#include "TDGate.h"
#include "EngineUtils.h"

/** Level 5 Doppellauf (0.64): zweiter Lauf neben dem ersten, Versatz im Kanzel-Raum (Modelle\TD_BoltTurret_Stufen.blend) */
static const FVector BolzenZweiterLauf(0.f, 95.f, 0.f);   // neben dem ersten (Blender -0,95 m = Unreal +95)

static const TCHAR* TurmStandardBolzen = TEXT("/Game/FactoryGame/Resource/Parts/SpikedRebar/Desc_SpikedRebar.Desc_SpikedRebar_C");

ATDBoltTurret::ATDBoltTurret()
{
	// Masse des Blender-Modells (Ursprung Boden-Mitte): Sockel 4x4 m, Drehpunkt auf 2,25 m, Neigeachse 1,9 m darueber
	mTurmName = TEXT("Bolzenturm");
	mDrehpunkt = FVector(0.f, 0.f, 290.f);   // 0.9.3: Sockel 65 cm hoeher (Bandeingang passt in die Wand)
	mNeigeachse = FVector(5.f, 0.f, 190.f);
	mMuendung = FVector(385.f, 0.f, 5.f);
	mYawModell = TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_BoltTurret_Yaw.SM_TD_BoltTurret_Yaw");
	mHeadModell = TEXT("/FicsitTD/Buildings/BoltTurret/Mesh/SM_TD_BoltTurret_Head.SM_TD_BoltTurret_Head");
	mStandardRezept = TEXT("/FicsitTD/Buildings/BoltTurret/Recipe_TD_Shoot_SpikedRebar.Recipe_TD_Shoot_SpikedRebar_C");
	mLampenOrt = FVector(-90.f, 205.f, 340.f);   // 0.9.3: Lampe oben auf dem Original-Statusmast
	// 0.9.3 Original-Anschluesse (Lagen aus Modelle\Original_Anschluesse.json)
	OriginalTeil(TDOriginal::Eingang, FVector(-210.f, 0.f, 0.f), 90.f);
	OriginalTeil(TDOriginal::Strom, FVector(0.f, -160.f, 130.f), -90.f);
	OriginalTeil(TDOriginal::Mast, FVector(-90.f, 205.f, 0.f), 180.f);

}

FTDBolzenWerte ATDBoltTurret::WerteFuer(const UClass* MunitionsKlasse)
{
	// Jede Bolzenart in jeder Stufe (Wunsch Marius): die Art bestimmt Schaden und Effekt, die Stufe nur den Turm selbst.
	// Werte aus der Balance-Datei (Munition.Bolzen.<Art>.*)
	FTDBolzenWerte W;
	const FString Name = MunitionsKlasse ? MunitionsKlasse->GetName() : FString();
	FString Art = TEXT("Armierungsbolzen");   // auch Rohbolzen und Unbekanntes
	if (Name.Contains(TEXT("Durchschlag")))     Art = TEXT("Durchschlagbolzen");
	else if (Name.Contains(TEXT("Explosive")))  Art = TEXT("Explosivbolzen");
	else if (Name.Contains(TEXT("Stunshot")))   Art = TEXT("Betaeubungsbolzen");
	else if (Name.Contains(TEXT("Spreadshot"))) Art = TEXT("Streubolzen");
	const FString V = TEXT("Munition.Bolzen.") + Art + TEXT(".");
	W.Schaden = TDBalance::Wert(V + TEXT("Schaden"), W.Schaden);
	if (Art == TEXT("Durchschlagbolzen"))
	{
		W.Durchschlag = TDBalance::Ganz(V + TEXT("WeitereZiele"), 2);
		W.Folgeschaden = FMath::Clamp(TDBalance::Wert(V + TEXT("Folgeschaden"), 0.5f), 0.f, 1.f);
	}
	else if (Art == TEXT("Explosivbolzen"))
	{
		W.FlaechenSchaden = TDBalance::Wert(V + TEXT("Flaechenschaden"), 60.f);
		W.FlaechenRadius = TDBalance::Wert(V + TEXT("Radius"), 4.5f) * 100.f;
	}
	else if (Art == TEXT("Betaeubungsbolzen"))
	{
		W.Betaeubung = TDBalance::Wert(V + TEXT("Betaeubung"), 3.f);
	}
	else if (Art == TEXT("Streubolzen"))
	{
		W.Splitter = TDBalance::Ganz(V + TEXT("Splitter"), 3);
		W.SplitterSchaden = TDBalance::Wert(V + TEXT("Splitterschaden"), 25.f);
		W.SplitterRadius = TDBalance::Wert(V + TEXT("Splitterradius"), 6.f) * 100.f;
	}
	return W;
}

void ATDBoltTurret::ZeigeSteckendenBolzen(AFGCreature* Ziel, const FVector& Von) const
{
	if (!Ziel || UTDWaveSubsystem::bStressOhneGeschoss || TDBalance::Wert(TEXT("Leistung.Geschosse"), 1.f) < 0.5f)
	{
		return;
	}
	UClass* Standard = LoadClass<UFGItemDescriptor>(nullptr, TurmStandardBolzen);
	const UFGAmmoTypeProjectile* Typ = Cast<UFGAmmoTypeProjectile>(Standard ? Standard->GetDefaultObject() : nullptr);
	if (!Typ || !Typ->GetProjectileClass())
	{
		return;
	}
	// Einstichpunkt: Koerpermitte, um den halben Kapselradius zum Turm hin versetzt; Spitze zeigt in Flugrichtung
	const FVector Mitte = Ziel->GetActorLocation();
	const FVector Richtung = (Mitte - Von).GetSafeNormal();
	const float Radius = Ziel->GetCapsuleComponent() ? Ziel->GetCapsuleComponent()->GetScaledCapsuleRadius() : 50.f;
	const FTransform Ort(Richtung.Rotation(), Mitte - Richtung * Radius * 0.6f);
	AFGProjectile* Bolzen = GetWorld()->SpawnActorDeferred<AFGProjectile>(Typ->GetProjectileClass(), Ort, const_cast<ATDBoltTurret*>(this), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Bolzen)
	{
		return;
	}
	Bolzen->SetImpactDamageTypes({});
	Bolzen->SetEndOfLifeDamageTypes({});
	UGameplayStatics::FinishSpawningActor(Bolzen, Ort);
	Bolzen->SetActorEnableCollision(false);
	if (UActorComponent* Flug = Bolzen->GetProjectileMovement())
	{
		Flug->Deactivate();
	}
	Bolzen->SetActorScale3D(FVector(FMath::Clamp(TDBalance::Wert(TEXT("Tuerme.Bolzenturm.GeschossGroesse"), 3.f), 0.5f, 8.f) * 0.8f));
	Bolzen->AttachToActor(Ziel, FAttachmentTransformRules::KeepWorldTransform);
	Bolzen->SetLifeSpan(TDBalance::Wert(TEXT("Kombo.Blitzableiter.Dauer"), 5.f));
}

void ATDBoltTurret::SpawneGeschoss(const FVector& Von, const FVector& Nach) const
{
	// T2c: Geschosse abschaltbar (Stresstest geschoss=aus bzw. Leistung.Geschosse = 0) - der Schaden kommt unabhaengig davon
	if (UTDWaveSubsystem::bStressOhneGeschoss || TDBalance::Wert(TEXT("Leistung.Geschosse"), 1.f) < 0.5f)
	{
		return;
	}
	TDLeistung::FUhr Uhr(TDLeistung::GeschossZyklen);
	++UTDWaveSubsystem::GeschosseGesamt;
	TSubclassOf<UFGItemDescriptor> Munition = AktuelleMunition();
	const UFGAmmoTypeProjectile* Typ = Cast<UFGAmmoTypeProjectile>(Munition ? Munition->GetDefaultObject() : nullptr);
	if (!Typ || !Typ->GetProjectileClass())
	{
		// z. B. Splitterbolzen haben kein einzelnes Geschoss: Armierungsbolzen als Optik
		UClass* Standard = LoadClass<UFGItemDescriptor>(nullptr, TurmStandardBolzen);
		Typ = Cast<UFGAmmoTypeProjectile>(Standard ? Standard->GetDefaultObject() : nullptr);
	}
	if (!Typ || !Typ->GetProjectileClass())
	{
		return;
	}
	const FVector Richtung = (Nach - Von).GetSafeNormal();
	const FTransform Start(Richtung.Rotation(), Von);
	if (AFGProjectile* Geschoss = GetWorld()->SpawnActorDeferred<AFGProjectile>(Typ->GetProjectileClass(), Start, const_cast<ATDBoltTurret*>(this), nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		// Rein optisch: kein eigener Schaden, durch alles hindurch, verschwindet am Ziel
		Geschoss->SetImpactDamageTypes({});
		Geschoss->SetEndOfLifeDamageTypes({});
		UGameplayStatics::FinishSpawningActor(Geschoss, Start);
		Geschoss->SetActorEnableCollision(false);
		// Marius 03.11.2026: man sah kaum, dass der Turm feuert - Bolzen groesser zeichnen
		Geschoss->SetActorScale3D(FVector(FMath::Clamp(TDBalance::Wert(TEXT("Tuerme.Bolzenturm.GeschossGroesse"), 3.f), 0.5f, 8.f)));
		Geschoss->SetInitialVelocity(Richtung * mProjectileSpeed);
		Geschoss->SetLifeSpan(FMath::Max(FVector::Dist(Von, Nach) / mProjectileSpeed, 0.05f));
	}
}

void ATDBoltTurret::Feuern(AFGCreature* Ziel)
{
	if (!Ziel)
	{
		return;   // Bolzen verbraucht, Ziel weg: Fehlschuss
	}
	const FTDBolzenWerte W = WerteFuer(AktuelleMunition());
	const float Faktor = SchadenBonus();
	Schuss(Ziel, GetMuzzleLocation(), W, Faktor);
	// Level 5 Doppellauf (0.64): zweiter Bolzen aus dem unteren Lauf auf ein zweites Ziel in Reichweite, sonst auf dasselbe
	if (GetLevel() >= 5 && mHeadMesh)
	{
		AFGCreature* Zweit = Ziel;
		float Best = FMath::Square(GetReichweite());
		TArray<AFGCreature*> Alle;
		SammleFeinde(Alle);
		for (AFGCreature* F : Alle)
		{
			const float D = F && F != Ziel && F->IsAliveAndWell() ? FVector::DistSquared(F->GetActorLocation(), GetActorLocation()) : Best;
			if (D < Best)
			{
				Best = D;
				Zweit = F;
			}
		}
		Schuss(Zweit, mHeadMesh->GetComponentTransform().TransformPosition(mMuendung + BolzenZweiterLauf), W, Faktor);
	}
}

void ATDBoltTurret::Schuss(AFGCreature* Ziel, const FVector& Muendung, const FTDBolzenWerte& W, float Faktor)
{
	++mSchuesse;
	const FVector ZielPunkt = Ziel->GetActorLocation();
	auto Flugzeit = [&](const FVector& Nach) { return FVector::Dist(Muendung, Nach) / mProjectileSpeed; };
	SpawneGeschoss(Muendung, ZielPunkt);
	// 0.73 Spezialisierung: Schockbolzen machen Strom-Schaden (Antwort des Bolzenturms auf Stinger und Wasserspucker)
	TrefferArt(Ziel, W.Schaden * Faktor, (ZielPunkt - Muendung).GetSafeNormal(), W.Betaeubung, Flugzeit(ZielPunkt),
		W.Betaeubung > 0.f ? ETDSchadensart::Strom : GetSchadensart());
	// 0.84 Blitzableiter: der Bolzen steckt einige Sekunden im Gegner. 0.84.1: sichtbar, sobald die Kombination freigeschaltet ist
	// (Marius 08.11.: Kombos muessen in der Optik erkennbar sein) - ein Bolzen je Gegner, solange die Markierung haelt
	if (TDKombo::MarkiereBolzen(this, Ziel) && TDKombo::Aktiv(this, TEXT("Blitzableiter")))
	{
		ZeigeSteckendenBolzen(Ziel, Muendung);
	}
	// 0.64 Munition sichtbar: Einschlag je Bolzenart (Betaeubung blauer Funkenring, Explosion orange, Armierung heller Funke)
	const float Ankunft = Flugzeit(ZielPunkt);
	if (W.Betaeubung > 0.f)
	{
		ZeigeLeuchten(ZielPunkt, FLinearColor(0.3f, 0.55f, 1.f), 4.f, 0.4f, Ankunft, false, 40.f);
		for (int32 k = 0; k < 6; ++k)
		{
			const FVector Richtung = FRotator(0.f, 60.f * k, 0.f).Vector();
			ZeigeSpur(ZielPunkt + Richtung * 40.f, ZielPunkt + Richtung * 180.f + FVector(0.f, 0.f, FMath::FRandRange(-40.f, 60.f)), FLinearColor(0.3f, 0.55f, 1.f), 0.2f, 0.25f, Ankunft);
		}
	}
	else if (W.FlaechenRadius > 0.f)
	{
		ZeigeLeuchten(ZielPunkt, FLinearColor(1.f, 0.4f, 0.08f), W.FlaechenRadius / 40.f, 0.35f, Ankunft, false, 45.f);
	}
	else if (W.Splitter <= 0)
	{
		ZeigeLeuchten(ZielPunkt, FLinearColor(1.f, 0.85f, 0.6f), 1.2f, 0.12f, Ankunft);
	}

	// Level 4: ein Gegner mehr (bei Bolzen ohne Durchschlag mit 60 % Folgeschaden)
	int32 Durchschlag = W.Durchschlag + FMath::Max(0, FMath::RoundToInt(LevelWert(TEXT("Durchschlag"))));
	// 0.9.1 E3 Engstelle (Endgame-Konzept 09.11.): am Tor laufen die Gegner in einer Reihe - der Bolzen trifft einen mehr
	bool bEngstelle = false;
	if (Ziel && TDKombo::Aktiv(this, TEXT("Engstelle")))
	{
		const float Radius = TDBalance::Wert(TEXT("Kombo.Engstelle.Radius"), 12.f) * 100.f;
		for (TActorIterator<ATDGate> It(GetWorld()); It && !bEngstelle; ++It)
		{
			bEngstelle = FVector::Dist(It->GetActorLocation(), Ziel->GetActorLocation()) < Radius;
		}
		Durchschlag += bEngstelle ? TDBalance::Ganz(TEXT("Kombo.Engstelle.Ziele"), 1) : 0;
	}
	const float Folgeschaden = W.Durchschlag > 0 ? W.Folgeschaden : 0.6f;
	TArray<AFGCreature*> Feinde;
	if (W.FlaechenRadius > 0.f || W.Splitter > 0 || Durchschlag > 0)
	{
		SammleFeinde(Feinde);
	}
	// Durchschlag: weitere Gegner auf der Schusslinie (bis 15 m hinter dem Ziel, hoechstens 2,5 m daneben), naechste zuerst
	if (Durchschlag > 0)
	{
		const FVector Richtung = (ZielPunkt - Muendung).GetSafeNormal();
		const FVector LinienEnde = ZielPunkt + Richtung * 1500.f;
		TArray<TPair<float, AFGCreature*>> AufLinie;
		for (AFGCreature* Andere : Feinde)
		{
			if (Andere == Ziel)
			{
				continue;
			}
			const FVector Ort = Andere->GetActorLocation();
			const FVector Naechster = FMath::ClosestPointOnSegment(Ort, Muendung, LinienEnde);
			if (FVector::DistSquared(Ort, Naechster) < FMath::Square(250.f))
			{
				AufLinie.Add({ FVector::DistSquared(Muendung, Ort), Andere });
			}
		}
		AufLinie.Sort([](const TPair<float, AFGCreature*>& A, const TPair<float, AFGCreature*>& B) { return A.Key < B.Key; });
		float Folge = W.Schaden * Faktor;
		FVector Ende = ZielPunkt;
		for (int32 i = 0; i < FMath::Min(Durchschlag, AufLinie.Num()); ++i)
		{
			AFGCreature* Andere = AufLinie[i].Value;
			Folge *= Folgeschaden;
			Treffer(Andere, Folge, Richtung, 0.f, Flugzeit(Andere->GetActorLocation()));
			Ende = Andere->GetActorLocation();
			if (bEngstelle && i == Durchschlag - 1)
			{
				TDKombo::Melde(TEXT("Engstelle"), this, Folge);   // der Zusatz-Treffer
			}
		}
		// weisse Spur durch alle Getroffenen (0.64)
		ZeigeSpur(Muendung, Ende + Richtung * 150.f, FLinearColor(0.9f, 0.95f, 1.f), Flugzeit(Ende) + 0.15f, 0.2f);
	}
	// Explosion: Flaechenschaden nur an Kreaturen (eigene Mauern bleiben heil)
	if (W.FlaechenRadius > 0.f)
	{
		for (AFGCreature* Andere : Feinde)
		{
			if (Andere != Ziel && FVector::Dist(Andere->GetActorLocation(), ZielPunkt) < W.FlaechenRadius)
			{
				TrefferArt(Andere, W.FlaechenSchaden * Faktor, (Andere->GetActorLocation() - ZielPunkt).GetSafeNormal(), 0.f, Flugzeit(ZielPunkt), ETDSchadensart::Explosion);
			}
		}
	}
	// Splitter: weitere Ziele in der Naehe des Hauptziels
	if (W.Splitter > 0)
	{
		int32 Rest = W.Splitter;
		for (AFGCreature* Andere : Feinde)
		{
			if (Rest <= 0)
			{
				break;
			}
			if (Andere != Ziel && FVector::Dist(Andere->GetActorLocation(), ZielPunkt) < W.SplitterRadius)
			{
				// Splitter fliegen vom Hauptziel zu den Nachbarn (0.64: gelbe Spuren statt zweiter Bolzen von der Muendung)
				ZeigeSpur(ZielPunkt, Andere->GetActorLocation(), FLinearColor(1.f, 0.8f, 0.25f), 0.15f, 0.25f, Ankunft);
				Treffer(Andere, W.SplitterSchaden * Faktor, (Andere->GetActorLocation() - ZielPunkt).GetSafeNormal(), 0.f, Ankunft + 0.1f);
				--Rest;
			}
		}
	}
}

float ATDBoltTurret::NennDPS() const
{
	const float Takt = Zykluszeit();
	return Takt > 0.f ? WerteFuer(AktuelleMunition()).Schaden * SchadenBonus() / Takt : 0.f;
}

void ATDBoltTurret::Kampfwerte(TArray<FTDKennwert>& Out) const
{
	const FTDBolzenWerte W = WerteFuer(AktuelleMunition());
	const float Schaden = W.Schaden * SchadenBonus();
	const float Takt = Zykluszeit();
	// Einheitlich fuer alle Tuerme (Marius 03.11.2026): Schaden je Treffer, Feuerrate in Schuss/s, Schaden/s
	Out.Add({ TDT(TEXT("Schaden je Treffer"), TEXT("Damage per hit")), FString::Printf(TEXT("%.0f"), Schaden), true });
	if (Takt > 0.f)
	{
		Out.Add({ TDT(TEXT("Feuerrate"), TEXT("Fire rate")), TDF(TEXT("%.1f Schuss/s"), TEXT("%.1f shots/s"), 1.f / Takt), true });
		Out.Add({ TDT(TEXT("Schaden/s"), TEXT("Damage/s")), TDF(TEXT("%.0f (ein Ziel)"), TEXT("%.0f (single target)"), Schaden / Takt), true });
	}
	if (W.Durchschlag > 0)
	{
		Out.Add({ TDT(TEXT("Durchschlag"), TEXT("Pierce")), TDF(TEXT("+%d Gegner dahinter, je x%.2f"), TEXT("+%d enemies behind, x%.2f each"), W.Durchschlag, W.Folgeschaden), true });
	}
	if (W.FlaechenRadius > 0.f)
	{
		Out.Add({ TDT(TEXT("Explosion"), TEXT("Explosion")), TDF(TEXT("+%.0f Schaden im Umkreis von %.1f m"), TEXT("+%.0f damage within %.1f m"), W.FlaechenSchaden * SchadenBonus(), W.FlaechenRadius / 100.f), true });
	}
	if (W.Betaeubung > 0.f)
	{
		Out.Add({ TDT(TEXT("Betäubung"), TEXT("Stun")), FString::Printf(TEXT("%.0f s"), W.Betaeubung), true });
	}
	if (W.Splitter > 0)
	{
		Out.Add({ TDT(TEXT("Splitter"), TEXT("Shrapnel")), TDF(TEXT("%d weitere Ziele je %.0f Schaden (%.0f m)"), TEXT("%d more targets, %.0f damage each (%.0f m)"), W.Splitter, W.SplitterSchaden * SchadenBonus(), W.SplitterRadius / 100.f), true });
	}
}
