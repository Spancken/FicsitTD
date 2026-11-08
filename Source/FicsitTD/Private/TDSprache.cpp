#include "TDSprache.h"
#include "FicsitTDModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

namespace
{
	struct FTDTextPaar { const TCHAR* De; const TCHAR* En; };
	// erzeugt von Scripts/erzeuge_uebersetzung.py aus Scripts/uebersetzung_en.json
	static const FTDTextPaar GTDTexteEn[] = {
#include "TDTexteEn.inl"
		{ nullptr, nullptr }
	};

	const TMap<FString, const TCHAR*>& Tabelle()
	{
		static TMap<FString, const TCHAR*> Karte;
		if (Karte.Num() == 0)
		{
			for (const FTDTextPaar& P : GTDTexteEn)
			{
				if (P.De)
				{
					Karte.Add(FString(P.De), P.En);
				}
			}
		}
		return Karte;
	}

	const TSet<FString>& EnglischeWerte()
	{
		static TSet<FString> Werte;
		if (Werte.Num() == 0)
		{
			for (const auto& Paar : Tabelle())
			{
				Werte.Add(FString(Paar.Value));
			}
		}
		return Werte;
	}

	/** einen Text ersetzen; ohne Uebersetzung landet er in Fehlend */
	int32 ErsetzeText(FText& Text, TSet<FString>& Fehlend)
	{
		const FString S = Text.ToString();
		if (S.TrimStartAndEnd().IsEmpty())
		{
			return 0;
		}
		if (const TCHAR* En = TDSprache::Nachschlagen(S))
		{
			Text = FText::FromString(En);
			return 1;
		}
		if (!EnglischeWerte().Contains(S))
		{
			Fehlend.Add(S);
		}
		return 0;
	}

	int32 ErsetzeIn(const UStruct* Typ, void* Daten, int32 Tiefe, TSet<FString>& Fehlend)
	{
		if (!Typ || !Daten || Tiefe > 4)
		{
			return 0;
		}
		int32 Anzahl = 0;
		for (TFieldIterator<FProperty> It(Typ); It; ++It)
		{
			FProperty* P = *It;
			for (int32 i = 0; i < P->ArrayDim; ++i)
			{
				void* Wert = P->ContainerPtrToValuePtr<void>(Daten, i);
				if (CastField<FTextProperty>(P))
				{
					Anzahl += ErsetzeText(*reinterpret_cast<FText*>(Wert), Fehlend);
				}
				else if (FStructProperty* SP = CastField<FStructProperty>(P))
				{
					Anzahl += ErsetzeIn(SP->Struct, Wert, Tiefe + 1, Fehlend);
				}
				else if (FArrayProperty* AP = CastField<FArrayProperty>(P))
				{
					FScriptArrayHelper Feld(AP, Wert);
					if (CastField<FTextProperty>(AP->Inner))
					{
						for (int32 k = 0; k < Feld.Num(); ++k)
						{
							Anzahl += ErsetzeText(*reinterpret_cast<FText*>(Feld.GetRawPtr(k)), Fehlend);
						}
					}
					else if (FStructProperty* InnerStruct = CastField<FStructProperty>(AP->Inner))
					{
						for (int32 k = 0; k < Feld.Num(); ++k)
						{
							Anzahl += ErsetzeIn(InnerStruct->Struct, Feld.GetRawPtr(k), Tiefe + 1, Fehlend);
						}
					}
				}
			}
		}
		return Anzahl;
	}
}

bool TDSprache::Englisch()
{
	// Testschalter: -TDSprache=en oder -TDSprache=de auf der Kommandozeile
	static int32 Vorgabe = -1;
	if (Vorgabe < 0)
	{
		FString Wahl;
		Vorgabe = FParse::Value(FCommandLine::Get(), TEXT("TDSprache="), Wahl) ? (Wahl.StartsWith(TEXT("de")) ? 0 : 1) : 2;
	}
	if (Vorgabe < 2)
	{
		return Vorgabe == 1;
	}
	return !FInternationalization::Get().GetCurrentLanguage()->GetTwoLetterISOLanguageName().Equals(TEXT("de"), ESearchCase::IgnoreCase);
}

const TCHAR* TDSprache::Nachschlagen(const FString& Deutsch)
{
	if (Deutsch.IsEmpty())
	{
		return nullptr;
	}
	const TCHAR* const* En = Tabelle().Find(Deutsch);
	return En ? *En : nullptr;
}

int32 TDSprache::UebersetzeAssets()
{
	if (!Englisch())
	{
		return 0;
	}
	// alle Mod-Assets laden, damit auch erst spaeter benutzte Klassen (Gegenstaende, Gebaeude) ersetzt sind
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(TEXT("/FicsitTD")), Assets, true);
	for (const FAssetData& A : Assets)
	{
		A.GetAsset();
	}
	int32 Ersetzt = 0, Objekte = 0;
	TSet<FString> Fehlend;
	for (TObjectIterator<UObject> It(RF_NoFlags); It; ++It)   // ohne RF_NoFlags fehlen die CDOs mit den Namen
	{
		UObject* O = *It;
		if (!O || O->IsA<UClass>() || O->IsA<UPackage>() || !O->GetPathName().StartsWith(TEXT("/FicsitTD/")))
		{
			continue;
		}
		++Objekte;
		Ersetzt += ErsetzeIn(O->GetClass(), O, 0, Fehlend);
	}
	// Liste der fehlenden Uebersetzungen fuer Scripts/erzeuge_uebersetzung.py
	TArray<FString> Liste = Fehlend.Array();
	Liste.Sort();
	FString Json = TEXT("[\n");
	for (int32 i = 0; i < Liste.Num(); ++i)
	{
		FString S = Liste[i];
		S.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		S.ReplaceInline(TEXT("\""), TEXT("\\\""));
		S.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		S.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		S.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		Json += FString::Printf(TEXT("  \"%s\"%s\n"), *S, i + 1 < Liste.Num() ? TEXT(",") : TEXT(""));
	}
	Json += TEXT("]\n");
	FFileHelper::SaveStringToFile(Json, *(FPaths::ProjectSavedDir() / TEXT("FicsitTD/Uebersetzung-fehlt.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogFicsitTD, Log, TEXT("Sprache: Englisch, %d Assets, %d Objekte, %d Texte ersetzt, %d ohne Uebersetzung (Saved/FicsitTD/Uebersetzung-fehlt.json), Tabelle %d"),
		Assets.Num(), Objekte, Ersetzt, Liste.Num(), Tabelle().Num());
	return Ersetzt;
}
