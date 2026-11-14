#include "TDKernTerminal.h"
#include "TDWellenStatistik.h"
#include "TDLaufProtokoll.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDForschung.h"
#include "TDPionier.h"
#include "TDSprache.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/UniformGridPanel.h"
#include "Engine/Texture2D.h"
#include "Resources/FGItemDescriptor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/GameplayStatics.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Interfaces/IPluginManager.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

/** Pausenmenue offen: eigene Anzeigen ausblenden (Marius 04.11.2026: Wellenanzeige lag ueber dem Pausenmenue) */
static void TDPauseAusblenden(UUserWidget* W)
{
	const bool bPause = W && W->GetWorld() && UGameplayStatics::IsGamePaused(W->GetWorld());
	const float Soll = bPause ? 0.f : 1.f;
	if (W && !FMath::IsNearlyEqual(W->GetRenderOpacity(), Soll))
	{
		W->SetRenderOpacity(Soll);
	}
}

/** Ist das Widget wirklich zu sehen? Alle Eltern sichtbar, in Umschaltern das aktive Kind, bis zum Fenster Bis (auch ueber verschachtelte Widgets) */
static bool TDImBaumSichtbar(const UWidget* W, const UUserWidget* Bis)
{
	for (int32 Tiefe = 0; W && Tiefe < 64; ++Tiefe)
	{
		if (W == Bis)
		{
			return true;
		}
		if (!W->IsVisible())
		{
			return false;
		}
		UPanelWidget* Eltern = W->GetParent();
		if (const UWidgetSwitcher* Umschalter = Cast<UWidgetSwitcher>(Eltern))
		{
			if (Umschalter->GetActiveWidget() != W)
			{
				return false;
			}
		}
		if (Eltern)
		{
			W = Eltern;
			continue;
		}
		// Wurzel eines Widget-Baums: weiter beim Widget, dem der Baum gehoert
		const UWidgetTree* Baum = Cast<UWidgetTree>(W->GetOuter());
		W = Baum ? Cast<UWidget>(Baum->GetOuter()) : nullptr;
	}
	return false;
}

static bool TestModus();

namespace TDUI
{
	// Farben der Maschinenfenster des Spiels (sRGB)
	static FLinearColor C(uint8 R, uint8 G, uint8 B, uint8 A = 255) { return FLinearColor(FColor(R, G, B, A)); }
	static const FLinearColor Kopfleiste = C(24, 24, 24);
	static const FLinearColor Reiter = C(58, 58, 58);
	static const FLinearColor Fenster = C(46, 46, 46);
	static const FLinearColor Feld = C(22, 22, 22);
	static const FLinearColor ZeileA = C(31, 31, 31);
	static const FLinearColor ZeileB = C(38, 38, 38);
	static const FLinearColor Akzent = C(250, 149, 73);       // FICSIT-Orange
	static const FLinearColor Hell = C(235, 235, 235);
	static const FLinearColor Grau = C(150, 150, 150);
	static const FLinearColor Gut = C(150, 215, 125);
	static const FLinearColor DunkelText = C(22, 22, 22);
	static const FLinearColor Fehler = C(235, 90, 70);
	// weisse Karten wie die Ein-/Ausgabefelder der Maschinen
	static const FLinearColor Karte = C(250, 250, 250);
	static const FLinearColor KarteText = C(51, 51, 51);
	static const FLinearColor KarteGrau = C(115, 115, 115);
	static const FLinearColor KarteOrange = C(228, 128, 45);
	// T3 Wellenvorschau: Karten und Rahmen
	static const FLinearColor NeuFeld = C(42, 33, 24);
	static const FLinearColor KarteDunkel = C(36, 36, 36);
	static const FLinearColor BildGrund = C(16, 16, 16);
	static const FLinearColor Elite = C(175, 169, 236);
	static const FLinearColor EliteRahmen = C(127, 119, 221);
	static const FLinearColor Boss = C(240, 120, 110);
	static const FLinearColor BossFeld = C(48, 24, 24);
	static const FLinearColor ChipStark = C(58, 42, 26);
	static const FLinearColor ChipGut = C(31, 51, 38);
	static const FLinearColor ChipSchlecht = C(54, 30, 30);
	static const FLinearColor ChipNeutral = C(48, 48, 48);

	/** Noto Sans wie im Spiel (Font-Face-Assets), sonst die Engine-Schrift */
	static FSlateFontInfo Schrift(int32 Groesse, bool bFett)
	{
		// absichtlich nie freigegeben: FGCObject darf nicht nach dem GC-System zerstoert werden
		static TSharedPtr<const FCompositeFont>* Fonts = nullptr;
		if (!Fonts)
		{
			Fonts = new TSharedPtr<const FCompositeFont>[2];
			const TCHAR* Pfade[2] = {
				TEXT("/Game/FactoryGame/Interface/Font/NotoSans-Regular.NotoSans-Regular"),
				TEXT("/Game/FactoryGame/Interface/Font/NotoSans-SemiBold.NotoSans-SemiBold") };
			for (int32 i = 0; i < 2; ++i)
			{
				if (UObject* Face = LoadObject<UObject>(nullptr, Pfade[i]))
				{
					TSharedRef<FStandaloneCompositeFont> CF = MakeShared<FStandaloneCompositeFont>();
					FTypefaceEntry Eintrag(TEXT("Regular"));
					Eintrag.Font = FFontData(Face);
					CF->DefaultTypeface.Fonts.Add(MoveTemp(Eintrag));
					Fonts[i] = CF;
				}
			}
			UE_LOG(LogFicsitTD, Log, TEXT("Kern-Menue: Spielschrift %s"), Fonts[0].IsValid() ? TEXT("geladen") : TEXT("fehlt, Engine-Schrift"));
		}
		const TSharedPtr<const FCompositeFont>& F = Fonts[bFett ? 1 : 0];
		return F.IsValid() ? FSlateFontInfo(F, Groesse, TEXT("Regular")) : FCoreStyle::GetDefaultFontStyle(bFett ? "Bold" : "Regular", Groesse);
	}

	/** UI-Textur des Spiels unter /Game/FactoryGame/Interface/UI/Assets/Shared (Skill satisfactory-design, Abschnitt 1) */
	static UTexture2D* SpielTextur(const TCHAR* Name)
	{
		static TMap<FString, TWeakObjectPtr<UTexture2D>> Vorrat;
		TWeakObjectPtr<UTexture2D>& T = Vorrat.FindOrAdd(Name);
		if (!T.IsValid())
		{
			T = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/FactoryGame/Interface/UI/Assets/Shared/%s.%s"), Name, Name));
			UE_LOG(LogFicsitTD, Log, TEXT("Kern-Menue: Spieltextur %s %s"), Name, T.IsValid() ? TEXT("geladen") : TEXT("FEHLT, Ersatzfarbe"));
		}
		return T.Get();
	}

	/** 9-Slice-Pinsel aus einer Spieltextur (Rand als Anteil), sonst abgerundete Ersatzfarbe */
	static FSlateBrush Bild(const TCHAR* Name, float Rand, const FLinearColor& Ersatz, const FLinearColor& Toenung = FLinearColor::White)
	{
		if (UTexture2D* T = SpielTextur(Name))
		{
			FSlateBrush P;
			P.SetResourceObject(T);
			P.ImageSize = FVector2D(T->GetSizeX(), T->GetSizeY());
			P.DrawAs = ESlateBrushDrawType::Box;
			P.Margin = FMargin(Rand);
			P.TintColor = FSlateColor(Toenung);
			return P;
		}
		return FSlateRoundedBoxBrush(Ersatz, 3.f);
	}

	static FButtonStyle KnopfStil(bool bHaupt)
	{
		FButtonStyle S;
		if (bHaupt)
		{
			S.SetNormal(FSlateRoundedBoxBrush(Akzent, 3.f));
			S.SetHovered(FSlateRoundedBoxBrush(C(255, 180, 115), 3.f));
			S.SetPressed(FSlateRoundedBoxBrush(C(205, 112, 45), 3.f));
		}
		else
		{
			// dunkle Spielknoepfe mit Fase (TXUI_DarkButton_*), wie "Einstellungen kopieren" im Maschinenfenster
			S.SetNormal(Bild(TEXT("TXUI_DarkButton_Normal"), 0.25f, C(72, 72, 72)));
			S.SetHovered(Bild(TEXT("TXUI_DarkButton_Selected"), 0.25f, C(100, 100, 100)));
			S.SetPressed(Bild(TEXT("TXUI_DarkButton_Pressed"), 0.25f, Akzent));
		}
		S.SetDisabled(FSlateRoundedBoxBrush(C(50, 50, 50), 3.f));
		S.SetNormalPadding(FMargin(14.f, 7.f));
		S.SetPressedPadding(FMargin(14.f, 8.f, 14.f, 6.f));
		return S;
	}

	/** 0.9.8 (Selbsttest 14.11.): Knoepfe auf weissen Karten (Turmfenster). TXUI_DarkButton ist bis auf die obere Fase
	 *  durchsichtig - auf dunklem Grund unsichtbar, auf Weiss blieb nur ein Strich. Deshalb volle dunkle Platte. */
	static FButtonStyle KartenKnopfStil()
	{
		FButtonStyle S;
		S.SetNormal(FSlateRoundedBoxBrush(C(72, 72, 72), 3.f));
		S.SetHovered(FSlateRoundedBoxBrush(C(100, 100, 100), 3.f));
		S.SetPressed(FSlateRoundedBoxBrush(C(45, 45, 45), 3.f));
		S.SetDisabled(FSlateRoundedBoxBrush(C(200, 200, 200, 160), 3.f));
		S.SetNormalPadding(FMargin(6.f, 4.f));
		S.SetPressedPadding(FMargin(6.f, 5.f, 6.f, 3.f));
		return S;
	}

	static UTextBlock* Text(UWidgetTree* Baum, const FString& Inhalt, int32 Groesse, const FLinearColor& Farbe, bool bFett = false)
	{
		UTextBlock* T = Baum->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Inhalt));
		T->SetFont(Schrift(Groesse, bFett));
		T->SetColorAndOpacity(FSlateColor(Farbe));
		return T;
	}

	/** Kern des Spielers: Interaktionsobjekt, sonst der naechste in 500 m */
	static ATDDefenseCore* SucheKern(UWorld* World, UObject* Interaktion)
	{
		if (ATDDefenseCore* Kern = Cast<ATDDefenseCore>(Interaktion))
		{
			return Kern;
		}
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		ATDDefenseCore* Best = nullptr;
		float BestAbstand = FMath::Square(50000.f);
		for (TActorIterator<ATDDefenseCore> It(World); It; ++It)
		{
			const float D = Pawn ? FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) : 0.f;
			if (D < BestAbstand)
			{
				BestAbstand = D;
				Best = *It;
			}
		}
		return Best;
	}
}

void UTDVorschauKnopf::Geklickt()
{
	if (UTDKernTerminal* M = Menue.Get())
	{
		M->VorschauWahl(Pfad, Welle);
	}
}

void UTDTalentKnopf::Geklickt()
{
	if (UTDKernTerminal* M = Menue.Get())
	{
		M->LerneTalent(Zweig);
	}
}

void UTDKaufKnopf::Geklickt()
{
	if (UTDKernTerminal* M = Menue.Get())
	{
		M->AendereKorb(Key, Schritt);
	}
}

UTDKernTerminal::UTDKernTerminal(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	mDesiredHorizontalAlignment = HAlign_Center;
	mDesiredVerticalAlignment = VAlign_Center;
	// Wie die Maschinenfenster: Bewegung und Umsehen sperren, Mauszeiger frei und sichtbar
	mUseKeyboard = true;
	mUseMouse = true;
	mMouseLockMode = EMouseLockMode::DoNotLock;
	mHideCursorDuringCapture = false;
	mDisablePlayerActions = true;
	mDisableBuildGunActions = true;
	mFlushMouseKeysOnOpen = true;
	SetIsFocusable(true);
}

TSharedRef<SWidget> UTDKernTerminal::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BaueOberflaeche();
	}
	return Super::RebuildWidget();
}

UTextBlock* UTDKernTerminal::NeuerText(const FString& Text, int32 Groesse, const FLinearColor& Farbe, bool bFett)
{
	return TDUI::Text(WidgetTree, Text, Groesse, Farbe, bFett);
}

UButton* UTDKernTerminal::NeuerKnopf(const FString& Text, UTextBlock*& OutText, bool bHaupt)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>();
	B->SetStyle(TDUI::KnopfStil(bHaupt));
	OutText = NeuerText(Text, 13, bHaupt ? TDUI::DunkelText : TDUI::Hell, bHaupt);
	B->AddChild(OutText);
	return B;
}

UBorder* UTDKernTerminal::NeuesFeld(const FLinearColor& Farbe, const FMargin& Abstand)
{
	UBorder* F = WidgetTree->ConstructWidget<UBorder>();
	F->SetBrush(FSlateRoundedBoxBrush(Farbe, 3.f));
	F->SetPadding(Abstand);
	return F;
}

void UTDKernTerminal::BaueOberflaeche()
{
	USizeBox* Groesse = WidgetTree->ConstructWidget<USizeBox>();
	Groesse->SetWidthOverride(1440.f);
	WidgetTree->RootWidget = Groesse;

	UVerticalBox* Aussen = WidgetTree->ConstructWidget<UVerticalBox>();
	Groesse->AddChild(Aussen);

	// Kopfleiste: Reiter wie im Spiel (Rezept auswaehlen | Produktion), rechts das X
	UBorder* Kopf = NeuesFeld(TDUI::Kopfleiste, FMargin(0.f));
	Aussen->AddChildToVerticalBox(Kopf);
	UHorizontalBox* KopfZeile = WidgetTree->ConstructWidget<UHorizontalBox>();
	Kopf->AddChild(KopfZeile);
	const FString ReiterNamen[] = { TDT(TEXT("Abwehrkern"), TEXT("Defense Core")), TDT(TEXT("Wellenvorschau"), TEXT("Wave Preview")), TDT(TEXT("Forschung"), TEXT("Research")),
		TDT(TEXT("Pionier"), TEXT("Pioneer")), TDT(TEXT("Statistik"), TEXT("Statistics")) };
	for (int32 i = 0; i < 5; ++i)
	{
		UButton* Reiter = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* Titel = NeuerText(ReiterNamen[i], 18, TDUI::Hell);
		Reiter->AddChild(Titel);
		if (i == 0) Reiter->OnClicked.AddDynamic(this, &UTDKernTerminal::ReiterLaden);
		if (i == 1) Reiter->OnClicked.AddDynamic(this, &UTDKernTerminal::ReiterVorschau);
		if (i == 2) Reiter->OnClicked.AddDynamic(this, &UTDKernTerminal::ReiterForschung);
		if (i == 3) Reiter->OnClicked.AddDynamic(this, &UTDKernTerminal::ReiterPionier);
		if (i == 4) Reiter->OnClicked.AddDynamic(this, &UTDKernTerminal::ReiterStatistik);
		if (UHorizontalBoxSlot* S = KopfZeile->AddChildToHorizontalBox(Reiter))
		{
			S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		}
		mReiter.Add(Reiter);
		mReiterTexte.Add(Titel);
	}
	mTitel = mReiterTexte[0];
	if (UHorizontalBoxSlot* S = KopfZeile->AddChildToHorizontalBox(WidgetTree->ConstructWidget<USizeBox>()))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UButton* Zu = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle ZuStil = TDUI::KnopfStil(false);
	ZuStil.SetNormal(FSlateRoundedBoxBrush(TDUI::Kopfleiste, 0.f));
	ZuStil.SetHovered(FSlateRoundedBoxBrush(TDUI::Akzent, 0.f));
	ZuStil.SetNormalPadding(FMargin(20.f, 8.f));
	ZuStil.SetPressedPadding(FMargin(20.f, 9.f, 20.f, 7.f));
	Zu->SetStyle(ZuStil);
	Zu->SetToolTipText(TDTX(TEXT("Schließen (Esc)"), TEXT("Close (Esc)")));
	Zu->AddChild(NeuerText(TEXT("X"), 18, TDUI::Hell, true));
	Zu->OnClicked.AddDynamic(this, &UTDKernTerminal::Schliessen);
	KopfZeile->AddChildToHorizontalBox(Zu);

	// Fensterkoerper
	UBorder* Koerper = WidgetTree->ConstructWidget<UBorder>();
	Koerper->SetBrush(TDUI::Bild(TEXT("TXUI_ButtonPanel"), 0.1f, TDUI::Fenster, TDUI::C(150, 150, 150)));
	Koerper->SetPadding(FMargin(14.f));
	Aussen->AddChildToVerticalBox(Koerper);
	UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
	Koerper->AddChild(Spalte);
	auto Zeile = [&](UWidget* W, float Abstand)
	{
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Abstand));
		}
	};

	// Kennzahlen im dunklen Feld: orange Beschriftung ueber weissem Wert (wie "Rezept:" im Maschinenfenster)
	UBorder* Status = NeuesFeld(TDUI::Feld, FMargin(18.f, 12.f));
	UVerticalBox* StatusSpalte = WidgetTree->ConstructWidget<UVerticalBox>();
	Status->AddChild(StatusSpalte);
	UHorizontalBox* Zahlen = WidgetTree->ConstructWidget<UHorizontalBox>();
	StatusSpalte->AddChildToVerticalBox(Zahlen);
	const FString Beschriftungen[3] = { TDT(TEXT("Welle:"), TEXT("Wave:")), TDT(TEXT("Kernzustand:"), TEXT("Core health:")),
		TDT(TEXT("Konto: Coupons für Kontingente, FP für den MAM:"), TEXT("Balance: Coupons for quotas, RP for the MAM:")) };
	TObjectPtr<UTextBlock>* Felder[3] = { &mWelle, &mKernLP, &mKonto };
	for (int32 i = 0; i < 3; ++i)
	{
		UVerticalBox* Paar = WidgetTree->ConstructWidget<UVerticalBox>();
		Paar->AddChildToVerticalBox(NeuerText(Beschriftungen[i], 12, TDUI::Akzent));
		// Umbrechen statt in die Nachbarspalte laufen (Test 0.24.2: lange Wellenzeile lag ueber "Kernzustand")
		*Felder[i] = NeuerText(TEXT("-"), 17, TDUI::Hell, true);
		(*Felder[i])->SetAutoWrapText(true);
		Paar->AddChildToVerticalBox(*Felder[i]);
		Paar->SetClipping(EWidgetClipping::ClipToBounds);
		if (UHorizontalBoxSlot* S = Zahlen->AddChildToHorizontalBox(Paar))
		{
			FSlateChildSize Anteil(ESlateSizeRule::Fill);
			Anteil.Value = i == 0 ? 1.6f : 1.f;
			S->SetSize(Anteil);
			S->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
	}
	mFronten = NeuerText(TEXT(""), 12, TDUI::Grau);
	mFronten->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = StatusSpalte->AddChildToVerticalBox(mFronten))
	{
		S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	Zeile(Status, 12.f);

	// Aktionen
	UHorizontalBox* Aktionen = WidgetTree->ConstructWidget<UHorizontalBox>();
	auto Aktion = [&](UButton* B)
	{
		if (UHorizontalBoxSlot* S = Aktionen->AddChildToHorizontalBox(B))
		{
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
		}
	};
	UTextBlock* Knopftext = nullptr;
	UButton* WelleKnopf = NeuerKnopf(TDT(TEXT("Welle starten"), TEXT("Start wave")), Knopftext, true);
	mWelleText = Knopftext;
	WelleKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::WelleStarten);
	Aktion(WelleKnopf);
	mWelleKnopf = WelleKnopf;
	// 0.9.6 (Marius 12.11.): nach Sieg oder Niederlage statt "Welle starten"
	mEndeWiederholen = NeuerKnopf(TDT(TEXT("Welle wiederholen"), TEXT("Retry wave")), Knopftext, true);
	mEndeWiederholen->OnClicked.AddDynamic(this, &UTDKernTerminal::EndeWiederholen);
	Aktion(mEndeWiederholen);
	mEndeNeustart = NeuerKnopf(TDT(TEXT("Neu starten (Welle 1)"), TEXT("Restart (wave 1)")), Knopftext, true);
	mEndeNeustart->OnClicked.AddDynamic(this, &UTDKernTerminal::EndeNeustart);
	Aktion(mEndeNeustart);
	mEndeBeenden = NeuerKnopf(TDT(TEXT("Einsatz beenden"), TEXT("End operation")), Knopftext);
	mEndeBeenden->OnClicked.AddDynamic(this, &UTDKernTerminal::EndeBeenden);
	Aktion(mEndeBeenden);
	UButton* WegeKnopf = NeuerKnopf(TDT(TEXT("Wegvorschau an"), TEXT("Path preview on")), Knopftext);
	mWegeText = Knopftext;
	WegeKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::WegeUmschalten);
	Aktion(WegeKnopf);
	UButton* FreiKnopf = NeuerKnopf(TDT(TEXT("Freies Bauen: aus"), TEXT("Free Build: off")), Knopftext);
	mFreiText = Knopftext;
	FreiKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::FreiUmschalten);
	Aktion(FreiKnopf);

	UButton* MeldenKnopf = NeuerKnopf(TDT(TEXT("Fehler melden / Idee"), TEXT("Report bug / idea")), Knopftext);
	MeldenKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::ReiterMelden);
	Aktion(MeldenKnopf);
	// 0.9.6 (Marius 12.11.): Unterstuetzen ueber Ko-fi - oeffnet nur den Browser, die Mod sendet nichts
	UButton* KofiKnopf = NeuerKnopf(TDT(TEXT("Mod unterstützen (Ko-fi)"), TEXT("Support the mod (Ko-fi)")), Knopftext);
	KofiKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::OeffneKofi);
	Aktion(KofiKnopf);

	mLaengeKnopf = NeuerKnopf(TDT(TEXT("Länge: Standard (50)"), TEXT("Length: Standard (50)")), Knopftext);
	mLaengeText = Knopftext;
	mLaengeKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::LaengeWechseln);
	Aktion(mLaengeKnopf);
	mStartKnopf = NeuerKnopf(TDT(TEXT("Einsatz starten (Normal, 30 Wellen)"), TEXT("Start operation (Normal, 30 waves)")), Knopftext, true);
	mStartText = Knopftext;
	mStartKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::EinsatzStarten);
	Aktion(mStartKnopf);
	Zeile(Aktionen, 12.f);

	// Drei Spalten: Laden | Auswahl (Vorschau + Bestaetigen) | Kontingent
	USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
	Hoehe->SetHeightOverride(600.f);
	UHorizontalBox* Spalten = WidgetTree->ConstructWidget<UHorizontalBox>();
	Hoehe->AddChild(Spalten);
	auto Block = [&](const FString& Titel, float Anteil, float Rechts) -> UVerticalBox*
	{
		UVerticalBox* B = WidgetTree->ConstructWidget<UVerticalBox>();
		UBorder* BlockKopf = NeuesFeld(TDUI::Reiter, FMargin(12.f, 7.f));
		BlockKopf->AddChild(NeuerText(Titel, 14, TDUI::Hell));
		B->AddChildToVerticalBox(BlockKopf);
		UBorder* Inhalt = NeuesFeld(TDUI::Feld, FMargin(6.f));
		UVerticalBox* Innen = WidgetTree->ConstructWidget<UVerticalBox>();
		Inhalt->AddChild(Innen);
		if (UVerticalBoxSlot* S = B->AddChildToVerticalBox(Inhalt))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UHorizontalBoxSlot* S = Spalten->AddChildToHorizontalBox(B))
		{
			FSlateChildSize Fuellen(ESlateSizeRule::Fill);
			Fuellen.Value = Anteil;
			S->SetSize(Fuellen);
			S->SetPadding(FMargin(0.f, 0.f, Rechts, 0.f));
		}
		return Innen;
	};
	auto Rollbar = [&](UVerticalBox* Innen, TObjectPtr<UVerticalBox>& Ziel)
	{
		UScrollBox* Rollen = WidgetTree->ConstructWidget<UScrollBox>();
		Ziel = WidgetTree->ConstructWidget<UVerticalBox>();
		Rollen->AddChild(Ziel);
		if (UVerticalBoxSlot* S = Innen->AddChildToVerticalBox(Rollen))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	};
	Rollbar(Block(TDT(TEXT("Laden"), TEXT("Shop")), 1.3f, 12.f), mLaden);

	UVerticalBox* Auswahl = Block(TDT(TEXT("Auswahl (Vorschau)"), TEXT("Selection (preview)")), 0.85f, 12.f);
	Rollbar(Auswahl, mKorbListe);
	UBorder* Fuss = NeuesFeld(TDUI::ZeileB, FMargin(12.f, 10.f));
	UVerticalBox* FussSpalte = WidgetTree->ConstructWidget<UVerticalBox>();
	Fuss->AddChild(FussSpalte);
	mKorbSumme = NeuerText(TDT(TEXT("Summe: 0 Coupons"), TEXT("Total: 0 Coupons")), 16, TDUI::Hell, true);
	FussSpalte->AddChildToVerticalBox(mKorbSumme);
	mKorbRest = NeuerText(TEXT(""), 13, TDUI::Grau);
	if (UVerticalBoxSlot* S = FussSpalte->AddChildToVerticalBox(mKorbRest))
	{
		S->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));
	}
	UHorizontalBox* FussKnoepfe = WidgetTree->ConstructWidget<UHorizontalBox>();
	mBestaetigen = NeuerKnopf(TDT(TEXT("Bestätigen"), TEXT("Confirm")), Knopftext, true);
	mBestaetigen->OnClicked.AddDynamic(this, &UTDKernTerminal::Bestaetigen);
	if (UHorizontalBoxSlot* S = FussKnoepfe->AddChildToHorizontalBox(mBestaetigen))
	{
		S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}
	UButton* Weg = NeuerKnopf(TDT(TEXT("Verwerfen"), TEXT("Discard")), Knopftext);
	Weg->OnClicked.AddDynamic(this, &UTDKernTerminal::Verwerfen);
	FussKnoepfe->AddChildToHorizontalBox(Weg);
	FussSpalte->AddChildToVerticalBox(FussKnoepfe);
	if (UVerticalBoxSlot* S = Auswahl->AddChildToVerticalBox(Fuss))
	{
		S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	}

	Rollbar(Block(TDT(TEXT("Kontingent (belegt / erlaubt)"), TEXT("Quota (used / allowed)")), 0.95f, 0.f), mKontingent);
	Zeile(Hoehe, 12.f);
	mSpalten = Hoehe;

	// T3: Wellenvorschau an derselben Stelle (gleiche Hoehe), umschaltbar ueber den Knopf
	USizeBox* VorschauHoehe = WidgetTree->ConstructWidget<USizeBox>();
	VorschauHoehe->SetHeightOverride(600.f);
	UBorder* VorschauFeld = NeuesFeld(TDUI::Feld, FMargin(14.f, 12.f, 30.f, 12.f));
	VorschauHoehe->AddChild(VorschauFeld);
	UScrollBox* VorschauRollen = WidgetTree->ConstructWidget<UScrollBox>();
	VorschauFeld->AddChild(VorschauRollen);
	mVorschauInhalt = WidgetTree->ConstructWidget<UVerticalBox>();
	VorschauRollen->AddChild(mVorschauInhalt);
	VorschauHoehe->SetVisibility(ESlateVisibility::Collapsed);
	mVorschau = VorschauHoehe;
	Zeile(VorschauHoehe, 12.f);
	auto Reiterflaeche = [&](TObjectPtr<UWidget>& Flaeche, TObjectPtr<UVerticalBox>& Inhalt)
	{
		USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
		Hoehe->SetHeightOverride(600.f);
		UBorder* Feld = NeuesFeld(TDUI::Feld, FMargin(14.f, 12.f, 30.f, 12.f));
		Hoehe->AddChild(Feld);
		UScrollBox* Rollen = WidgetTree->ConstructWidget<UScrollBox>();
		Feld->AddChild(Rollen);
		Inhalt = WidgetTree->ConstructWidget<UVerticalBox>();
		Rollen->AddChild(Inhalt);
		Hoehe->SetVisibility(ESlateVisibility::Collapsed);
		Flaeche = Hoehe;
		Zeile(Hoehe, 12.f);
	};
	Reiterflaeche(mForschung, mForschungInhalt);
	Reiterflaeche(mPionier, mPionierInhalt);
	Reiterflaeche(mStatistik, mStatistikInhalt);
	Reiterflaeche(mMelden, mMeldenInhalt);

	UBorder* Meldefeld = NeuesFeld(TDUI::Feld, FMargin(14.f, 8.f));
	mMeldung = NeuerText(TEXT(" "), 13, TDUI::Gut);
	mMeldung->SetAutoWrapText(true);
	Meldefeld->AddChild(mMeldung);
	Zeile(Meldefeld, 0.f);
	ZeigeAnsicht(0);   // Reiter-Optik setzen
}

