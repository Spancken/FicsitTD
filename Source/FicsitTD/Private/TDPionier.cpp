#include "TDPionier.h"
#include "FicsitTDModule.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDWaveSubsystem.h"
#include "Creature/FGCreature.h"
#include "Equipment/FGWeapon.h"
#include "EngineUtils.h"
#include "FGCharacterMovementComponent.h"
#include "FGCharacterPlayer.h"
#include "FGChatManager.h"
#include "FGHealthComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
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
}

void UTDPionier::Abschuss(const AFGCreature* Kreatur)
{
	const double* Zeit = mSpielerTreffer.Find(Kreatur);
	if (!Zeit || FPlatformTime::Seconds() - *Zeit > 5.0)
	{
		return;
	}
	mSpielerTreffer.Remove(Kreatur);
	const bool bBoss = Kreatur->Tags.Contains(TEXT("FicsitTDBoss"));
	const bool bElite = Kreatur->Tags.Contains(TEXT("FicsitTDElite"));
	GibXP(bBoss ? TDBalance::Ganz(TEXT("Pionier.XPAbschussBoss"), 50) : bElite ? TDBalance::Ganz(TEXT("Pionier.XPAbschussElite"), 10)
		: TDBalance::Ganz(TEXT("Pionier.XPAbschuss"), 3), TEXT("Abschuss"));
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
		return 1.f;
	}
	const int32 Stufe = GetStufe(W);
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
			Zusatz = W == TDPionierWerte::Gewehr ? 120.f : 0.5f * Menge * Faktor;
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
			GetWorld()->GetTimerManager().SetTimerForNextTick([Opfer, Zusatz]()
			{
				for (const TWeakObjectPtr<AFGCreature>& C : Opfer)
				{
					if (C.IsValid() && C->IsAliveAndWell())
					{
						UGameplayStatics::ApplyDamage(C.Get(), Zusatz, nullptr, nullptr, UDamageType::StaticClass());
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
