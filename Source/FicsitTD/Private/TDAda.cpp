// 0.12-dev (Marius 16.11.): ADA-Ansagen bei neuen Gegnerarten und Bossen. Ton: hoeflich, korporativ, trocken; Witz aus
// Untertreibung und FICSIT-Effizienzdenken, eigene Texte im Stil des Spiels (Entwurf Notizen\ADA-Ansagen-Entwurf-2026-11-16.md).
#include "TDAda.h"
#include "TDDefenseCore.h"
#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDSprache.h"
#include "FicsitTDModule.h"
#include "FGChatManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

namespace
{
	struct FTDAdaSatz
	{
		const TCHAR* Key;
		/** Klassenname der Kreatur (ohne _C) fuer neue Arten, leer bei Bossen */
		const TCHAR* Klasse;
		const TCHAR* De;
		const TCHAR* En;
	};

	const FTDAdaSatz TDAdaSaetze[] = {
		{ TEXT("Hog"), TEXT("Char_Hog"),
			TEXT("Bewegung im Gelände erfasst. Die lokale Fauna hat Ihre Anwesenheit bemerkt und reagiert erwartungsgemäß unkooperativ."),
			TEXT("Movement detected. The local fauna has noticed your presence and is responding as uncooperatively as projected.") },
		{ TEXT("AlphaHog"), TEXT("Char_AlphaHog"),
			TEXT("Ein größeres Exemplar nähert sich. FICSIT empfiehlt, es nicht persönlich zu nehmen. Es meint es persönlich."),
			TEXT("A larger specimen is approaching. FICSIT recommends not taking it personally. It does.") },
		{ TEXT("StingerKlein"), TEXT("Char_Stinger_Child"),
			TEXT("Neue Phase. Kleine, schnelle Organismen im Anflug. Bitte zählen Sie sie nicht, das demotiviert nur."),
			TEXT("New phase. Small, fast organisms inbound. Please do not count them. It only hurts morale.") },
		{ TEXT("Stinger"), TEXT("Char_Stinger"),
			TEXT("Die Stinger sind ausgewachsen. Die Abteilung für Arbeitssicherheit bittet um Abstand. Mehr Abstand."),
			TEXT("The Stingers have grown up. Workplace Safety requests that you keep your distance. More distance.") },
		{ TEXT("Spucker"), TEXT("Char_SpitterForestSmall"),
			TEXT("Neue Phase. Diese Art greift aus der Ferne an. Für Ihre Rüstung haftet FICSIT nicht, für Säureflecken ebenfalls nicht."),
			TEXT("New phase. This species attacks from range. FICSIT is not liable for your armor. Or for acid stains.") },
		{ TEXT("RoterSpucker"), TEXT("Char_SpitterForestRedSmall"),
			TEXT("Rote Spucker. Die Farbe ist kein Hinweis auf Freundlichkeit."),
			TEXT("Red Spitters. The color is not an indicator of friendliness.") },
		{ TEXT("AlphaSpucker"), TEXT("Char_SpitterForestAlpha"),
			TEXT("Ein Alpha-Spucker. Größer, säurehaltiger, und nach unseren Daten schlecht gelaunt."),
			TEXT("An Alpha Spitter. Larger, more acidic and, according to our data, in a bad mood.") },
		{ TEXT("RoterAlphaSpucker"), TEXT("Char_SpitterForestRedAlpha"),
			TEXT("Roter Alpha-Spucker gemeldet. Bitte prüfen Sie, ob Ihre Munitionsversorgung Ihre Zuversicht teilt."),
			TEXT("Red Alpha Spitter reported. Please check whether your ammunition supply shares your confidence.") },
		{ TEXT("KlippenHog"), TEXT("Char_CliffHog"),
			TEXT("Neue Phase. Fremde Arten aus anderen Biomen. Sie haben die Klippen nicht als Hindernis verstanden."),
			TEXT("New phase. Foreign species from other biomes. They did not interpret the cliffs as an obstacle.") },
		{ TEXT("WuestenSpucker"), TEXT("Char_SpitterDesertSmall"),
			TEXT("Wüstenspucker im Anmarsch. Sie haben einen weiten Weg hinter sich und schlechte Absichten vor sich."),
			TEXT("Desert Spitters approaching. They have come a long way and have poor intentions.") },
		{ TEXT("WuestenAlpha"), TEXT("Char_SpitterDesertAlpha"),
			TEXT("Ein Wüsten-Alpha. Hitzebeständig. Ihre Flammenwerfer bitte entsprechend einordnen."),
			TEXT("A Desert Alpha. Heat resistant. Please adjust your expectations of the flamethrowers accordingly.") },
		{ TEXT("WasserSpucker"), TEXT("Char_SpitterAquaticSmall"),
			TEXT("Wasserspucker an Land. Ein bemerkenswerter Karriereschritt."),
			TEXT("Aquatic Spitters on land. A remarkable career move.") },
		{ TEXT("WasserAlpha"), TEXT("Char_SpitterAquaticAlpha"),
			TEXT("Wasser-Alpha erfasst. Der Abschnitt „Nicht schwimmen“ im Handbuch wurde gerade wichtiger."),
			TEXT("Aquatic Alpha detected. The handbook section \"Do not swim\" has just become more relevant.") },
		{ TEXT("EliteStinger"), TEXT("Char_EliteStinger"),
			TEXT("Elite-Stinger. Sie stoßen Gas aus. Bitte atmen Sie weniger."),
			TEXT("Elite Stingers. They emit gas. Please breathe less.") },
		{ TEXT("NuklearHog"), TEXT("Char_NuclearHog"),
			TEXT("Neue Phase. Strahlung erfasst. FICSIT erinnert daran, dass Strahlenschäden nicht unter die Pionier-Versicherung fallen. Es gibt keine Pionier-Versicherung."),
			TEXT("New phase. Radiation detected. FICSIT reminds you that radiation damage is not covered by Pioneer insurance. There is no Pioneer insurance.") },
		{ TEXT("Johnny"), TEXT("Char_Johnny"),
			TEXT("Bitte bewahren Sie Ruhe. Das ist Johnny. Johnny hat keine Personalakte. Johnny braucht keine."),
			TEXT("Please remain calm. That is Johnny. Johnny does not have a personnel file. Johnny does not need one.") },
		{ TEXT("Boss10"), TEXT(""),
			TEXT("Ein ungewöhnlich großes Objekt hält auf den Kern zu. Die Abteilung für Optimismus ist heute nicht besetzt."),
			TEXT("An unusually large object is heading for the Core. The Department of Optimism is unstaffed today.") },
		{ TEXT("Boss20"), TEXT(""),
			TEXT("Die Brutmutter. Alle Stinger, die Sie bisher getroffen haben, waren ihre Kinder. Sie ist entsprechend gestimmt."),
			TEXT("The Brood Mother. Every Stinger you have met so far was one of her children. She is in a corresponding mood.") },
		{ TEXT("Boss30"), TEXT(""),
			TEXT("Die Säurekönigin. FICSIT empfiehlt die Verteidigung des Kerns. Ihre Rüstung empfiehlt etwas anderes."),
			TEXT("The Acid Queen. FICSIT recommends defending the Core. Your armor recommends otherwise.") },
		{ TEXT("Boss40"), TEXT(""),
			TEXT("Ein Gasriese. Zur Einordnung: Die Elite-Stinger waren die Vorstufe."),
			TEXT("A Gas Giant. For context: the Elite Stingers were the warm-up.") },
		{ TEXT("Boss50"), TEXT(""),
			TEXT("Mehrere Johnnys. FICSIT hält das statistisch für unwahrscheinlich und bittet Sie trotzdem, es zu verhindern."),
			TEXT("Multiple Johnnys. FICSIT considers this statistically unlikely and asks you to prevent it anyway.") },
		// Endlos ab Welle 60: die Bosse kommen reihum wieder - ein kurzer Satz statt Wiederholung
		{ TEXT("BossWieder"), TEXT(""),
			TEXT("Bosswelle. FICSIT hat aufgehört, die Bosse zu zählen. Sie bitte nicht."),
			TEXT("Boss wave. FICSIT has stopped counting the bosses. Please do not.") },
	};