UWidget* UTDKernTerminal::NeuesSymbol(UTexture2D* Bild, float Kante)
{
	USizeBox* Rahmen = WidgetTree->ConstructWidget<USizeBox>();
	Rahmen->SetWidthOverride(Kante);
	Rahmen->SetHeightOverride(Kante);
	if (Bild)
	{
		UImage* Symbol = WidgetTree->ConstructWidget<UImage>();
		Symbol->SetBrushFromTexture(Bild, false);
		Symbol->SetDesiredSizeOverride(FVector2D(Kante, Kante));
		Rahmen->AddChild(Symbol);
	}
	return Rahmen;
}

ATDDefenseCore* UTDKernTerminal::Kern() const
{
	return TDUI::SucheKern(GetWorld(), mInteractObject);
}

void UTDKernTerminal::NativeConstruct()
{
	Super::NativeConstruct();
	// Forschungspunkte, die wegen vollem Inventar warten, beim Oeffnen nachreichen
	if (UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr)
	{
		if (const int32 Nach = Einsatz->GibOffeneFP())
		{
			Melde(TDF(TEXT("%d Forschungspunkte ins Inventar nachgereicht."), TEXT("%d Research Points added to your inventory."), Nach));
		}
		if (const int32 Teile = Einsatz->GibOffenesMaterial())
		{
			Melde(TDF(TEXT("%d Teile Baumaterial vom Kern ins Inventar geholt."), TEXT("%d building materials moved from the Core to your inventory."), Teile));
		}
	}
	BaueListen();
	Aktualisiere();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(mTimer, FTimerDelegate::CreateUObject(this, &UTDKernTerminal::Aktualisiere), 0.5f, true);
	}
	// Sicherheitsnetz: hat das Spiel den Mauszeiger nicht eingeschaltet, selbst Eingabemodus Spiel+UI setzen
	APlayerController* PC = GetOwningPlayer();
	const bool bSchonDa = PC && PC->ShouldShowMouseCursor();
	if (PC && !bSchonDa)
	{
		PC->SetShowMouseCursor(true);
		FInputModeGameAndUI Modus;
		Modus.SetWidgetToFocus(TakeWidget());
		Modus.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Modus.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Modus);
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
		mZeigerSelbst = true;
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Kern-Menue: geoeffnet, Mauszeiger %s"), bSchonDa ? TEXT("vom Spiel") : mZeigerSelbst ? TEXT("selbst eingeschaltet") : TEXT("ohne Spieler"));
}

void UTDKernTerminal::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(mTimer);
	}
	if (mZeigerSelbst)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			PC->SetShowMouseCursor(false);
			PC->SetInputMode(FInputModeGameOnly());
			PC->ResetIgnoreLookInput();
			PC->ResetIgnoreMoveInput();
		}
		mZeigerSelbst = false;
	}
	Super::NativeDestruct();
}

void UTDKernTerminal::Aktualisiere()
{
	UWorld* World = GetWorld();
	const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	ATDDefenseCore* K = Kern();
	if (!Waves || !Einsatz || !mWelle)
	{
		return;
	}
	const bool bEinsatz = Einsatz->IstAktiv();
	mTitel->SetText(bEinsatz ? TDTX(TEXT("Abwehrkern - Einsatz"), TEXT("Defense Core - Operation")) : TDTX(TEXT("Abwehrkern"), TEXT("Defense Core")));
	FString WelleText;
	if (Waves->LaeuftWelle())
	{
		// ohne Richtungen, die stehen darunter bei den Fronten
		WelleText = Waves->GetLaufText();
		if (!WelleText.Split(TEXT(", aus "), &WelleText, nullptr)) { WelleText.Split(TEXT(", from "), &WelleText, nullptr); }
	}
	else if (Waves->LaeuftKernVerteidigung())
	{
		WelleText = TDF(TEXT("%d / %d bereit"), TEXT("%d / %d ready"), Waves->GetWelle() + 1, Waves->mWellenGesamt);
	}
	else
	{
		WelleText = TDT(TEXT("kein Lauf"), TEXT("no run"));
	}
	mWelle->SetText(FText::FromString(WelleText));
	mKernLP->SetText(FText::FromString(K ? FString::Printf(TEXT("%.0f %%"), 100.f * K->GetTDHealth() / FMath::Max(K->GetTDMaxHealth(), 1.f)) : TEXT("-")));
	mKonto->SetText(FText::FromString(bEinsatz ? TDF(TEXT("%d Coupons  ·  %d FP"), TEXT("%d Coupons  ·  %d RP"), Einsatz->Konto(), Einsatz->FPImInventar()) : FString(TEXT("-"))));
	mWelleText->SetText(Waves->LaeuftWelle() ? TDTX(TEXT("Welle läuft ..."), TEXT("Wave running ...")) : TDTX(TEXT("Welle starten"), TEXT("Start wave")));
	mWegeText->SetText(K && K->ZeigtWege() ? TDTX(TEXT("Wegvorschau aus"), TEXT("Path preview off")) : TDTX(TEXT("Wegvorschau an"), TEXT("Path preview on")));
	mFreiText->SetText(Einsatz->IstFrei() ? TDTX(TEXT("Freies Bauen: AN"), TEXT("Free Build: ON")) : TDTX(TEXT("Freies Bauen: aus"), TEXT("Free Build: off")));
	mStartKnopf->SetVisibility(bEinsatz ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	{
		const int32 Ende = Einsatz->LaufEnde();
		if (mWelleKnopf)
		{
			mWelleKnopf->SetVisibility(Ende ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		}
		if (mEndeWiederholen)
		{
			mEndeWiederholen->SetVisibility(Ende == 2 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			mEndeNeustart->SetVisibility(Ende ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			mEndeBeenden->SetVisibility(Ende ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (Ende)
		{
			mWelle->SetText(Ende == 1 ? TDTX(TEXT("SIEG"), TEXT("VICTORY")) : TDFX(TEXT("Niederlage in Welle %d"), TEXT("Defeat in wave %d"), K ? K->mLaufEndWelle : 0));
		}
	}
	if (mLaengeKnopf)
	{
		const FString Namen[] = { TDT(TEXT("Kurz (20)"), TEXT("Short (20)")), TDT(TEXT("Mittel (30)"), TEXT("Medium (30)")), TDT(TEXT("Standard (50)"), TEXT("Standard (50)")),
			TDT(TEXT("Endlos"), TEXT("Endless")) };   // 0.77 Marius: 50 Wellen sind Standard
		const FString Wellen[] = { TDT(TEXT("20 Wellen"), TEXT("20 waves")), TDT(TEXT("30 Wellen"), TEXT("30 waves")), TDT(TEXT("50 Wellen"), TEXT("50 waves")), TDT(TEXT("endlos"), TEXT("endless")) };
		mLaengeKnopf->SetVisibility(bEinsatz ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		mLaengeText->SetText(TDFX(TEXT("Länge: %s"), TEXT("Length: %s"), *Namen[mLaenge]));
		mStartText->SetText(TDFX(TEXT("Einsatz starten (Normal, %s)"), TEXT("Start operation (Normal, %s)"), *Wellen[mLaenge]));
	}
	if (mZeigtVorschau && mVorschauCountdown)
	{
		UTDWaveSubsystem* W = World->GetSubsystem<UTDWaveSubsystem>();
		const FTDWellenVorschau V = W ? W->GetVorschau() : FTDWellenVorschau();
		if (V.bGueltig && V.Welle != mVorschauNaechste)
		{
			BaueVorschau();   // Welle hat gewechselt
		}
		else if (V.bGueltig && mAuswahlWelle > 0 && mAuswahlWelle != V.Welle)
		{
			mVorschauCountdown->SetText(TDFX(TEXT("aktuell: Welle %d"), TEXT("current: wave %d"), V.Welle));
		}
		else if (V.bGueltig)
		{
			const int32 Sek = FMath::Max(0, FMath::CeilToInt(V.Countdown));
			mVorschauCountdown->SetText(FText::FromString(V.bLaeuft ? TDT(TEXT("Welle läuft"), TEXT("Wave running")) : V.Countdown < 0.f ? TDT(TEXT("Start am Kern"), TEXT("Start at the Core"))
				: FString::Printf(TEXT("Start in %d:%02d"), Sek / 60, Sek % 60)));
		}
	}
}

void UTDKernTerminal::VorschauWahl(const FString& Pfad, int32 Welle)
{
	if (Welle > 0)
	{
		mAuswahlWelle = Welle;
		mAuswahlArt.Reset();
	}
	else
	{
		mAuswahlArt = mAuswahlArt == Pfad ? FString() : Pfad;   // zweiter Klick schliesst den Steckbrief
	}
	BaueVorschau();
}

void UTDKernTerminal::Springen()
{
	UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	if (Waves && mAuswahlWelle > 0)
	{
		const bool bOk = Waves->SpringeZuWelle(mAuswahlWelle);
		if (bOk)
		{
			if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
			{
				Lauf->Eingriff(FString::Printf(TEXT("Testsprung zu Welle %d"), mAuswahlWelle));
			}
		}
		Melde(bOk ? TDF(TEXT("Test: nächste Welle ist jetzt %d."), TEXT("Test: next wave is now %d."), mAuswahlWelle) : TDT(TEXT("Springen geht nur in der Bauphase."), TEXT("Jumping only works in the build phase.")));
		mAuswahlWelle = -1;
		BaueVorschau();
	}
}

UButton* UTDKernTerminal::NeueKlickflaeche(UWidget* Inhalt, const FString& Pfad, int32 Welle)
{
	UButton* B = WidgetTree->ConstructWidget<UButton>();
	FButtonStyle Stil;
	Stil.SetNormal(FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.f));
	Stil.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.08f), 3.f));
	Stil.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.15f), 3.f));
	Stil.SetNormalPadding(FMargin(0.f));
	Stil.SetPressedPadding(FMargin(0.f));
	B->SetStyle(Stil);
	B->AddChild(Inhalt);
	UTDVorschauKnopf* K = NewObject<UTDVorschauKnopf>(this);
	K->Pfad = Pfad;
	K->Welle = Welle;
	K->Menue = this;
	B->OnClicked.AddDynamic(K, &UTDVorschauKnopf::Geklickt);
	mVorschauKnoepfe.Add(K);
	return B;
}

UWidget* UTDKernTerminal::NeueZeitleiste(int32 Gezeigt)
{
	UWorld* World = GetWorld();
	const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	UScrollBox* Rollen = WidgetTree->ConstructWidget<UScrollBox>();
	Rollen->SetOrientation(Orient_Horizontal);
	UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
	Rollen->AddChild(Reihe);
	if (!Waves)
	{
		return Rollen;
	}
	UWidget* Ziel = nullptr;
	for (const FTDZeitleistenWelle& Z : Waves->GetZeitleiste())
	{
		// Farbe nach Zustand: geschafft grau, naechste/laufende orange, kommende dunkel; Boss rot umrandet, gezeigte Welle weiss
		const bool bJetzt = Z.Zustand == 1 || Z.Zustand == 2;
		const FLinearColor Grund = bJetzt ? TDUI::Akzent : Z.Zustand == 0 ? TDUI::ZeileA : TDUI::KarteDunkel;
		const FLinearColor Schrift = bJetzt ? TDUI::DunkelText : Z.Zustand == 0 ? TDUI::Grau : TDUI::Hell;
		const FLinearColor Rand = Z.Welle == Gezeigt ? TDUI::Hell : Z.bBoss ? TDUI::Boss : Grund;
		UBorder* Rahmen = NeuesFeld(Rand, FMargin(2.f));
		USizeBox* Groesse = WidgetTree->ConstructWidget<USizeBox>();
		Groesse->SetWidthOverride(62.f);
		Groesse->SetHeightOverride(100.f);
		Rahmen->AddChild(Groesse);
		UBorder* Feld = NeuesFeld(Grund, FMargin(3.f, 4.f));
		Groesse->AddChild(Feld);
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Feld->AddChild(Spalte);
		UTextBlock* Nummer = NeuerText(FString::FromInt(Z.Welle), 16, Schrift, true);
		Nummer->SetJustification(ETextJustify::Center);
		Spalte->AddChildToVerticalBox(Nummer);
		// Kennzeichen: Bild der ersten neuen Art, sonst Boss/Front als Text
		if (Z.NeueArten.Num() > 0)
		{
			if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(NeuesKreaturBild(UTDWaveSubsystem::Steckbrief(Z.NeueArten[0]).Deskriptor, 30.f, TEXT(""), TDUI::Akzent)))
			{
				S->SetHorizontalAlignment(HAlign_Center);
			}
		}
		FString Zeichen = Z.bBoss ? FString(TEXT("BOSS")) : Z.NeueArten.Num() > 0 ? TDT(TEXT("NEU"), TEXT("NEW")) : Z.bNeueFront ? FString(TEXT("+Front")) : FString();
		if (Z.bNeueFront && (Z.bBoss || Z.NeueArten.Num() > 0))
		{
			Zeichen += TEXT(" +F");
		}
		UTextBlock* Marke = NeuerText(Zeichen, 11, Z.bBoss ? (bJetzt ? TDUI::DunkelText : TDUI::Boss) : Schrift, true);
		Marke->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Marke))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
		}
		// Kuenftige Wellen sieht man nur in der Testphase vorab; sonst ist die Zeitleiste reine Anzeige
		if (!TestModus())
		{
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Rahmen))
			{
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			}
			continue;
		}
		UButton* Knopf = NeueKlickflaeche(Rahmen, FString(), Z.Welle);
		Knopf->SetToolTipText(TDFX(TEXT("Welle %d%s%s%s"), TEXT("Wave %d%s%s%s"), Z.Welle, Z.bBoss ? *TDT(TEXT(" - Bosskampf"), TEXT(" - boss fight")) : TEXT(""),
			Z.NeueArten.Num() > 0 ? *TDT(TEXT(" - neue Gegnerart"), TEXT(" - new enemy type")) : TEXT(""), Z.bNeueFront ? *TDT(TEXT(" - neue Front"), TEXT(" - new front")) : TEXT("")));
		if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Knopf))
		{
			S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		}
		if (Z.Welle == Gezeigt)
		{
			Ziel = Knopf;
		}
	}
	if (Ziel)
	{
		Rollen->ScrollWidgetIntoView(Ziel, false, EDescendantScrollDestination::Center);
	}
	return Rollen;
}

void UTDKernTerminal::VorschauUmschalten()
{
	ZeigeAnsicht(mAnsicht == 1 ? 0 : 1);
}

void UTDKernTerminal::ForschungUmschalten()
{
	ZeigeAnsicht(mAnsicht == 2 ? 0 : 2);
}

void UTDKernTerminal::PionierUmschalten()
{
	ZeigeAnsicht(mAnsicht == 3 ? 0 : 3);
}

void UTDKernTerminal::ReiterLaden()
{
	ZeigeAnsicht(0);
}

void UTDKernTerminal::ReiterVorschau()
{
	ZeigeAnsicht(1);
}

void UTDKernTerminal::ReiterForschung()
{
	ZeigeAnsicht(2);
}

void UTDKernTerminal::ReiterPionier()
{
	ZeigeAnsicht(3);
}

void UTDKernTerminal::ReiterStatistik()
{
	ZeigeAnsicht(4);
}

