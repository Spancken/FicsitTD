#include "Commands/TDDebugCommand.h"
#include "Command/CommandSender.h"
#include "TDLaufProtokoll.h"
#include "Components/PrimitiveComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "FGPlayerController.h"
#include "FicsitTDModule.h"
#include "TDTurretBase.h"
#include "TDFlameTurret.h"
#include "TDDefenseCore.h"
#include "TDTestlauf.h"
#include "TDMarschTest.h"
#include "TDGegnerHirn.h"
#include "TDEinsatz.h"
#include "TDBalance.h"
#include "TDWaveSubsystem.h"
#include "TDKernplatz.h"
#include "TDSzenario.h"
#include "TDBandUpgrade.h"
#include "Buildables/FGBuildablePipeReservoir.h"
#include "FGInventoryComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeNetwork.h"
#include "FGPipeSubsystem.h"
#include "Resources/FGItemDescriptor.h"
#include "TDEffektProbe.h"
#include "AI/FGCreatureController.h"
#include "Creature/FGCreature.h"
#include "Creature/FGCreatureAction.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Materials/MaterialInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/** Chat-Antwort, zusaetzlich im Log - Testlaeufe lassen sich danach ohne Screenshots auswerten */
static void Antwort(UCommandSender* Sender, const FString& Text)
{
	UE_LOG(LogFicsitTD, Log, TEXT("Chat: %s"), *Text);
	Sender->SendChatMessage(Text);
}
#include "Engine/World.h"
#include "TDDecoy.h"

ATDDebugCommand::ATDDebugCommand()
{
	CommandName = TEXT("td");
	Usage = NSLOCTEXT("FicsitTD", "TD.Usage", "/td umgebung|instanzen [Radius_m] - Objekte bzw. Instanzen um dich auflisten | /td koeder - alle Koeder entfernen | /td reichweite [an|aus] - Reichweitenkreise | /td turmlevel <1-5> - naechsten Turm auf Level setzen (Test) | /td teil [Name] - Turmteil ein-/ausblenden | /td probe gewehr|flamme|weg - Effekt-Vorschau | /td fuellen [art] [m3] - Puffer/Flammenwerfer fuellen | /td kern start [erkundung|normal|hart|albtraum] [Wellen] [Min] [Min]|weiter|stop|status|fronten [neu]|wege [aus]|stufen|tempo [Faktor]|stress [N ...] [lp=20] [s=20] [mix|art=<Gegner>] [anim=immer|sichtbar] [kollision=an|aus] [ragdoll=an|aus] [tuerme=aus] [geschoss=aus] [angriff=aus] [drehen=aus] [schatten=aus] [effekte=aus] [df=aus] [drehtakt=N] [toeten=N/s]|stress stop - Kern-Verteidigung | /td test [karte|gegner|marsch] - automatischer Test | /td einsatz start [stufe] [wellen]|frei [an|aus]|status|stop - Einsatz mit Kontingenten | /td laden, /td kaufe <Gebaeude> [n] - Laden | /td freischalten - alles frei | /td balance - Balance-Datei neu lesen | /td platz [liste|scan <Biom>|scan stop|pruefen <Id|S1..S8|hier [Biom]>|merken <Biom> [Notiz]|gemerkt|zurueck|bauen <Biom> [1-3]] - Kernplaetze suchen, pruefen, merken und bauen | /td szenario [liste|<1-5>|pruefen [Nr]|fertig|leeren] - Testszenarien | /td ada [bank] - ADA-Begruessung | /td baender <2-6>|pruefen|reparieren - alle Baender aufruesten, offene Band-Enden pruefen oder verbinden");
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer = true;
}