	const FTDAdaSatz* Finde(const FString& Key)
	{
		for (const FTDAdaSatz& S : TDAdaSaetze)
		{
			if (Key == S.Key)
			{
				return &S;
			}
		}
		return nullptr;
	}

	/** Stimme ueber die Unreal-Audioengine wie die Begruessung (Scripts/Audio/Erzeuge-ADA-Ansagen.ps1 erzeugt SW_TD_ADA_<Key>_<de|en>).
	 *  Kein Nachrichtenfenster: Wwise lehnt Mod-Banken in 1.2.5 ab, der Text steht deshalb immer im Chat. */
	USoundBase* LadeTon(const FString& Key)
	{
		const FString Sprache = TDSprache::Englisch() ? TEXT("en") : TEXT("de");
		return LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/FicsitTD/Audio/SW_TD_ADA_%s_%s.SW_TD_ADA_%s_%s"), *Key, *Sprache, *Key, *Sprache));
	}

	/** 16.11. (Marius: "die ADA-Ansagen am Anfang ueberlagern sich"): ADA spricht nie gleichzeitig. Bis wann sie belegt ist
	 *  (Echtzeit), je Welt - die Begruessung meldet sich ueber TDAda::Belege an. */
	TMap<TWeakObjectPtr<UWorld>, double>& BelegtBis()
	{
		static TMap<TWeakObjectPtr<UWorld>, double> Karte;
		return Karte;
	}