void UTDKernTerminal::OeffneKofi()
{
	FString Fehler;
	FPlatformProcess::LaunchURL(TEXT("https://ko-fi.com/mariusspancken"), nullptr, &Fehler);
	UE_LOG(LogFicsitTD, Log, TEXT("Ko-fi: Browser geoeffnet%s"), Fehler.IsEmpty() ? TEXT("") : *(TEXT(", Fehler: ") + Fehler));
	Melde(Fehler.IsEmpty()
		? TDT(TEXT("Danke! Ko-fi öffnet sich in deinem Browser."), TEXT("Thank you! Ko-fi opens in your browser."))
		: TDT(TEXT("Browser ließ sich nicht öffnen: ko-fi.com/mariusspancken"), TEXT("Could not open the browser: ko-fi.com/mariusspancken")));
}

void UTDKernTerminal::ReiterMelden()
{
	ZeigeAnsicht(mAnsicht == 5 ? 0 : 5);
}

void UTDKernTerminal::MeldeArtFehler()
{
	mMeldeArt = 0;
	const FText Alt = mMeldeText ? mMeldeText->GetText() : FText::GetEmpty();
	BaueMelden();
	if (mMeldeText) { mMeldeText->SetText(Alt); }
}

void UTDKernTerminal::MeldeArtIdee()
{
	mMeldeArt = 1;
	const FText Alt = mMeldeText ? mMeldeText->GetText() : FText::GetEmpty();
	BaueMelden();
	if (mMeldeText) { mMeldeText->SetText(Alt); }
}

void UTDKernTerminal::BaueMelden()
{
	if (!mMeldenInhalt)
	{
		return;
	}
	mMeldenInhalt->ClearChildren();
	auto Abschnitt = [&](UWidget* W, float Unten)
	{
		if (UVerticalBoxSlot* S = mMeldenInhalt->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Unten));
		}
	};
	Abschnitt(NeuerText(TDT(TEXT("Fehler melden oder Idee vorschlagen"), TEXT("Report a bug or suggest an idea")), 18, TDUI::Akzent, true), 4.f);
	UTextBlock* Erklaerung = NeuerText(TDT(
		TEXT("Beschreibe kurz, was passiert ist oder was du dir wünschst. Mit \"Auf GitHub öffnen\" startet dein Browser mit einem ausgefüllten Issue (Version, Welle, Stufe und Sprache trägt die Mod ein). Zusätzlich öffnet sich ein Ordner mit Log und Bildschirmfoto: Zieh die Dateien bei einem Fehler ins Issue. Die Mod selbst sendet nichts, du schickst das Issue ab. Für GitHub brauchst du ein kostenloses Konto."),
		TEXT("Briefly describe what happened or what you would like. \"Open on GitHub\" starts your browser with a prefilled issue (the mod adds version, wave, difficulty and language). A folder with the log and a screenshot opens as well: for a bug, drag the files into the issue. The mod itself sends nothing, you submit the issue. GitHub needs a free account.")), 13, TDUI::Grau);
	Erklaerung->SetAutoWrapText(true);
	Abschnitt(Erklaerung, 10.f);
	// Art: Fehler oder Idee (gewaehlte orange)
	UHorizontalBox* Arten = WidgetTree->ConstructWidget<UHorizontalBox>();
	UTextBlock* T = nullptr;
	UButton* Fehler = NeuerKnopf(TDT(TEXT("Fehler"), TEXT("Bug")), T, mMeldeArt == 0);
	Fehler->OnClicked.AddDynamic(this, &UTDKernTerminal::MeldeArtFehler);
	UButton* Idee = NeuerKnopf(TDT(TEXT("Vorschlag / Idee"), TEXT("Suggestion / idea")), T, mMeldeArt == 1);
	Idee->OnClicked.AddDynamic(this, &UTDKernTerminal::MeldeArtIdee);
	if (UHorizontalBoxSlot* S = Arten->AddChildToHorizontalBox(Fehler)) { S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f)); }
	Arten->AddChildToHorizontalBox(Idee);
	Abschnitt(Arten, 10.f);
	// Text
	mMeldeText = WidgetTree->ConstructWidget<UMultiLineEditableTextBox>();
	mMeldeText->SetHintText(mMeldeArt == 0
		? TDTX(TEXT("Was ist passiert? Was hast du davor gemacht? Was hättest du erwartet?"), TEXT("What happened? What did you do before? What did you expect?"))
		: TDTX(TEXT("Was möchtest du ändern oder neu haben, und warum?"), TEXT("What would you like to change or add, and why?")));
	USizeBox* TextHoehe = WidgetTree->ConstructWidget<USizeBox>();
	TextHoehe->SetHeightOverride(220.f);
	TextHoehe->AddChild(mMeldeText);
	Abschnitt(TextHoehe, 10.f);
	UButton* Senden = NeuerKnopf(TDT(TEXT("Auf GitHub öffnen"), TEXT("Open on GitHub")), T, true);
	Senden->OnClicked.AddDynamic(this, &UTDKernTerminal::MeldungAbschicken);
	UHorizontalBox* Knoepfe = WidgetTree->ConstructWidget<UHorizontalBox>();
	Knoepfe->AddChildToHorizontalBox(Senden);
	Abschnitt(Knoepfe, 6.f);
}

/** 0.9.6: Prozent-Kodierung fuer die Issue-Adresse (UTF-8, nur unreservierte Zeichen bleiben) */
static FString TDUrlKodiert(const FString& Text)
{
	const FTCHARToUTF8 Utf8(*Text);
	FString Aus;
	for (int32 i = 0; i < Utf8.Length(); ++i)
	{
		const uint8 C = static_cast<uint8>(Utf8.Get()[i]);
		if ((C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C == '~')
		{
			Aus.AppendChar(static_cast<TCHAR>(C));
		}
		else
		{
			Aus += FString::Printf(TEXT("%%%02X"), C);
		}
	}
	return Aus;
}

void UTDKernTerminal::MeldungAbschicken()
{
	const FString Text = mMeldeText ? mMeldeText->GetText().ToString().TrimStartAndEnd() : FString();
	if (Text.Len() < 5)
	{
		Melde(TDT(TEXT("Bitte beschreibe kurz, worum es geht."), TEXT("Please describe briefly what it is about.")));
		return;
	}
	UWorld* World = GetWorld();
	const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FicsitTD"));
	const FString Version = Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString(TEXT("?"));
	const bool bFehler = mMeldeArt == 0;
	// Angaben ohne persoenliche Daten: Version, Spielstand des Einsatzes, Sprache
	const FString Info = FString::Printf(TEXT("FicsitTD %s, Satisfactory %s, %s, wave %d/%d, difficulty %d, operation %s, language %s"),
		*Version, FApp::GetBuildVersion(), *FPlatformMisc::GetOSVersion(),
		Waves ? Waves->GetWelle() : 0, Waves ? Waves->mWellenGesamt : 0, Waves ? Waves->GetLaufStufe() : -1,
		Einsatz && Einsatz->IstAktiv() ? TEXT("active") : TEXT("off"), TDSprache::Englisch() ? TEXT("en") : TEXT("de"));
	// Ordner mit Meldung, Log und Bildschirmfoto (das Foto kommt mit dem naechsten Bild, ohne das Menue)
	const FString Ordner = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FicsitTD"), TEXT("Meldungen"),
		FDateTime::Now().ToString(TEXT("%Y-%m-%d_%H%M%S"))));
	IFileManager::Get().MakeDirectory(*Ordner, true);
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("%s\n\n%s\n\n%s\n"), bFehler ? TEXT("Bug") : TEXT("Suggestion"), *Text, *Info),
		*FPaths::Combine(Ordner, TEXT("Meldung.txt")), FFileHelper::EEncodingOptions::ForceUTF8);
	if (bFehler)
	{
		GLog->Flush();
		IFileManager::Get().Copy(*FPaths::Combine(Ordner, TEXT("FactoryGame.log")), *FPaths::Combine(FPaths::ProjectLogDir(), TEXT("FactoryGame.log")));
		FScreenshotRequest::RequestScreenshot(FPaths::Combine(Ordner, TEXT("Screenshot.png")), false, false);
	}
	// Issue-Adresse: Titel aus der ersten Zeile, Text und Angaben im Inhalt, Label bug bzw. enhancement
	FString Titel = Text.Left(70);
	Titel.Split(TEXT("\n"), &Titel, nullptr);
	Titel = FString(bFehler ? TEXT("[Bug] ") : TEXT("[Idea] ")) + Titel.TrimEnd();
	FString Inhalt = FString::Printf(TEXT("%s\n\n---\n%s\n"), *Text.Left(5000), *Info);
	if (bFehler)
	{
		Inhalt += TEXT("\n_Log and screenshot: please drag `FactoryGame.log` and `Screenshot.png` from the folder that opened into this issue._\n");
	}
	const FString Adresse = FString::Printf(TEXT("https://github.com/Spancken/FicsitTD/issues/new?labels=%s&title=%s&body=%s"),
		bFehler ? TEXT("bug") : TEXT("enhancement"), *TDUrlKodiert(Titel), *TDUrlKodiert(Inhalt));
	FString Fehlertext;
	FPlatformProcess::LaunchURL(*Adresse, nullptr, &Fehlertext);
	FPlatformProcess::ExploreFolder(*Ordner);
	UE_LOG(LogFicsitTD, Log, TEXT("Meldung (%s): Browser mit GitHub-Issue geoeffnet, Ordner %s%s"), bFehler ? TEXT("Fehler") : TEXT("Idee"), *Ordner,
		Fehlertext.IsEmpty() ? TEXT("") : *(TEXT(", Browser-Fehler: ") + Fehlertext));
	Melde(Fehlertext.IsEmpty()
		? TDF(TEXT("Dein Browser öffnet ein ausgefülltes GitHub-Issue. Log und Bildschirmfoto liegen in %s."), TEXT("Your browser opens a prefilled GitHub issue. Log and screenshot are in %s."), *Ordner)
		: TDF(TEXT("Browser ließ sich nicht öffnen. Deine Meldung liegt in %s, bitte auf github.com/Spancken/FicsitTD/issues einstellen."), TEXT("Could not open the browser. Your report is in %s, please post it at github.com/Spancken/FicsitTD/issues."), *Ordner));
	if (mMeldeText)
	{
		mMeldeText->SetText(FText::GetEmpty());
	}
}

UWidget* UTDKernTerminal::NeueKachel(UTexture2D* Bild, float Kante, const FString& Plakette)
{
	USizeBox* Rahmen = WidgetTree->ConstructWidget<USizeBox>();
	Rahmen->SetWidthOverride(Kante);
	Rahmen->SetHeightOverride(Kante);
	UOverlay* Ebenen = WidgetTree->ConstructWidget<UOverlay>();
	Rahmen->AddChild(Ebenen);
	UBorder* Grund = NeuesFeld(TDUI::C(205, 205, 205), FMargin(Kante * 0.08f));
	Grund->SetBrush(FSlateRoundedBoxBrush(TDUI::C(205, 205, 205), 2.f));
	if (Bild)
	{
		UImage* Symbol = WidgetTree->ConstructWidget<UImage>();
		Symbol->SetBrushFromTexture(Bild, false);
		Grund->AddChild(Symbol);
	}
	if (UOverlaySlot* S = Ebenen->AddChildToOverlay(Grund))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	if (!Plakette.IsEmpty())
	{
		UBorder* Marke = NeuesFeld(TDUI::Akzent, FMargin(4.f, 0.f));
		Marke->SetBrush(FSlateRoundedBoxBrush(TDUI::Akzent, 2.f));
		Marke->AddChild(NeuerText(Plakette, FMath::Clamp(FMath::RoundToInt(Kante * 0.24f), 10, 15), FLinearColor::White, true));
		if (UOverlaySlot* S = Ebenen->AddChildToOverlay(Marke))
		{
			S->SetHorizontalAlignment(HAlign_Right);
			S->SetVerticalAlignment(VAlign_Bottom);
		}
	}
	return Rahmen;
}

void UTDKernTerminal::ZeigeAnsicht(int32 Ansicht)
{
	mAnsicht = Ansicht;
	// aktiver Reiter heller (wie im Spiel), die anderen in der Farbe der Kopfleiste
	for (int32 i = 0; i < mReiter.Num(); ++i)
	{
		const bool bAktiv = i == Ansicht;
		FButtonStyle Stil;
		Stil.SetNormal(FSlateRoundedBoxBrush(bAktiv ? TDUI::Reiter : TDUI::Kopfleiste, 0.f));
		Stil.SetHovered(FSlateRoundedBoxBrush(bAktiv ? TDUI::Reiter : TDUI::C(48, 48, 48), 0.f));
		Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::Reiter, 0.f));
		Stil.SetNormalPadding(FMargin(22.f, 10.f, 30.f, 10.f));
		Stil.SetPressedPadding(FMargin(22.f, 10.f, 30.f, 10.f));
		mReiter[i]->SetStyle(Stil);
		if (mReiterTexte.IsValidIndex(i))
		{
			mReiterTexte[i]->SetColorAndOpacity(FSlateColor(bAktiv ? TDUI::Hell : TDUI::Grau));
		}
	}
	mZeigtVorschau = Ansicht == 1;
	if (Ansicht == 1)
	{
		BaueVorschau();
	}
	else if (Ansicht == 2)
	{
		BaueForschung();
	}
	else if (Ansicht == 3)
	{
		BauePionier();
	}
	else if (Ansicht == 4)
	{
		BaueStatistik();
	}
	else if (Ansicht == 5)
	{
		BaueMelden();
	}
	UWidget* Flaechen[6] = { mSpalten, mVorschau, mForschung, mPionier, mStatistik, mMelden };
	for (int32 i = 0; i < 6; ++i)
	{
		if (Flaechen[i])
		{
			Flaechen[i]->SetVisibility(i == Ansicht ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	if (mVorschauKnopfText)
	{
		mVorschauKnopfText->SetText(Ansicht == 1 ? TDTX(TEXT("Zurück zum Laden"), TEXT("Back to shop")) : TDTX(TEXT("Wellenvorschau"), TEXT("Wave Preview")));
	}
	if (mForschungKnopfText)
	{
		mForschungKnopfText->SetText(Ansicht == 2 ? TDTX(TEXT("Zurück zum Laden"), TEXT("Back to shop")) : TDTX(TEXT("Forschung"), TEXT("Research")));
	}
	if (mPionierKnopfText)
	{
		mPionierKnopfText->SetText(Ansicht == 3 ? TDTX(TEXT("Zurück zum Laden"), TEXT("Back to shop")) : TDTX(TEXT("Pionier"), TEXT("Pioneer")));
	}
}

void UTDKernTerminal::BaueForschung()
{
	if (!mForschungInhalt)
	{
		return;
	}
	mForschungInhalt->ClearChildren();
	auto Abschnitt = [&](UWidget* W, float Unten)
	{
		if (UVerticalBoxSlot* S = mForschungInhalt->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Unten));
		}
	};
	UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	const UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const int32 Naechste = Waves ? Waves->GetWelle() + 1 : 1;
	int32 Anzahl[5] = { 0, 0, 0, 0, 0 };
	for (const FTDForschungKnoten& Kn : TDForschung::Alle())
	{
		++Anzahl[static_cast<int32>(TDForschung::Zustand(this, Kn))];
	}
	Abschnitt(NeuerText(TDF(TEXT("Forschung im MAM, Bäume FICSIT-Abwehr. Im Inventar: %d FP. Nächste Welle: %d."),
		TEXT("Research in the MAM, FICSIT Defense trees. In inventory: %d RP. Next wave: %d."),
		Einsatz ? Einsatz->FPImInventar() : 0, Naechste), 16, TDUI::Hell, true), 4.f);
	Abschnitt(NeuerText(TDF(TEXT("%d erforscht  ·  %d jetzt erforschbar  ·  %d warten auf eine spätere Welle  ·  %d brauchen erst einen anderen Knoten"),
		TEXT("%d researched  ·  %d available now  ·  %d waiting for a later wave  ·  %d need another node first"),
		Anzahl[0], Anzahl[1], Anzahl[2], Anzahl[3]), 13, TDUI::Grau), 14.f);
	FString Baum;
	UWrapBox* Raster = nullptr;
	for (const FTDForschungKnoten& Kn : TDForschung::Alle())
	{
		if (Kn.Baum != Baum || !Raster)
		{
			Baum = Kn.Baum;
			Abschnitt(NeuerText(Kn.BaumName, 16, TDUI::Akzent, true), 6.f);
			Raster = WidgetTree->ConstructWidget<UWrapBox>();
			Raster->SetInnerSlotPadding(FVector2D(10.f, 10.f));
			Abschnitt(Raster, 16.f);
		}
		const ETDForschungZustand Z = TDForschung::Zustand(this, Kn);
		FString Lage;
		FLinearColor LageFarbe = TDUI::Grau;
		FLinearColor Grund = TDUI::ChipNeutral;
		switch (Z)
		{
		case ETDForschungZustand::Erforscht:
			Lage = TDT(TEXT("ERFORSCHT"), TEXT("RESEARCHED"));
			LageFarbe = TDUI::Gut;
			Grund = TDUI::ChipGut;
			break;
		case ETDForschungZustand::Moeglich:
			Lage = TDF(TEXT("JETZT ERFORSCHBAR  ·  %d FP"), TEXT("AVAILABLE NOW  ·  %d RP"), Kn.FP);
			LageFarbe = TDUI::Akzent;
			Grund = TDUI::ChipStark;
			break;
		case ETDForschungZustand::ZuFrueh:
			Lage = TDF(TEXT("AB WELLE %d  ·  %d FP"), TEXT("FROM WAVE %d  ·  %d RP"), TDForschung::AbWelle(Kn), Kn.FP);
			break;
		case ETDForschungZustand::Ausgeschlossen:
			Lage = TDT(TEXT("AUSGESCHLOSSEN: andere Seite der Abzweigung gewählt"), TEXT("EXCLUDED: other side of the branch chosen"));
			LageFarbe = TDUI::Fehler;
			break;
		default:
		{
			TArray<FString> Fehlt;
			for (const FString& V : Kn.Braucht)
			{
				if (!TDForschung::Hat(this, *V))
				{
					const FTDForschungKnoten* Vk = TDForschung::Finde(V);
					Fehlt.Add(Vk ? Vk->Name : V);
				}
			}
			Lage = TDT(TEXT("BRAUCHT ERST: "), TEXT("REQUIRES FIRST: ")) + FString::Join(Fehlt, TEXT(", "));
			break;
		}
		}
		USizeBox* Breite = WidgetTree->ConstructWidget<USizeBox>();
		Breite->SetWidthOverride(330.f);
		UBorder* Karte = NeuesFeld(Grund, FMargin(10.f, 8.f));
		Breite->AddChild(Karte);
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Karte->AddChild(Spalte);
		auto Zeile = [&](const FString& Text, int32 Groesse, const FLinearColor& Farbe, bool bFett, float Unten)
		{
			UTextBlock* T = NeuerText(Text, Groesse, Farbe, bFett);
			T->SetAutoWrapText(true);
			if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(T))
			{
				S->SetPadding(FMargin(0.f, 0.f, 0.f, Unten));
			}
		};
		Zeile(Kn.Name, 14, TDUI::Hell, true, 2.f);
		Zeile(Lage, 12, LageFarbe, true, 4.f);
		Zeile(Kn.Text, 12, TDUI::Hell, false, 3.f);
		if (!Kn.Laden.IsEmpty())
		{
			Zeile(TDT(TEXT("Im Laden: "), TEXT("In shop: ")) + Kn.Laden, 12, TDUI::Gut, false, 1.f);
		}
		if (!Kn.Gegen.IsEmpty())
		{
			Zeile(TDT(TEXT("Gut gegen: "), TEXT("Good against: ")) + Kn.Gegen, 12, TDUI::Grau, false, 1.f);
		}
		Raster->AddChildToWrapBox(Breite);
	}
}

