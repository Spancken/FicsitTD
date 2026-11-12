#include "TDForschung.h"
#include "TDBalance.h"
#include "TDWaveSubsystem.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"
#include "Resources/FGItemDescriptor.h"
#include "Unlocks/FGUnlockInfoOnly.h"
#include "FicsitTDModule.h"
#include "TDSprache.h"

#include "TDForschungInfo.inl"

namespace TDForschung
{
	const TArray<FTDForschungKnoten>& Alle()
	{
		static TArray<FTDForschungKnoten> Liste;
		if (Liste.Num() == 0)
		{
			for (const FTDForschungInfoRoh& R : TDForschungInfoRoh)
			{
				FTDForschungKnoten K;
				K.Baum = R.Baum;
				K.BaumName = R.BaumName;
				K.Key = R.Key;
				K.Name = R.Name;
				K.Text = R.Text;
				K.Laden = R.Laden;
				K.Gegen = R.Gegen;
				K.FP = R.FP;
				K.StandardWelle = R.AbWelle;
				FString(R.Braucht).ParseIntoArray(K.Braucht, TEXT(","), true);
				K.Andere = R.Andere;
				K.Pfad = R.Pfad;
				Liste.Add(K);
			}
		}
		return Liste;
	}

	const FTDForschungKnoten* Finde(const FString& Key)
	{
		return Alle().FindByPredicate([&Key](const FTDForschungKnoten& K) { return K.Key == Key; });
	}

	static bool Erforscht(const UObject* WorldContext, const FString& Pfad)
	{
		// Klassen einmal laden und behalten (die Schematics sind ohnehin registriert)
		static TMap<FString, TWeakObjectPtr<UClass>> Klassen;
		TWeakObjectPtr<UClass>& Klasse = Klassen.FindOrAdd(Pfad);
		if (!Klasse.IsValid())
		{
			Klasse = LoadClass<UFGSchematic>(nullptr, *Pfad);
		}
		UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		AFGSchematicManager* Schemata = World ? AFGSchematicManager::Get(World) : nullptr;
		return Schemata && Klasse.IsValid() && Schemata->IsSchematicPurchased(Klasse.Get());
	}

	bool Hat(const UObject* WorldContext, const TCHAR* Key)
	{
		const FTDForschungKnoten* K = Finde(Key);
		return K && Erforscht(WorldContext, K->Pfad);
	}

	int32 AbWelle(const FTDForschungKnoten& K)
	{
		// 0.74 (Selbsttest: Warnung "Forschung.Kettenblitz.AbWelle fehlt"): eigene Welle nur, wenn der Knoten einen Schluessel hat,
		// sonst erbt er sie von Eltern und Voraussetzungen (wie eff_welle im Forschungsskript)
		const FString Schluessel = FString::Printf(TEXT("Forschung.%s.AbWelle"), *K.Key);
		int32 W = TDBalance::Hat(Schluessel) ? TDBalance::Ganz(Schluessel, 0) : 0;
		for (const FString& V : K.Braucht)
		{
			if (const FTDForschungKnoten* E = Finde(V))
			{
				W = FMath::Max(W, AbWelle(*E));
			}
		}
		return W;
	}

	FString GewaehlteAndere(const UObject* WorldContext, const FTDForschungKnoten& K)
	{
		// 0.73: Gabelung mit mehreren Seiten (Doktrin), Andere = Schluessel mit Komma
		TArray<FString> Seiten;
		K.Andere.ParseIntoArray(Seiten, TEXT(","), true);
		for (const FString& A : Seiten)
		{
			if (Hat(WorldContext, *A))
			{
				return A;
			}
		}
		return FString();
	}

