#include "TDForschung.h"
#include "TDBalance.h"
#include "TDWaveSubsystem.h"
#include "FGSchematic.h"
#include "FGSchematicManager.h"
#include "Engine/World.h"
#include "Unlocks/FGUnlockInfoOnly.h"
#include "FicsitTDModule.h"

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

	static FString NameVon(const FString& Key, const FString& EigenerBaum)
	{
		const FTDForschungKnoten* V = Finde(Key);
		if (!V)
		{
			return Key;
		}
		return V->Baum == EigenerBaum ? V->Name : FString::Printf(TEXT("%s (%s)"), *V->Name, *V->BaumName);
	}

	FString StatusTitel(const UObject* WorldContext, const FTDForschungKnoten& K, FString& OutText)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
		const int32 Naechste = Waves ? Waves->GetWelle() + 1 : 1;
		switch (Zustand(WorldContext, K))
		{
		case ETDForschungZustand::Erforscht:
			OutText = TEXT("Dieser Knoten ist erforscht.");
			return TEXT("Erforscht");
		case ETDForschungZustand::Ausgeschlossen:
			OutText = FString::Printf(TEXT("Abzweigung: In diesem Einsatz ist schon %s erforscht. Nur eine Seite ist möglich."), *NameVon(GewaehlteAndere(WorldContext, K), K.Baum));
			return FString::Printf(TEXT("Ausgeschlossen: %s gewählt"), *NameVon(GewaehlteAndere(WorldContext, K), K.Baum));
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
			OutText = FString::Printf(TEXT("Zuerst im MAM erforschen: %s."), *FString::Join(Fehlt, TEXT(", ")));
			return FString::Printf(TEXT("Gesperrt: zuerst %s"), Fehlt.Num() ? *Fehlt[0] : TEXT("Voraussetzung"));
		}
		case ETDForschungZustand::ZuFrueh:
		{
			const int32 Ab = AbWelle(K);
			const int32 Noch = FMath::Max(1, Ab - Naechste);
			OutText = FString::Printf(TEXT("Freigabe ab Welle %d. Nächste Welle: %d, also noch %d %s."), Ab, Naechste, Noch, Noch == 1 ? TEXT("Welle") : TEXT("Wellen"));
			return FString::Printf(TEXT("Gesperrt: ab Welle %d"), Ab);
		}
		default:
			OutText = FString::Printf(TEXT("Alle Voraussetzungen erfüllt. Kosten: %d FP (FICSIT-Kampfdaten)."), K.FP);
			return TEXT("Jetzt erforschbar");
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
				return K.Name;
			}
		}
		return UFGSchematic::GetSchematicDisplayName(TSubclassOf<UFGSchematic>(const_cast<UClass*>(Schematic))).ToString();
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
			FString Titel = K.Name;
			switch (Zustand(WorldContext, K))
			{
			case ETDForschungZustand::ZuFrueh:
				Titel = FString::Printf(TEXT("%s (ab Welle %d)"), *K.Name, AbWelle(K));
				break;
			case ETDForschungZustand::Gesperrt:
				for (const FString& V : K.Braucht)
				{
					if (!Hat(WorldContext, *V))
					{
						Titel = FString::Printf(TEXT("%s (zuerst %s)"), *K.Name, *NameVon(V, K.Baum));
						break;
					}
				}
				break;
			case ETDForschungZustand::Ausgeschlossen:
				Titel = FString::Printf(TEXT("%s (ausgeschlossen: %s gewählt)"), *K.Name, *NameVon(GewaehlteAndere(WorldContext, K), K.Baum));
				break;
			default:
				break;
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