void UTDKernTerminal::BauePionier()
{
	if (!mPionierInhalt)
	{
		return;
	}
	mPionierInhalt->ClearChildren();
	mTalentKnoepfe.Reset();
	auto Abschnitt = [&](UWidget* W, float Unten)
	{
		if (UVerticalBoxSlot* S = mPionierInhalt->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Unten));
		}
	};
	UTDPionier* P = UTDPionier::Get(this);
	const UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	if (!P || !Einsatz || !Einsatz->IstAktiv())
	{
		Abschnitt(NeuerText(TDT(TEXT("Rang und Talente gibt es im Einsatz. Starte am Kern einen Einsatz."), TEXT("Rank and talents come with an operation. Start an operation at the Core.")), 15, TDUI::Grau), 0.f);
		return;
	}
	const int32 Rang = P->GetRang();
	const int32 XP = P->GetXP();
	const int32 Frei = P->FreiePunkte();
	UHorizontalBox* Kopf = WidgetTree->ConstructWidget<UHorizontalBox>();
	UBorder* Marke = NeuesFeld(TDUI::Akzent, FMargin(12.f, 3.f));
	Marke->AddChild(NeuerText(TDF(TEXT("Pionier-Rang %d"), TEXT("Pioneer Rank %d"), Rang), 18, TDUI::DunkelText, true));
	if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(Marke))
	{
		S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(NeuerText(Frei > 0
		? (Frei == 1 ? TDT(TEXT("1 Talentpunkt frei"), TEXT("1 Talent Point free")) : TDF(TEXT("%d Talentpunkte frei"), TEXT("%d Talent Points free"), Frei))
		: TDT(TEXT("Kein Talentpunkt frei"), TEXT("No Talent Points free")), 17, Frei > 0 ? TDUI::Gut : TDUI::Grau, true)))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	Abschnitt(Kopf, 8.f);
	const bool bMax = Rang >= UTDPionier::MaxRang();
	const int32 Von = UTDPionier::XPFuerRang(Rang);
	const int32 Bis = UTDPionier::XPFuerRang(Rang + 1);
	UProgressBar* Balken = WidgetTree->ConstructWidget<UProgressBar>();
	Balken->SetPercent(bMax ? 1.f : FMath::Clamp(float(XP - Von) / FMath::Max(1, Bis - Von), 0.f, 1.f));
	Balken->SetFillColorAndOpacity(TDUI::Akzent);
	USizeBox* BalkenHoehe = WidgetTree->ConstructWidget<USizeBox>();
	BalkenHoehe->SetHeightOverride(12.f);
	BalkenHoehe->AddChild(Balken);
	Abschnitt(BalkenHoehe, 4.f);
	Abschnitt(NeuerText(bMax ? TDF(TEXT("%d XP, Höchstrang erreicht"), TEXT("%d XP, max rank reached"), XP)
		: TDF(TEXT("%d / %d XP bis Rang %d"), TEXT("%d / %d XP to rank %d"), XP, Bis, Rang + 1), 13, TDUI::Grau), 4.f);
	// 0.9.8 P3: XP vor allem fuer eigene Leistung, hoechstens 15 Punkte, Rangboni ohne Punkte
	UTextBlock* Info = NeuerText(TDF(TEXT("XP gibt es für deinen Anteil am Wellenschaden, für Abschüsse (normal 3, Elite 10, Boss 50), Assists und reparierte LP, dazu etwas für jede abgewehrte Welle. "
		"Jeder Rang bringt einen Talentpunkt, höchstens %d: Wähle zwei Rollen ganz oder drei zur Hälfte. Alle 3 Ränge: +10 LP und +3 %% Lauftempo (jetzt +%d LP). Rang und Talente gelten für diesen Einsatz."),
		TEXT("You earn XP for your share of the wave damage, for kills (normal 3, Elite 10, Boss 50), assists and repaired HP, plus a little for every repelled wave. "
		"Each rank gives one Talent Point, at most %d: pick two roles in full or three halfway. Every 3 ranks: +10 HP and +3 %% walk speed (now +%d HP). Rank and talents apply to this operation."),
		UTDPionier::MaxRang() - 1, 10 * P->RangBoni()), 13, TDUI::Grau);
	Info->SetAutoWrapText(true);
	Abschnitt(Info, 14.f);

	UHorizontalBox* Spalten = WidgetTree->ConstructWidget<UHorizontalBox>();
	const TArray<FTDTalentZweig>& Zweige = UTDPionier::Zweige();
	for (int32 i = 0; i < Zweige.Num(); ++i)
	{
		const FTDTalentZweig& Z = Zweige[i];
		const int32 Stufe = P->GetStufe(i);
		const FString Fehlt;   // 0.9.8: kein Zweig braucht mehr einen Laden-Posten
		UBorder* Feld = NeuesFeld(TDUI::ZeileB, FMargin(10.f, 8.f));
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Feld->AddChild(Spalte);
		Spalte->AddChildToVerticalBox(NeuerText(FString::Printf(TEXT("%s  %d/%d"), Z.Name, Stufe, UTDPionier::MaxStufe), 16, TDUI::Hell, true));
		Spalte->AddChildToVerticalBox(NeuerText(Z.Rolle, 12, TDUI::Akzent));
		// 0.9.8 P3 (Analyse 3.4): Ist-Werte der letzten Welle je Zweig
		const FString Ist = P->IstWerte(i);
		UTextBlock* Bedingung = NeuerText(Ist.IsEmpty() ? TDT(TEXT("Letzte Welle: noch keine Wirkung"), TEXT("Last wave: no effect yet"))
			: TDF(TEXT("Letzte Welle: %s"), TEXT("Last wave: %s"), *Ist), 12, Ist.IsEmpty() ? TDUI::Grau : TDUI::Gut);
		Bedingung->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Bedingung))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 8.f));
		}
		for (int32 s = 0; s < UTDPionier::MaxStufe; ++s)
		{
			const FString Text = TDF(TEXT("Stufe %d: %s"), TEXT("Tier %d: %s"), s + 1, Z.Stufen[s]);
			UWidget* Eintrag = nullptr;
			if (s == Stufe && Frei > 0 && Fehlt.IsEmpty())
			{
				UTextBlock* KnopfText = nullptr;
				UButton* Knopf = NeuerKnopf(TDT(TEXT("Lernen · "), TEXT("Learn · ")) + Text, KnopfText, true);
				if (KnopfText)
				{
					KnopfText->SetAutoWrapText(true);
				}
				UTDTalentKnopf* Objekt = NewObject<UTDTalentKnopf>(this);
				Objekt->Zweig = i;
				Objekt->Menue = this;
				Knopf->OnClicked.AddDynamic(Objekt, &UTDTalentKnopf::Geklickt);
				mTalentKnoepfe.Add(Objekt);
				Eintrag = Knopf;
			}
			else
			{
				UBorder* Zelle = NeuesFeld(s < Stufe ? TDUI::ChipGut : TDUI::ChipNeutral, FMargin(8.f, 5.f));
				UTextBlock* T = NeuerText(Text, 12, s < Stufe ? TDUI::Gut : TDUI::Grau, s < Stufe);
				T->SetAutoWrapText(true);
				Zelle->AddChild(T);
				Eintrag = Zelle;
			}
			if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Eintrag))
			{
				S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
			}
		}
		if (UHorizontalBoxSlot* S = Spalten->AddChildToHorizontalBox(Feld))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetPadding(FMargin(0.f, 0.f, i + 1 < Zweige.Num() ? 10.f : 0.f, 0.f));
		}
	}
	Abschnitt(Spalten, 14.f);
	UTextBlock* NeuText = nullptr;
	UButton* Neu = NeuerKnopf(TDF(TEXT("Talente neu verteilen (%d Coupons)"), TEXT("Reset talents (%d Coupons)"), UTDPionier::NeuVerteilenPreis()), NeuText);
	Neu->OnClicked.AddDynamic(this, &UTDKernTerminal::TalenteNeu);
	UHorizontalBox* Fuss = WidgetTree->ConstructWidget<UHorizontalBox>();
	Fuss->AddChildToHorizontalBox(Neu);
	Abschnitt(Fuss, 0.f);
}

void UTDKernTerminal::LerneTalent(int32 Zweig)
{
	if (UTDPionier* P = UTDPionier::Get(this))
	{
		Melde(P->Lerne(Zweig));
		BauePionier();
	}
}

void UTDKernTerminal::TalenteNeu()
{
	if (UTDPionier* P = UTDPionier::Get(this))
	{
		Melde(P->NeuVerteilen());
		BauePionier();
	}
}

/** Spiel-Icon einer Kreatur ueber ihren Deskriptor (GetBigIcon beachtet die Arachnophobie-Einstellung), einmal je Art gecacht */
static UTexture2D* KreaturTextur(const FString& Deskriptor)
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> Bilder;
	UTexture2D* Bild = Bilder.FindRef(Deskriptor).Get();
	if (!Bild && !Deskriptor.IsEmpty())
	{
		if (UClass* Desc = LoadClass<UFGItemDescriptor>(nullptr, *Deskriptor))
		{
			Bild = UFGItemDescriptor::GetBigIcon(Desc);
		}
		Bilder.Add(Deskriptor, Bild);
		UE_LOG(LogFicsitTD, Log, TEXT("Wellenvorschau: Bild %s %s"), *Deskriptor, Bild ? TEXT("geladen") : TEXT("fehlt"));
	}
	return Bild;
}

UWidget* UTDKernTerminal::NeuesKreaturBild(const FString& Deskriptor, float Kante, const FString& Abzeichen, const FLinearColor& AbzeichenFarbe)
{
	UTexture2D* Bild = KreaturTextur(Deskriptor);
	UOverlay* Stapel = WidgetTree->ConstructWidget<UOverlay>();
	UBorder* Grund = NeuesFeld(TDUI::BildGrund, FMargin(4.f));
	Grund->AddChild(NeuesSymbol(Bild, Kante));
	Stapel->AddChildToOverlay(Grund);
	if (!Abzeichen.IsEmpty())
	{
		UBorder* Marke = NeuesFeld(AbzeichenFarbe, FMargin(7.f, 1.f));
		Marke->AddChild(NeuerText(Abzeichen, 11, TDUI::DunkelText, true));
		if (UOverlaySlot* S = Stapel->AddChildToOverlay(Marke))
		{
			S->SetHorizontalAlignment(HAlign_Left);
			S->SetVerticalAlignment(VAlign_Top);
		}
	}
	return Stapel;
}

/** Auswahl-Schluessel einer Gruppe: Boss getrennt von einer gleichartigen normalen Gruppe */
static FString AuswahlSchluessel(const FTDPlanGruppe& G)
{
	return G.bBoss ? TEXT("BOSS|") + G.Pfad : G.Pfad;
}

/** Testphase: Wellen in der Zeitleiste waehlen und springen (spaeter Balance Test.WellenSprung = 0) */
static bool TestModus()
{
	return !UTDLaufProtokoll::bBalancing && TDBalance::Ganz(TEXT("Test.WellenSprung"), 1) > 0;
}

static FString RollenName(int32 Rolle)
{
	switch (FMath::Clamp(Rolle, 0, 4))
	{
	case 0: return TDT(TEXT("Ansturm"), TEXT("Rusher"));
	case 1: return TDT(TEXT("Panzer"), TEXT("Tank"));
	case 2: return TDT(TEXT("Fernkampf"), TEXT("Ranged"));
	case 3: return TDT(TEXT("Schwarm"), TEXT("Swarm"));
	default: return TEXT("Boss");
	}
}

UWidget* UTDKernTerminal::NeuerSteckbrief(const FTDPlanGruppe& G, bool bZahlen)
{
	const FTDKreaturSteckbrief S = UTDWaveSubsystem::Steckbrief(G.Pfad);
	const bool bBoss = G.bBoss;
	const FLinearColor Farbe = bBoss ? TDUI::Boss : TDUI::Akzent;
	UBorder* Karte = NeuesFeld(bBoss ? TDUI::BossFeld : G.bNeu ? TDUI::NeuFeld : TDUI::KarteDunkel, FMargin(12.f));
	UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
	Karte->AddChild(Reihe);
	if (UHorizontalBoxSlot* Sl = Reihe->AddChildToHorizontalBox(NeuesKreaturBild(S.Deskriptor, 128.f, bBoss ? FString(TEXT("BOSS")) : G.bNeu ? TDT(TEXT("NEU"), TEXT("NEW")) : FString(), Farbe)))
	{
		Sl->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		Sl->SetVerticalAlignment(VAlign_Top);
	}
	UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
	if (UHorizontalBoxSlot* Sl = Reihe->AddChildToHorizontalBox(Spalte))
	{
		Sl->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	// Name, Anzahl, Rolle
	UHorizontalBox* Kopf = WidgetTree->ConstructWidget<UHorizontalBox>();
	Kopf->AddChildToHorizontalBox(NeuerText(bBoss && !G.BossName.IsEmpty() ? G.BossName : S.Name, 20, TDUI::Hell, true));
	const FString Neben = FString::Printf(TEXT("   %s%s"), bZahlen ? *FString::Printf(TEXT("× %d  ·  "), G.Anzahl) : TEXT(""), *RollenName(bBoss ? 4 : S.Rolle));
	if (UHorizontalBoxSlot* Sl = Kopf->AddChildToHorizontalBox(NeuerText(Neben, 14, TDUI::Grau)))
	{
		Sl->SetVerticalAlignment(VAlign_Bottom);
	}
	Spalte->AddChildToVerticalBox(Kopf);
	UTextBlock* Text = NeuerText(S.Text, 14, TDUI::Hell);
	Text->SetAutoWrapText(true);
	if (UVerticalBoxSlot* Sl = Spalte->AddChildToVerticalBox(Text))
	{
		Sl->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));
	}
	// Werte und Wirkung der Tuerme als Chips
	UWrapBox* Chips = WidgetTree->ConstructWidget<UWrapBox>();
	Chips->SetInnerSlotPadding(FVector2D(6.f, 6.f));
	auto Chip = [&](const FString& T, const FLinearColor& Grund, const FLinearColor& Schrift)
	{
		UBorder* C = NeuesFeld(Grund, FMargin(9.f, 3.f));
		C->AddChild(NeuerText(T, 12, Schrift));
		Chips->AddChildToWrapBox(C);
	};
	Chip(TDF(TEXT("Tempo %.1f m/s"), TEXT("Speed %.1f m/s"), S.Tempo), TDUI::ChipNeutral, TDUI::Hell);
	Chip(S.Reichweite >= 8.f ? TDF(TEXT("Fernkampf %.0f m"), TEXT("Ranged %.0f m"), S.Reichweite) : TDT(TEXT("Nahkampf"), TEXT("Melee")), TDUI::ChipNeutral, TDUI::Hell);
	Chip(TDF(TEXT("Schaden %.0f je Treffer"), TEXT("Damage %.0f per hit"), S.Schaden), TDUI::ChipStark, TDUI::Akzent);
	FString Gut, Schlecht;
	for (const TPair<FString, float>& W : S.Wirkung)
	{
		if (W.Value >= 1.15f)
		{
			Gut += (Gut.IsEmpty() ? TEXT("") : TEXT(", ")) + W.Key;
		}
		else if (W.Value <= 0.85f)
		{
			Schlecht += (Schlecht.IsEmpty() ? TEXT("") : TEXT(", ")) + W.Key;
		}
	}
	if (!Gut.IsEmpty())
	{
		Chip(TDT(TEXT("Schwach gegen: "), TEXT("Weak to: ")) + Gut, TDUI::ChipGut, TDUI::Gut);
	}
	if (!Schlecht.IsEmpty())
	{
		Chip(TDT(TEXT("Hält aus: "), TEXT("Resists: ")) + Schlecht, TDUI::ChipSchlecht, TDUI::Boss);
	}
	if (bZahlen && G.Elite > 0)
	{
		Chip(FString::Printf(TEXT("%d Elite"), G.Elite), TDUI::ChipNeutral, TDUI::Elite);
	}
	Spalte->AddChildToVerticalBox(Chips);
	return Karte;
}

UWidget* UTDKernTerminal::NeueGegnerKarte(const FTDPlanGruppe& G, bool bZahlen)
{
	const FTDKreaturSteckbrief S = UTDWaveSubsystem::Steckbrief(G.Pfad);
	// Rahmen: orange = neu, violett = mit Elite, rot = Boss
	const FLinearColor Rahmen = AuswahlSchluessel(G) == mAuswahlArt ? TDUI::Hell : G.bBoss ? TDUI::Boss : G.bNeu ? TDUI::Akzent : bZahlen && G.Elite > 0 ? TDUI::EliteRahmen : TDUI::Reiter;
	UBorder* Aussen = NeuesFeld(Rahmen, FMargin(2.f));
	USizeBox* Breite = WidgetTree->ConstructWidget<USizeBox>();
	Breite->SetWidthOverride(150.f);
	Aussen->AddChild(Breite);
	UBorder* Karte = NeuesFeld(TDUI::KarteDunkel, FMargin(8.f));
	Breite->AddChild(Karte);
	UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
	Karte->AddChild(Spalte);
	const FString Abzeichen = G.bBoss ? FString(TEXT("BOSS")) : G.bNeu ? TDT(TEXT("NEU"), TEXT("NEW")) : FString();
	if (UVerticalBoxSlot* Sl = Spalte->AddChildToVerticalBox(NeuesKreaturBild(S.Deskriptor, 112.f, Abzeichen, G.bBoss ? TDUI::Boss : TDUI::Akzent)))
	{
		Sl->SetHorizontalAlignment(HAlign_Center);
	}
	UTextBlock* Name = NeuerText(G.bBoss && !G.BossName.IsEmpty() ? G.BossName : S.Name, 14, TDUI::Hell, true);
	Name->SetJustification(ETextJustify::Center);
	Name->SetAutoWrapText(true);
	if (UVerticalBoxSlot* Sl = Spalte->AddChildToVerticalBox(Name))
	{
		Sl->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		Sl->SetHorizontalAlignment(HAlign_Fill);
	}
	UTextBlock* Zahl = NeuerText(bZahlen ? FString::Printf(TEXT("× %d"), G.Anzahl) : TEXT("× ?"), 18, TDUI::Akzent, true);
	Zahl->SetJustification(ETextJustify::Center);
	Spalte->AddChildToVerticalBox(Zahl);
	UTextBlock* Zusatz = NeuerText(bZahlen && G.Elite > 0 ? FString::Printf(TEXT("%d Elite"), G.Elite) : RollenName(G.bBoss ? 4 : S.Rolle), 12,
		bZahlen && G.Elite > 0 ? TDUI::Elite : TDUI::Grau);
	Zusatz->SetJustification(ETextJustify::Center);
	Spalte->AddChildToVerticalBox(Zusatz);
	UButton* Knopf = NeueKlickflaeche(Aussen, AuswahlSchluessel(G), -1);
	Knopf->SetToolTipText(TDTX(TEXT("Steckbrief anzeigen"), TEXT("Show profile")));
	return Knopf;
}

void UTDKernTerminal::BaueVorschau()
{
	if (!mVorschauInhalt)
	{
		return;
	}
	mVorschauInhalt->ClearChildren();
	mVorschauKnoepfe.Reset();
	mVorschauCountdown = nullptr;
	UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const FTDWellenVorschau Jetzt = Waves ? Waves->GetVorschau() : FTDWellenVorschau();
	mVorschauNaechste = Jetzt.Welle;
	if (mAuswahlWelle == Jetzt.Welle)
	{
		mAuswahlWelle = -1;
	}
	if (!TestModus())
	{
		mAuswahlWelle = -1;
	}
	const FTDWellenVorschau V = Waves && mAuswahlWelle > 0 ? Waves->GetVorschau(mAuswahlWelle) : Jetzt;
	auto Abschnitt = [&](UWidget* W, float Unten)
	{
		if (UVerticalBoxSlot* S = mVorschauInhalt->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Unten));
		}
	};
	if (!V.bGueltig)
	{
		Abschnitt(NeuerText(TDT(TEXT("Keine Welle geplant. Starte am Kern einen Einsatz oder eine Kern-Verteidigung, dann steht hier die nächste Welle."),
			TEXT("No wave planned. Start an operation or a core defense at the Core, then the next wave shows here.")), 15, TDUI::Grau), 0.f);
		return;
	}
	// Kopf: Welle, Art, Countdown; darunter Fronten, Zahlen, Tipp
	UHorizontalBox* Kopf = WidgetTree->ConstructWidget<UHorizontalBox>();
	UBorder* Marke = NeuesFeld(TDUI::Akzent, FMargin(10.f, 2.f));
	Marke->AddChild(NeuerText(V.WellenGesamt > 0 ? TDF(TEXT("Welle %d / %d"), TEXT("Wave %d / %d"), V.Welle, V.WellenGesamt) : TDF(TEXT("Welle %d"), TEXT("Wave %d"), V.Welle), 16, TDUI::DunkelText, true));
	if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(Marke))
	{
		S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		S->SetVerticalAlignment(VAlign_Center);
	}
	if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(NeuerText(FString::Printf(TEXT("%s: %s"), *(V.bLaeuft ? TDT(TEXT("Laufende Welle"), TEXT("Current wave")) : V.bAktuell ? TDT(TEXT("Vorschau"), TEXT("Preview")) : TDT(TEXT("Ausblick"), TEXT("Outlook"))), *V.Art), 20, TDUI::Hell, true)))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		S->SetVerticalAlignment(VAlign_Center);
	}
	// Testphase: zur gewaehlten Welle springen (spaeter ueber Balance Test.WellenSprung = 0 abschalten)
	if (!V.bAktuell && !Jetzt.bLaeuft && TestModus())
	{
		UTextBlock* SprungText = nullptr;
		UButton* Sprung = NeuerKnopf(TDF(TEXT("Test: zu Welle %d springen"), TEXT("Test: jump to wave %d"), V.Welle), SprungText);
		Sprung->OnClicked.AddDynamic(this, &UTDKernTerminal::Springen);
		if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(Sprung))
		{
			S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
	}
	mVorschauCountdown = NeuerText(TEXT(""), 16, TDUI::Grau);
	if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(mVorschauCountdown))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	Abschnitt(Kopf, 10.f);
	// Zeitleiste aller Wellen: wo wir stehen, Bosse, neue Arten, neue Fronten; Klick zeigt die Welle
	Abschnitt(NeueZeitleiste(V.Welle), 10.f);
	// TDF nicht in TDF verschachteln (Makro expandiert sich im eigenen Argument nicht) -> Teile vorher bauen
	const int32 MaxGleichzeitig = TDBalance::Ganz(TEXT("Wellen.MaxGleichzeitig"), 60);
	const FString EliteTeil = V.Elite > 0 ? TDF(TEXT(", davon %d Elite"), TEXT(", %d of them Elite"), V.Elite) : FString();
	const FString SchubTeil = V.Gegner > MaxGleichzeitig ? TDF(TEXT(" in Schüben (höchstens %d gleichzeitig)"), TEXT(" in batches (max %d at once)"), MaxGleichzeitig) : FString();
	const FString Zahlen = V.bZusammensetzung
		? TDF(TEXT("Fronten: %s     ·     %d Gegner%s%s     ·     Bedrohung %d Punkte"), TEXT("Fronts: %s     ·     %d enemies%s%s     ·     Threat %d points"),
			V.Fronten.IsEmpty() ? TEXT("-") : *V.Fronten, V.Gegner, *EliteTeil, *SchubTeil, V.Punkte)
		: TDF(TEXT("Fronten: %s     ·     %d Gegner (Stufe zeigt keine Zusammensetzung)"), TEXT("Fronts: %s     ·     %d enemies (difficulty hides the composition)"),
			V.Fronten.IsEmpty() ? TEXT("-") : *V.Fronten, V.Gegner);
	Abschnitt(NeuerText(Zahlen, 13, TDUI::Grau), 4.f);
	if (V.bZusammensetzung)
	{
		Abschnitt(NeuerText(TDT(TEXT("Tipp: "), TEXT("Tip: ")) + V.Tipp, 13, TDUI::Gut), 14.f);
	}
	// Genau ein Steckbrief (spart Platz, Marius 0.44): angeklickte Art, sonst die neue Art, sonst der Boss
	const FTDPlanGruppe* Gezeigt = V.Gruppen.FindByPredicate([this](const FTDPlanGruppe& G) { return AuswahlSchluessel(G) == mAuswahlArt; });
	const bool bGewaehlt = Gezeigt != nullptr;
	if (!Gezeigt)
	{
		Gezeigt = V.Gruppen.FindByPredicate([](const FTDPlanGruppe& G) { return G.bNeu; });
	}
	if (!Gezeigt)
	{
		Gezeigt = V.Gruppen.FindByPredicate([](const FTDPlanGruppe& G) { return G.bBoss; });
	}
	if (Gezeigt)
	{
		const FString Titel = bGewaehlt ? TDT(TEXT("Steckbrief (nochmal anklicken zum Schließen)"), TEXT("Profile (click again to close)"))
			: Gezeigt->bNeu ? TDT(TEXT("Neu in dieser Welle"), TEXT("New this wave")) : TDT(TEXT("Bosskampf"), TEXT("Boss fight"));
		Abschnitt(NeuerText(Titel, 14, Gezeigt->bBoss ? TDUI::Boss : Gezeigt->bNeu ? TDUI::Akzent : TDUI::Hell, true), 6.f);
		Abschnitt(NeuerSteckbrief(*Gezeigt, V.bZusammensetzung), 10.f);
	}
	// Alle Arten als Karten
	Abschnitt(NeuerText(TDT(TEXT("Alle Gegner (anklicken für den Steckbrief)"), TEXT("All enemies (click for the profile)")), 14, TDUI::Hell, true), 8.f);
	UWrapBox* Raster = WidgetTree->ConstructWidget<UWrapBox>();
	Raster->SetInnerSlotPadding(FVector2D(10.f, 10.f));
	for (const FTDPlanGruppe& G : V.Gruppen)
	{
		Raster->AddChildToWrapBox(NeueGegnerKarte(G, V.bZusammensetzung));
	}
	Abschnitt(Raster, 0.f);
	Aktualisiere();
}

