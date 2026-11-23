#include "TDSicherheitsnetz.h"

namespace TDNetz
{
	static int32 GZaehler[static_cast<int32>(EArt::Anzahl)] = {};

	void Melde(EArt Art)
	{
		++GZaehler[static_cast<int32>(Art)];
	}

	int32 Zahl(EArt Art)
	{
		return GZaehler[static_cast<int32>(Art)];
	}

	const TCHAR* Name(EArt Art)
	{
		static const TCHAR* Namen[] = { TEXT("Festgefahren versetzt"), TEXT("Festgefahren ohne Boden"), TEXT("Nachzuegler versetzt"),
			TEXT("Nachzuegler entfernt"), TEXT("Absturz zurueckgesetzt"), TEXT("Absturz entfernt"), TEXT("0 LP getoetet"),
			TEXT("Fronten ungeprueft"), TEXT("Kern abgeriegelt") };
		static_assert(UE_ARRAY_COUNT(Namen) == static_cast<int32>(EArt::Anzahl), "Namen der Sicherheitsnetze");
		return Namen[static_cast<int32>(Art)];
	}

	static int32 GHilfen[static_cast<int32>(EHilfe::Anzahl)] = {};

	void MeldeHilfe(EHilfe Art)
	{
		++GHilfen[static_cast<int32>(Art)];
	}

	FString HilfenText()
	{
		static const TCHAR* Namen[] = { TEXT("Bewuchs geraeumt"), TEXT("Klippe gesperrt"), TEXT("Stau versetzt"), TEXT("Netzinsel"),
			TEXT("Front verlegt"), TEXT("Rundungsrest"), };
		static_assert(UE_ARRAY_COUNT(Namen) == static_cast<int32>(EHilfe::Anzahl), "Namen der Hilfen");
		TArray<FString> Teile;
		for (int32 i = 0; i < static_cast<int32>(EHilfe::Anzahl); ++i)
		{
			if (GHilfen[i] > 0)
			{
				Teile.Add(FString::Printf(TEXT("%s %d"), Namen[i], GHilfen[i]));
			}
		}
		return Teile.Num() ? FString::Join(Teile, TEXT(", ")) : FString(TEXT("keine"));
	}

	void Zuruecksetzen()
	{
		for (int32& Z : GZaehler)
		{
			Z = 0;
		}
		for (int32& Z : GHilfen)
		{
			Z = 0;
		}
	}

	FString Text(int32* OutSumme)
	{
		TArray<FString> Teile;
		int32 Summe = 0;
		for (int32 i = 0; i < static_cast<int32>(EArt::Anzahl); ++i)
		{
			if (GZaehler[i] > 0)
			{
				Teile.Add(FString::Printf(TEXT("%s %d"), Name(static_cast<EArt>(i)), GZaehler[i]));
				Summe += GZaehler[i];
			}
		}
		if (OutSumme)
		{
			*OutSumme = Summe;
		}
		return Teile.Num() ? FString::Join(Teile, TEXT(", ")) : FString(TEXT("keine"));
	}
}