EExecutionStatus ATDDebugCommand::ExecuteCommand_Implementation(UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
	AFGPlayerController* Player = Sender->GetPlayer();
	APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	// /td optik: Knochen (mit Lage in der Grundhaltung), Sockel, Materialien und ihre Parameter aller Gegnerarten -
	// Grundlage fuer Anbauteile aus Blender und fuer das Einfaerben (T4). Je Art eine Datei unter Saved/FicsitTD/Optik/.
	if (Arguments[0].Equals(TEXT("optik"), ESearchCase::IgnoreCase))
	{
		static const TCHAR* Pfade[] = {
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Char_Hog.Char_Hog_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/AlphaHog/Char_AlphaHog.Char_AlphaHog_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/CliffHog/Char_CliffHog.Char_CliffHog_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/NuclearHog/Char_NuclearHog.Char_NuclearHog_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Hog/Johnny/Char_Johnny.Char_Johnny_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestSmall.Char_SpitterForestSmall_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterForest/Char_SpitterForestAlpha.Char_SpitterForestAlpha_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Spitter/SpitterAquatic/Char_SpitterAquaticAlpha.Char_SpitterAquaticAlpha_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/SmallStinger/Char_Stinger_Child.Char_Stinger_Child_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/Char_Stinger.Char_Stinger_C"),
			TEXT("/Game/FactoryGame/Character/Creature/Enemy/Stinger/BigStinger/Char_EliteStinger.Char_EliteStinger_C"),
		};
		const FString Ordner = FPaths::ProjectSavedDir() / TEXT("FicsitTD") / TEXT("Optik");
		int32 Arten = 0;
		for (const TCHAR* Pfad : Pfade)
		{
			UClass* Klasse = LoadClass<AFGCreature>(nullptr, Pfad);
			const AFGCreature* Vorlage = Klasse ? Klasse->GetDefaultObject<AFGCreature>() : nullptr;
			const USkeletalMeshComponent* Komp = Vorlage ? Vorlage->GetMesh() : nullptr;
			USkeletalMesh* Modell = Komp ? Komp->GetSkeletalMeshAsset() : nullptr;
			if (!Modell)
			{
				UE_LOG(LogFicsitTD, Log, TEXT("Optik: %s ohne Modell"), Pfad);
				continue;
			}
			TArray<FString> Z;
			const FBoxSphereBounds Grenzen = Modell->GetBounds();
			Z.Add(FString::Printf(TEXT("Art %s | Modell %s | Skelett %s"), *Klasse->GetName(), *Modell->GetPathName(), Modell->GetSkeleton() ? *Modell->GetSkeleton()->GetPathName() : TEXT("-")));
			Z.Add(FString::Printf(TEXT("Komponente: Lage %s, Drehung %s, Massstab %s | Grenzen Mitte %s, Ausdehnung %s (cm)"),
				*Komp->GetRelativeLocation().ToCompactString(), *Komp->GetRelativeRotation().ToCompactString(), *Komp->GetRelativeScale3D().ToCompactString(),
				*Grenzen.Origin.ToCompactString(), *Grenzen.BoxExtent.ToCompactString()));
			// Knochen: Name, Eltern, Lage im Modellraum (Grundhaltung, aufsummiert)
			const FReferenceSkeleton& Ref = Modell->GetRefSkeleton();
			TArray<FTransform> Welt;
			Welt.SetNum(Ref.GetNum());
			Z.Add(FString::Printf(TEXT("Knochen (%d): Index; Name; Eltern; Lage im Modell x y z (cm)"), Ref.GetNum()));
			for (int32 i = 0; i < Ref.GetNum(); ++i)
			{
				const int32 Eltern = Ref.GetParentIndex(i);
				Welt[i] = Eltern >= 0 ? Ref.GetRefBonePose()[i] * Welt[Eltern] : Ref.GetRefBonePose()[i];
				const FVector L = Welt[i].GetLocation();
				Z.Add(FString::Printf(TEXT("%d; %s; %s; %.1f %.1f %.1f"), i, *Ref.GetBoneName(i).ToString(), Eltern >= 0 ? *Ref.GetBoneName(Eltern).ToString() : TEXT("-"), L.X, L.Y, L.Z));
			}
			Z.Add(TEXT("Sockel:"));
			for (const USkeletalMeshSocket* S : Modell->GetActiveSocketList())
			{
				if (S)
				{
					Z.Add(FString::Printf(TEXT("  %s an %s, Lage %s"), *S->SocketName.ToString(), *S->BoneName.ToString(), *S->RelativeLocation.ToCompactString()));
				}
			}
			Z.Add(TEXT("Materialien:"));
			const TArray<FSkeletalMaterial>& Materialien = Modell->GetMaterials();
			for (int32 m = 0; m < Materialien.Num(); ++m)
			{
				UMaterialInterface* Mat = Komp->GetMaterial(m) ? Komp->GetMaterial(m) : Materialien[m].MaterialInterface.Get();
				Z.Add(FString::Printf(TEXT("  Platz %d %s: %s"), m, *Materialien[m].MaterialSlotName.ToString(), Mat ? *Mat->GetPathName() : TEXT("-")));
				if (Mat)
				{
					TArray<FMaterialParameterInfo> Infos;
					TArray<FGuid> Ids;
					Mat->GetAllScalarParameterInfo(Infos, Ids);
					FString Liste;
					for (const FMaterialParameterInfo& I : Infos)
					{
						float W = 0.f;
						Mat->GetScalarParameterValue(I, W);
						Liste += FString::Printf(TEXT("%s=%.2f "), *I.Name.ToString(), W);
					}
					Z.Add(TEXT("    Skalar: ") + Liste);
					Infos.Reset();
					Ids.Reset();
					Mat->GetAllVectorParameterInfo(Infos, Ids);
					Liste.Reset();
					for (const FMaterialParameterInfo& I : Infos)
					{
						FLinearColor W;
						Mat->GetVectorParameterValue(I, W);
						Liste += FString::Printf(TEXT("%s=(%.2f %.2f %.2f) "), *I.Name.ToString(), W.R, W.G, W.B);
					}
					Z.Add(TEXT("    Vektor: ") + Liste);
					Infos.Reset();
					Ids.Reset();
					Mat->GetAllTextureParameterInfo(Infos, Ids);
					Liste.Reset();
					for (const FMaterialParameterInfo& I : Infos)
					{
						Liste += I.Name.ToString() + TEXT(" ");
					}
					Z.Add(TEXT("    Textur: ") + Liste);
				}
			}
			FFileHelper::SaveStringArrayToFile(Z, *(Ordner / (Klasse->GetName() + TEXT(".txt"))), FFileHelper::EEncodingOptions::ForceUTF8);
			UE_LOG(LogFicsitTD, Log, TEXT("Optik: %s - %d Knochen, %d Materialien, Grenzen %s"), *Klasse->GetName(), Ref.GetNum(), Materialien.Num(), *Grenzen.BoxExtent.ToCompactString());
			++Arten;
		}
		Antwort(Sender, FString::Printf(TEXT("Optik: %d Gegnerarten ausgewertet, Dateien in %s"), Arten, *FPaths::ConvertRelativePathToFull(Ordner)));
		return EExecutionStatus::COMPLETED;
	}
	// /td kampf [Radius_m]: Diagnose "Gegner kaempfen untereinander" (Test 0.45) - je Kreatur im Umkreis Herkunft, Tempo,
	// laufende Montage, Spiel-KI (Verhaltensbaum, Zustand, Aktion) und ihre Aggro-Ziele; Liste ins Log, Zusammenfassung in den Chat
	if (Arguments[0].Equals(TEXT("kampf"), ESearchCase::IgnoreCase))
	{
		const float Radius = (Arguments.Num() > 1 ? FMath::Max(10.f, FCString::Atof(*Arguments[1])) : 150.f) * 100.f;
		int32 Wellen = 0, Wild = 0, MitMontage = 0, MitKI = 0, MitZiel = 0, ZielWelle = 0;
		for (TActorIterator<AFGCreature> It(GetWorld()); It; ++It)
		{
			AFGCreature* K = *It;
			if (!Pawn || !K->IsAliveAndWell() || FVector::Dist(K->GetActorLocation(), Pawn->GetActorLocation()) > Radius)
			{
				continue;
			}
			const bool bWelle = K->Tags.Contains(TEXT("FicsitTD"));
			(bWelle ? Wellen : Wild)++;
			const UAnimInstance* Anim = K->GetMesh() ? K->GetMesh()->GetAnimInstance() : nullptr;
			const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
			MitMontage += Montage ? 1 : 0;
			const AFGCreatureController* KI = Cast<AFGCreatureController>(K->GetController());
			const bool bBaum = KI && KI->BrainComponent && KI->BrainComponent->IsRunning();
			MitKI += bBaum ? 1 : 0;
			FString Ziele;
			if (KI)
			{
				for (const FFGCreatureAggroData& D : KI->GetAggroTargets())
				{
					if (D.Target && D.Aggro > 0.f)
					{
						const bool bZielWelle = D.Target->Tags.Contains(TEXT("FicsitTD"));
						ZielWelle += bZielWelle ? 1 : 0;
						Ziele += FString::Printf(TEXT("%s%s%s %.0f"), Ziele.IsEmpty() ? TEXT("") : TEXT(", "), *D.Target->GetClass()->GetName(), bZielWelle ? TEXT("(Welle)") : TEXT(""), D.Aggro);
					}
				}
			}
			MitZiel += Ziele.IsEmpty() ? 0 : 1;
			const UTDGegnerHirn* Hirn = UTDGegnerHirn::Von(K);
			UE_LOG(LogFicsitTD, Log, TEXT("Kampf-Diagnose: %s %s in %.0f m | Tempo %.0f | Montage %s | Controller %s, Baum %s, Zustand %d, Aktion %s | Hirn %s | Aggro: %s"),
				bWelle ? TEXT("WELLE") : TEXT("wild"), *K->GetClass()->GetName(), FVector::Dist(K->GetActorLocation(), Pawn->GetActorLocation()) / 100.f,
				K->GetVelocity().Size(), Montage ? *Montage->GetName() : TEXT("-"), K->GetController() ? *K->GetController()->GetClass()->GetName() : TEXT("-"),
				bBaum ? TEXT("laeuft") : TEXT("aus"), KI ? static_cast<int32>(KI->GetCurrentCreatureState()) : -1,
				KI && KI->GetCurrentAction() ? *KI->GetCurrentAction()->GetClass()->GetName() : TEXT("-"),
				Hirn ? *Hirn->Diagnose() : TEXT("-"), Ziele.IsEmpty() ? TEXT("-") : *Ziele);
		}
		Antwort(Sender, FString::Printf(TEXT("Kampf-Diagnose %.0f m: %d Wellengegner, %d Wildtiere; %d spielen eine Montage, %d mit laufender Spiel-KI, %d mit Aggro-Ziel (%d Ziele davon Wellengegner). Einzelheiten im Log."),
			Radius / 100.f, Wellen, Wild, MitMontage, MitKI, MitZiel, ZielWelle));
		return EExecutionStatus::COMPLETED;
	}
	// /td koeder weg: alle Koeder entfernen (Diagnose fuer den schwarzen Schatten; beim Neuladen kommen sie wieder)
	if (Arguments[0].Equals(TEXT("koeder"), ESearchCase::IgnoreCase))
	{
		int32 Entfernt = 0;
		for (TActorIterator<ATDDecoy> It(GetWorld()); It; ++It)
		{
			It->Destroy();
			++Entfernt;
		}
		UE_LOG(LogFicsitTD, Log, TEXT("Diagnose: %d Koeder entfernt"), Entfernt);
		Antwort(Sender, FString::Printf(TEXT("%d Koeder entfernt. Ist der schwarze Schatten jetzt weg? (Neu laden bringt sie zurueck)"), Entfernt));
		return EExecutionStatus::COMPLETED;
	}
	// /td test [karte|gegner]: automatischer Test um den naechsten Abwehrkern (sonst um den Spieler), Bericht ins Log
	if (Arguments[0].Equals(TEXT("test"), ESearchCase::IgnoreCase))
	{
		const FString Was = Arguments.Num() > 1 ? Arguments[1].ToLower() : TEXT("alles");
		ATDDefenseCore* Kern = nullptr;
		float Best = FMath::Square(50000.f);
		for (TActorIterator<ATDDefenseCore> It(GetWorld()); It; ++It)
		{
			const float D = Pawn ? FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) : 0.f;
			if (D < Best)
			{
				Best = D;
				Kern = *It;
			}
		}
		// /td test marsch: Gegner mit eigenem Gehirn (Weg C), drei Abschnitte zuschauen/wegschauen/weit weg
		if (Was == TEXT("marsch"))
		{
			if (!Kern)
			{
				Antwort(Sender, TEXT("Kein Abwehrkern in 500 m."));
				return EExecutionStatus::COMPLETED;
			}
			FActorSpawnParameters MarschParams;
			MarschParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ATDMarschTest* Marsch = GetWorld()->SpawnActor<ATDMarschTest>(Kern->GetActorLocation(), FRotator::ZeroRotator, MarschParams))
			{
				Marsch->Starte(Kern, Sender);
			}
			return EExecutionStatus::COMPLETED;
		}
		// Ziel wie im echten Lauf: ein Koeder des Kerns; ohne Kern der Spieler
		AActor* Ziel = Pawn;
		if (Kern)
		{
			for (TActorIterator<ATDDecoy> It(GetWorld()); It; ++It)
			{
				if (It->GetActiveTarget() == Kern)
				{
					Ziel = *It;
					break;
				}
			}
		}
		const FVector Mitte = Kern ? Kern->GetActorLocation() : (Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATDTestlauf* Test = GetWorld()->SpawnActor<ATDTestlauf>(Mitte, FRotator::ZeroRotator, Params);
		if (Test)
		{
			Antwort(Sender, FString::Printf(TEXT("Test um %s gestartet ..."), Kern ? TEXT("den Abwehrkern") : TEXT("dich (kein Kern in 500 m)")));
			Test->Starte(Ziel, Was != TEXT("gegner"), Was != TEXT("karte"), Sender);
		}
		return EExecutionStatus::COMPLETED;
	}
	// /td kern start|weiter|stop|status: Modus Kern-Verteidigung am naechsten Abwehrkern
	// /td einsatz start [stufe] [wellen] | frei [an|aus] | status | stop  (Einsatz-Konzept, Stufe 1)
	if (Arguments[0].Equals(TEXT("einsatz"), ESearchCase::IgnoreCase))
	{
		UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
		const FString Was = Arguments.Num() > 1 ? Arguments[1].ToLower() : TEXT("status");
		if (!Einsatz)
		{
			return EExecutionStatus::UNCOMPLETED;
		}
		if (Was == TEXT("start"))
		{
			// Kern in 500 m uebernehmen, sonst setzt der Einsatz einen neuen vor den Spieler
			ATDDefenseCore* Kern = nullptr;
			float Best = FMath::Square(50000.f);
			for (TActorIterator<ATDDefenseCore> It(GetWorld()); It; ++It)
			{
				const float D = Pawn ? FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) : 0.f;
				if (D < Best)
				{
					Best = D;
					Kern = *It;
				}
			}
			int32 Stufe = 1, Wellen = 30;   // Early Game bis Boss 10, zweite Front ab 11
			for (int32 i = 2; i < Arguments.Num(); ++i)
			{
				const int32 S = UTDWaveSubsystem::StufeAusText(Arguments[i]);
				if (S >= 0)
				{
					Stufe = S;
				}
				else if (Arguments[i].IsNumeric())
				{
					Wellen = FCString::Atoi(*Arguments[i]);
				}
			}
			TArray<FString> Meldungen;
			Einsatz->Starte(Kern, Stufe, Wellen, Pawn, Meldungen);
			for (const FString& Zeile : Meldungen)
			{
				Antwort(Sender, Zeile);
			}
		}
		else if (Was == TEXT("frei"))
		{
			const bool bAn = Arguments.Num() > 2 ? !Arguments[2].Equals(TEXT("aus"), ESearchCase::IgnoreCase) : !Einsatz->IstFrei();
			Einsatz->SetzeFrei(bAn);
			Antwort(Sender, bAn ? TEXT("Freies Bauen AN: keine Kontingente, keine Bausperre (Rezepte: /td freischalten, Baukosten: Spieleinstellung 'Kostenloses Bauen').")
				: TEXT("Freies Bauen AUS: Kontingente und Bausperre gelten wieder."));
		}
		else if (Was == TEXT("stop"))
		{
			Einsatz->Beende();
			Antwort(Sender, TEXT("Einsatz beendet, Kontingente aus."));
		}
		else
		{
			for (const FString& Zeile : Einsatz->StatusText())
			{
				Antwort(Sender, Zeile);
			}
		}
		return EExecutionStatus::COMPLETED;
	}
	// /td laden | /td kaufe <Gebaeude> [Anzahl] | /td freischalten
	// /td balance [neu]: Balance-Datei neu lesen (wirkt sofort; Gegnerwerte ab der naechsten Welle)
	// /td notiz <1-5> [Text]: Einschaetzung zur laufenden bzw. letzten Welle (Balancing-Lauf, Saved/FicsitTD/Notizen.csv)
	if (Arguments[0].Equals(TEXT("notiz"), ESearchCase::IgnoreCase))
	{
		UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this);
		if (!Lauf || Arguments.Num() < 2 || !Arguments[1].IsNumeric())
		{
			Antwort(Sender, TEXT("/td notiz <1-5> [Text] - 1 zu leicht, 3 passend, 5 zu schwer; z. B. /td notiz 2 Munition reicht locker"));
			return EExecutionStatus::BAD_ARGUMENTS;
		}
		TArray<FString> Rest(Arguments);
		Rest.RemoveAt(0, 2);
		Antwort(Sender, Lauf->Notiz(FCString::Atoi(*Arguments[1]), FString::Join(Rest, TEXT(" "))));
		return EExecutionStatus::COMPLETED;
	}
	// /td lauf balancing [an|aus] | status: Balancing-Lauf (Testsprung aus, Eingriffe vermerkt, Spielstand je Phase)
	if (Arguments[0].Equals(TEXT("lauf"), ESearchCase::IgnoreCase))
	{
		UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this);
		if (!Lauf)
		{
			return EExecutionStatus::UNCOMPLETED;
		}
		if (Arguments.Num() > 1 && Arguments[1].Equals(TEXT("balancing"), ESearchCase::IgnoreCase))
		{
			const bool bAn = Arguments.Num() > 2 ? !Arguments[2].Equals(TEXT("aus"), ESearchCase::IgnoreCase) : true;
			Lauf->SetzeBalancing(bAn);
			Antwort(Sender, bAn ? TEXT("Balancing-Lauf an: Testsprung aus, /gib, /td fuellen usw. werden als Eingriff vermerkt, Spielstand nach jeder Phase. Noten mit /td notiz <1-5> [Text].")
				: TEXT("Balancing-Lauf aus."));
			return EExecutionStatus::COMPLETED;
		}
		Antwort(Sender, FString::Printf(TEXT("Balancing-Lauf: %s. /td lauf balancing [an|aus]"), Lauf->IstBalancing() ? TEXT("an") : TEXT("aus")));
		return EExecutionStatus::COMPLETED;
	}
	if (Arguments[0].Equals(TEXT("balance"), ESearchCase::IgnoreCase))
	{
		Antwort(Sender, TDBalance::Lade());
		Antwort(Sender, FString::Printf(TEXT("Datei: %s"), *TDBalance::Pfad()));
		return EExecutionStatus::COMPLETED;
	}
	if (Arguments[0].Equals(TEXT("laden"), ESearchCase::IgnoreCase) || Arguments[0].Equals(TEXT("kaufe"), ESearchCase::IgnoreCase)
		|| Arguments[0].Equals(TEXT("kaufen"), ESearchCase::IgnoreCase))
	{
		UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
		if (!Einsatz)
		{
			return EExecutionStatus::UNCOMPLETED;
		}
		if (Arguments[0].Equals(TEXT("laden"), ESearchCase::IgnoreCase) || Arguments.Num() < 2)
		{
			for (const FString& Zeile : Einsatz->LadenText())
			{
				Antwort(Sender, Zeile);
			}
		}
		else
		{
			Antwort(Sender, Einsatz->Kaufe(Arguments[1], Arguments.Num() > 2 ? FCString::Atoi(*Arguments[2]) : 1));
		}
		return EExecutionStatus::COMPLETED;
	}
	if (Arguments[0].Equals(TEXT("freischalten"), ESearchCase::IgnoreCase))
	{
		if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
		{
			Waves->SchalteSandkastenFrei();
		}
		Antwort(Sender, TEXT("Alle FicsitTD-Gebaeude und -Rezepte freigeschaltet."));
		return EExecutionStatus::COMPLETED;
	}
	if (Arguments[0].Equals(TEXT("kern"), ESearchCase::IgnoreCase))
	{
		UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
		const FString Was = Arguments.Num() > 1 ? Arguments[1].ToLower() : TEXT("status");
		ATDDefenseCore* Kern = nullptr;
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<ATDDefenseCore> It(GetWorld()); It; ++It)
		{
			const float D = Pawn ? FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) : 0.f;
			if (D < Best)
			{
				Best = D;
				Kern = *It;
			}
		}
		if (!Waves)
		{
			return EExecutionStatus::UNCOMPLETED;
		}
		if (Was == TEXT("start"))
		{
			if (!Kern)
			{
				Antwort(Sender, TEXT("Kein Abwehrkern gebaut (Baumenue: Verteidigung)."));
			}
			// /td kern start [Stufe] [Wellen] [Minuten bis Welle 1] [Minuten zwischen den Wellen]; Stufe als Wort an beliebiger Stelle
			else
			{
				int32 Stufe = 1;
				TArray<float> Zahlen;
				for (int32 i = 2; i < Arguments.Num(); ++i)
				{
					const int32 S = UTDWaveSubsystem::StufeAusText(Arguments[i]);
					if (S >= 0)
					{
						Stufe = S;
					}
					else
					{
						Zahlen.Add(FCString::Atof(*Arguments[i]));
					}
				}
				Waves->SetManuell(false);   // freie Kern-Verteidigung mit Countdown (Einsatz: /td einsatz start)
				Waves->SchalteSandkastenFrei();
				if (!Waves->StarteKernVerteidigung(Kern,
					Zahlen.Num() > 0 ? FMath::RoundToInt(Zahlen[0]) : 30,
					Zahlen.Num() > 1 ? Zahlen[1] * 60.f : -1.f,
					Zahlen.Num() > 2 ? Zahlen[2] * 60.f : -1.f, Stufe))
				{
					Antwort(Sender, TEXT("Es laeuft schon eine Kern-Verteidigung (/td kern stop)."));
				}
			}
		}
		else if (Was == TEXT("fronten"))
		{
			if (!Kern)
			{
				Antwort(Sender, TEXT("Kein Abwehrkern gebaut."));
			}
			if (Arguments.Num() > 2 && Arguments[2].Equals(TEXT("neu"), ESearchCase::IgnoreCase))
			{
				Waves->VerwerfeFronten();
			}
			for (const FString& Zeile : Waves->GetFrontenText(Kern))
			{
				Antwort(Sender, Zeile);
			}
		}
		// /td kern wege [aus]: Leuchtringe entlang der Gegnerwege; erneut aufrufen nach Mauerbau = neu berechnen
		else if (Was == TEXT("wege"))
		{
			if (!Kern)
			{
				Antwort(Sender, TEXT("Kein Abwehrkern gebaut."));
			}
			else if (Arguments.Num() > 2 && Arguments[2].Equals(TEXT("aus"), ESearchCase::IgnoreCase))
			{
				Kern->VersteckeWege();
				Antwort(Sender, TEXT("Wegvorschau aus."));
			}
			else
			{
				FString Bericht;
				Kern->ZeigeWege(Waves->BerechneWege(Kern, Bericht));
				Antwort(Sender, Bericht.Left(250));
				Antwort(Sender, TEXT("Leuchtringe alle 10 m zeigen die Wege. Nach Mauerbau erneut /td kern wege, aus: /td kern wege aus"));
			}
		}
		else if (Was == TEXT("aufraeumen") || Was == TEXT("aufräumen"))
		{
			// /td kern aufraeumen [Radius_m]: verwaiste Wellengegner alter Spielstaende entfernen (Standard 600 m um den Kern)
			const float Radius = (Arguments.Num() > 2 ? FMath::Max(10.f, FCString::Atof(*Arguments[2])) : 600.f) * 100.f;
			FString Bericht;
			const int32 Weg = UTDWaveSubsystem::RaeumeWaisen(GetWorld(), Kern ? Kern->GetActorLocation() : (Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector), Radius, Bericht);
			Antwort(Sender, FString::Printf(TEXT("%d verwaiste Wellengegner entfernt%s."), Weg, Bericht.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Bericht)));
		}
		else if (Was == TEXT("stufen"))
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const FTDStufe& S = UTDWaveSubsystem::GetStufe(i);
				Antwort(Sender, FString::Printf(TEXT("%s: Gegner x%.1f, LP x%.1f, Aufbau %.0f/%.0f min, %d Front(en), neue ab Welle %d, Elite ab %d, %.0f-%.0f m, Kern %.0f LP"),
					S.Name, S.Budget, S.LP, S.Aufbau / 60.f, S.Zwischen / 60.f, S.FrontenStart, S.NeueFrontAb, S.EliteAb, S.MinAbstand / 100.f, S.MaxAbstand / 100.f, S.KernLP));
			}
		}
		// /td kern tempo [Faktor]: Lauftempo aller Wellengegner (1 = Standard je Art), wirkt sofort
		else if (Was == TEXT("tempo"))
		{
			if (Arguments.Num() > 2)
			{
				UTDGegnerHirn::TempoFaktor = FMath::Clamp(FCString::Atof(*Arguments[2]), 0.2f, 3.f);
			}
			Antwort(Sender, FString::Printf(TEXT("Lauftempo der Wellengegner: x%.2f (Hog %.1f m/s, Stinger %.1f m/s, Spitter %.1f m/s)"), UTDGegnerHirn::TempoFaktor,
				7.5f * UTDGegnerHirn::TempoFaktor, 8.5f * UTDGegnerHirn::TempoFaktor, 4.5f * UTDGegnerHirn::TempoFaktor));
		}
		else if (Was == TEXT("springe"))
		{
			// /td kern springe <N>: Test, naechste Welle ist N (nur im Aufbau)
			const int32 N = Arguments.Num() > 2 ? FCString::Atoi(*Arguments[2]) : 0;
			if (!Waves->SpringeZuWelle(N))
			{
				Antwort(Sender, TEXT("/td kern springe <Welle> - nur zwischen den Wellen, Welle 1 bis Wellenzahl."));
			}
		}
		else if (Was == TEXT("stress"))
		{
			// /td kern stress [N ...] [lp=20] [s=20] [mix] | stop: Messreihe (T1, Gegnertakt-Recherche), Standard 40 80 120 160
			if (Arguments.Num() > 2 && Arguments[2].Equals(TEXT("stop"), ESearchCase::IgnoreCase))
			{
				Waves->StoppeStress(TEXT("Stresstest abgebrochen."));
				return EExecutionStatus::COMPLETED;
			}
			TArray<int32> Stufen;
			float LP = 20.f, Sekunden = 60.f, Sterben = 0.f;   // 60 s: Gegner erreichen die Verteidigung (Test 0.33)
			FString Art = TEXT("hog");
			for (int32 i = 2; i < Arguments.Num(); ++i)
			{
				const FString A = Arguments[i].ToLower();
				if (A.StartsWith(TEXT("lp=")))     LP = FMath::Max(0.1f, FCString::Atof(*A.Mid(3)));
				else if (A.StartsWith(TEXT("s="))) Sekunden = FMath::Clamp(FCString::Atof(*A.Mid(2)), 5.f, 120.f);
				else if (A == TEXT("mix") || A == TEXT("hog")) Art = A;
				else if (A.StartsWith(TEXT("art=")))  Art = A.Mid(4);
				else if (A == TEXT("anim=immer"))     UTDGegnerHirn::AnimNurSichtbarErzwungen = 0;
				else if (A == TEXT("anim=sichtbar"))  UTDGegnerHirn::AnimNurSichtbarErzwungen = 1;
				else if (A == TEXT("kollision=aus"))  UTDGegnerHirn::KollisionErzwungen = 0;
				else if (A == TEXT("kollision=an"))   UTDGegnerHirn::KollisionErzwungen = 1;
				else if (A == TEXT("ragdoll=aus"))    UTDGegnerHirn::RagdollErzwungen = 0;
				else if (A == TEXT("ragdoll=an"))     UTDGegnerHirn::RagdollErzwungen = 1;
				else if (A == TEXT("tuerme=aus"))     UTDWaveSubsystem::bStressTuermeAus = true;
				else if (A == TEXT("geschoss=aus"))   UTDWaveSubsystem::bStressOhneGeschoss = true;
				else if (A == TEXT("angriff=aus"))    UTDGegnerHirn::bAngriffAus = true;
				else if (A == TEXT("drehen=aus"))     UTDWaveSubsystem::bStressDrehenAus = true;
				else if (A == TEXT("schatten=aus"))   UTDWaveSubsystem::bStressSchattenAus = true;
				else if (A == TEXT("effekte=aus"))    UTDWaveSubsystem::bStressEffekteAus = true;
				else if (A == TEXT("df=aus"))         UTDWaveSubsystem::bStressDfAus = true;
				else if (A.StartsWith(TEXT("drehtakt="))) UTDWaveSubsystem::StressDrehtakt = FMath::Clamp(FCString::Atof(*A.Mid(9)), 1.f, 30.f);
				else if (A.StartsWith(TEXT("toeten="))) Sterben = FMath::Clamp(FCString::Atof(*A.Mid(7)), 0.f, 20.f);
				else if (const int32 N = FCString::Atoi(*A); N > 0) Stufen.Add(FMath::Min(N, 300));
			}
			if (Stufen.Num() == 0)
			{
				Stufen = { 40, 80, 120, 160 };
			}
			FString Meldung;
			if (!Waves->StarteStress(Kern, Stufen, LP, Sekunden, Art, Sterben, Meldung))
			{
				// Optionen nicht stehen lassen, wenn der Test gar nicht startet
				UTDWaveSubsystem::bStressTuermeAus = false;
				UTDWaveSubsystem::bStressOhneGeschoss = false;
				UTDWaveSubsystem::bStressDrehenAus = UTDWaveSubsystem::bStressSchattenAus = UTDWaveSubsystem::bStressEffekteAus = UTDWaveSubsystem::bStressDfAus = false;
				UTDWaveSubsystem::StressDrehtakt = 0.f;
				UTDGegnerHirn::bAngriffAus = false;
				UTDGegnerHirn::AnimNurSichtbarErzwungen = UTDGegnerHirn::KollisionErzwungen = UTDGegnerHirn::RagdollErzwungen = -1;
			}
			Antwort(Sender, Meldung);
		}
		else if (Was == TEXT("weiter"))
		{
			if (!Waves->NaechsteWelleJetzt())
			{
				Antwort(Sender, TEXT("Gerade kein Aufbau-Countdown."));
			}
		}
		else if (Was == TEXT("stop"))
		{
			Waves->StoppeLauf(TEXT("Kern-Verteidigung abgebrochen."));
		}
		else
		{
			Antwort(Sender, Kern ? FString::Printf(TEXT("Kern %.0f / %.0f LP - %s"), Kern->GetTDHealth(), Kern->GetTDMaxHealth(), *Waves->GetLaufText())
				: FString(TEXT("Kein Abwehrkern gebaut.")));
		}
		return EExecutionStatus::COMPLETED;
	}
	// /td fuellen [art] [m3]: Testhilfe - Fluessigkeitspuffer bzw. Flammenwerfer, auf den du schaust, mit Fluessigkeit fuellen
	if (Arguments[0].Equals(TEXT("fuellen"), ESearchCase::IgnoreCase) || Arguments[0].Equals(TEXT("füllen"), ESearchCase::IgnoreCase))
	{
		static const TMap<FString, FString> Arten = {
			{ TEXT("treibstoff"), TEXT("/Game/FactoryGame/Resource/Parts/Fuel/Desc_LiquidFuel.Desc_LiquidFuel_C") },
			{ TEXT("bio"), TEXT("/Game/FactoryGame/Resource/Parts/BioFuel/Desc_LiquidBiofuel.Desc_LiquidBiofuel_C") },
			{ TEXT("turbo"), TEXT("/Game/FactoryGame/Resource/Parts/Turbofuel/Desc_LiquidTurboFuel.Desc_LiquidTurboFuel_C") },
			{ TEXT("rakete"), TEXT("/Game/FactoryGame/Resource/Parts/RocketFuel/Desc_RocketFuel.Desc_RocketFuel_C") },
			{ TEXT("ionisiert"), TEXT("/Game/FactoryGame/Resource/Parts/IonizedFuel/Desc_IonizedFuel.Desc_IonizedFuel_C") },
			{ TEXT("wasser"), TEXT("/Game/FactoryGame/Resource/RawResources/Water/Desc_Water.Desc_Water_C") },
		};
		const FString Art = Arguments.Num() > 1 ? Arguments[1].ToLower() : TEXT("treibstoff");
		const FString* Pfad = Arten.Find(Art);
		UClass* Fluessigkeit = Pfad ? LoadClass<UFGItemDescriptor>(nullptr, **Pfad) : nullptr;
		if (!Fluessigkeit || !Pawn)
		{
			Antwort(Sender, TEXT("/td fuellen [treibstoff|bio|turbo|rakete|ionisiert|wasser] [m3] - auf einen Fluessigkeitspuffer oder Flammenwerfer schauen"));
			return EExecutionStatus::BAD_ARGUMENTS;
		}
		const float Menge = Arguments.Num() > 2 ? FMath::Max(1.f, FCString::Atof(*Arguments[2])) : 10000.f;
		FVector Auge; FRotator Blick;
		Player->GetPlayerViewPoint(Auge, Blick);
		FHitResult Hit;
		FCollisionQueryParams Query(TEXT("FicsitTDFuellen"), false, Pawn);
		AActor* Ziel = GetWorld()->LineTraceSingleByChannel(Hit, Auge, Auge + Blick.Vector() * 3000.f, ECC_Visibility, Query) ? Hit.GetActor() : nullptr;
		const FString Name = UFGItemDescriptor::GetItemName(Fluessigkeit).ToString();
		if (AFGBuildablePipeReservoir* Puffer = Cast<AFGBuildablePipeReservoir>(Ziel))
		{
			// Netz auf die Fluessigkeit festlegen (klappt nur, wenn es leer ist bzw. dieselbe Fluessigkeit fuehrt), dann Puffer fuellen
			FFluidBox* Box = Puffer->GetFluidBox();
			AFGPipeNetwork* Netz = nullptr;
			if (AFGPipeSubsystem* Rohre = AFGPipeSubsystem::Get(GetWorld()))
			{
				for (UFGPipeConnectionComponent* K : Puffer->GetPipeConnections())
				{
					if (K && (Netz = Rohre->FindPipeNetwork(K->GetPipeNetworkID())) != nullptr)
					{
						break;
					}
				}
			}
			if (Netz)
			{
				Netz->TrySetFluidDescriptor(Fluessigkeit);
			}
			if (!Box || (Netz && Netz->GetFluidDescriptor() != Fluessigkeit))
			{
				Antwort(Sender, FString::Printf(TEXT("Puffer fuehrt schon %s - erst leeren (oder andere Art waehlen)."),
					Netz && Netz->GetFluidDescriptor() ? *UFGItemDescriptor::GetItemName(Netz->GetFluidDescriptor()).ToString() : TEXT("eine andere Fluessigkeit")));
				return EExecutionStatus::COMPLETED;
			}
			Box->Content = FMath::Min(Box->MaxContent, Box->Content + Menge);
			Antwort(Sender, FString::Printf(TEXT("Puffer: %.0f / %.0f m3 %s"), Box->Content, Box->MaxContent, *Name));
			return EExecutionStatus::COMPLETED;
		}
		if (ATDFlameTurret* Flamme = Cast<ATDFlameTurret>(Ziel))
		{
			UFGInventoryComponent* Eingang = Flamme->GetInputInventory();
			const int32 Liter = FMath::RoundToInt(FMath::Min(Menge, 50.f) * 1000.f);
			const int32 Rein = Eingang ? Eingang->AddStack(FInventoryStack(Liter, Fluessigkeit), true) : 0;
			Antwort(Sender, FString::Printf(TEXT("Flammenwerfer: %.1f m3 %s eingefuellt (Rezept muss zur Fluessigkeit passen)."), Rein / 1000.f, *Name));
			return EExecutionStatus::COMPLETED;
		}
		Antwort(Sender, Ziel ? FString::Printf(TEXT("%s ist kein Fluessigkeitspuffer/Flammenwerfer."), *Ziel->GetClass()->GetName())
			: FString(TEXT("Nichts im Blick (max. 30 m).")));
		return EExecutionStatus::COMPLETED;
	}
	// /td probe gewehr|flamme|weg: Vorfuehrmodell mit Animation/Effekt 10 m vor dem Spieler (Seitenansicht)
	if (Arguments[0].Equals(TEXT("probe"), ESearchCase::IgnoreCase))
	{
		const FString Art = Arguments.Num() > 1 ? Arguments[1].ToLower() : FString();
		int32 Weg = 0;
		for (TActorIterator<ATDEffektProbe> It(GetWorld()); It; ++It)
		{
			It->Destroy();
			++Weg;
		}
		if (Art != TEXT("gewehr") && Art != TEXT("flamme"))
		{
			Antwort(Sender, FString::Printf(TEXT("%d Probe(n) entfernt. /td probe gewehr|flamme stellt eine neue auf."), Weg));
			return EExecutionStatus::COMPLETED;
		}
		if (!Pawn)
		{
			return EExecutionStatus::BAD_ARGUMENTS;
		}
		FVector Auge; FRotator Blick;
		Player->GetPlayerViewPoint(Auge, Blick);
		const FRotator Richtung(0.f, Blick.Yaw, 0.f);
		FVector Ort = Pawn->GetActorLocation() + Richtung.Vector() * 1000.f;
		FHitResult Boden;
		if (GetWorld()->LineTraceSingleByChannel(Boden, Ort + FVector(0, 0, 1500.f), Ort - FVector(0, 0, 3000.f), ECC_Visibility))
		{
			Ort = Boden.ImpactPoint;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// quer zur Blickrichtung: man sieht den Turm von der Seite, die Muendung zeigt nach rechts
		ATDEffektProbe* Probe = GetWorld()->SpawnActor<ATDEffektProbe>(Ort, FRotator(0.f, Blick.Yaw + 90.f, 0.f), Params);
		if (Probe)
		{
			Probe->Einrichten(Art == TEXT("flamme"));
		}
		Antwort(Sender, Art == TEXT("flamme")
			? TEXT("Flammenwerfer-Probe steht 10 m vor dir: 3 s Flammenstrahl, 3 s Pause. /td probe weg entfernt sie.")
			: TEXT("Gewehrturm-Probe steht 10 m vor dir: 3 s Feuern (Laeufe drehen hoch, Muendungsfeuer), 3 s Auslaufen. /td probe weg entfernt sie."));
		return EExecutionStatus::COMPLETED;
	}
	// /td teil [Name]: Diagnose der schwarzen Flaechen - ohne Name Bauteile auflisten, mit Name an allen Bolzentuermen umschalten
	if (Arguments[0].Equals(TEXT("teil"), ESearchCase::IgnoreCase))
	{
		FString Liste;
		int32 Umgeschaltet = 0;
		bool bJetztSichtbar = false;
		for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
		{
			TInlineComponentArray<UPrimitiveComponent*> Teile(*It);
			for (UPrimitiveComponent* P : Teile)
			{
				if (Arguments.Num() < 2)
				{
					if (!Liste.Contains(P->GetName()))
					{
						Liste += P->GetName() + TEXT(" ");
					}
				}
				else if (P->GetName().StartsWith(Arguments[1], ESearchCase::IgnoreCase))
				{
					P->SetVisibility(!P->IsVisible());
					bJetztSichtbar = P->IsVisible();
					++Umgeschaltet;
				}
			}
		}
		Antwort(Sender, Arguments.Num() < 2
			? TEXT("Bauteile: ") + Liste + TEXT("| /td teil <Name> schaltet es an allen Tuermen um")
			: FString::Printf(TEXT("%d x '%s' jetzt %s"), Umgeschaltet, *Arguments[1], bJetztSichtbar ? TEXT("sichtbar") : TEXT("versteckt")));
		return EExecutionStatus::COMPLETED;
	}
	// /td baender <2|3>: alle Foerderbaender und Lifte von Hand aufruesten (0.71, sonst automatisch nach MAM-Forschung)
	if (Arguments[0].Equals(TEXT("baender"), ESearchCase::IgnoreCase) || Arguments[0].Equals(TEXT("bänder"), ESearchCase::IgnoreCase))
	{
		// 0.78.3: /td baender pruefen|reparieren - offene Enden mit Gegenstueck zaehlen bzw. verbinden (Spielstaende, die 0.71-0.78.2 aufgeruestet haben)
		if (Arguments.Num() > 1 && (Arguments[1].Equals(TEXT("reparieren"), ESearchCase::IgnoreCase) || Arguments[1].Equals(TEXT("pruefen"), ESearchCase::IgnoreCase)))
		{
			FString Bericht;
			UTDBandUpgrade::VerbindeOffeneEnden(GetWorld(), Arguments[1].Equals(TEXT("pruefen"), ESearchCase::IgnoreCase), &Bericht);
			Antwort(Sender, Bericht);
			return EExecutionStatus::COMPLETED;
		}
		UTDBandUpgrade* Upgrade = UTDBandUpgrade::Get(this);
		Antwort(Sender, Upgrade ? Upgrade->RuesteUm(FMath::Clamp(Arguments.Num() > 1 ? FCString::Atoi(*Arguments[1]) : 2, 2, 6)) : TEXT("nicht verfuegbar"));
		return EExecutionStatus::COMPLETED;
	}
	// /td szenario [liste|<1-5>|pruefen [Nr]|leeren]: Testszenarien (Konzept-Kernplaetze S5, 0.70)
	if (Arguments[0].Equals(TEXT("szenario"), ESearchCase::IgnoreCase))
	{
		const FString Was = Arguments.Num() > 1 ? Arguments[1].ToLower() : TEXT("liste");
		TArray<FString> Zeilen;
		if (Was == TEXT("pruefen") || Was == TEXT("prüfen"))
		{
			Zeilen = TDSzenario::Pruefen(this, Arguments.Num() > 2 ? FCString::Atoi(*Arguments[2]) : 0);
		}
		else if (Was == TEXT("fertig"))
		{
			Zeilen.Add(TDSzenario::Fertig(this));
		}
		else if (Was == TEXT("leeren"))
		{
			Zeilen.Add(TDSzenario::TuermeLeeren(this));
		}
		else if (Was.IsNumeric())
		{
			Zeilen = TDSzenario::Anwenden(this, FCString::Atoi(*Was), Pawn);
		}
		else
		{
			Zeilen = TDSzenario::Liste();
		}
		for (const FString& Z : Zeilen)
		{
			Antwort(Sender, Z);
		}
		return EExecutionStatus::COMPLETED;
	}
	// /td ada: ADA-Begruessung erneut abspielen (0.69.4)
	if (Arguments[0].Equals(TEXT("ada"), ESearchCase::IgnoreCase))
	{
		if (Arguments.Num() > 1 && Arguments[1].Equals(TEXT("bank"), ESearchCase::IgnoreCase))
		{
			Antwort(Sender, UTDKernplatz::PruefeWwiseInitBank());
			return EExecutionStatus::COMPLETED;
		}
		Antwort(Sender, UTDKernplatz::SpieleBegruessung(this, Pawn) ? TEXT("ADA spielt die Begruessung ab.") : TEXT("Begruessung nicht moeglich (siehe Log)."));
		return EExecutionStatus::COMPLETED;
	}
	// /td platz pruefen <G1..D3|hier> | zurueck | liste: Kandidaten fuer feste Kernplaetze (Konzept-Kernplaetze.md, S1)
	if (Arguments[0].Equals(TEXT("platz"), ESearchCase::IgnoreCase))
	{
		UTDKernplatz* Platz = UTDKernplatz::Get(this);
		const FString Was = Arguments.Num() > 1 ? Arguments[1].ToLower() : TEXT("liste");
		if (!Platz)
		{
			return EExecutionStatus::BAD_ARGUMENTS;
		}
		if (Was == TEXT("pruefen") || Was == TEXT("prüfen"))
		{
			Antwort(Sender, Platz->Pruefe(Arguments.Num() > 2 ? Arguments[2] : TEXT("hier"), Pawn, Arguments.Num() > 3 ? Arguments[3] : FString()));
		}
		else if (Was == TEXT("merken"))
		{
			TArray<FString> Rest;
			for (int32 i = 3; i < Arguments.Num(); ++i)
			{
				Rest.Add(Arguments[i]);
			}
			Antwort(Sender, Platz->Merke(Arguments.Num() > 2 ? Arguments[2] : FString(), FString::Join(Rest, TEXT(" ")), Pawn));
		}
		else if (Was == TEXT("bauen"))
		{
			Antwort(Sender, Platz->BauePlatz(Arguments.Num() > 2 ? Arguments[2] : FString(), Arguments.Num() > 3 ? Arguments[3] : FString(), Pawn));
		}
		else if (Was == TEXT("scan"))
		{
			const FString Ziel = Arguments.Num() > 2 ? Arguments[2] : FString();
			Antwort(Sender, Ziel.Equals(TEXT("stop"), ESearchCase::IgnoreCase) || Ziel.Equals(TEXT("abbrechen"), ESearchCase::IgnoreCase)
				? Platz->StoppeScan() : Platz->StarteScan(Ziel, Pawn));
		}
		else if (Was == TEXT("gemerkt"))
		{
			for (const FString& Z : Platz->Gemerkt())
			{
				Antwort(Sender, Z);
			}
		}
		else if (Was == TEXT("zurueck") || Was == TEXT("zurück"))
		{
			Antwort(Sender, Platz->Zurueck(Pawn));
		}
		else
		{
			for (const FTDPlatzKandidat& K : UTDKernplatz::Kandidaten())
			{
				Antwort(Sender, FString::Printf(TEXT("%s: %s bei x %.0f / y %.0f"), K.Id, K.Biom, K.X, K.Y));
			}
			Antwort(Sender, TEXT("/td platz pruefen hier <grasfelder|felsenwueste|wald|duenen> - deinen Standort pruefen (du stehst auf der Kernmitte), /td platz merken <Biom> [Notiz] - speichern, /td platz gemerkt - Liste, /td platz pruefen <Id> - Kandidat anfliegen, /td platz zurueck. Ergebnisse in Saved/FicsitTD/Kernplaetze.csv"));
		}
		return EExecutionStatus::COMPLETED;
	}
	// /td turmlevel <1-5>: naechsten Turm auf ein Level setzen (Level-Optik ansehen, 0.63). Im Balancing-Lauf ein Eingriff.
	if (Arguments[0].Equals(TEXT("turmlevel"), ESearchCase::IgnoreCase) || Arguments[0].Equals(TEXT("turmstufe"), ESearchCase::IgnoreCase))
	{
		const int32 Stufe = Arguments.Num() > 1 ? FCString::Atoi(*Arguments[1]) : 5;
		ATDTurretBase* Bester = nullptr;
		float BestAbstand = TNumericLimits<float>::Max();
		for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
		{
			const float D = Pawn ? FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) : 0.f;
			if (D < BestAbstand)
			{
				BestAbstand = D;
				Bester = *It;
			}
		}
		if (!Bester)
		{
			Antwort(Sender, TEXT("Kein Turm gefunden."));
			return EExecutionStatus::BAD_ARGUMENTS;
		}
		Bester->SetzeLevelTest(Stufe);
		if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
		{
			Lauf->Eingriff(FString::Printf(TEXT("turmlevel %d"), Stufe));
		}
		Antwort(Sender, FString::Printf(TEXT("%s in %.0f m: Level %d (%s)"), *Bester->GetTurmName(), FMath::Sqrt(BestAbstand) / 100.f, Bester->GetLevel(), *Bester->LevelSumme()));
		return EExecutionStatus::COMPLETED;
	}
	// /td reichweite [an|aus]: Kreise an allen Bolzentuermen (ohne Angabe umschalten)
	if (Arguments[0].Equals(TEXT("reichweite"), ESearchCase::IgnoreCase))
	{
		static bool bAn = false;
		bAn = Arguments.Num() > 1 ? Arguments[1].Equals(TEXT("an"), ESearchCase::IgnoreCase) : !bAn;
		int32 Tuerme = 0;
		for (TActorIterator<ATDTurretBase> It(GetWorld()); It; ++It)
		{
			It->ZeigeReichweite(bAn);
			if (Tuerme++ < 8)
			{
				Antwort(Sender, FString::Printf(TEXT("%s %d: Reichweite %.1f m (Level %d)"), *It->GetTurmName(), Tuerme, It->GetReichweite() / 100.f, It->GetLevel()));
			}
		}
		Antwort(Sender, FString::Printf(TEXT("Reichweitenkreise %s an %d Tuermen"), bAn ? TEXT("eingeblendet") : TEXT("ausgeblendet"), Tuerme));
		return EExecutionStatus::COMPLETED;
	}
	const bool bInstanzen = Arguments[0].Equals(TEXT("instanzen"), ESearchCase::IgnoreCase);
	if (!Pawn || !(bInstanzen || Arguments[0].Equals(TEXT("umgebung"), ESearchCase::IgnoreCase)))
	{
		PrintCommandUsage(Sender);
		return EExecutionStatus::BAD_ARGUMENTS;
	}
	const float Radius = (Arguments.Num() > 1 ? FMath::Clamp(FCString::Atof(*Arguments[1]), 1.f, 50.f) : 10.f) * 100.f;
	const FVector Mitte = Pawn->GetActorLocation();

	if (bInstanzen)
	{
		// Gebaeude werden als Instanzen gezeichnet (kein eigener sichtbarer Actor) - einzelne Instanzen im Umkreis suchen
		int32 Gefunden = 0;
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			TInlineComponentArray<UInstancedStaticMeshComponent*> ISMs(*It);
			for (UInstancedStaticMeshComponent* ISM : ISMs)
			{
				for (int32 i = 0; i < ISM->GetInstanceCount(); ++i)
				{
					FTransform T;
					if (!ISM->GetInstanceTransform(i, T, true) || FVector::Dist(T.GetLocation(), Mitte) > Radius)
					{
						continue;
					}
					const FVector L = T.GetLocation() - Mitte;
					const FString Text = FString::Printf(TEXT("%.1f m (%+.1f,%+.1f,%+.1f) %s Instanz %d Skala %s | %s"), L.Size() / 100.f, L.X / 100.f, L.Y / 100.f, L.Z / 100.f,
						ISM->GetStaticMesh() ? *ISM->GetStaticMesh()->GetName() : TEXT("?"), i, *T.GetScale3D().ToCompactString(), *It->GetClass()->GetName());
					UE_LOG(LogFicsitTD, Log, TEXT("Instanz: %s"), *Text);
					if (Gefunden++ < 12)
					{
						Antwort(Sender, Text.Left(160));
					}
				}
			}
		}
		Antwort(Sender, FString::Printf(TEXT("%d Instanzen im Umkreis von %.0f m (vollstaendig im Log)"), Gefunden, Radius / 100.f));
		return EExecutionStatus::COMPLETED;
	}

	struct FFund { float Abstand; FString Text; };
	TArray<FFund> Funde;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* A = *It;
		if (A == Pawn || A->IsHidden() && !A->GetRootComponent())
		{
			continue;
		}
		const float Abstand = FVector::Dist(A->GetActorLocation(), Mitte);
		if (Abstand > Radius)
		{
			continue;
		}
		FString Sichtbar;
		TInlineComponentArray<UPrimitiveComponent*> Teile(A);
		for (const UPrimitiveComponent* P : Teile)
		{
			if (P->IsVisible() && !P->bHiddenInGame)
			{
				const UStaticMeshComponent* SM = Cast<UStaticMeshComponent>(P);
				Sichtbar += FString::Printf(TEXT("%s%s "), *P->GetClass()->GetName(),
					SM && SM->GetStaticMesh() ? *FString::Printf(TEXT("[%s]"), *SM->GetStaticMesh()->GetName()) : TEXT(""));
			}
		}
		const FVector L = A->GetActorLocation() - Mitte;
		Funde.Add({ Abstand, FString::Printf(TEXT("%.1f m (%+.0f,%+.0f,%+.0f) %s %s | sichtbar: %s"), Abstand / 100.f, L.X / 100.f, L.Y / 100.f, L.Z / 100.f,
			*A->GetClass()->GetName(), A->IsHidden() ? TEXT("(versteckt)") : TEXT(""), Sichtbar.IsEmpty() ? TEXT("-") : *Sichtbar) });
	}
	Funde.Sort([](const FFund& X, const FFund& Y) { return X.Abstand < Y.Abstand; });
	Antwort(Sender, FString::Printf(TEXT("%d Objekte im Umkreis von %.0f m (vollstaendig im Log, LogFicsitTD):"), Funde.Num(), Radius / 100.f));
	for (int32 i = 0; i < Funde.Num(); ++i)
	{
		UE_LOG(LogFicsitTD, Log, TEXT("Umgebung: %s"), *Funde[i].Text);
		if (i < 12)
		{
			Antwort(Sender, Funde[i].Text.Left(160));
		}
	}
	return EExecutionStatus::COMPLETED;
}