int32 UTDKernTerminal::KorbSumme() const
{
	int32 Summe = 0;
	for (const FTDLadenZeile& Z : mLadenZeilen)
	{
		for (int32 i = 0; i < mKorb.FindRef(Z.Key); ++i)
		{
			Summe += Z.PreisNach(i);
		}
	}
	return Summe;
}

void UTDKernTerminal::BaueListen()
{
	UWorld* World = GetWorld();
	UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	if (!Einsatz || !mLaden)
	{
		return;
	}
	mLaden->ClearChildren();
	mKontingent->ClearChildren();
	mKorbListe->ClearChildren();
	mKaufKnoepfe.Reset();
	if (ATDDefenseCore* K = Kern())
	{
		if (Waves)
		{
			const TArray<FString> Fronten = Waves->GetFrontenText(K);
			mFronten->SetText(FText::FromString(FString::Join(Fronten, TEXT("   "))));
		}
	}
	const bool bWelle = Waves && Waves->LaeuftWelle();
	auto Fuellen = [](UHorizontalBox* Reihe, UWidget* W)
	{
		if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(W))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
		}
	};
	auto Mittig = [](UHorizontalBox* Reihe, UWidget* W, float Links)
	{
		if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(W))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(Links, 0.f, 0.f, 0.f));
		}
	};
	// Karte wie die weissen Felder der Maschinenfenster
	auto NeueKarte = [this](UVerticalBox* Liste, const FLinearColor& Farbe, UHorizontalBox*& Reihe)
	{
		UBorder* Karte = NeuesFeld(Farbe, FMargin(8.f, 6.f, 10.f, 6.f));
		Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
		Karte->AddChild(Reihe);
		if (UVerticalBoxSlot* S = Liste->AddChildToVerticalBox(Karte))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
	};
	auto PlusMinus = [this, bWelle](const FString& Key, int32 Schritt, bool bErlaubt) -> UButton*
	{
		UTextBlock* T = nullptr;
		UButton* B = NeuerKnopf(Schritt > 0 ? TEXT("+") : TEXT("-"), T, Schritt > 0);
		B->SetIsEnabled(bErlaubt && !bWelle);
		UTDKaufKnopf* Ziel = NewObject<UTDKaufKnopf>(this);
		Ziel->Key = Key;
		Ziel->Schritt = Schritt;
		Ziel->Menue = this;
		B->OnClicked.AddDynamic(Ziel, &UTDKaufKnopf::Geklickt);
		mKaufKnoepfe.Add(Ziel);
		return B;
	};

	if (!Einsatz->IstAktiv())
	{
		mLadenZeilen.Reset();
		mKorb.Reset();
		UHorizontalBox* Reihe = nullptr;
		NeueKarte(mLaden, TDUI::ZeileA, Reihe);
		Fuellen(Reihe, NeuerText(TDT(TEXT("Erst einen Einsatz starten."), TEXT("Start an operation first.")), 13, TDUI::Grau));
		mKorbSumme->SetText(TDTX(TEXT("Summe: 0 Coupons"), TEXT("Total: 0 Coupons")));
		mKorbRest->SetText(FText::GetEmpty());
		mBestaetigen->SetIsEnabled(false);
	}
	else
	{
		mLadenZeilen = Einsatz->LadenZeilen();
		const int32 Konto = Einsatz->Konto();
		const int32 Summe = KorbSumme();
		// 0.9.6 (Marius 11.11.): Abschnitte je Kategorie wie in der Rezeptauswahl (grauer Balken, rechts - zum Einklappen);
		// noch nicht Erforschtes wird nicht gezeigt, nur unten gezaehlt
		int32 Gesperrt = 0;
		for (const FTDLadenZeile& Z : mLadenZeilen)
		{
			Gesperrt += Z.bVerfuegbar ? 0 : 1;
		}
		for (int32 Kat = 0; Kat < FTDLadenZeile::Kategorien; ++Kat)
		{
		int32 InKat = 0, KorbKat = 0;
		for (const FTDLadenZeile& Z : mLadenZeilen)
		{
			if (Z.Kategorie == Kat && Z.bVerfuegbar)
			{
				++InKat;
				KorbKat += mKorb.FindRef(Z.Key);
			}
		}
		if (InKat == 0)
		{
			continue;
		}
		const bool bZu = mLadenZu.Contains(Kat);
		{
			UButton* Kopf = WidgetTree->ConstructWidget<UButton>();
			FButtonStyle Stil;
			Stil.SetNormal(FSlateRoundedBoxBrush(TDUI::C(90, 90, 90), 3.f));
			Stil.SetHovered(FSlateRoundedBoxBrush(TDUI::C(110, 110, 110), 3.f));
			Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::C(75, 75, 75), 3.f));
			Stil.SetNormalPadding(FMargin(10.f, 5.f));
			Stil.SetPressedPadding(FMargin(10.f, 6.f, 10.f, 4.f));
			Kopf->SetStyle(Stil);
			UHorizontalBox* KopfReihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			if (UButtonSlot* KopfSlot = Cast<UButtonSlot>(Kopf->AddChild(KopfReihe)))
			{
				KopfSlot->SetHorizontalAlignment(HAlign_Fill);   // Titel links, - rechts (sonst mittig)
			}
			Fuellen(KopfReihe, NeuerText(FString::Printf(TEXT("%s (%d)"), *FTDLadenZeile::KategorieName(Kat), InKat), 15, TDUI::Hell, true));
			if (KorbKat > 0)
			{
				Mittig(KopfReihe, NeuerText(TDF(TEXT("%d gewählt"), TEXT("%d selected"), KorbKat), 13, TDUI::Akzent, true), 8.f);
			}
			Mittig(KopfReihe, NeuerText(bZu ? TEXT("+") : TEXT("-"), 18, TDUI::Hell, true), 12.f);
			UTDKaufKnopf* Ziel = NewObject<UTDKaufKnopf>(this);
			Ziel->Key = FString::FromInt(Kat);
			Ziel->Schritt = 0;   // 0 = Abschnitt auf/zu
			Ziel->Menue = this;
			Kopf->OnClicked.AddDynamic(Ziel, &UTDKaufKnopf::Geklickt);
			mKaufKnoepfe.Add(Ziel);
			if (UVerticalBoxSlot* S = mLaden->AddChildToVerticalBox(Kopf))
			{
				S->SetPadding(FMargin(0.f, Kat == 0 ? 0.f : 6.f, 0.f, 4.f));
			}
		}
		if (bZu)
		{
			continue;
		}
		for (const FTDLadenZeile& Z : mLadenZeilen)
		{
			if (Z.Kategorie != Kat || !Z.bVerfuegbar)
			{
				continue;
			}
			const int32 ImKorb = mKorb.FindRef(Z.Key);
			UHorizontalBox* Reihe = nullptr;
			NeueKarte(mLaden, Z.bVerfuegbar ? TDUI::Karte : TDUI::ZeileB, Reihe);
			Mittig(Reihe, NeueKachel(Z.Symbol, 52.f, Z.bVerfuegbar ? FString::Printf(TEXT("+%d"), Z.Paket) : FString()), 0.f);
			UVerticalBox* Texte = WidgetTree->ConstructWidget<UVerticalBox>();
			Texte->AddChildToVerticalBox(NeuerText(UTDEinsatzSubsystem::Anzeige(Z.Key), 15, Z.bVerfuegbar ? TDUI::KarteText : TDUI::Grau, true));
			if (Z.bVerfuegbar)
			{
				UHorizontalBox* Unter = WidgetTree->ConstructWidget<UHorizontalBox>();
				Unter->AddChildToHorizontalBox(NeuerText(FString::Printf(TEXT("%d Coupons"), Z.PreisNach(ImKorb)), 13, TDUI::KarteOrange, true));
				const FString Einheit = !Z.Einheit.IsEmpty() ? Z.Einheit : Z.Paket == 1 ? TDT(TEXT("Gebäude"), TEXT("building")) : TDT(TEXT("Stück"), TEXT("pcs"));
				Unter->AddChildToHorizontalBox(NeuerText(Z.Max > 0
					? TDF(TEXT("  für +%d %s (%d/%d)"), TEXT("  for +%d %s (%d/%d)"), Z.Paket, *Einheit, Z.Gekauft, Z.Max)
					: TDF(TEXT("  für +%d %s"), TEXT("  for +%d %s"), Z.Paket, *Einheit), 13, TDUI::KarteGrau));
				Texte->AddChildToVerticalBox(Unter);
			}
			else
			{
				Texte->AddChildToVerticalBox(NeuerText(TDT(TEXT("im MAM erforschen"), TEXT("research in the MAM")), 13, TDUI::Grau));
			}
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Texte))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(12.f, 0.f, 8.f, 0.f));
			}
			if (Z.bVerfuegbar)
			{
				Mittig(Reihe, PlusMinus(Z.Key, -1, ImKorb > 0), 0.f);
				// Anzahl als Plakette wie die Stapelzahl im Inventar
				UBorder* Zahl = NeuesFeld(ImKorb > 0 ? TDUI::Akzent : TDUI::C(205, 205, 205), FMargin(0.f));
				USizeBox* ZahlGroesse = WidgetTree->ConstructWidget<USizeBox>();
				ZahlGroesse->SetWidthOverride(40.f);
				ZahlGroesse->SetHeightOverride(30.f);
				UTextBlock* ZahlText = NeuerText(FString::FromInt(ImKorb), 15, ImKorb > 0 ? TDUI::Hell : TDUI::KarteGrau, true);
				ZahlText->SetJustification(ETextJustify::Center);
				Zahl->SetVerticalAlignment(VAlign_Center);
				Zahl->AddChild(ZahlText);
				ZahlGroesse->AddChild(Zahl);
				Mittig(Reihe, ZahlGroesse, 6.f);
				Mittig(Reihe, PlusMinus(Z.Key, +1, Summe + Z.PreisNach(ImKorb) <= Konto && (Z.Max <= 0 || Z.Gekauft + ImKorb < Z.Max)), 6.f);
			}
		}
		}
		if (Gesperrt > 0)
		{
			UTextBlock* Rest = NeuerText(TDF(TEXT("%d weitere im MAM erforschbar"), TEXT("%d more can be researched in the MAM"), Gesperrt), 13, TDUI::Grau);
			if (UVerticalBoxSlot* S = mLaden->AddChildToVerticalBox(Rest))
			{
				S->SetPadding(FMargin(4.f, 6.f, 0.f, 0.f));
			}
		}

		// Auswahl: was gewaehlt ist, Einzelpreise, Summe, Rest
		for (const FTDLadenZeile& Z : mLadenZeilen)
		{
			const int32 Anzahl = mKorb.FindRef(Z.Key);
			if (Anzahl <= 0)
			{
				continue;
			}
			TArray<FString> Preise;
			int32 Posten = 0;
			for (int32 i = 0; i < Anzahl; ++i)
			{
				Preise.Add(FString::FromInt(Z.PreisNach(i)));
				Posten += Z.PreisNach(i);
			}
			UHorizontalBox* Reihe = nullptr;
			NeueKarte(mKorbListe, TDUI::Karte, Reihe);
			Mittig(Reihe, NeueKachel(Z.Symbol, 44.f, FString::Printf(TEXT("%d"), Anzahl * Z.Paket)), 0.f);
			UVerticalBox* Texte = WidgetTree->ConstructWidget<UVerticalBox>();
			Texte->AddChildToVerticalBox(NeuerText(FString::Printf(TEXT("%d x %s"), Anzahl, *UTDEinsatzSubsystem::Anzeige(Z.Key)), 14, TDUI::KarteText, true));
			Texte->AddChildToVerticalBox(NeuerText(TDF(TEXT("+%d Kontingent, %s Coupons"), TEXT("+%d quota, %s Coupons"), Anzahl * Z.Paket, *FString::Join(Preise, TEXT(" + "))), 12, TDUI::KarteGrau));
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Texte))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetPadding(FMargin(10.f, 0.f, 6.f, 0.f));
			}
			Mittig(Reihe, NeuerText(FString::Printf(TEXT("%d Coupons"), Posten), 15, TDUI::KarteOrange, true), 0.f);
		}
		if (mKorb.Num() == 0)
		{
			UHorizontalBox* Reihe = nullptr;
			NeueKarte(mKorbListe, TDUI::ZeileA, Reihe);
			UTextBlock* Hinweis = NeuerText(TDT(TEXT("Mit + im Laden auswählen. Gekauft wird erst mit \"Bestätigen\"."), TEXT("Select with + in the shop. Nothing is bought until \"Confirm\".")), 13, TDUI::Grau);
			Hinweis->SetAutoWrapText(true);
			Fuellen(Reihe, Hinweis);
		}
		mKorbSumme->SetText(TDFX(TEXT("Summe: %d Coupons"), TEXT("Total: %d Coupons"), Summe));
		mKorbRest->SetText(TDFX(TEXT("Konto %d Coupons  ->  danach %d Coupons"), TEXT("Balance %d Coupons  ->  after %d Coupons"), Konto, Konto - Summe));
		mKorbRest->SetColorAndOpacity(FSlateColor(Konto - Summe < 0 ? TDUI::Fehler : TDUI::Gut));
		mBestaetigen->SetIsEnabled(mKorb.Num() > 0 && Summe <= Konto && !bWelle);
	}

	// Kontingent mit Vorschau der Auswahl
	TArray<FTDKontingentZeile> Zeilen = Einsatz->KontingentZeilen();
	for (const TPair<FString, int32>& Posten : mKorb)
	{
		if (!Zeilen.ContainsByPredicate([&](const FTDKontingentZeile& Z) { return Z.Key == Posten.Key; }))
		{
			FTDKontingentZeile Neu;
			Neu.Key = Posten.Key;
			Neu.Grenze = Einsatz->Grenze(Posten.Key);
			Neu.Belegt = Einsatz->Belegt(Posten.Key);
			Neu.Symbol = UTDEinsatzSubsystem::Symbol(Posten.Key);
			Zeilen.Add(Neu);
		}
	}
	for (int32 i = 0; i < Zeilen.Num(); ++i)
	{
		const FTDKontingentZeile& Z = Zeilen[i];
		const int32 Dazu = mKorb.FindRef(Z.Key) * UTDEinsatzSubsystem::Paket(Z.Key);
		UHorizontalBox* Reihe = nullptr;
		NeueKarte(mKontingent, i % 2 ? TDUI::ZeileB : TDUI::ZeileA, Reihe);
		Mittig(Reihe, NeueKachel(Z.Symbol, 40.f, FString()), 0.f);
		if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(NeuerText(UTDEinsatzSubsystem::Anzeige(Z.Key), 14, TDUI::Hell)))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f));
		}
		Mittig(Reihe, NeuerText(FString::Printf(TEXT("%d / %d"), Z.Belegt, Z.Grenze), 14,
			Z.Belegt >= Z.Grenze ? TDUI::Akzent : TDUI::Hell, true), 0.f);
		if (Dazu > 0)
		{
			Mittig(Reihe, NeuerText(FString::Printf(TEXT("->  %d / %d"), Z.Belegt, Z.Grenze + Dazu), 14, TDUI::Gut, true), 10.f);
		}
	}
}

void UTDKernTerminal::Melde(const FString& Text)
{
	if (mMeldung)
	{
		mMeldung->SetText(FText::FromString(Text));
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Kern-Menue: %s"), *Text);
}

void UTDKernTerminal::AendereKorb(const FString& Key, int32 Schritt)
{
	// 0.9.6: Schritt 0 = Laden-Abschnitt (Key = Kategorie) auf- oder zuklappen, auch waehrend der Welle
	if (Schritt == 0)
	{
		const int32 Kat = FCString::Atoi(*Key);
		if (mLadenZu.Contains(Kat))
		{
			mLadenZu.Remove(Kat);
		}
		else
		{
			mLadenZu.Add(Kat);
		}
		BaueListen();
		return;
	}
	const UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (Waves && Waves->LaeuftWelle())
	{
		Melde(TDT(TEXT("Während der Welle ist der Laden geschlossen."), TEXT("The shop is closed during a wave.")));
		return;
	}
	int32& Anzahl = mKorb.FindOrAdd(Key);
	Anzahl = FMath::Max(0, Anzahl + Schritt);
	if (Anzahl == 0)
	{
		mKorb.Remove(Key);
	}
	BaueListen();
}

void UTDKernTerminal::Bestaetigen()
{
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (!Einsatz)
	{
		return;
	}
	const int32 Vorher = Einsatz->Konto();
	Melde(Einsatz->KaufeKorb(mKorb));
	if (Einsatz->Konto() != Vorher)
	{
		mKorb.Reset();
		Einsatz->AktualisiereToDo();
	}
	BaueListen();
	Aktualisiere();
}

void UTDKernTerminal::Verwerfen()
{
	mKorb.Reset();
	Melde(TDT(TEXT("Auswahl verworfen."), TEXT("Selection discarded.")));
	BaueListen();
}

void UTDKernTerminal::WelleStarten()
{
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!Waves || !Waves->LaeuftKernVerteidigung())
	{
		Melde(TDT(TEXT("Es läuft kein Einsatz - zuerst \"Einsatz starten\"."), TEXT("No operation running - first \"Start operation\".")));
		return;
	}
	if (Waves->NaechsteWelleJetzt())
	{
		Melde(TDT(TEXT("Welle gestartet."), TEXT("Wave started.")));
		Schliessen();   // ab zur Verteidigung
	}
	else
	{
		Melde(TDT(TEXT("Die Welle läuft bereits."), TEXT("The wave is already running.")));
	}
}

void UTDKernTerminal::EndeWiederholen()
{
	if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
	{
		Melde(Einsatz->WiederholeWelle(GetOwningPlayer()));
		Schliessen();
	}
}

void UTDKernTerminal::EndeNeustart()
{
	if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
	{
		APlayerController* PC = GetOwningPlayer();
		Melde(Einsatz->StarteNeu(PC ? PC->GetPawn() : nullptr));
		mKorb.Reset();
		BaueListen();
		Aktualisiere();
	}
}

void UTDKernTerminal::EndeBeenden()
{
	if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
	{
		Melde(Einsatz->BeendeNachEnde());
		mKorb.Reset();
		BaueListen();
		Aktualisiere();
	}
}

void UTDKernTerminal::WegeUmschalten()
{
	ATDDefenseCore* K = Kern();
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	if (!K || !Waves)
	{
		return;
	}
	if (K->ZeigtWege())
	{
		K->VersteckeWege();
		Melde(TDT(TEXT("Wegvorschau aus."), TEXT("Path preview off.")));
	}
	else
	{
		FString Bericht;
		K->ZeigeWege(Waves->BerechneWege(K, Bericht));
		Melde(Bericht);
	}
	Aktualisiere();
}

void UTDKernTerminal::FreiUmschalten()
{
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (!Einsatz || !Einsatz->IstAktiv())
	{
		Melde(TDT(TEXT("Freies Bauen gibt es nur im Einsatz."), TEXT("Free Build is only available in an operation.")));
		return;
	}
	const bool bAn = !Einsatz->IstFrei();
	if (bAn && UTDLaufProtokoll::bBalancing)
	{
		Melde(TDT(TEXT("Im Balancing-Lauf ist Freies Bauen gesperrt (/td lauf balancing aus)."), TEXT("Free Build is locked in a balancing run (/td lauf balancing aus).")));
		return;
	}
	Einsatz->SetzeFrei(bAn);
	if (bAn)
	{
		if (UTDLaufProtokoll* Lauf = UTDLaufProtokoll::Get(this))
		{
			Lauf->Eingriff(TEXT("Freies Bauen"));
		}
	}
	Melde(bAn ? TDT(TEXT("Freies Bauen AN: keine Kontingente, keine Bausperre. Forschung bleibt wie sie ist."), TEXT("Free Build ON: no quotas, no build lock. Research stays as it is."))
		: TDT(TEXT("Freies Bauen aus: Kontingente gelten wieder."), TEXT("Free Build off: quotas apply again.")));
	Aktualisiere();
}

void UTDKernTerminal::EinsatzStarten()
{
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	APlayerController* PC = GetOwningPlayer();
	if (!Einsatz)
	{
		return;
	}
	TArray<FString> Meldungen;
	static const int32 Wellen[] = { 20, 30, 50, 0 };
	Einsatz->Starte(Kern(), 1, Wellen[FMath::Clamp(mLaenge, 0, 3)], PC ? PC->GetPawn() : nullptr, Meldungen);
	Melde(FString::Join(Meldungen, TEXT("\n")));
	BaueListen();
	Aktualisiere();
}