	void Chat(UWorld* World, const FString& Text)
	{
		if (AFGChatManager* Chat = AFGChatManager::Get(World))
		{
			FChatMessageStruct Nachricht;
			Nachricht.MessageText = FText::FromString(Text);
			Nachricht.MessageType = EFGChatMessageType::CMT_CustomMessage;
			Nachricht.MessageSender = FText::FromString(TEXT("ADA"));
			Nachricht.MessageSenderColor = FLinearColor(0.95f, 0.6f, 0.15f);
			Chat->BroadcastChatMessage(Nachricht);
		}
	}
}

namespace TDAda
{
	bool Sage(const UObject* WorldContext, ATDDefenseCore* Kern, const FString& Key)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const FTDAdaSatz* S = Finde(Key);
		if (!World || !S || TDBalance::Wert(TEXT("ADA.Ansagen"), 1.f) <= 0.f || (Kern && Kern->mAdaGesagt.Contains(Key)))
		{
			return false;
		}
		if (Kern)
		{
			Kern->mAdaGesagt.Add(Key);
		}
		const FString Text = TDSprache::Englisch() ? S->En : S->De;
		USoundBase* Ton = LadeTon(Key);
		// Warteschlange: erst sprechen, wenn ADA frei ist (Begruessung, vorige Ansage), Chatzeile zum Beginn der Stimme
		const double Jetzt = FPlatformTime::Seconds();
		double& Frei = BelegtBis().FindOrAdd(World);
		const double Start = FMath::Max(Jetzt, Frei);
		Frei = Start + (Ton ? Ton->GetDuration() : 4.f) + TDBalance::Wert(TEXT("ADA.Pause"), 1.5f);
		const float Warten = static_cast<float>(Start - Jetzt);
		UE_LOG(LogFicsitTD, Log, TEXT("ADA-Ansage %s (%s, in %.1f s): %s"), *Key, Ton ? TEXT("Stimme") : TEXT("Chat"), Warten, *Text);
		TWeakObjectPtr<UWorld> WeakWelt = World;
		auto Sprechen = [WeakWelt, Key, Text]()
		{
			if (UWorld* W = WeakWelt.Get())
			{
				if (USoundBase* Jetzt = LadeTon(Key))   // erst jetzt laden: in der Wartezeit koennte die Speicherbereinigung ihn entfernen
				{
					UGameplayStatics::PlaySound2D(W, Jetzt, 1.f, 1.f, 0.f, nullptr, nullptr, true);
				}
				Chat(W, Text);
			}
		};
		if (Warten < 0.05f)
		{
			Sprechen();
		}
		else
		{
			// Timer laufen in Spielzeit: Echtzeit-Wartezeit mit dem Zeitraffer umrechnen
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda(Sprechen), Warten * FMath::Max(0.01f, World->GetWorldSettings()->GetEffectiveTimeDilation()), false);
		}
		return true;
	}

	void Belege(const UObject* WorldContext, float Sekunden)
	{
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		if (World)
		{
			double& Frei = BelegtBis().FindOrAdd(World);
			Frei = FMath::Max(Frei, FPlatformTime::Seconds()) + Sekunden + TDBalance::Wert(TEXT("ADA.Pause"), 1.5f);
		}
	}

	int32 VorWelle(const UObject* WorldContext, ATDDefenseCore* Kern, int32 Welle, const TArray<FTDPlanGruppe>& Plan)
	{
		int32 N = 0;
		// Boss zuerst: er ist die wichtigere Nachricht
		if (Welle % 10 == 0)
		{
			N += Sage(WorldContext, Kern, Welle <= 50 ? FString::Printf(TEXT("Boss%d"), Welle) : FString(TEXT("BossWieder"))) ? 1 : 0;
			if (Welle > 50 && Kern)
			{
				Kern->mAdaGesagt.Remove(TEXT("BossWieder"));   // jede Bosswelle ab 60 darf den kurzen Satz wieder sagen
			}
		}
		for (const FTDPlanGruppe& G : Plan)
		{
			// Welle 1 hat keine Einfuehrungsgruppe (die Hogs sind die ganze Welle, Test Endlos 16.11.: keine Ansage) - dort zaehlt jede Art
			if ((!G.bNeu && Welle != 1) || G.bBoss)
			{
				continue;
			}
			FString Klasse = FPaths::GetExtension(G.Pfad);   // ".../Char_Hog.Char_Hog_C" -> "Char_Hog_C"
			Klasse.RemoveFromEnd(TEXT("_C"));
			for (const FTDAdaSatz& S : TDAdaSaetze)
			{
				if (Klasse == S.Klasse)
				{
					N += Sage(WorldContext, Kern, S.Key) ? 1 : 0;
				}
			}
		}
		return N;
	}

	TArray<FString> Schluessel()
	{
		TArray<FString> Alle;
		for (const FTDAdaSatz& S : TDAdaSaetze)
		{
			Alle.Add(S.Key);
		}
		return Alle;
	}
}