	ETDForschungZustand Zustand(const UObject* WorldContext, const FTDForschungKnoten& K)
	{
		if (Erforscht(WorldContext, K.Pfad))
		{
			return ETDForschungZustand::Erforscht;
		}
		if (!GewaehlteAndere(WorldContext, K).IsEmpty())
		{
			return ETDForschungZustand::Ausgeschlossen;
		}
		for (const FString& V : K.Braucht)
		{
			if (!Hat(WorldContext, *V))
			{
				return ETDForschungZustand::Gesperrt;
			}
		}
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
		const int32 Naechste = Waves ? Waves->GetWelle() + 1 : 1;
		return AbWelle(K) > Naechste ? ETDForschungZustand::ZuFrueh : ETDForschungZustand::Moeglich;
	}

	/** 0.85 (L1): Knoten- und Baumname in der Spielsprache. Die Knotentabelle ist deutsch (Schluessel, Vergleiche);
	 *  englisch kommen die Baumnamen von hier, die Knotennamen aus der Asset-Uebersetzung (TDSprache::Nachschlagen) */
	static FString Anzeige(const FString& De)
	{
		if (!TDSprache::Englisch())
		{
			return De;
		}
		if (De == TEXT("Industrie")) return TEXT("Industry");
		if (De == TEXT("Ballistik")) return TEXT("Ballistics");
		if (De == TEXT("Elementar")) return TEXT("Elemental");
		if (De == TEXT("Festung")) return TEXT("Fortress");
		const TCHAR* En = TDSprache::Nachschlagen(De);
		return En ? FString(En) : De;
	}

	static FString NameVon(const FString& Key, const FString& EigenerBaum)
	{
		const FTDForschungKnoten* V = Finde(Key);
		if (!V)
		{
			return Key;
		}
		return V->Baum == EigenerBaum ? Anzeige(V->Name) : FString::Printf(TEXT("%s (%s)"), *Anzeige(V->Name), *Anzeige(V->BaumName));
	}