void UTDKernTerminal::LaengeWechseln()
{
	mLaenge = (mLaenge + 1) % 4;
	Aktualisiere();
}

void UTDKernTerminal::Schliessen()
{
	OnEscapePressed();
}

// ---------------- Turm-Werte ----------------

namespace TDTurmInfo
{
	// Weisses Ausgabe-Feld im Maschinenfenster, Anteil an der Fenstergroesse (gemessen an den Screenshots 0.21/0.22)
	static const FVector2D FeldVon(0.41f, 0.134f);
	static const FVector2D FeldBis(0.65f, 0.516f);
	// 0.78.5 (Marius 05.11.: die Fenster fuer Energiesplitter und Somersloop lagen unter Zielwahl/Bevorzugt): beide Karten in einem
	// eigenen Seitenfeld links neben dem Maschinenfenster, x negativ = links vom Fenster. Kopfleiste oben buendig mit dem Fenster.
	static const FVector2D SeiteVon(-0.400f, 0.000f);
	static const FVector2D SeiteBis(-0.012f, 0.800f);
	static const FVector2D LinksVon(-0.385f, 0.075f);
	static const FVector2D LinksBis(-0.027f, 0.420f);
	static const FVector2D RechtsVon(-0.385f, 0.440f);
	static const FVector2D RechtsBis(-0.027f, 0.785f);
}

void UTDZielKnopf::Geklickt()
{
	if (UTDTurmInfo* I = Info.Get())
	{
		I->WaehleZiel(Modus, Familie, bFamilie);
	}
}

void UTDUpgradeKnopf::Geklickt()
{
	if (UTDTurmInfo* I = Info.Get())
	{
		I->KaufeUpgrade(Art);
	}
}

void UTDTurmInfo::KaufeUpgrade(int32 Art)
{
	if (ATDTurretBase* Turm = mTurm.Get(); Turm && Art == 20)
	{
		// 0.9.8 P4: Knopf Uebersteuern
		UTDPionier* Pionier = UTDPionier::Get(Turm);
		mUpgradeMeldung = Pionier ? Pionier->Uebersteuere(Turm) : FString();
		mUpgradeMeldungZeit = 6.f;
		Fuelle();
		return;
	}
	if (ATDTurretBase* Turm = mTurm.Get())
	{
		mUpgradeMeldung = Turm->KaufeLevel();
		mUpgradeMeldungZeit = 6.f;
		Fuelle();
	}
}

void UTDTurmInfo::FuelleUpgrades()
{
	const ATDTurretBase* Turm = mTurm.Get();
	if (!Turm || !mUpgradeReihe || !mUpgradeHinweis)
	{
		return;
	}
	if (!Turm->HatUpgrades())
	{
		mUpgradeReihe->SetVisibility(ESlateVisibility::Collapsed);
		mUpgradeHinweis->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	// Turmlevel (0.63): Kaestchen = erreichte Level, daneben was das naechste bringt, Knopf mit Preis
	const UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	const int32 Konto = Einsatz ? Einsatz->Konto() : 0;
	const int32 Level = Turm->GetLevel();
	const int32 Preis = Turm->LevelPreis();
	const FString Sperre = Turm->LevelSperre();
	const bool bMax = Level >= Turm->MaxLevel();   // 0.9.6: Drohnenplatz hat nur 3 Level
	mUpgradeReihe->SetVisibility(ESlateVisibility::Visible);
	if (mUpgradeTexte.IsValidIndex(0) && mUpgradeKnoepfe.IsValidIndex(0))
	{
		mUpgradeTexte[0]->SetText(FText::FromString(bMax ? FString(TEXT("max")) : Sperre.IsEmpty() ? FString::Printf(TEXT("%d Coupons"), Preis) : TDT(TEXT("gesperrt"), TEXT("locked"))));
		mUpgradeKnoepfe[0]->SetIsEnabled(!bMax && Sperre.IsEmpty() && Preis > 0 && Preis <= Konto);
	}
	for (int32 k = 0; k < mUpgradeStufenMax; ++k)
	{
		if (UBorder* Kaestchen = mUpgradeStufen.IsValidIndex(k) ? mUpgradeStufen[k].Get() : nullptr)
		{
			Kaestchen->SetVisibility(k < Turm->MaxLevel() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			Kaestchen->SetBrush(k < Level ? FSlateRoundedBoxBrush(TDUI::KarteOrange, 2.f)
				: FSlateRoundedBoxBrush(TDUI::C(255, 255, 255), 2.f, TDUI::C(185, 185, 185), 1.f));
		}
	}
	if (mUpgradeWirkung.IsValidIndex(0))
	{
		// Zeile ist schmal (Test 0.64: Vorschau abgeschnitten) -> nur das naechste Level, die Wirkung steht in der Hinweiszeile
		mUpgradeWirkung[0]->SetText(FText::GetEmpty());   // Test 0.64.2: "-> n" ueberlagerte den Knopf, Vorschau steht in der Hinweiszeile
	}
	// Test 0.64.2: "Level 2: ..." las sich wie das aktuelle Level -> ausdruecklich "Jetzt Level n, naechstes ..."
	const FString Naechstes = bMax ? TDF(TEXT("Jetzt Level %d (höchstes Level)"), TEXT("Now level %d (max level)"), Level)
		: TDF(TEXT("Jetzt Level %d. Upgrade auf %d: %s"), TEXT("Now level %d. Upgrade to %d: %s"), Level, Level + 1, *Turm->LevelText(Level + 1));
	const FString Hinweis = mUpgradeMeldungZeit > 0.f ? mUpgradeMeldung
		: Sperre.IsEmpty() || bMax ? TDF(TEXT("%s · Konto %d Coupons"), TEXT("%s · Balance %d Coupons"), *Naechstes, Konto)
		: FString::Printf(TEXT("%s · %s"), *Naechstes, *Sperre);
	mUpgradeHinweis->SetText(FText::FromString(Hinweis));
	mUpgradeHinweis->SetVisibility(ESlateVisibility::Visible);
}

void UTDTurmInfo::WaehleZiel(int32 Modus, const FString& Familie, bool bFamilie)
{
	if (ATDTurretBase* Turm = mTurm.Get())
	{
		Turm->SetzeZielwahl(bFamilie ? Turm->GetZielModus() : Modus, bFamilie ? Familie : Turm->GetZielFamilie());
		FuelleZielwahl();
	}
}

UBorder* UTDTurmInfo::NeuesZielPanel(UPanelWidget* Flaeche, const TCHAR* Titel, UPanelWidget*& OutInhalt)
{
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrush(FSlateRoundedBoxBrush(TDUI::Karte, 2.f));
	Panel->SetPadding(FMargin(10.f, 6.f));
	Panel->SetClipping(EWidgetClipping::ClipToBounds);
	Panel->SetVisibility(ESlateVisibility::Visible);
	UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->AddChild(Spalte);
	UTextBlock* Ueber = TDUI::Text(WidgetTree, Titel, 12, TDUI::KarteOrange);
	Ueber->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Ueber))
	{
		S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}
	UScrollBox* Rollen = WidgetTree->ConstructWidget<UScrollBox>();
	if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Rollen))
	{
		S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	OutInhalt = Rollen;
	if (UCanvasPanelSlot* S = Cast<UCanvasPanel>(Flaeche)->AddChildToCanvas(Panel))
	{
		S->SetAnchors(FAnchors(0.f, 0.f));
		S->SetPosition(FVector2D(-5000.f, -5000.f));
		S->SetSize(FVector2D(200.f, 150.f));
	}
	return Panel;
}

/** Ein Eintrag der Zielwahl: ganze Zeile klickbar, Text mit Auswahlpunkt */
static UButton* ZielZeile(UWidgetTree* Baum, UObject* Besitzer, UTDTurmInfo* Info, int32 Modus, const FString& Familie, bool bFamilie,
	TArray<TObjectPtr<UTextBlock>>& Texte, TArray<TObjectPtr<UTDZielKnopf>>& Knoepfe, TArray<TObjectPtr<UBorder>>& Punkte)
{
	UButton* B = Baum->ConstructWidget<UButton>();
	FButtonStyle Stil;
	Stil.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.f), 2.f));
	Stil.SetHovered(FSlateRoundedBoxBrush(TDUI::C(250, 149, 73, 60), 2.f));
	Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::C(250, 149, 73, 110), 2.f));
	Stil.SetNormalPadding(FMargin(4.f, 2.f));
	Stil.SetPressedPadding(FMargin(4.f, 2.f));
	B->SetStyle(Stil);
	UHorizontalBox* Reihe = Baum->ConstructWidget<UHorizontalBox>();
	B->AddChild(Reihe);
	USizeBox* PunktGroesse = Baum->ConstructWidget<USizeBox>();
	PunktGroesse->SetWidthOverride(12.f);
	PunktGroesse->SetHeightOverride(12.f);
	UBorder* Punkt = Baum->ConstructWidget<UBorder>();
	PunktGroesse->AddChild(Punkt);
	Punkte.Add(Punkt);
	if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(PunktGroesse))
	{
		S->SetVerticalAlignment(VAlign_Center);
		S->SetPadding(FMargin(0.f, 0.f, 7.f, 0.f));
	}
	UTextBlock* T = TDUI::Text(Baum, TEXT(""), 12, TDUI::KarteText);
	if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(T))
	{
		S->SetVerticalAlignment(VAlign_Center);
	}
	Texte.Add(T);
	UTDZielKnopf* K = NewObject<UTDZielKnopf>(Besitzer);
	K->Modus = Modus;
	K->Familie = Familie;
	K->bFamilie = bFamilie;
	K->Info = Info;
	B->OnClicked.AddDynamic(K, &UTDZielKnopf::Geklickt);
	Knoepfe.Add(K);
	return B;
}

void UTDTurmInfo::FuelleZielwahl()
{
	const ATDTurretBase* Turm = mTurm.Get();
	if (!Turm)
	{
		return;
	}
	auto Punkt = [](UBorder* P, bool bAn)
	{
		if (P)
		{
			P->SetBrush(bAn ? FSlateRoundedBoxBrush(TDUI::KarteOrange, 6.f, TDUI::C(190, 100, 30), 1.f)
				: FSlateRoundedBoxBrush(TDUI::C(255, 255, 255), 6.f, TDUI::C(160, 160, 160), 1.5f));
		}
	};
	for (int32 i = 0; i < mModusTexte.Num(); ++i)
	{
		const bool bAn = Turm->GetZielModus() == i;
		mModusTexte[i]->SetText(FText::FromString(ATDTurretBase::ZielModusName(i)));
		mModusTexte[i]->SetColorAndOpacity(bAn ? TDUI::KarteOrange : TDUI::KarteText);
		Punkt(mModusPunkte.IsValidIndex(i) ? mModusPunkte[i].Get() : nullptr, bAn);
	}
	const TArray<FString>& Familien = ATDTurretBase::ZielFamilien();
	for (int32 i = 0; i < mFamilienTexte.Num() && i < Familien.Num(); ++i)
	{
		const bool bAn = Turm->GetZielFamilie() == Familien[i];
		mFamilienTexte[i]->SetText(FText::FromString(ATDTurretBase::FamilienName(Familien[i])));
		mFamilienTexte[i]->SetColorAndOpacity(bAn ? TDUI::KarteOrange : TDUI::KarteText);
		Punkt(mFamilienPunkte.IsValidIndex(i) ? mFamilienPunkte[i].Get() : nullptr, bAn);
	}
	// 0.9.8 P4: Uebersteuern - sichtbar mit Talent, Text zeigt Laufzeit oder Pause
	if (mUeberKnopf && mUeberText)
	{
		const UTDPionier* Pionier = UTDPionier::Get(Turm);
		const bool bKann = Pionier && Pionier->KannUebersteuern() && Turm->HatUpgrades();
		mUeberKnopf->SetVisibility(bKann ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		const float Rest = Turm->UebersteuernRest();
		const float Pause = Turm->UebersteuernPause();
		mUeberText->SetText(FText::FromString(Rest > 0.f ? TDF(TEXT("Übersteuert: noch %.0f s"), TEXT("Overdrive: %.0f s left"), Rest)
			: Pause > 0.f ? TDF(TEXT("Übersteuern: Pause %.0f s"), TEXT("Overdrive: cooldown %.0f s"), Pause)
			: TDT(TEXT("Übersteuern (10 s +50 % Takt)"), TEXT("Overdrive (10 s +50 % clock)"))));
		mUeberKnopf->SetIsEnabled(bKann && Rest <= 0.f && Pause <= 0.f);
	}
}

TSharedRef<SWidget> UTDTurmInfo::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		// gleiche Optik wie das Eingangsfeld daneben: weiss, dunkle Schrift, orange Zahlen
		mPanel = WidgetTree->ConstructWidget<UBorder>();
		mPanel->SetBrush(FSlateRoundedBoxBrush(TDUI::Karte, 2.f));
		mPanel->SetPadding(FMargin(14.f, 10.f));
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		mPanel->AddChild(Spalte);
		UTextBlock* Ueber = TDUI::Text(WidgetTree, TDT(TEXT("Turmwerte:"), TEXT("Turret stats:")), 12, TDUI::KarteOrange);
		Ueber->SetJustification(ETextJustify::Center);
		Spalte->AddChildToVerticalBox(Ueber);
		mKopf = TDUI::Text(WidgetTree, TDT(TEXT("Turm"), TEXT("Turret")), 16, TDUI::KarteText, true);
		mKopf->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(mKopf))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
		// Turm-Upgrades (0.56): drei Knoepfe unter dem Namen, darueber eine Hinweiszeile
		mUpgradeHinweis = TDUI::Text(WidgetTree, TEXT(""), 11, TDUI::KarteGrau);
		mUpgradeHinweis->SetJustification(ETextJustify::Center);
		mUpgradeHinweis->SetAutoWrapText(true);
		Spalte->AddChildToVerticalBox(mUpgradeHinweis);
		UVerticalBox* Upgrades = WidgetTree->ConstructWidget<UVerticalBox>();
		// 0.63: eine Zeile "Level" mit fuenf Kaestchen, Vorschau des naechsten Levels und Kaufknopf
		mUpgradeStufenMax = ATDTurretBase::TurmMaxLevel;
		for (int32 Art = 0; Art < 1; ++Art)
		{
			// Zeile: Name | Stufenkaestchen | Wirkung jetzt -> naechste Stufe | Kaufknopf
			UBorder* Zeile = WidgetTree->ConstructWidget<UBorder>();
			Zeile->SetBrush(FSlateRoundedBoxBrush(TDUI::C(243, 243, 243), 3.f));
			Zeile->SetPadding(FMargin(8.f, 3.f, 3.f, 3.f));
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			Zeile->AddChild(Reihe);
			USizeBox* NamenBreite = WidgetTree->ConstructWidget<USizeBox>();
			NamenBreite->SetWidthOverride(48.f);
			NamenBreite->AddChild(TDUI::Text(WidgetTree, TEXT("Level"), 12, TDUI::KarteText, true));
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(NamenBreite))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			for (int32 k = 0; k < mUpgradeStufenMax; ++k)
			{
				USizeBox* Groesse = WidgetTree->ConstructWidget<USizeBox>();
				Groesse->SetWidthOverride(14.f);
				Groesse->SetHeightOverride(10.f);
				UBorder* Kaestchen = WidgetTree->ConstructWidget<UBorder>();
				Groesse->AddChild(Kaestchen);
				mUpgradeStufen.Add(Kaestchen);
				if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Groesse))
				{
					S->SetVerticalAlignment(VAlign_Center);
					S->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
				}
			}
			UTextBlock* Wirkung = TDUI::Text(WidgetTree, TEXT(""), 11, TDUI::KarteGrau);
			Wirkung->SetAutoWrapText(true);
			mUpgradeWirkung.Add(Wirkung);
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Wirkung))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(6.f, 0.f));
			}
			UButton* Knopf = WidgetTree->ConstructWidget<UButton>();
			FButtonStyle Stil = TDUI::KartenKnopfStil();
			Stil.SetNormalPadding(FMargin(4.f, 3.f));
			Stil.SetPressedPadding(FMargin(4.f, 4.f, 4.f, 2.f));
			Knopf->SetStyle(Stil);
			UTextBlock* Text = TDUI::Text(WidgetTree, TEXT(""), 11, TDUI::Akzent, true);
			Text->SetJustification(ETextJustify::Center);
			Knopf->AddChild(Text);
			UTDUpgradeKnopf* Objekt = NewObject<UTDUpgradeKnopf>(this);
			Objekt->Art = Art;
			Objekt->Info = this;
			Knopf->OnClicked.AddDynamic(Objekt, &UTDUpgradeKnopf::Geklickt);
			USizeBox* KnopfBreite = WidgetTree->ConstructWidget<USizeBox>();
			KnopfBreite->SetWidthOverride(84.f);   // "gesperrt" passte nicht in 62
			KnopfBreite->AddChild(Knopf);
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(KnopfBreite))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UVerticalBoxSlot* S = Upgrades->AddChildToVerticalBox(Zeile))
			{
				S->SetPadding(FMargin(0.f, 1.f));
			}
			mUpgradeKnoepfe.Add(Knopf);
			mUpgradeTexte.Add(Text);
			mUpgradeObjekte.Add(Objekt);
		}
		mUpgradeReihe = Upgrades;
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Upgrades))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
		}
		// Rollbar (Mausrad): das Panel nimmt Treffer an, der Rest der Flaeche laesst sie durch
		UScrollBox* Rollen = WidgetTree->ConstructWidget<UScrollBox>();
		Rollen->SetAlwaysShowScrollbar(true);
		Rollen->SetScrollbarThickness(FVector2D(6.f, 6.f));
		mListe = WidgetTree->ConstructWidget<UVerticalBox>();
		Rollen->AddChild(mListe);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Rollen))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		mPanel->SetClipping(EWidgetClipping::ClipToBounds);
		mPanel->SetVisibility(ESlateVisibility::Visible);
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(mPanel))
		{
			S->SetAnchors(FAnchors(0.f, 0.f));
			S->SetPosition(FVector2D(-5000.f, -5000.f));   // bis zur ersten Platzierung ausserhalb
			S->SetSize(FVector2D(420.f, 480.f));
		}
		// Zielwahl (Marius 03.11.2026) in den beiden fuer Tuerme gesperrten Feldern unten: links Reihenfolge, rechts Vorzug
		UPanelWidget* Inhalt = nullptr;
		// 0.78.5: Seitenfeld wie ein Spielfenster - dunkle Kopfleiste mit Titel, darunter der Fensterkoerper
		mSeite = WidgetTree->ConstructWidget<UBorder>();
		mSeite->SetBrush(FSlateRoundedBoxBrush(TDUI::Fenster, 2.f, TDUI::Kopfleiste, 2.f));
		mSeite->SetPadding(FMargin(0.f));
		{
			UVerticalBox* Aufbau = WidgetTree->ConstructWidget<UVerticalBox>();
			mSeite->AddChild(Aufbau);
			UBorder* Kopf = WidgetTree->ConstructWidget<UBorder>();
			Kopf->SetBrush(FSlateRoundedBoxBrush(TDUI::Kopfleiste, 0.f));
			Kopf->SetPadding(FMargin(14.f, 8.f));
			Kopf->AddChild(TDUI::Text(WidgetTree, TDT(TEXT("Turmsteuerung"), TEXT("Turret Control")), 18, TDUI::Hell));
			Aufbau->AddChildToVerticalBox(Kopf);
			if (UCanvasPanelSlot* S = Cast<UCanvasPanel>(Flaeche)->AddChildToCanvas(mSeite))
			{
				S->SetAnchors(FAnchors(0.f, 0.f));
				S->SetPosition(FVector2D(-5000.f, -5000.f));
				S->SetSize(FVector2D(200.f, 150.f));
			}
		}
		mModusPanel = NeuesZielPanel(Flaeche, *TDT(TEXT("Zielwahl:"), TEXT("Targeting:")), Inhalt);
		UUniformGridPanel* Raster = WidgetTree->ConstructWidget<UUniformGridPanel>();
		Raster->SetSlotPadding(FMargin(2.f));
		Inhalt->AddChild(Raster);
		for (int32 i = 0; i < ATDTurretBase::ZielModusAnzahl; ++i)
		{
			Raster->AddChildToUniformGrid(ZielZeile(WidgetTree, this, this, i, FString(), false, mModusTexte, mZielKnoepfe, mModusPunkte), i, 0);
		}
		mFamilienPanel = NeuesZielPanel(Flaeche, *TDT(TEXT("Bevorzugt:"), TEXT("Preferred:")), Inhalt);
		UVerticalBox* Liste = WidgetTree->ConstructWidget<UVerticalBox>();
		Inhalt->AddChild(Liste);
		for (const FString& Familie : ATDTurretBase::ZielFamilien())
		{
			Liste->AddChildToVerticalBox(ZielZeile(WidgetTree, this, this, -1, Familie, true, mFamilienTexte, mZielKnoepfe, mFamilienPunkte));
		}
		// 0.9.8 P4: Uebersteuern (Techniker 3 / Kommandant 5) unter der bevorzugten Familie
		{
			mUeberKnopf = WidgetTree->ConstructWidget<UButton>();
			mUeberKnopf->SetStyle(TDUI::KartenKnopfStil());
			mUeberText = TDUI::Text(WidgetTree, TDT(TEXT("Übersteuern"), TEXT("Overdrive")), 12, TDUI::Akzent, true);
			mUeberText->SetJustification(ETextJustify::Center);
			mUeberKnopf->AddChild(mUeberText);
			mUeberObjekt = NewObject<UTDUpgradeKnopf>(this);
			mUeberObjekt->Art = 20;
			mUeberObjekt->Info = this;
			mUeberKnopf->OnClicked.AddDynamic(mUeberObjekt, &UTDUpgradeKnopf::Geklickt);
			mUeberKnopf->SetVisibility(ESlateVisibility::Collapsed);
			if (UVerticalBoxSlot* S = Liste->AddChildToVerticalBox(mUeberKnopf))
			{
				S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			}
		}
		// Klicks neben dem Panel gehen an das Maschinenfenster darunter
		Flaeche->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UTDTurmInfo::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!mTurm.IsValid())
	{
		RemoveFromParent();
		return;
	}
	Platziere(MyGeometry);
	// Rezeptauswahl offen: Turmwerte und Zielwahl ausblenden (sie lagen ueber der Rezeptliste)
	mVerdecktTakt -= InDeltaTime;
	if (mVerdecktTakt <= 0.f)
	{
		mVerdecktTakt = 0.2f;
		const bool bVerdeckt = RezeptAuswahlOffen();
		if (bVerdeckt != mVerdeckt)
		{
			mVerdeckt = bVerdeckt;
			for (UWidget* P : { static_cast<UWidget*>(mPanel), static_cast<UWidget*>(mModusPanel), static_cast<UWidget*>(mFamilienPanel) })
			{
				if (P)
				{
					P->SetVisibility(bVerdeckt ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
				}
			}
		}
	}
	mTakt -= InDeltaTime;
	mUpgradeMeldungZeit -= InDeltaTime;
	if (mTakt <= 0.f)
	{
		mTakt = 0.5f;
		Fuelle();
	}
}

bool UTDTurmInfo::RezeptAuswahlOffen()
{
	const UUserWidget* Fenster = mFenster.Get();
	if (!Fenster)
	{
		return false;
	}
	// Rezept-Knoepfe (Widget_RecipeButton u. ae.) im Maschinenfenster, die gerade zu sehen sind
	TArray<FString> Klassen;
	bool bOffen = false;
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (W == Fenster || W == this || W->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || W->GetWorld() != GetWorld())
		{
			continue;
		}
		const FString Klasse = W->GetClass()->GetName();
		if (!Klasse.Contains(TEXT("Recipe")) || !TDImBaumSichtbar(W, Fenster))
		{
			continue;
		}
		Klassen.AddUnique(Klasse);
		if (Klasse.Contains(TEXT("RecipeIcon")) || Klasse.Contains(TEXT("RecipeButton")) || Klasse.Contains(TEXT("RecipeSelect")) || Klasse.Contains(TEXT("RecipeList")))
		{
			bOffen = true;
		}
	}
	// Diagnose: welche Rezept-Widgets sind sichtbar (einmal je Aenderung)
	const FString Liste = FString::Join(Klassen, TEXT(", "));
	if (Liste != mFensterKlassen)
	{
		mFensterKlassen = Liste;
		UE_LOG(LogFicsitTD, Log, TEXT("Turm-Info: sichtbare Rezept-Widgets im Maschinenfenster: %s -> Rezeptauswahl %s"), Liste.IsEmpty() ? TEXT("-") : *Liste, bOffen ? TEXT("offen") : TEXT("zu"));
	}
	return bOffen;
}

void UTDTurmInfo::Platziere(const FGeometry& Eigene)
{
	UCanvasPanelSlot* PanelSlot = mPanel ? Cast<UCanvasPanelSlot>(mPanel->Slot) : nullptr;
	if (!PanelSlot)
	{
		return;
	}
	if (!mFenster.IsValid())
	{
		for (TObjectIterator<UFGInteractWidget> It; It; ++It)
		{
			if (It->mInteractObject == mTurm.Get() && !It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && It->GetCachedWidget().IsValid())
			{
				mFenster = *It;
				break;
			}
		}
	}
	// Bezug: das Maschinenfenster, sonst der ganze Bildschirm
	const FGeometry Bezug = mFenster.IsValid() && mFenster->GetCachedGeometry().GetLocalSize().X > 100.f ? mFenster->GetCachedGeometry() : Eigene;
	const FVector2D Groesse = Bezug.GetLocalSize();
	const FVector2D Von = Eigene.AbsoluteToLocal(Bezug.LocalToAbsolute(Groesse * TDTurmInfo::FeldVon));
	const FVector2D Bis = Eigene.AbsoluteToLocal(Bezug.LocalToAbsolute(Groesse * TDTurmInfo::FeldBis));
	PanelSlot->SetPosition(Von);
	PanelSlot->SetSize(Bis - Von);
	SetzeFeld(mSeite, Eigene, Bezug, TDTurmInfo::SeiteVon, TDTurmInfo::SeiteBis);
	SetzeFeld(mModusPanel, Eigene, Bezug, TDTurmInfo::LinksVon, TDTurmInfo::LinksBis);
	SetzeFeld(mFamilienPanel, Eigene, Bezug, TDTurmInfo::RechtsVon, TDTurmInfo::RechtsBis);
	if (!mGemeldet && Groesse.X > 100.f)
	{
		mGemeldet = true;
		UE_LOG(LogFicsitTD, Log, TEXT("Turm-Info: Fenster %s, Bezug %.0f x %.0f, Bildschirm %.0f x %.0f, Panel bei %.0f/%.0f Groesse %.0f x %.0f"),
			mFenster.IsValid() ? *mFenster->GetClass()->GetName() : TEXT("nicht gefunden"), Groesse.X, Groesse.Y,
			Eigene.GetLocalSize().X, Eigene.GetLocalSize().Y, Von.X, Von.Y, (Bis - Von).X, (Bis - Von).Y);
	}
}

void UTDTurmInfo::SetzeFeld(UWidget* Panel, const FGeometry& Eigene, const FGeometry& Bezug, const FVector2D& Von, const FVector2D& Bis)
{
	UCanvasPanelSlot* S = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
	if (!S)
	{
		return;
	}
	const FVector2D Groesse = Bezug.GetLocalSize();
	const FVector2D A = Eigene.AbsoluteToLocal(Bezug.LocalToAbsolute(Groesse * Von));
	const FVector2D E = Eigene.AbsoluteToLocal(Bezug.LocalToAbsolute(Groesse * Bis));
	S->SetPosition(A);
	S->SetSize(E - A);
}

void UTDTurmInfo::Fuelle()
{
	ATDTurretBase* Turm = mTurm.Get();
	if (!Turm || !mListe)
	{
		return;
	}
	FuelleZielwahl();
	FuelleUpgrades();
	mKopf->SetText(FText::FromString(Turm->GetTurmAnzeige()));
	TArray<FTDKennwert> Werte;
	Turm->Kennwerte(Werte);
	if (Werte.Num() != mWerte.Num())
	{
		mListe->ClearChildren();
		mNamen.Reset();
		mWerte.Reset();
		for (int32 i = 0; i < Werte.Num(); ++i)
		{
			UBorder* Hinter = WidgetTree->ConstructWidget<UBorder>();
			Hinter->SetBrush(FSlateRoundedBoxBrush(i % 2 ? TDUI::C(240, 240, 240) : TDUI::Karte, 0.f));
			Hinter->SetPadding(FMargin(6.f, 2.f));
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			Hinter->AddChild(Reihe);
			USizeBox* NamenBreite = WidgetTree->ConstructWidget<USizeBox>();
			NamenBreite->SetWidthOverride(110.f);
			UTextBlock* N = TDUI::Text(WidgetTree, TEXT(""), 12, TDUI::KarteGrau);
			NamenBreite->AddChild(N);
			UTextBlock* W = TDUI::Text(WidgetTree, TEXT(""), 12, TDUI::KarteText, true);
			W->SetAutoWrapText(true);
			W->SetJustification(ETextJustify::Right);
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(NamenBreite))
			{
				S->SetVerticalAlignment(VAlign_Center);
			}
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(W))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetVerticalAlignment(VAlign_Center);
			}
			mListe->AddChildToVerticalBox(Hinter);
			mNamen.Add(N);
			mWerte.Add(W);
		}
	}
	for (int32 i = 0; i < Werte.Num(); ++i)
	{
		mNamen[i]->SetText(FText::FromString(Werte[i].Name));
		mWerte[i]->SetText(FText::FromString(Werte[i].Wert));
		mWerte[i]->SetColorAndOpacity(FSlateColor(Werte[i].bHervor ? TDUI::KarteOrange : TDUI::KarteText));
	}
}

// ---------------- HUD ----------------

TSharedRef<SWidget> UTDHud::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		UBorder* Rahmen = WidgetTree->ConstructWidget<UBorder>();
		Rahmen->SetBrush(FSlateRoundedBoxBrush(TDUI::C(24, 24, 24, 215), 3.f));
		Rahmen->SetPadding(FMargin(14.f, 7.f));
		mText = TDUI::Text(WidgetTree, TEXT(""), 13, TDUI::Akzent, true);
		Rahmen->AddChild(mText);
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Rahmen))
		{
			// oben links: unten rechts liegen die Tastenhinweise des Spiels (Test 0.24.2)
			S->SetAnchors(FAnchors(0.f, 0.f));
			S->SetAlignment(FVector2D(0.f, 0.f));
			S->SetPosition(FVector2D(24.f, 24.f));
			S->SetAutoSize(true);
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UTDHud::NativeConstruct()
{
	Super::NativeConstruct();
	Aktualisiere();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(mTimer, FTimerDelegate::CreateUObject(this, &UTDHud::Aktualisiere), 1.f, true);
	}
}

void UTDHud::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(mTimer);
	}
	Super::NativeDestruct();
}

void UTDHud::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TDPauseAusblenden(this);
}

void UTDHud::Aktualisiere()
{
	UWorld* World = GetWorld();
	const UTDWaveSubsystem* Waves = World ? World->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	UTDEinsatzSubsystem* Einsatz = World ? World->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
	if (!Waves || !Einsatz || !mText)
	{
		return;
	}
	const ATDDefenseCore* K = Einsatz->GetKern();
	FString Lauf = Waves->GetLaufText();
	if (!Lauf.Split(TEXT(", aus "), &Lauf, nullptr)) { Lauf.Split(TEXT(", from "), &Lauf, nullptr); }
	const FString Welle = Waves->LaeuftWelle() ? Lauf
		: Waves->LaeuftKernVerteidigung() ? TDF(TEXT("Welle %d/%d bereit (Kern-Menü: E)"), TEXT("Wave %d/%d ready (Core Menu: E)"), Waves->GetWelle() + 1, Waves->mWellenGesamt)
		: TDT(TEXT("Kein Lauf"), TEXT("No run"));
	const UTDPionier* Pionier = UTDPionier::Get(this);
	const int32 Frei = Pionier ? Pionier->FreiePunkte() : 0;
	const FString Punkte = Frei <= 0 ? FString() : Frei == 1 ? TDT(TEXT(" (1 Talentpunkt)"), TEXT(" (1 Talent Point)")) : TDF(TEXT(" (%d Talentpunkte)"), TEXT(" (%d Talent Points)"), Frei);
	mText->SetText(TDFX(TEXT("%s  |  Kern %.0f %%  |  %d Coupons  |  %d FP  |  Rang %d%s"), TEXT("%s  |  Core %.0f %%  |  %d Coupons  |  %d RP  |  Rank %d%s"), *Welle,
		K ? 100.f * K->GetTDHealth() / FMath::Max(K->GetTDMaxHealth(), 1.f) : 0.f, Einsatz->Konto(), Einsatz->FPImInventar(),
		Pionier ? Pionier->GetRang() : 1, *Punkte));
}

// ---------------------------------------------------------------------------------------------------------------
// T4 Boss-Lebensbalken
// ---------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UTDBossBalken::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		UBorder* Rahmen = WidgetTree->ConstructWidget<UBorder>();
		Rahmen->SetBrush(FSlateRoundedBoxBrush(TDUI::C(20, 20, 20, 215), 4.f));
		Rahmen->SetPadding(FMargin(16.f, 8.f, 16.f, 10.f));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Rahmen))
		{
			S->SetAnchors(FAnchors(0.5f, 0.f));
			S->SetAlignment(FVector2D(0.5f, 0.f));
			S->SetPosition(FVector2D(0.f, 70.f));
			S->SetSize(FVector2D(640.f, 96.f));   // 0.9.8: Platz fuer die Jaeger-Zeile
		}
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Rahmen->AddChild(Spalte);
		UHorizontalBox* Kopf = WidgetTree->ConstructWidget<UHorizontalBox>();
		mName = TDUI::Text(WidgetTree, TEXT("BOSS"), 18, TDUI::C(240, 120, 110), true);
		if (UHorizontalBoxSlot* S = Kopf->AddChildToHorizontalBox(mName))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		mWerte = TDUI::Text(WidgetTree, TEXT(""), 14, TDUI::Hell);
		Kopf->AddChildToHorizontalBox(mWerte);
		Spalte->AddChildToVerticalBox(Kopf);
		mBalken = WidgetTree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Stil;
		Stil.SetBackgroundImage(FSlateRoundedBoxBrush(TDUI::C(55, 25, 25), 3.f));
		Stil.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
		mBalken->SetWidgetStyle(Stil);
		mBalken->SetFillColorAndOpacity(TDUI::C(225, 60, 45));
		mBalken->SetPercent(1.f);
		USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
		Hoehe->SetHeightOverride(16.f);
		Hoehe->AddChild(mBalken);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Hoehe))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		// 0.9.8 P3 Jaeger 4: Resistenzen und Spieleranteil (nur mit dem Talent sichtbar)
		mJagd = TDUI::Text(WidgetTree, TEXT(""), 13, TDUI::Hell);
		mJagd->SetVisibility(ESlateVisibility::Collapsed);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(mJagd))
		{
			S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UTDBossBalken::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TDPauseAusblenden(this);
	mTakt -= InDeltaTime;
	if (mTakt > 0.f)
	{
		return;
	}
	mTakt = 0.2f;
	const UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	FString Name;
	float LP = 0.f, MaxLP = 0.f;
	int32 Lebend = 0, Gesamt = 0;
	if (!Waves || !Waves->GetBossStatus(Name, LP, MaxLP, Lebend, Gesamt) || !mBalken)
	{
		return;
	}
	mName->SetText(FText::FromString(TEXT("BOSS: ") + Name));
	mWerte->SetText(TDFX(TEXT("%s / %s LP   ·   %d / %d"), TEXT("%s / %s HP   ·   %d / %d"), *FText::AsNumber(FMath::RoundToInt(LP)).ToString(),
		*FText::AsNumber(FMath::RoundToInt(MaxLP)).ToString(), Lebend, Gesamt));
	mBalken->SetPercent(MaxLP > 0.f ? LP / MaxLP : 0.f);
	// 0.9.8 P3 Jaeger 4: Resistenzen des Bosses (nur Abweichungen) und dein Anteil am Bossschaden
	const UTDPionier* Pionier = UTDPionier::Get(this);
	const AFGCreature* Boss = Waves->GetErsterBoss();
	if (mJagd && Pionier && Boss && Pionier->GetStufe(UTDPionier::Jaeger) >= 4)
	{
		static const ETDSchadensart Arten[] = { ETDSchadensart::Durchschlag, ETDSchadensart::Kugel, ETDSchadensart::Feuer, ETDSchadensart::Strom, ETDSchadensart::Explosion, ETDSchadensart::Gift };
		static const TCHAR* NamenEn[] = { TEXT("Piercing"), TEXT("Bullet"), TEXT("Fire"), TEXT("Electric"), TEXT("Explosive"), TEXT("Poison") };
		TArray<FString> Teile;
		for (int32 i = 0; i < UE_ARRAY_COUNT(Arten); ++i)
		{
			const float R = ATDTurretBase::Resistenz(Arten[i], Boss);
			if (!FMath::IsNearlyEqual(R, 1.f, 0.01f))
			{
				Teile.Add(FString::Printf(TEXT("%s ×%s"), TDSprache::Englisch() ? NamenEn[i] : ATDTurretBase::SchadensartName(Arten[i]), *FText::AsNumber(R).ToString()));
			}
		}
		const float Verloren = FMath::Max(MaxLP - LP, 1.f);
		const float Anteil = 100.f * FMath::Min(Pionier->GetWellenBilanz().JagdSchaden / Verloren, 1.f);
		mJagd->SetText(FText::FromString(TDF(TEXT("Resistenzen: %s   ·   Dein Anteil: %.0f %%"), TEXT("Resistances: %s   ·   Your share: %.0f %%"),
			Teile.Num() ? *FString::Join(Teile, TEXT(", ")) : *TDT(TEXT("keine"), TEXT("none")), Anteil)));
		mJagd->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else if (mJagd)
	{
		mJagd->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Seitenleiste: Stand der laufenden Welle
// ---------------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> UTDWellenLeiste::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		UBorder* Rahmen = WidgetTree->ConstructWidget<UBorder>();
		Rahmen->SetBrush(FSlateRoundedBoxBrush(TDUI::C(20, 20, 20, 200), 4.f));
		Rahmen->SetPadding(FMargin(12.f, 10.f));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Rahmen))
		{
			// links, etwas ueber der Mitte: rechts lag sie ueber der ToDo-Liste (Test 0.53), unten links sitzt der Chat
			S->SetAnchors(FAnchors(0.f, 0.4f));
			S->SetAlignment(FVector2D(0.f, 0.5f));
			S->SetPosition(FVector2D(20.f, 0.f));
			S->SetAutoSize(true);
		}
		USizeBox* Breite = WidgetTree->ConstructWidget<USizeBox>();
		Breite->SetWidthOverride(250.f);
		Rahmen->AddChild(Breite);
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Breite->AddChild(Spalte);
		mKopf = TDUI::Text(WidgetTree, TDT(TEXT("Welle"), TEXT("Wave")), 16, TDUI::Akzent, true);
		Spalte->AddChildToVerticalBox(mKopf);
		mBalken = WidgetTree->ConstructWidget<UProgressBar>();
		FProgressBarStyle Stil;
		Stil.SetBackgroundImage(FSlateRoundedBoxBrush(TDUI::C(55, 55, 55), 3.f));
		Stil.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 3.f));
		mBalken->SetWidgetStyle(Stil);
		mBalken->SetFillColorAndOpacity(TDUI::Akzent);
		USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
		Hoehe->SetHeightOverride(10.f);
		Hoehe->AddChild(mBalken);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Hoehe))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 4.f));
		}
		mZahlen = TDUI::Text(WidgetTree, TEXT(""), 12, TDUI::Hell);
		mZahlen->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(mZahlen))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
		mListe = WidgetTree->ConstructWidget<UVerticalBox>();
		Spalte->AddChildToVerticalBox(mListe);
		// 0.66: Schaden je Turmart in dieser Welle
		UTextBlock* SchadenKopf = TDUI::Text(WidgetTree, TDT(TEXT("Schaden dieser Welle"), TEXT("Damage this wave")), 14, TDUI::Akzent, true);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(SchadenKopf))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
		}
		mSchadenListe = WidgetTree->ConstructWidget<UVerticalBox>();
		Spalte->AddChildToVerticalBox(mSchadenListe);
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget();
}

void UTDWellenLeiste::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TDPauseAusblenden(this);
	mTakt -= InDeltaTime;
	if (mTakt <= 0.f)
	{
		mTakt = 0.4f;
		Fuelle();
		FuelleSchaden();
	}
}

void UTDWellenLeiste::FuelleSchaden()
{
	const UTDWellenStatistik* Statistik = UTDWellenStatistik::Get(this);
	if (!Statistik || !mSchadenListe)
	{
		return;
	}
	const TArray<FTDTurmStat> Liste = Statistik->Live();
	float Summe = 0.f, Hoechst = 1.f;
	TArray<FString> Arten;
	for (const FTDTurmStat& T : Liste)
	{
		Summe += T.Schaden;
		Hoechst = FMath::Max(Hoechst, T.Schaden);
		Arten.Add(T.Art);
	}
	if (Arten != mSchadenArten)
	{
		mSchadenArten = Arten;
		mSchadenListe->ClearChildren();
		mSchadenTexte.Reset();
		mSchadenBalken.Reset();
		for (const FTDTurmStat& T : Liste)
		{
			UTextBlock* Text = TDUI::Text(WidgetTree, TEXT(""), 12, TDUI::Hell);
			Text->SetAutoWrapText(true);
			mSchadenListe->AddChildToVerticalBox(Text);
			UProgressBar* Balken = WidgetTree->ConstructWidget<UProgressBar>();
			FProgressBarStyle Stil;
			Stil.SetBackgroundImage(FSlateRoundedBoxBrush(TDUI::C(55, 55, 55), 2.f));
			Stil.SetFillImage(FSlateRoundedBoxBrush(FLinearColor::White, 2.f));
			Balken->SetWidgetStyle(Stil);
			Balken->SetFillColorAndOpacity(UTDWellenStatistik::Farbe(T.Art));
			USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
			Hoehe->SetHeightOverride(6.f);
			Hoehe->AddChild(Balken);
			if (UVerticalBoxSlot* S = mSchadenListe->AddChildToVerticalBox(Hoehe))
			{
				S->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
			}
			mSchadenTexte.Add(Text);
			mSchadenBalken.Add(Balken);
		}
	}
	for (int32 i = 0; i < Liste.Num() && i < mSchadenTexte.Num(); ++i)
	{
		const FTDTurmStat& T = Liste[i];
		mSchadenTexte[i]->SetText(TDFX(TEXT("%s (%d): %s Schaden · %.0f %% · %d Abschüsse"), TEXT("%s (%d): %s damage · %.0f %% · %d kills"), *T.Art, T.Anzahl,
			*FText::AsNumber(FMath::RoundToInt(T.Schaden)).ToString(), Summe > 0.f ? 100.f * T.Schaden / Summe : 0.f, T.Kills));
		mSchadenBalken[i]->SetPercent(T.Schaden / Hoechst);
	}
}