	FString StatusTitel(const UObject* WorldContext, const FTDForschungKnoten& K, FString& OutText)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
		const int32 Naechste = Waves ? Waves->GetWelle() + 1 : 1;
		switch (Zustand(WorldContext, K))
		{
		case ETDForschungZustand::Erforscht:
			OutText = TDT(TEXT("Dieser Knoten ist erforscht."), TEXT("This node is researched."));
			return TDT(TEXT("Erforscht"), TEXT("Researched"));
		case ETDForschungZustand::Ausgeschlossen:
			OutText = TDF(TEXT("Abzweigung: In diesem Einsatz ist schon %s erforscht. Nur eine Seite ist möglich."),
				TEXT("Branch: %s is already researched in this operation. Only one side is possible."), *NameVon(GewaehlteAndere(WorldContext, K), K.Baum));
			return TDF(TEXT("Ausgeschlossen: %s gewählt"), TEXT("Excluded: %s chosen"), *NameVon(GewaehlteAndere(WorldContext, K), K.Baum));
		case ETDForschungZustand::Gesperrt:
		{
			TArray<FString> Fehlt;
			for (const FString& V : K.Braucht)
			{
				if (!Hat(WorldContext, *V))
				{
					Fehlt.Add(NameVon(V, K.Baum));
				}
			}
			OutText = TDF(TEXT("Zuerst im MAM erforschen: %s."), TEXT("Research in the MAM first: %s."), *FString::Join(Fehlt, TEXT(", ")));
			return TDF(TEXT("Gesperrt: zuerst %s"), TEXT("Locked: first %s"), Fehlt.Num() ? *Fehlt[0] : *TDT(TEXT("Voraussetzung"), TEXT("prerequisite")));
		}
		case ETDForschungZustand::ZuFrueh:
		{
			const int32 Ab = AbWelle(K);
			const int32 Noch = FMath::Max(1, Ab - Naechste);
			OutText = TDF(TEXT("Freigabe ab Welle %d. Nächste Welle: %d, also noch %d %s."), TEXT("Unlocks from wave %d. Next wave: %d, so %d more %s."), Ab, Naechste, Noch,
				Noch == 1 ? *TDT(TEXT("Welle"), TEXT("wave")) : *TDT(TEXT("Wellen"), TEXT("waves")));
			return TDF(TEXT("Gesperrt: ab Welle %d"), TEXT("Locked: from wave %d"), Ab);
		}
		default:
			OutText = TDF(TEXT("Alle Voraussetzungen erfüllt. Kosten: %d FP (FICSIT-Kampfdaten)."), TEXT("All prerequisites met. Cost: %d RP (FICSIT Combat Data)."), K.FP);
			return TDT(TEXT("Jetzt erforschbar"), TEXT("Researchable now"));
		}
	}

	FString Basisname(const UClass* Schematic)
	{
		if (!Schematic)
		{
			return FString();
		}
		const FString Pfad = Schematic->GetPathName();
		for (const FTDForschungKnoten& K : Alle())
		{
			if (K.Pfad == Pfad)
			{
				return Anzeige(K.Name);
			}
		}
		return UFGSchematic::GetSchematicDisplayName(TSubclassOf<UFGSchematic>(const_cast<UClass*>(Schematic))).ToString();
	}

	/** 0.9.6: besseres Symbol je Knoten (Beschreibungsklasse, deren grosses Icon genommen wird); leer = Symbol aus create_research.py */
	static UTexture2D* BesseresSymbol(const FString& Key)
	{
		static const TMap<FString, FString> Desc = [] {
			const FString F = TEXT("/Game/FactoryGame/Buildable/Factory/");
			const FString T = TEXT("/FicsitTD/Buildings/");
			auto Spiel = [&F](const TCHAR* N) { return FString::Printf(TEXT("%s%s/Desc_%s.Desc_%s_C"), *F, N, N, N); };
			auto Turm = [&T](const TCHAR* Ordner, const TCHAR* N) { return FString::Printf(TEXT("%s%s/Desc_TD_%s.Desc_TD_%s_C"), *T, Ordner, N, N); };
			TMap<FString, FString> M;
			// Industrie: Maschinen und Baender
			M.Add(TEXT("Kohlekraft"), Spiel(TEXT("GeneratorCoal")));
			M.Add(TEXT("Assembler"), Spiel(TEXT("AssemblerMk1")));
			M.Add(TEXT("Giesserei"), Spiel(TEXT("SmelterMk1")));
			M.Add(TEXT("Raffinerie"), Spiel(TEXT("OilRefinery")));
			M.Add(TEXT("Treibstoffkraft"), Spiel(TEXT("GeneratorFuel")));
			M.Add(TEXT("Schwerindustrie"), Spiel(TEXT("ManufacturerMk1")));
			M.Add(TEXT("Mischtechnik"), Spiel(TEXT("Blender")));
			M.Add(TEXT("BandMk2"), Spiel(TEXT("ConveyorBeltMk2")));
			M.Add(TEXT("BandMk3"), Spiel(TEXT("ConveyorBeltMk3")));
			// neue Tuerme und Bauten: das Gebaeude selbst
			M.Add(TEXT("Gewehrturm"), Turm(TEXT("GunTurret"), TEXT("GunTurret")));
			M.Add(TEXT("NobeliskWerfer"), Turm(TEXT("NobeliskLauncher"), TEXT("NobeliskLauncher")));
			M.Add(TEXT("Himmelsturm"), Turm(TEXT("FlakTurret"), TEXT("FlakTurret")));
			M.Add(TEXT("Singularitaet"), Turm(TEXT("SingularityLauncher"), TEXT("SingularityLauncher")));
			M.Add(TEXT("Teslaspule"), Turm(TEXT("TeslaCoil"), TEXT("TeslaCoil")));
			M.Add(TEXT("Flammenwerfer"), Turm(TEXT("FlameTurret"), TEXT("FlameTurret")));
			M.Add(TEXT("Oelschlick"), Turm(TEXT("SlickPlate"), TEXT("SlickPlate")));
			M.Add(TEXT("Elektrozaun"), Turm(TEXT("ConductorWall"), TEXT("ConductorWall")));
			M.Add(TEXT("Reparaturdrohnen"), Turm(TEXT("DronePort"), TEXT("DronePort")));
			M.Add(TEXT("Kampfdrohnen"), Turm(TEXT("DronePort"), TEXT("DronePort")));
			M.Add(TEXT("Tor"), Turm(TEXT("Gate"), TEXT("Gate")));
			M.Add(TEXT("Lockfeuer"), Turm(TEXT("LureBeacon"), TEXT("LureBeacon")));
			M.Add(TEXT("Funkmast"), Turm(TEXT("RadioMast"), TEXT("RadioMast")));
			M.Add(TEXT("Schildgenerator"), Turm(TEXT("ShieldGenerator"), TEXT("ShieldGenerator")));
			M.Add(TEXT("Stoerfeld"), Turm(TEXT("FieldProjector"), TEXT("FieldProjector")));
			M.Add(TEXT("MauerMk2"), Turm(TEXT("DefenseWall"), TEXT("DefenseWall")));
			M.Add(TEXT("Stachelmauer"), Turm(TEXT("DefenseWall"), TEXT("DefenseWall")));
			// Kern-Faehigkeiten: der Kern
			for (const TCHAR* K : { TEXT("KernPanzer"), TEXT("KernNotrep"), TEXT("KernSchild"), TEXT("KernGeschuetz") })
			{
				M.Add(K, Turm(TEXT("DefenseCore"), TEXT("DefenseCore")));
			}
			// Doktrinen: der Turm der Linie
			M.Add(TEXT("DoktrinBolzen"), Turm(TEXT("BoltTurret"), TEXT("BoltTurret")));
			M.Add(TEXT("DoktrinGewehr"), Turm(TEXT("GunTurret"), TEXT("GunTurret")));
			M.Add(TEXT("DoktrinNobelisk"), Turm(TEXT("NobeliskLauncher"), TEXT("NobeliskLauncher")));
			M.Add(TEXT("DoktrinTesla"), Turm(TEXT("TeslaCoil"), TEXT("TeslaCoil")));
			M.Add(TEXT("DoktrinFlamme"), Turm(TEXT("FlameTurret"), TEXT("FlameTurret")));
			// Tesla-Faehigkeiten: Sprung = Spule, Ueberlast = Energie-Splitter, Puffer = Batterie
			M.Add(TEXT("Kettenblitz"), Turm(TEXT("TeslaCoil"), TEXT("TeslaCoil")));
			M.Add(TEXT("Ueberlast"), TEXT("/Game/FactoryGame/Resource/Environment/Crystal/Desc_CrystalShard.Desc_CrystalShard_C"));
			M.Add(TEXT("Akkubank"), TEXT("/Game/FactoryGame/Resource/Parts/Battery/Desc_Battery.Desc_Battery_C"));
			M.Add(TEXT("Munitionspuffer"), Spiel(TEXT("StorageContainerMk1")));
			return M;
		}();
		const FString* Pfad = Desc.Find(Key);
		UClass* Klasse = Pfad ? LoadClass<UFGItemDescriptor>(nullptr, **Pfad) : nullptr;
		return Klasse ? UFGItemDescriptor::GetBigIcon(TSubclassOf<UFGItemDescriptor>(Klasse)) : nullptr;
	}

	void AktualisiereMamKarten(const UObject* WorldContext)
	{
		static FTextProperty* NameProp = FindFProperty<FTextProperty>(UFGSchematic::StaticClass(), TEXT("mDisplayName"));
		static TMap<FString, FString> Zuletzt;
		static bool bGemeldet = false;
		if (!NameProp)
		{
			return;
		}
		int32 Knoten = 0;
		for (const FTDForschungKnoten& K : Alle())
		{
			UClass* Klasse = LoadClass<UFGSchematic>(nullptr, *K.Pfad);
			UObject* CDO = Klasse ? Klasse->GetDefaultObject() : nullptr;
			if (!CDO)
			{
				continue;
			}
			++Knoten;
			const FString Name = Anzeige(K.Name);
			FString Titel = Name;
			switch (Zustand(WorldContext, K))
			{
			case ETDForschungZustand::ZuFrueh:
				Titel = TDF(TEXT("%s (ab Welle %d)"), TEXT("%s (from wave %d)"), *Name, AbWelle(K));
				break;
			case ETDForschungZustand::Gesperrt:
				for (const FString& V : K.Braucht)
				{
					if (!Hat(WorldContext, *V))
					{
						Titel = TDF(TEXT("%s (zuerst %s)"), TEXT("%s (first %s)"), *Name, *NameVon(V, K.Baum));
						break;
					}
				}
				break;
			case ETDForschungZustand::Ausgeschlossen:
				Titel = TDF(TEXT("%s (ausgeschlossen: %s gewählt)"), TEXT("%s (excluded: %s chosen)"), *Name, *NameVon(GewaehlteAndere(WorldContext, K), K.Baum));
				break;
			// 0.78.3 (Marius 05.11.: "immer noch nicht transparent, was kann ich schon und was nicht"): jeder Zustand steht im Titel,
			// auch erforscht und jetzt erforschbar (vorher nur der blanke Name)
			case ETDForschungZustand::Erforscht:
				Titel = TDF(TEXT("%s (erforscht)"), TEXT("%s (researched)"), *Name);
				break;
			case ETDForschungZustand::Moeglich:
				Titel = TDF(TEXT("%s (jetzt erforschbar)"), TEXT("%s (researchable now)"), *Name);
				break;
			default:
				break;
			}
			// 0.9.6 (Marius 12.11.: "bei alternativen Knoten nicht sichtbar, dass der andere nicht mehr erforscht werden kann"):
			// der ausgeschlossene Knoten zeigt im Baum ein rotes X (Spielsymbol TXUI_MIcon_Stop_X) statt seines Bildes
			{
				static FStructProperty* IconProp = FindFProperty<FStructProperty>(UFGSchematic::StaticClass(), TEXT("mSchematicIcon"));
				static TMap<FString, FSlateBrush> Original;
				static UTexture2D* Kreuz = LoadObject<UTexture2D>(nullptr, TEXT("/Game/FactoryGame/Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Stop_X.TXUI_MIcon_Stop_X"));
				if (IconProp && Kreuz)
				{
					FSlateBrush* Icon = IconProp->ContainerPtrToValuePtr<FSlateBrush>(CDO);
					if (!Original.Contains(K.Key))
					{
						// 0.9.6 (Marius 12.11.: "komische Symbole fuer das, was sie sagen"): Knoten mit neuem Gebaeude zeigen das
						// Gebaeude, Faehigkeiten ein passenderes Teil (vorher oft Draht, Kupferblech, Kabel)
						FSlateBrush Besser = *Icon;
						if (UTexture2D* Symbol = BesseresSymbol(K.Key))
						{
							Besser.SetResourceObject(Symbol);
							*Icon = Besser;
						}
						Original.Add(K.Key, Besser);
					}
					if (Zustand(WorldContext, K) == ETDForschungZustand::Ausgeschlossen)
					{
						if (Icon->GetResourceObject() != Kreuz)
						{
							FSlateBrush Gesperrt = Original[K.Key];
							Gesperrt.SetResourceObject(Kreuz);
							Gesperrt.TintColor = FSlateColor(FLinearColor(0.83f, 0.10f, 0.06f));
							*Icon = Gesperrt;
						}
					}
					else if (Icon->GetResourceObject() == Kreuz)
					{
						*Icon = Original[K.Key];
					}
				}
			}
			FString& Alt = Zuletzt.FindOrAdd(K.Key);
			if (Alt == Titel)
			{
				continue;
			}
			Alt = Titel;
			NameProp->SetPropertyValue_InContainer(CDO, FText::FromString(Titel));
		}
		if (!bGemeldet)
		{
			bGemeldet = true;
			UE_LOG(LogFicsitTD, Log, TEXT("MAM-Statustitel: %d von %d Knoten"), Knoten, Alle().Num());
		}
	}
}