void UTDWellenLeiste::Fuelle()
{
	const UTDWaveSubsystem* Waves = GetWorld() ? GetWorld()->GetSubsystem<UTDWaveSubsystem>() : nullptr;
	const FTDWellenStatus S = Waves ? Waves->GetWellenStatus() : FTDWellenStatus();
	if (!S.bGueltig || !mListe)
	{
		return;
	}
	mKopf->SetText(FText::FromString(S.WellenGesamt > 0 ? TDF(TEXT("Welle %d / %d · %s"), TEXT("Wave %d / %d · %s"), S.Welle, S.WellenGesamt, *S.Art)
		: TDF(TEXT("Welle %d · %s"), TEXT("Wave %d · %s"), S.Welle, *S.Art)));
	mBalken->SetPercent(S.Groesse > 0 ? float(S.Besiegt) / S.Groesse : 0.f);
	mZahlen->SetText(TDFX(TEXT("%d von %d besiegt\n%d unterwegs · %d kommen noch"), TEXT("%d of %d defeated\n%d on the way · %d still coming"), S.Besiegt, S.Groesse, S.Lebend, S.Offen));
	// Zeilen neu bauen, wenn sich die Arten aendern (neue Welle), sonst nur die Zahlen
	TArray<FString> Arten;
	for (const FTDArtStatus& A : S.Arten)
	{
		Arten.Add((A.bBoss ? TEXT("BOSS|") : TEXT("")) + A.Pfad);
	}
	if (Arten != mArten)
	{
		mArten = Arten;
		mListe->ClearChildren();
		mZeilen.Reset();
		for (const FTDArtStatus& A : S.Arten)
		{
			const FTDKreaturSteckbrief B = UTDWaveSubsystem::Steckbrief(A.Pfad);
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			USizeBox* Rahmen = WidgetTree->ConstructWidget<USizeBox>();
			Rahmen->SetWidthOverride(32.f);
			Rahmen->SetHeightOverride(32.f);
			if (UTexture2D* Bild = KreaturTextur(B.Deskriptor))
			{
				UImage* Symbol = WidgetTree->ConstructWidget<UImage>();
				Symbol->SetBrushFromTexture(Bild, false);
				Symbol->SetDesiredSizeOverride(FVector2D(32.f, 32.f));
				Rahmen->AddChild(Symbol);
			}
			if (UHorizontalBoxSlot* Sl = Reihe->AddChildToHorizontalBox(Rahmen))
			{
				Sl->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
				Sl->SetVerticalAlignment(VAlign_Center);
			}
			UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
			Text->AddChildToVerticalBox(TDUI::Text(WidgetTree, A.bBoss ? TEXT("BOSS: ") + B.Name : B.Name, 13, A.bBoss ? TDUI::Boss : TDUI::Hell, true));
			UTextBlock* Zahl = TDUI::Text(WidgetTree, TEXT(""), 12, TDUI::Grau);
			Zahl->SetAutoWrapText(true);   // 0.66.1: lief aus der Box
			Text->AddChildToVerticalBox(Zahl);
			mZeilen.Add(Zahl);
			if (UHorizontalBoxSlot* Sl = Reihe->AddChildToHorizontalBox(Text))
			{
				Sl->SetVerticalAlignment(VAlign_Center);
				Sl->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			if (UVerticalBoxSlot* Sl = mListe->AddChildToVerticalBox(Reihe))
			{
				Sl->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
			}
		}
	}
	for (int32 i = 0; i < S.Arten.Num() && i < mZeilen.Num(); ++i)
	{
		const FTDArtStatus& A = S.Arten[i];
		const int32 Weg = A.Gesamt - A.Lebend - A.Offen;
		mZeilen[i]->SetText(FText::FromString(A.Lebend + A.Offen == 0 ? TDF(TEXT("alle %d besiegt"), TEXT("all %d defeated"), A.Gesamt)
			: TDF(TEXT("%d unterwegs · %d kommen · %d / %d besiegt"), TEXT("%d on the way · %d coming · %d / %d defeated"), A.Lebend, A.Offen, Weg, A.Gesamt)));
		mZeilen[i]->SetColorAndOpacity(FSlateColor(A.Lebend + A.Offen == 0 ? TDUI::Gut : TDUI::Grau));
	}
}

void UTDKernTerminal::BaueStatistik()
{
	if (!mStatistikInhalt)
	{
		return;
	}
	mStatistikInhalt->ClearChildren();
	const UTDWellenStatistik* Statistik = UTDWellenStatistik::Get(this);
	const TArray<FTDWellenStat> Leer;
	const TArray<FTDWellenStat>& Verlauf = Statistik ? Statistik->Verlauf() : Leer;
	// 0.66.1: das Spiel-Font hat kein Pfeilzeichen (Test 0.66: Fragezeichen-Raute) -> nur ASCII, Umlaute, Mittelpunkt und Durchmesser
	auto Abschnitt = [&](UWidget* W, float Unten)
	{
		if (UVerticalBoxSlot* S = mStatistikInhalt->AddChildToVerticalBox(W))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, Unten));
		}
	};
	auto Zahl = [](float Wert) { return FText::AsNumber(FMath::RoundToInt(Wert)).ToString(); };
	auto Spalte = [&](UHorizontalBox* Reihe, const FString& Text, float Breite, const FLinearColor& Farbe, bool bFett, bool bRechts)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Breite);
		UTextBlock* T = NeuerText(Text, 13, Farbe, bFett);
		T->SetJustification(bRechts ? ETextJustify::Right : ETextJustify::Left);
		T->SetClipping(EWidgetClipping::ClipToBounds);
		Box->AddChild(T);
		if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Box))
		{
			S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
	};
	// Balken auf einer Bahn: Ebenen von hinten nach vorn (Breite 0..1 der Bahn, Farbe, Hoehe)
	struct FEbene { float Anteil; FLinearColor Farbe; float Hoehe; };
	const float Bahn = 700.f;
	auto Balken = [&](UHorizontalBox* Reihe, const TArray<FEbene>& Ebenen)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Bahn);
		Box->SetHeightOverride(18.f);
		UOverlay* Lagen = WidgetTree->ConstructWidget<UOverlay>();
		Box->AddChild(Lagen);
		for (const FEbene& E : Ebenen)
		{
			if (E.Anteil * Bahn < 1.f)
			{
				continue;
			}
			USizeBox* Teil = WidgetTree->ConstructWidget<USizeBox>();
			Teil->SetWidthOverride(FMath::Min(1.f, E.Anteil) * Bahn);
			Teil->SetHeightOverride(E.Hoehe);
			Teil->AddChild(NeuesFeld(E.Farbe, FMargin(0.f)));
			if (UOverlaySlot* S = Lagen->AddChildToOverlay(Teil))
			{
				S->SetHorizontalAlignment(HAlign_Left);
				S->SetVerticalAlignment(VAlign_Center);
			}
		}
		if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(Box))
		{
			S->SetVerticalAlignment(VAlign_Center);
			S->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));
		}
	};
	auto Ueberschrift = [&](const FString& Titel, const FString& Erklaerung)
	{
		if (UVerticalBoxSlot* S = mStatistikInhalt->AddChildToVerticalBox(NeuerText(Titel, 17, TDUI::Akzent, true)))
		{
			S->SetPadding(FMargin(0.f, 12.f, 0.f, 2.f));
		}
		Abschnitt(NeuerText(Erklaerung, 13, TDUI::Grau), 6.f);
	};
	auto Legende = [&](const TArray<TPair<FString, FLinearColor>>& Eintraege)
	{
		UHorizontalBox* Zeile = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (const TPair<FString, FLinearColor>& E : Eintraege)
		{
			if (UHorizontalBoxSlot* S = Zeile->AddChildToHorizontalBox(NeuesFeld(E.Value, FMargin(6.f))))
			{
				S->SetVerticalAlignment(VAlign_Center);
				S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
			}
			if (UHorizontalBoxSlot* S = Zeile->AddChildToHorizontalBox(NeuerText(E.Key, 13, TDUI::Hell)))
			{
				S->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
			}
		}
		Abschnitt(Zeile, 6.f);
	};

	Abschnitt(NeuerText(TDT(TEXT("Statistik je Welle"), TEXT("Statistics per wave")), 20, TDUI::Akzent, true), 4.f);
	if (Verlauf.Num() == 0)
	{
		Abschnitt(NeuerText(TDT(TEXT("Noch keine abgeschlossene Welle in diesem Einsatz. Nach jeder Welle steht hier, welche Turmart wie viel Schaden gemacht hat, wie es um Munition und Strom stand und wie viel der Kern verloren hat."),
			TEXT("No completed wave in this operation yet. After each wave this shows how much damage each turret type dealt, how ammo and power held up and how much the Core lost.")), 14, TDUI::Grau), 0.f);
		return;
	}

	// ---------- 1. Schaden ----------
	TArray<FString> Arten;
	float HoechstSchaden = 1.f, HoechstStrom = 1.f, HoechstMunition = 1.f;
	for (const FTDWellenStat& W : Verlauf)
	{
		float Summe = 0.f;
		for (const FTDTurmStat& T : W.Tuerme)
		{
			Arten.AddUnique(T.Art);
			Summe += T.Schaden;
			HoechstMunition = FMath::Max3(HoechstMunition, float(T.Schuesse), T.ProduktionMin * W.Dauer / 60.f);
			HoechstMunition = FMath::Max(HoechstMunition, float(FMath::Max(T.VorratStart, T.VorratEnde)));
		}
		HoechstSchaden = FMath::Max(HoechstSchaden, Summe);
		HoechstStrom = FMath::Max3(HoechstStrom, W.StromKapazitaet, W.StromSpitze);
	}
	Ueberschrift(TDT(TEXT("Schaden"), TEXT("Damage")), TDT(TEXT("Wirksamer Schaden je Turmart (ohne Overkill), rechts der Kern vor und nach der Welle."),
		TEXT("Effective damage per turret type (without overkill), on the right the Core before and after the wave.")));
	TArray<TPair<FString, FLinearColor>> ArtFarben;
	for (const FString& Art : Arten)
	{
		ArtFarben.Add({ Art, UTDWellenStatistik::Farbe(Art) });
	}
	Legende(ArtFarben);
	for (const FTDWellenStat& W : Verlauf)
	{
		UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
		Spalte(Reihe, TDF(TEXT("Welle %d"), TEXT("Wave %d"), W.Welle), 80.f, TDUI::Hell, true, false);
		// gestapelt: jede Ebene reicht bis zur Summe der Arten davor plus ihrer eigenen
		TArray<FEbene> Ebenen;
		float Summe = 0.f;
		for (const FTDTurmStat& T : W.Tuerme)
		{
			Summe += T.Schaden;
		}
		float Bis = Summe;
		for (const FTDTurmStat& T : W.Tuerme)
		{
			Ebenen.Add({ Bis / HoechstSchaden, UTDWellenStatistik::Farbe(T.Art), 16.f });
			Bis -= T.Schaden;
		}
		Balken(Reihe, Ebenen);
		const float Verlust = W.KernStart - W.KernEnde;
		Spalte(Reihe, Zahl(Summe), 70.f, TDUI::Hell, false, true);
		Spalte(Reihe, TDF(TEXT("Kern %.0f %% auf %.0f %%"), TEXT("Core %.0f %% to %.0f %%"), W.KernStart, W.KernEnde), 200.f,
			Verlust >= 15.f ? TDUI::Fehler : Verlust >= 5.f ? TDUI::Akzent : TDUI::Gut, false, false);
		Abschnitt(Reihe, 4.f);
	}

	// ---------- 2. Energie ----------
	const FLinearColor KapFarbe = TDUI::C(70, 70, 70), SpitzeFarbe = TDUI::C(110, 170, 250), MittelFarbe = TDUI::C(60, 110, 190);
	Ueberschrift(TDT(TEXT("Energie"), TEXT("Power")), TDT(TEXT("Kapazität aller Kraftwerke (grau), höchster Verbrauch in der Welle (hellblau) und Durchschnitt (dunkelblau). Rot: Überlast oder Sicherung."),
		TEXT("Capacity of all power plants (gray), peak consumption in the wave (light blue) and average (dark blue). Red: overload or fuse.")));
	Legende({ { TDT(TEXT("Kapazität"), TEXT("Capacity")), KapFarbe }, { TDT(TEXT("Spitze"), TEXT("Peak")), SpitzeFarbe }, { TDT(TEXT("Durchschnitt"), TEXT("Average")), MittelFarbe },
		{ TDT(TEXT("Überlast"), TEXT("Overload")), TDUI::Fehler } });
	for (const FTDWellenStat& W : Verlauf)
	{
		UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
		Spalte(Reihe, TDF(TEXT("Welle %d"), TEXT("Wave %d"), W.Welle), 80.f, TDUI::Hell, true, false);
		const bool bUeber = W.StromUeberlast > 0.f;
		Balken(Reihe, { { W.StromKapazitaet / HoechstStrom, KapFarbe, 16.f }, { W.StromSpitze / HoechstStrom, bUeber ? TDUI::Fehler : SpitzeFarbe, 10.f },
			{ W.StromMittel / HoechstStrom, MittelFarbe, 10.f } });
		const float Reserve = W.StromKapazitaet > 0.f ? 100.f * (1.f - W.StromSpitze / W.StromKapazitaet) : 0.f;
		Spalte(Reihe, TDF(TEXT("Ø %.0f · Spitze %.0f von %.0f MW"), TEXT("Ø %.0f · peak %.0f of %.0f MW"), W.StromMittel, W.StromSpitze, W.StromKapazitaet), 270.f, TDUI::Hell, false, false);
		Spalte(Reihe, bUeber ? TDF(TEXT("%.0f s Überlast"), TEXT("%.0f s overload"), W.StromUeberlast) : TDF(TEXT("Reserve %.0f %%"), TEXT("Reserve %.0f %%"), Reserve), 150.f,
			bUeber || Reserve < 10.f ? TDUI::Fehler : Reserve < 25.f ? TDUI::Akzent : TDUI::Gut, false, false);
		Abschnitt(Reihe, 4.f);
	}

	// ---------- 3. Munition ----------
	const FLinearColor FabrikFarbe = TDUI::C(150, 215, 125), VorratFarbe = TDUI::C(80, 80, 80);
	Ueberschrift(TDT(TEXT("Munition"), TEXT("Ammo")), TDT(TEXT("Je Welle und Turmart: verschossen (Farbe der Turmart) gegen das, was die Fabrik in der Wellendauer liefern kann (grün), dahinter der Vorrat zu Beginn (grau). Ist verschossen länger als grün, zehren die Türme vom Vorrat."),
		TEXT("Per wave and turret type: fired (turret type color) against what the factory can deliver during the wave (green), behind it the stock at the start (gray). If fired is longer than green, the turrets eat into their stock.")));
	Legende({ { TDT(TEXT("Vorrat zu Beginn"), TEXT("Stock at start")), VorratFarbe }, { TDT(TEXT("Fabrik in der Welle"), TEXT("Factory during wave")), FabrikFarbe }, { TDT(TEXT("verschossen"), TEXT("fired")), TDUI::Akzent } });
	for (const FTDWellenStat& W : Verlauf)
	{
		for (const FTDTurmStat& T : W.Tuerme)
		{
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			Spalte(Reihe, TDF(TEXT("Welle %d"), TEXT("Wave %d"), W.Welle), 80.f, TDUI::Hell, true, false);
			const float Fabrik = T.ProduktionMin * W.Dauer / 60.f;
			Balken(Reihe, { { T.VorratStart / HoechstMunition, VorratFarbe, 18.f }, { Fabrik / HoechstMunition, FabrikFarbe, 11.f },
				{ T.Schuesse / HoechstMunition, UTDWellenStatistik::Farbe(T.Art), 5.f } });
			Spalte(Reihe, T.Art, 140.f, UTDWellenStatistik::Farbe(T.Art), true, false);
			Spalte(Reihe, TDF(TEXT("%d verschossen · Fabrik %.0f (%.0f/min) · Vorrat %d auf %d"), TEXT("%d fired · factory %.0f (%.0f/min) · stock %d to %d"), T.Schuesse, Fabrik, T.ProduktionMin, T.VorratStart, T.VorratEnde),
				420.f, T.Hungrig > 0.f || T.VorratEnde == 0 ? TDUI::Fehler : TDUI::Hell, false, false);
			Abschnitt(Reihe, 4.f);
		}
	}

	// ---------- 4. Details, neueste Welle zuerst ----------
	Ueberschrift(TDT(TEXT("Wellen im Detail"), TEXT("Waves in detail")), TDT(TEXT("Rot markiert: Engpässe (Vorrat leer, Zeit ohne Munition oder Strom)."), TEXT("Marked red: bottlenecks (stock empty, time without ammo or power).")));
	for (int32 i = Verlauf.Num() - 1; i >= 0; --i)
	{
		const FTDWellenStat& W = Verlauf[i];
		UBorder* Karte = NeuesFeld(TDUI::KarteDunkel, FMargin(12.f, 10.f));
		UVerticalBox* Inhalt = WidgetTree->ConstructWidget<UVerticalBox>();
		Karte->AddChild(Inhalt);
		UTextBlock* Kopfzeile = NeuerText(TDF(TEXT("Welle %d · %.0f s · %d Gegner · Kern %.0f %% auf %.0f %% · +%d Coupons, +%d FP"),
			TEXT("Wave %d · %.0f s · %d enemies · Core %.0f %% to %.0f %% · +%d Coupons, +%d RP"),
			W.Welle, W.Dauer, W.Gegner, W.KernStart, W.KernEnde, W.EP, W.FP), 16, TDUI::Akzent, true);
		Kopfzeile->SetAutoWrapText(true);
		Inhalt->AddChildToVerticalBox(Kopfzeile);
		const bool bStromKnapp = W.StromUeberlast > 0.f || (W.StromKapazitaet > 0.f && W.StromSpitze > 0.9f * W.StromKapazitaet);
		const FString Ueber = W.StromUeberlast > 0.f ? TDF(TEXT(", %.0f s Überlast"), TEXT(", %.0f s overload"), W.StromUeberlast) : FString();
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(NeuerText(TDF(TEXT("Strom: Ø %.0f MW, Spitze %.0f MW von %.0f MW Kapazität%s"), TEXT("Power: Ø %.0f MW, peak %.0f MW of %.0f MW capacity%s"),
			W.StromMittel, W.StromSpitze, W.StromKapazitaet, *Ueber), 13, bStromKnapp ? TDUI::Fehler : TDUI::Grau)))
		{
			S->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
		}
		// Spalten breit genug fuer die fetten Ueberschriften (Test 0.66: ueberlagert)
		static const float Breiten[] = { 150.f, 60.f, 90.f, 70.f, 100.f, 80.f, 110.f, 110.f, 100.f, 130.f, 110.f };
		const FString Koepfe[] = { TDT(TEXT("Turmart"), TEXT("Turret type")), TDT(TEXT("Türme"), TEXT("Turrets")), TDT(TEXT("Schaden"), TEXT("Damage")),
			TDT(TEXT("Anteil"), TEXT("Share")), TDT(TEXT("Abschüsse"), TEXT("Kills")), TDT(TEXT("Schüsse"), TEXT("Shots")),
			TDT(TEXT("Vorrat Start"), TEXT("Stock start")), TDT(TEXT("Vorrat Ende"), TEXT("Stock end")), TDT(TEXT("Fabrik/min"), TEXT("Factory/min")),
			TDT(TEXT("ohne Munition"), TEXT("no ammo")), TDT(TEXT("ohne Strom"), TEXT("no power")) };
		UHorizontalBox* Kopf = WidgetTree->ConstructWidget<UHorizontalBox>();
		for (int32 k = 0; k < 11; ++k)
		{
			Spalte(Kopf, Koepfe[k], Breiten[k], TDUI::Grau, true, k > 0);
		}
		Inhalt->AddChildToVerticalBox(Kopf);
		float Summe = 0.f;
		for (const FTDTurmStat& T : W.Tuerme)
		{
			Summe += T.Schaden;
		}
		int32 Nr = 0;
		for (const FTDTurmStat& T : W.Tuerme)
		{
			UBorder* Zeile = NeuesFeld(Nr++ % 2 ? TDUI::ZeileB : TDUI::ZeileA, FMargin(0.f, 3.f));
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			Zeile->AddChild(Reihe);
			Spalte(Reihe, T.Art, Breiten[0], UTDWellenStatistik::Farbe(T.Art), true, false);
			Spalte(Reihe, FString::FromInt(T.Anzahl), Breiten[1], TDUI::Hell, false, true);
			Spalte(Reihe, Zahl(T.Schaden), Breiten[2], TDUI::Hell, false, true);
			Spalte(Reihe, FString::Printf(TEXT("%.0f %%"), Summe > 0.f ? 100.f * T.Schaden / Summe : 0.f), Breiten[3], TDUI::Hell, false, true);
			Spalte(Reihe, FString::FromInt(T.Kills), Breiten[4], TDUI::Hell, false, true);
			Spalte(Reihe, FString::FromInt(T.Schuesse), Breiten[5], TDUI::Hell, false, true);
			Spalte(Reihe, FString::FromInt(T.VorratStart), Breiten[6], TDUI::Hell, false, true);
			Spalte(Reihe, FString::FromInt(T.VorratEnde), Breiten[7], T.VorratEnde == 0 && T.Anzahl > 0 ? TDUI::Fehler : TDUI::Hell, false, true);
			Spalte(Reihe, T.ProduktionMin > 0.f ? FString::Printf(TEXT("%.0f"), T.ProduktionMin) : FString(TEXT("-")), Breiten[8], TDUI::Hell, false, true);
			Spalte(Reihe, FString::Printf(TEXT("%.0f s"), T.Hungrig), Breiten[9], T.Hungrig > 0.f ? TDUI::Fehler : TDUI::Hell, false, true);
			Spalte(Reihe, FString::Printf(TEXT("%.0f s"), T.OhneStrom), Breiten[10], T.OhneStrom > 0.f ? TDUI::Fehler : TDUI::Hell, false, true);
			Inhalt->AddChildToVerticalBox(Zeile);
		}
		UTextBlock* Fuss = NeuerText(TDT(TEXT("Schüsse und Vorrat in Turm-Einheiten, Fabrik/min = Soll-Produktion der eingestellten Munition. \"ohne Munition\" zählt Turm-Sekunden mit Ziel, aber leerem Magazin."),
			TEXT("Shots and stock in turret units, factory/min = target production of the selected ammo. \"no ammo\" counts turret seconds with a target but an empty magazine.")), 12, TDUI::Grau);
		Fuss->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(Fuss))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		Abschnitt(Karte, 10.f);
	}
}
