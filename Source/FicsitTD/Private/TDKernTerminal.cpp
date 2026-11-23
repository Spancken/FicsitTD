#include "TDKernTerminal.h"
#include "TDSpielstand.h"
#include "TDWellenStatistik.h"
#include "TDLaufProtokoll.h"
#include "FicsitTDModule.h"
#include "TDDefenseCore.h"
#include "TDEinsatz.h"
#include "TDShop.h"
#include "TDKernplatz.h"
#include "TDTurretBase.h"
#include "TDWaveSubsystem.h"
#include "TDBalance.h"
#include "TDGameInstanceModule.h"
#include "Settings/FGUserSetting.h"
#include "FGOptionInterface.h"
#include "TDForschung.h"
#include "TDPionier.h"
#include "TDSprache.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "SessionSettings/SessionSettingsManager.h"
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
		TDT(TEXT("Konto (Coupons · Festplatten):"), TEXT("Balance (Coupons · Hard Drives):")) };
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
	// 0.12-dev Freies Spiel mit Abwehr: Knoten, Stufen und Lohn der Bohrung
	mBohrInfo = NeuerText(TEXT(""), 12, TDUI::Hell);
	mBohrInfo->SetAutoWrapText(true);
	if (UVerticalBoxSlot* S = StatusSpalte->AddChildToVerticalBox(mBohrInfo))
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
	// 0.12-dev Freies Spiel mit Abwehr: Bohrung starten (drei Stufen) und abbrechen
	mBohr0 = NeuerKnopf(TDF(TEXT("Bohrung %s"), TEXT("Drill %s"), *UTDWaveSubsystem::BohrStufenName(0)), Knopftext, true);
	mBohr0->OnClicked.AddDynamic(this, &UTDKernTerminal::Bohre0);
	Aktion(mBohr0);
	mBohr1 = NeuerKnopf(TDF(TEXT("Bohrung %s"), TEXT("Drill %s"), *UTDWaveSubsystem::BohrStufenName(1)), Knopftext, true);
	mBohr1->OnClicked.AddDynamic(this, &UTDKernTerminal::Bohre1);
	Aktion(mBohr1);
	mBohr2 = NeuerKnopf(TDF(TEXT("Bohrung %s"), TEXT("Drill %s"), *UTDWaveSubsystem::BohrStufenName(2)), Knopftext, true);
	mBohr2->OnClicked.AddDynamic(this, &UTDKernTerminal::Bohre2);
	Aktion(mBohr2);
	mBohrAbbruch = NeuerKnopf(TDT(TEXT("Bohrung abbrechen"), TEXT("Abort drilling")), Knopftext);
	mBohrAbbruch->OnClicked.AddDynamic(this, &UTDKernTerminal::BohrungAbbrechen);
	Aktion(mBohrAbbruch);
	// 0.11 Endlos (Marius 15.11.): nach der Bosswelle einer von drei Boni
	mBonus0 = NeuerKnopf(UTDEinsatzSubsystem::EndlosBonusName(0), Knopftext, true);
	mBonus0->OnClicked.AddDynamic(this, &UTDKernTerminal::WaehleBonus0);
	Aktion(mBonus0);
	mBonus1 = NeuerKnopf(UTDEinsatzSubsystem::EndlosBonusName(1), Knopftext, true);
	mBonus1->OnClicked.AddDynamic(this, &UTDKernTerminal::WaehleBonus1);
	Aktion(mBonus1);
	mBonus2 = NeuerKnopf(UTDEinsatzSubsystem::EndlosBonusName(2), Knopftext, true);
	mBonus2->OnClicked.AddDynamic(this, &UTDKernTerminal::WaehleBonus2);
	Aktion(mBonus2);
	UButton* WegeKnopf = NeuerKnopf(TDT(TEXT("Wegvorschau an"), TEXT("Path preview on")), Knopftext);
	mWegeText = Knopftext;
	WegeKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::WegeUmschalten);
	Aktion(WegeKnopf);
	UButton* FreiKnopf = NeuerKnopf(TDT(TEXT("Freies Bauen: aus"), TEXT("Free Build: off")), Knopftext);
	mFreiText = Knopftext;
	FreiKnopf->OnClicked.AddDynamic(this, &UTDKernTerminal::FreiUmschalten);
	Aktion(FreiKnopf);
	// 0.11 (Gegencheck Einsatz A3, Marius 16.11.): Freies Bauen nur im Entwicklermodus
	FreiKnopf->SetVisibility(UTDKernplatz::Entwicklermodus() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	mFreiKnopf = FreiKnopf;

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
		// 16.11. (Nachpruefung F4): ohne Einsatz (Freies Spiel mit Abwehr, nach dem Laden) holt das Menue ueber diesen Kern ab
		Einsatz->SetzeAbwehrKern(Kern());
		if (const int32 Nach = Einsatz->GibOffeneFP())
		{
			Melde(TDF(TEXT("%d Festplatten ins Inventar nachgereicht."), TEXT("%d Hard Drives added to your inventory."), Nach));
		}
		if (const int32 Coupons = Einsatz->GibOffeneCoupons())
		{
			Melde(TDF(TEXT("%d FICSIT-Coupons vom Kern ins Inventar geholt."), TEXT("%d FICSIT Coupons moved from the Core to your inventory."), Coupons));
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
	mTitel->SetText(bEinsatz ? TDTX(TEXT("Abwehrkern - Einsatz"), TEXT("Defense Core - Operation"))
		: UTDKernplatz::IstAbwehr(this) ? TDTX(TEXT("Abwehrkern - Tiefbohrer"), TEXT("Defense Core - Deep Drill")) : TDTX(TEXT("Abwehrkern"), TEXT("Defense Core")));
	FString WelleText;
	if (Waves->LaeuftWelle())
	{
		// ohne Richtungen, die stehen darunter bei den Fronten
		WelleText = Waves->GetLaufText();
		if (!WelleText.Split(TEXT(", aus "), &WelleText, nullptr)) { WelleText.Split(TEXT(", from "), &WelleText, nullptr); }
	}
	else if (Waves->LaeuftKernVerteidigung())
	{
		WelleText = Waves->GetBereitText(true);   // Gegencheck 15.11. (F1): auf Zeit mit Countdown
	}
	else
	{
		WelleText = TDT(TEXT("kein Lauf"), TEXT("no run"));
	}
	mWelle->SetText(FText::FromString(WelleText));
	mKernLP->SetText(FText::FromString(K ? FString::Printf(TEXT("%.0f %%"), 100.f * K->GetTDHealth() / FMath::Max(K->GetTDMaxHealth(), 1.f)) : TEXT("-")));
	mKonto->SetText(FText::FromString(bEinsatz || UTDKernplatz::IstAbwehr(this) ? TDF(TEXT("%d Coupons  ·  %d Festplatten"), TEXT("%d Coupons  ·  %d Hard Drives"), Einsatz->Konto(), Einsatz->FPImInventar()) : FString(TEXT("-"))));
	{
		// 0.11 Endlos: der Knopf zieht vor und nennt den Vorzieh-Bonus
		const int32 VorziehBonus = Waves->IstEndlos() && Waves->GetCountdown() > 0.f
			? FMath::FloorToInt(Waves->GetCountdown() / FMath::Max(1.f, TDBalance::Wert(TEXT("Endlos.VorziehSekundenJeCoupon"), 10.f))) : 0;
		mWelleText->SetText(Waves->LaeuftWelle() ? TDTX(TEXT("Welle läuft ..."), TEXT("Wave running ..."))
			// 0.13 K1: der Start wartet, bis der Kern wieder erreichbar ist (Sperrstelle auf der Karte)
			: Waves->WartetAufKernZugang() ? TDTX(TEXT("Kern abgeriegelt (Karte)"), TEXT("Core sealed off (map)"))
			: VorziehBonus > 0 ? TDFX(TEXT("Welle vorziehen (+%d Coupons)"), TEXT("Bring wave forward (+%d Coupons)"), VorziehBonus)
			: TDTX(TEXT("Welle starten"), TEXT("Start wave")));
	}
	mWegeText->SetText(K && K->ZeigtWege() ? TDTX(TEXT("Wegvorschau aus"), TEXT("Path preview off")) : TDTX(TEXT("Wegvorschau an"), TEXT("Path preview on")));
	mFreiText->SetText(Einsatz->IstFrei() ? TDTX(TEXT("Freies Bauen: AN"), TEXT("Free Build: ON")) : TDTX(TEXT("Freies Bauen: aus"), TEXT("Free Build: off")));
	// 0.11 (Modus Aus): von Hand nur mit Spielmodus "Einsatz" oder im Entwicklermodus (Selbsttests)
	// 0.11 (Gegencheck Einsatz A2/A5, Marius 16.11.): ohne Entwicklermodus nur ein Einsatz je Kern - ein neuer Lauf ist ein neues Spiel
	const ATDSpielstand* StandE = ATDSpielstand::Get(this);
	const bool bSchonBeendet = (K && K->mEinsatzStartkit) || (StandE && StandE->mEinsatzBeendet);
	mStartKnopf->SetVisibility(bEinsatz || !(UTDKernplatz::Entwicklermodus() || (UTDKernplatz::TowerDefenseAn(this) && !bSchonBeendet)) ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	{
		const int32 Ende = Einsatz->LaufEnde();
		if (mWelleKnopf)
		{
			mWelleKnopf->SetVisibility(Ende ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
		}
		const bool bEndlosEnde = K && K->mLaufEndlos;
		// 0.12-dev Freies Spiel mit Abwehr: Bohr-Knoepfe statt Einsatz; "Welle starten" nur in der Bohrung (zieht vor)
		const bool bAbwehr = UTDKernplatz::IstAbwehr(this) && !bEinsatz;
		if (mBohr0)
		{
			const bool bBohrt = Waves->IstBohrung();
			auto Sicht = [](bool b) { return b ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; };
			mBohr0->SetVisibility(Sicht(bAbwehr && !bBohrt));
			mBohr1->SetVisibility(Sicht(bAbwehr && !bBohrt && UTDWaveSubsystem::BohrStufeFrei(K, 1)));
			mBohr2->SetVisibility(Sicht(bAbwehr && !bBohrt && UTDWaveSubsystem::BohrStufeFrei(K, 2)));
			mBohrAbbruch->SetVisibility(Sicht(bAbwehr && bBohrt));
			mBohrInfo->SetVisibility(Sicht(bAbwehr));
			if (bAbwehr && mWelleKnopf && !bBohrt)
			{
				mWelleKnopf->SetVisibility(ESlateVisibility::Collapsed);
			}
			// Platzpruefung kostet eine Wegsuche: hoechstens alle 3 s neu
			mBohrInfoUhr -= 0.5f;
			if (bAbwehr && mBohrInfoUhr <= 0.f && K)
			{
				mBohrInfoUhr = 3.f;
				mBohrInfo->SetText(FText::FromString(FString::Join(const_cast<UTDWaveSubsystem*>(Waves)->BohrInfo(K), TEXT("\n"))));
			}
		}
		if (mBonus0)
		{
			const ESlateVisibility Sicht = !Ende && Einsatz->EndlosBonusOffen() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
			mBonus0->SetVisibility(Sicht);
			mBonus1->SetVisibility(Sicht);
			mBonus2->SetVisibility(Sicht);
		}
		if (mEndeWiederholen)
		{
			// 0.11 (Marius 15.11.): ein Endlos-Lauf endet mit Wertung - kein Wiederholen
			mEndeWiederholen->SetVisibility(Ende == 2 && !bEndlosEnde ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			// 0.11 (Gegencheck Einsatz A2, Marius 16.11.): "Neu starten" behielt Coupons, Turmlevel und Forschung und
			// zahlte alle Wellen erneut - fuer Spieler raus, ein neuer Lauf ist ein neues Spiel. Bleibt fuer Selbsttests.
			mEndeNeustart->SetVisibility(Ende && UTDKernplatz::Entwicklermodus() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			mEndeBeenden->SetVisibility(Ende ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
		if (Ende)
		{
			mWelle->SetText(Ende == 1 ? TDTX(TEXT("SIEG"), TEXT("VICTORY"))
				: bEndlosEnde ? TDFX(TEXT("Ende in Welle %d (Bestwert %d)"), TEXT("Over in wave %d (best %d)"), K->mLaufEndWelle, UTDEinsatzSubsystem::EndlosBestwert(K->mLaufStufe))
				: TDFX(TEXT("Niederlage in Welle %d"), TEXT("Defeat in wave %d"), K ? K->mLaufEndWelle : 0));
		}
	}
	if (mLaengeKnopf)
	{
		const FString Namen[] = { TDT(TEXT("Kurz (20)"), TEXT("Short (20)")), TDT(TEXT("Mittel (30)"), TEXT("Medium (30)")), TDT(TEXT("Standard (50)"), TEXT("Standard (50)")),
			TDT(TEXT("Endlos"), TEXT("Endless")) };   // 0.77 Marius: 50 Wellen sind Standard
		const FString Wellen[] = { TDT(TEXT("20 Wellen"), TEXT("20 waves")), TDT(TEXT("30 Wellen"), TEXT("30 waves")), TDT(TEXT("50 Wellen"), TEXT("50 waves")), TDT(TEXT("endlos"), TEXT("endless")) };
		// 16.11. (Nachpruefung F10): nur zusammen mit "Einsatz starten" (sonst stand er im Freien Spiel ohne Wirkung da)
		mLaengeKnopf->SetVisibility(mStartKnopf ? mStartKnopf->GetVisibility() : ESlateVisibility::Collapsed);
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
	const int32 Naechste = Waves ? Waves->GetFreigabeWelle() : 1;   // 17.11. (P16/A13)
	int32 Anzahl[5] = { 0, 0, 0, 0, 0 };
	for (const FTDForschungKnoten& Kn : TDForschung::Alle())
	{
		++Anzahl[static_cast<int32>(TDForschung::Zustand(this, Kn))];
	}
	Abschnitt(NeuerText(TDF(TEXT("Forschung im MAM, Bäume FICSIT-Abwehr. Im Inventar: %d Festplatten. Nächste Welle: %d."),
		TEXT("Research in the MAM, FICSIT Defense trees. In inventory: %d Hard Drives. Next wave: %d."),
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
			Abschnitt(NeuerText(TDForschung::Anzeige(Kn.BaumName), 16, TDUI::Akzent, true), 6.f);   // 23.11. Gegencheck F5
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
			Lage = TDF(TEXT("JETZT ERFORSCHBAR  ·  %d Festplatten"), TEXT("AVAILABLE NOW  ·  %d Hard Drives"), Kn.FP);
			LageFarbe = TDUI::Akzent;
			Grund = TDUI::ChipStark;
			break;
		case ETDForschungZustand::ZuFrueh:
			Lage = TDF(TEXT("AB WELLE %d  ·  %d Festplatten"), TEXT("FROM WAVE %d  ·  %d Hard Drives"), TDForschung::AbWelle(Kn), Kn.FP);
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
		Zeile(TDForschung::Anzeige(Kn.Name), 14, TDUI::Hell, true, 2.f);
		Zeile(Lage, 12, LageFarbe, true, 4.f);
		Zeile(TDForschung::Anzeige(Kn.Text), 12, TDUI::Hell, false, 3.f);
		if (!Kn.Laden.IsEmpty())
		{
			Zeile(TDT(TEXT("Im Laden: "), TEXT("In shop: ")) + TDForschung::Anzeige(Kn.Laden), 12, TDUI::Gut, false, 1.f);
		}
		if (!Kn.Gegen.IsEmpty())
		{
			Zeile(TDT(TEXT("Gut gegen: "), TEXT("Good against: ")) + TDForschung::Anzeige(Kn.Gegen), 12, TDUI::Grau, false, 1.f);
		}
		Raster->AddChildToWrapBox(Breite);
	}
}

namespace TDTalentBild
{
	// 0.10.0 (Marius 14.11.: Talentbaum mit passenden Symbolen): Spiel-Icons zur Laufzeit (Pfad der Textur),
	// "Kreatur:" = Icon eines Kreatur-Deskriptors (beachtet die Arachnophobie-Einstellung)
	#define TD_ICO(Pfad) TEXT("/Game/FactoryGame/") TEXT(Pfad)
	static const TCHAR* const Rolle[4] = {
		TD_ICO("Equipment/Rifle/UI/IconDesc_Rifle_256.IconDesc_Rifle_256"),
		TD_ICO("Resource/Equipment/Beacon/UI/IconDesc_Beacon_256.IconDesc_Beacon_256"),
		TD_ICO("Equipment/BuildGun/UI/IconDesc_TrinketWrench_256.IconDesc_TrinketWrench_256"),
		TEXT("Kreatur:/Game/FactoryGame/Character/Creature/CreatureDescriptors/Desc_HogAlpha.Desc_HogAlpha_C"),
	};
	static const TCHAR* const Stufe[4][5] = {
		{ TD_ICO("Resource/Parts/CartridgeStandard/UI/Rifle_Magazine_256.Rifle_Magazine_256"),
		  TD_ICO("Equipment/RebarGun/UI/IconDesc_Rebar_256.IconDesc_Rebar_256"),
		  TD_ICO("Resource/Parts/SpikedRebar/UI/IconDesc_SpikedRebar_256.IconDesc_SpikedRebar_256"),
		  TD_ICO("Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Recycle.TXUI_MIcon_Recycle"),
		  TD_ICO("Equipment/RebarGun/UI/IconDesc_ExpRebar_256.IconDesc_ExpRebar_256") },
		{ TD_ICO("Resource/Equipment/Beacon/UI/IconDesc_Beacon_256.IconDesc_Beacon_256"),
		  TD_ICO("Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Aggro.TXUI_MIcon_Aggro"),
		  TD_ICO("Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Next.TXUI_MIcon_Next"),
		  TD_ICO("Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Portal.TXUI_MIcon_Portal"),
		  TD_ICO("Resource/Environment/Crystal/UI/PowerShard_256.PowerShard_256") },
		{ TD_ICO("Resource/Parts/IronPlate/UI/IconDesc_IronPlates_256.IconDesc_IronPlates_256"),
		  TD_ICO("Equipment/BuildGun/UI/IconDesc_TrinketWrench_256.IconDesc_TrinketWrench_256"),
		  TD_ICO("Resource/Environment/Crystal/UI/PowerShard_256.PowerShard_256"),
		  TD_ICO("Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Crate.TXUI_MIcon_Crate"),
		  TD_ICO("Resource/Parts/IronPlateReinforced/UI/IconDesc_ReinforcedIronPlates_256.IconDesc_ReinforcedIronPlates_256") },
		{ TEXT("Kreatur:/Game/FactoryGame/Character/Creature/CreatureDescriptors/Desc_HogAlpha.Desc_HogAlpha_C"),
		  TD_ICO("Resource/Environment/CrashSites/UI/HardDrive_256.HardDrive_256"),
		  TD_ICO("Resource/Equipment/ShockShank/UI/IconDesc_XenoZapper_256.IconDesc_XenoZapper_256"),
		  TD_ICO("Interface/UI/Assets/MonochromeIcons/TXUI_MIcon_Speedometer.TXUI_MIcon_Speedometer"),
		  TD_ICO("Resource/Equipment/StunSpear/UI/IconDesc_XenoBasher_256.IconDesc_XenoBasher_256") },
	};
	#undef TD_ICO

	static UTexture2D* Lade(const TCHAR* Pfad)
	{
		static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
		const FString Schluessel(Pfad ? Pfad : TEXT(""));
		if (UTexture2D* T = Cache.FindRef(Schluessel).Get())
		{
			return T;
		}
		UTexture2D* T = nullptr;
		if (Schluessel.StartsWith(TEXT("Kreatur:")))
		{
			if (UClass* Desc = LoadClass<UFGItemDescriptor>(nullptr, *Schluessel.Mid(8)))
			{
				T = UFGItemDescriptor::GetBigIcon(Desc);
			}
		}
		else if (!Schluessel.IsEmpty())
		{
			T = LoadObject<UTexture2D>(nullptr, *Schluessel);
		}
		Cache.Add(Schluessel, T);
		UE_LOG(LogFicsitTD, Log, TEXT("Talentbaum: Symbol %s %s"), *Schluessel, T ? TEXT("geladen") : TEXT("FEHLT"));
		return T;
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
	if (!P || !Einsatz || (!Einsatz->IstAktiv() && !UTDKernplatz::IstAbwehr(this)))
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
	// 21.11. (Marius, C10): Talentpunkte nur aus Bosswellen und Bohrung, der Rang bringt die Boni
	UTextBlock* Info = UTDPionier::TalenteAusRang()
		? NeuerText(TDF(TEXT("XP gibt es für deinen Anteil am Wellenschaden, für Abschüsse (normal 3, Elite 10, Boss 50), Assists und reparierte LP, dazu etwas für jede abgewehrte Welle. "
		"Jeder Rang bringt einen Talentpunkt, höchstens %d: Wähle zwei Rollen ganz oder drei zur Hälfte. Alle 3 Ränge: +10 LP und +3 %% Lauftempo (jetzt +%d LP). Rang und Talente gehören zum Spielstand."),
		TEXT("You earn XP for your share of the wave damage, for kills (normal 3, Elite 10, Boss 50), assists and repaired HP, plus a little for every repelled wave. "
		"Each rank gives one Talent Point, at most %d: pick two roles in full or three halfway. Every 3 ranks: +10 HP and +3 %% walk speed (now +%d HP). Rank and talents belong to the save."),
		FMath::Min(UTDPionier::MaxRang() - 1, TDBalance::Ganz(TEXT("Pionier.MaxPunkte"), 15)), 10 * P->RangBoni()), 13, TDUI::Grau)
		: NeuerText(TDF(TEXT("Talentpunkte gibt es für jede geschaffte Bosswelle und für Erfolge in der Bohrung, höchstens %d. Wähle mit Bedacht: Im Einsatz kommen nur wenige. "
		"XP gibt es für deinen Anteil am Wellenschaden, für Abschüsse, Assists und reparierte LP. Alle 3 Ränge: +10 LP und +3 %% Lauftempo (jetzt +%d LP). Rang und Talente gehören zum Spielstand."),
		TEXT("You get a Talent Point for every boss wave you clear and for drilling successes, at most %d. Choose carefully: an operation gives only a few. "
		"You earn XP for your share of the wave damage, for kills, assists and repaired HP. Every 3 ranks: +10 HP and +3 %% walk speed (now +%d HP). Rank and talents belong to the save."),
		TDBalance::Ganz(TEXT("Pionier.MaxPunkte"), 15), 10 * P->RangBoni()), 13, TDUI::Grau);
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
		// 0.10.0: Kopf mit Rollen-Symbol, Name und Stufen-Plakette
		UHorizontalBox* RollenKopf = WidgetTree->ConstructWidget<UHorizontalBox>();
		UBorder* RollenBild = NeuesFeld(TDUI::BildGrund, FMargin(4.f));
		RollenBild->AddChild(NeuesSymbol(TDTalentBild::Lade(TDTalentBild::Rolle[FMath::Clamp(i, 0, 3)]), 40.f));
		if (UHorizontalBoxSlot* S = RollenKopf->AddChildToHorizontalBox(RollenBild))
		{
			S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UVerticalBox* RollenText = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBox* NameZeile = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UHorizontalBoxSlot* S = NameZeile->AddChildToHorizontalBox(NeuerText(Z.Name, 17, TDUI::Hell, true)))
		{
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		UBorder* StufenMarke = NeuesFeld(Stufe > 0 ? TDUI::Akzent : TDUI::ChipNeutral, FMargin(7.f, 1.f));
		StufenMarke->AddChild(NeuerText(FString::Printf(TEXT("%d/%d"), Stufe, UTDPionier::MaxStufe), 12, Stufe > 0 ? TDUI::DunkelText : TDUI::Grau, true));
		if (UHorizontalBoxSlot* S = NameZeile->AddChildToHorizontalBox(StufenMarke))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		RollenText->AddChildToVerticalBox(NameZeile);
		RollenText->AddChildToVerticalBox(NeuerText(Z.Rolle, 12, TDUI::Akzent));
		if (UHorizontalBoxSlot* S = RollenKopf->AddChildToHorizontalBox(RollenText))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(RollenKopf))
		{
			S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
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
			// 0.10.0: Kachel je Stufe - Symbol links, Stufe in Versalien, Faehigkeit; gelernt orange, lernbar mit orangem Rahmen, gesperrt gedaempft
			const bool bGelernt = s < Stufe;
			const bool bLernbar = s == Stufe && Frei > 0 && Fehlt.IsEmpty();
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			UBorder* SymbolGrund = NeuesFeld(TDUI::BildGrund, FMargin(3.f));
			SymbolGrund->AddChild(NeuesSymbol(TDTalentBild::Lade(TDTalentBild::Stufe[FMath::Clamp(i, 0, 3)][s]), 30.f));
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(SymbolGrund))
			{
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
				S->SetVerticalAlignment(VAlign_Top);
			}
			UVerticalBox* KachelText = WidgetTree->ConstructWidget<UVerticalBox>();
			KachelText->AddChildToVerticalBox(NeuerText(bGelernt ? TDF(TEXT("STUFE %d · GELERNT"), TEXT("TIER %d · LEARNED"), s + 1)
				: bLernbar ? TDF(TEXT("STUFE %d · JETZT LERNEN"), TEXT("TIER %d · LEARN NOW"), s + 1)
				: TDF(TEXT("STUFE %d"), TEXT("TIER %d"), s + 1), 11, bGelernt || bLernbar ? TDUI::Akzent : TDUI::Grau, true));
			UTextBlock* T = NeuerText(Z.Stufen[s], 12, bGelernt || bLernbar ? TDUI::Hell : TDUI::Grau, false);
			T->SetAutoWrapText(true);
			KachelText->AddChildToVerticalBox(T);
			if (UHorizontalBoxSlot* S = Reihe->AddChildToHorizontalBox(KachelText))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			UWidget* Eintrag = nullptr;
			if (bLernbar)
			{
				UButton* Knopf = WidgetTree->ConstructWidget<UButton>();
				FButtonStyle Stil;
				Stil.SetNormal(FSlateRoundedBoxBrush(TDUI::ChipStark, 3.f, TDUI::Akzent, 2.f));
				Stil.SetHovered(FSlateRoundedBoxBrush(TDUI::C(84, 58, 34), 3.f, TDUI::Akzent, 2.f));
				Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::C(110, 72, 38), 3.f, TDUI::Akzent, 2.f));
				Stil.SetNormalPadding(FMargin(6.f, 5.f));
				Stil.SetPressedPadding(FMargin(6.f, 6.f, 6.f, 4.f));
				Knopf->SetStyle(Stil);
				Knopf->AddChild(Reihe);
				UTDTalentKnopf* Objekt = NewObject<UTDTalentKnopf>(this);
				Objekt->Zweig = i;
				Objekt->Menue = this;
				Knopf->OnClicked.AddDynamic(Objekt, &UTDTalentKnopf::Geklickt);
				mTalentKnoepfe.Add(Objekt);
				Eintrag = Knopf;
			}
			else
			{
				UBorder* Zelle = NeuesFeld(bGelernt ? TDUI::ChipStark : TDUI::ChipNeutral, FMargin(8.f, 7.f));
				Zelle->AddChild(Reihe);
				if (!bGelernt)
				{
					Zelle->SetRenderOpacity(0.55f);
				}
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

/** Testphase: Wellen in der Zeitleiste waehlen und springen.
 *  0.11 (Gegencheck Einsatz A1, Marius 16.11.): nur im Entwicklermodus - sonst brachte 1 -> 50 rund 3400 Coupons ohne Kampf */
static bool TestModus()
{
	return UTDKernplatz::Entwicklermodus() && !UTDLaufProtokoll::bBalancing && TDBalance::Ganz(TEXT("Test.WellenSprung"), 1) > 0;
}

TArray<FString> UTDKernTerminal::SichtbareTestKnoepfe() const
{
	TArray<FString> Namen;
	auto Sichtbar = [](const UWidget* W) { return W && W->GetVisibility() != ESlateVisibility::Collapsed && W->GetVisibility() != ESlateVisibility::Hidden; };
	if (Sichtbar(mFreiKnopf)) Namen.Add(TEXT("FreiesBauen"));
	if (Sichtbar(mEndeNeustart)) Namen.Add(TEXT("NeuStarten"));
	if (Sichtbar(mStartKnopf)) Namen.Add(TEXT("EinsatzStarten"));
	if (TestModus()) Namen.Add(TEXT("Testsprung"));
	return Namen;
}

int32 UTDKernTerminal::FreiePlusKnoepfe(int32& OutAlle) const
{
	int32 Frei = 0;
	OutAlle = 0;
	for (const UTDKaufKnopf* K : mKaufKnoepfe)
	{
		if (K && K->Schritt > 0 && K->Knopf.IsValid())
		{
			++OutAlle;
			Frei += K->Knopf->GetIsEnabled() ? 1 : 0;
		}
	}
	return Frei;
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
	// 0.12 (C7): dieselbe Regel wie Kaufe() - im Endlos ist der Laden auch in der Welle offen.
	// Vorher stand hier nur "laeuft eine Welle", die Knoepfe waren im Endlos gesperrt, obwohl der Kauf erlaubt war.
	const bool bWelle = Einsatz->LadenInWelleZu();
	{
		static int32 TDLadenStandGemeldet = -1;
		const int32 Stand = (Waves && Waves->LaeuftWelle() ? 1 : 0) + (Waves && Waves->IstEndlos() ? 2 : 0) + (bWelle ? 4 : 0);
		if (Stand != TDLadenStandGemeldet)
		{
			TDLadenStandGemeldet = Stand;
			UE_LOG(LogFicsitTD, Log, TEXT("Kern-Menue Laden: Welle %s, Endlos %s -> Knoepfe %s"),
				Waves && Waves->LaeuftWelle() ? TEXT("ja") : TEXT("nein"), Waves && Waves->IstEndlos() ? TEXT("ja") : TEXT("nein"),
				bWelle ? TEXT("gesperrt") : TEXT("frei"));
		}
	}
	auto Fuellen =[](UHorizontalBox* Reihe, UWidget* W)
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
		Ziel->Knopf = B;
		B->OnClicked.AddDynamic(Ziel, &UTDKaufKnopf::Geklickt);
		mKaufKnoepfe.Add(Ziel);
		return B;
	};

	// 0.12 E4 (Marius 14./21.11.): sobald ein AWESOME-Shop steht, verkauft der Kern nicht mehr
	const bool bShopSteht = Einsatz->IstAktiv() && Einsatz->KernVerkauftNicht();
	if (!Einsatz->IstAktiv() || bShopSteht)
	{
		mLadenZeilen.Reset();
		mKorb.Reset();
		UHorizontalBox* Reihe = nullptr;
		NeueKarte(mLaden, TDUI::ZeileA, Reihe);
		Fuellen(Reihe, NeuerText(bShopSteht
			? TDT(TEXT("Gekauft wird jetzt im AWESOME-Shop: Kontingente, Ausrüstung, Kernreparatur und Festplatten (Bereiche Türme, Verteidigung, Produktion, Strom, Logistik, Pionier und Kern). Den Stand deiner Kontingente siehst du rechts."),
				TEXT("You now buy in the AWESOME Shop: quotas, gear, Core repairs and Hard Drives (sections Turrets, Defense, Production, Power, Logistics, Pioneer and Core). Your quotas are listed on the right."))
			: UTDKernplatz::IstAbwehr(this)
			? TDT(TEXT("Im Freien Spiel mit Abwehr gibt es keinen Laden: Coupons aus dem AWESOME-Sink bezahlen Abwehr-Forschung im MAM und Turmlevel."),
				TEXT("Free play with defense has no shop: Coupons from the AWESOME Sink pay for defense research in the MAM and turret levels."))
			: TDT(TEXT("Erst einen Einsatz starten."), TEXT("Start an operation first.")), 13, TDUI::Grau));
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
	const UTDEinsatzSubsystem* EinsatzRegel = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	if (EinsatzRegel && EinsatzRegel->LadenInWelleZu())
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

void UTDKernTerminal::Bohre0() { Bohre(0); }
void UTDKernTerminal::Bohre1() { Bohre(1); }
void UTDKernTerminal::Bohre2() { Bohre(2); }

void UTDKernTerminal::Bohre(int32 Stufe)
{
	UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>();
	UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>();
	ATDDefenseCore* K = Kern();
	if (!Waves || !K)
	{
		return;
	}
	FString Meldung;
	Waves->StarteBohrung(K, Stufe, Meldung);
	Melde(Meldung);
	if (Einsatz)
	{
		Einsatz->Melde(Meldung);
	}
	mBohrInfoUhr = 0.f;
	Aktualisiere();
}

void UTDKernTerminal::BohrungAbbrechen()
{
	if (UTDWaveSubsystem* Waves = GetWorld()->GetSubsystem<UTDWaveSubsystem>())
	{
		Melde(Waves->BrecheBohrungAb());
		mBohrInfoUhr = 0.f;
		Aktualisiere();
	}
}

void UTDKernTerminal::WaehleBonus0() { WaehleBonus(0); }
void UTDKernTerminal::WaehleBonus1() { WaehleBonus(1); }
void UTDKernTerminal::WaehleBonus2() { WaehleBonus(2); }

void UTDKernTerminal::WaehleBonus(int32 Nr)
{
	if (UTDEinsatzSubsystem* Einsatz = GetWorld()->GetSubsystem<UTDEinsatzSubsystem>())
	{
		Melde(Einsatz->WaehleEndlosBonus(Nr));
		Aktualisiere();
	}
}

void UTDKernTerminal::EndeNeustart()
{
	if (!UTDKernplatz::Entwicklermodus())
	{
		return;
	}
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
	if (!UTDKernplatz::Entwicklermodus())
	{
		return;
	}
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
	const ATDSpielstand* StandS = ATDSpielstand::Get(this);
	if (!UTDKernplatz::Entwicklermodus() && ((Kern() && Kern()->mEinsatzStartkit) || (StandS && StandS->mEinsatzBeendet)))
	{
		Melde(TDT(TEXT("An diesem Kern lief schon ein Einsatz. Ein neuer Lauf beginnt mit einem neuen Spiel."), TEXT("This Core already ran an operation. A new run starts with a new game.")));
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
		// 0.10.0: Rollenabzeichen links (Symbol der staerksten Rolle), ausgeblendet ohne Talent
		UHorizontalBox* Zeile = WidgetTree->ConstructWidget<UHorizontalBox>();
		mRolle = WidgetTree->ConstructWidget<UImage>();
		mRolle->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
		mRolle->SetVisibility(ESlateVisibility::Collapsed);
		UBorder* RolleGrund = WidgetTree->ConstructWidget<UBorder>();
		RolleGrund->SetBrush(FSlateRoundedBoxBrush(TDUI::BildGrund, 3.f));
		RolleGrund->SetPadding(FMargin(2.f));
		RolleGrund->AddChild(mRolle);
		RolleGrund->SetVisibility(ESlateVisibility::Collapsed);
		if (UHorizontalBoxSlot* S = Zeile->AddChildToHorizontalBox(RolleGrund))
		{
			S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			S->SetVerticalAlignment(VAlign_Center);
		}
		if (UHorizontalBoxSlot* S = Zeile->AddChildToHorizontalBox(mText))
		{
			S->SetVerticalAlignment(VAlign_Center);
		}
		Rahmen->AddChild(Zeile);
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
		: Waves->LaeuftKernVerteidigung() ? Waves->GetBereitText()
		: TDT(TEXT("Kein Lauf"), TEXT("No run"));
	const UTDPionier* Pionier = UTDPionier::Get(this);
	const int32 Frei = Pionier ? Pionier->FreiePunkte() : 0;
	FString Punkte = Frei <= 0 ? FString() : Frei == 1 ? TDT(TEXT(" (1 Talentpunkt)"), TEXT(" (1 Talent Point)")) : TDF(TEXT(" (%d Talentpunkte)"), TEXT(" (%d Talent Points)"), Frei);
	// 0.10.0 (Marius 14.11.: Rolle sichtbar machen): staerkste Rolle(n) als Name, die staerkste als Symbol
	int32 Beste = -1;
	TArray<FString> Rollen;
	if (Pionier)
	{
		const TArray<FTDTalentZweig>& Zweige = UTDPionier::Zweige();
		int32 Hoechste = 0;
		for (int32 z = 0; z < Zweige.Num(); ++z)
		{
			Hoechste = FMath::Max(Hoechste, Pionier->GetStufe(z));
		}
		for (int32 z = 0; z < Zweige.Num() && Hoechste > 0; ++z)
		{
			if (Pionier->GetStufe(z) == Hoechste)
			{
				Rollen.Add(Zweige[z].Name);
				Beste = Beste < 0 ? z : Beste;
			}
		}
	}
	if (Rollen.Num() > 0)
	{
		Punkte = TEXT(" · ") + FString::Join(Rollen, TEXT("/")) + Punkte;
	}
	if (mRolle && mRolleZweig != Beste)
	{
		mRolleZweig = Beste;
		UTexture2D* Bild = Beste >= 0 ? TDTalentBild::Lade(TDTalentBild::Rolle[FMath::Clamp(Beste, 0, 3)]) : nullptr;
		if (Bild)
		{
			mRolle->SetBrushFromTexture(Bild, false);
			mRolle->SetDesiredSizeOverride(FVector2D(28.f, 28.f));
		}
		mRolle->SetVisibility(Bild ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UWidget* Grund = mRolle->GetParent())
		{
			Grund->SetVisibility(Bild ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	// 21.11. (Marius, C15): zweite Zeile mit "Noch baubar" bzw. der Shop-Aufgabe (vorher im Notizblock der To-do-Liste)
	const FString Zweite = Einsatz->GetHudZeile();
	const FString Erste = TDF(TEXT("%s  |  Kern %.0f %%  |  %d Coupons  |  %d Festplatten  |  Rang %d%s"), TEXT("%s  |  Core %.0f %%  |  %d Coupons  |  %d Hard Drives  |  Rank %d%s"), *Welle,
		K ? 100.f * K->GetTDHealth() / FMath::Max(K->GetTDMaxHealth(), 1.f) : 0.f, Einsatz->Konto(), Einsatz->FPImInventar(),
		Pionier ? Pionier->GetRang() : 1, *Punkte);
	mText->SetText(FText::FromString(Zweite.IsEmpty() ? Erste : Erste + TEXT("\n") + Zweite));
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
		// links: rechts lag sie ueber der ToDo-Liste (Test 0.53), unten links sitzt der Chat.
		// 0.11 (Marius 17.11., W42 mit 10 Gegnerarten): mittig verankert wuchs sie nach oben unter die Statuszeile und nach
		// unten aus dem Bild. Jetzt ein fester Bereich von 9 % bis 74 % der Hoehe; die Leiste waechst von oben nach unten,
		// und was dann noch nicht passt, wird unten abgeschnitten statt aus dem Bild zu laufen.
		UVerticalBox* Bereich = WidgetTree->ConstructWidget<UVerticalBox>();
		Bereich->SetClipping(EWidgetClipping::ClipToBounds);
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Bereich))
		{
			S->SetAnchors(FAnchors(0.f, 0.09f, 0.f, 0.74f));
			S->SetAlignment(FVector2D(0.f, 0.f));
			S->SetOffsets(FMargin(20.f, 0.f, 274.f, 0.f));   // links, oben, Breite, unten
		}
		if (UVerticalBoxSlot* S = Bereich->AddChildToVerticalBox(Rahmen))
		{
			S->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
			S->SetHorizontalAlignment(HAlign_Fill);
			S->SetVerticalAlignment(VAlign_Top);
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
		// 0.12 (C1): Nachweis der Anzeigenamen fuer den Selbsttest (Plan Schadensleiste) - je Turmart und Sprache eine Zeile, einmal je Spielstart
		static TSet<FString> Gemeldet;
		const TCHAR* Sprache = TDSprache::Englisch() ? TEXT("en") : TEXT("de");
		for (const FTDTurmStat& T : Liste)
		{
			const FString Name = UTDEinsatzSubsystem::Anzeige(T.Art);
			const FString Merker = FString(Sprache) + T.Art;
			if (!Gemeldet.Contains(Merker))
			{
				Gemeldet.Add(Merker);
				UE_LOG(LogFicsitTD, Log, TEXT("Wellenleiste Schaden %s: %s (Schluessel %s)"), Sprache, *Name, *T.Art);
			}
		}
	}
	for (int32 i = 0; i < Liste.Num() && i < mSchadenTexte.Num(); ++i)
	{
		const FTDTurmStat& T = Liste[i];
		// 0.11: ohne "Schaden" (steht in der Ueberschrift), damit die Zeile meist einzeilig bleibt
		// 0.12 (C1): T.Art ist der deutsche Schluessel der Statistik - dem Spieler den Anzeigenamen zeigen
		mSchadenTexte[i]->SetText(TDFX(TEXT("%s (%d): %s · %.0f %% · %d Abschüsse"), TEXT("%s (%d): %s · %.0f %% · %d kills"), *UTDEinsatzSubsystem::Anzeige(T.Art), T.Anzahl,
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
	// Zeilen neu bauen, wenn sich die sichtbaren Arten aendern (neue Welle, Art besiegt), sonst nur die Zahlen.
	// 0.11 (Marius 17.11.): ab 8 Arten werden ganz besiegte Arten ausgeblendet und die Symbole kleiner, damit die Leiste ins Bild passt
	const bool bLang = S.Arten.Num() >= 8;
	const float Symbolgroesse = bLang ? 24.f : 32.f;
	TArray<FString> Arten;
	TArray<int32> Sichtbar;
	for (int32 i = 0; i < S.Arten.Num(); ++i)
	{
		const FTDArtStatus& A = S.Arten[i];
		if (bLang && !A.bBoss && A.Lebend + A.Offen == 0)
		{
			continue;
		}
		Sichtbar.Add(i);
		Arten.Add((A.bBoss ? TEXT("BOSS|") : TEXT("")) + A.Pfad);
	}
	if (Arten != mArten)
	{
		mArten = Arten;
		mSichtbar = Sichtbar;
		mListe->ClearChildren();
		mZeilen.Reset();
		for (const int32 Index : mSichtbar)
		{
			const FTDArtStatus& A = S.Arten[Index];
			const FTDKreaturSteckbrief B = UTDWaveSubsystem::Steckbrief(A.Pfad);
			UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
			USizeBox* Rahmen = WidgetTree->ConstructWidget<USizeBox>();
			Rahmen->SetWidthOverride(Symbolgroesse);
			Rahmen->SetHeightOverride(Symbolgroesse);
			if (UTexture2D* Bild = KreaturTextur(B.Deskriptor))
			{
				UImage* Symbol = WidgetTree->ConstructWidget<UImage>();
				Symbol->SetBrushFromTexture(Bild, false);
				Symbol->SetDesiredSizeOverride(FVector2D(Symbolgroesse, Symbolgroesse));
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
				Sl->SetPadding(FMargin(0.f, 0.f, 0.f, bLang ? 3.f : 6.f));
			}
		}
	}
	for (int32 z = 0; z < mSichtbar.Num() && z < mZeilen.Num(); ++z)
	{
		if (!S.Arten.IsValidIndex(mSichtbar[z]))
		{
			continue;
		}
		const FTDArtStatus& A = S.Arten[mSichtbar[z]];
		const int32 Weg = A.Gesamt - A.Lebend - A.Offen;
		const int32 i = z;
		// 0.11: kurz und einzeilig ("8 + 5 offen" = unterwegs + kommen noch); Kopfzeile erklaert die Zahlen ausfuehrlich
		mZeilen[i]->SetText(FText::FromString(A.Lebend + A.Offen == 0 ? TDF(TEXT("alle %d besiegt"), TEXT("all %d defeated"), A.Gesamt)
			: TDF(TEXT("%d + %d offen · %d / %d besiegt"), TEXT("%d + %d left · %d / %d defeated"), A.Lebend, A.Offen, Weg, A.Gesamt)));
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
			Spalte(Reihe, UTDEinsatzSubsystem::Anzeige(T.Art), 140.f, UTDWellenStatistik::Farbe(T.Art), true, false);
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
		UTextBlock* Kopfzeile = NeuerText(TDF(TEXT("Welle %d · %.0f s · %d Gegner · Kern %.0f %% auf %.0f %% · +%d Coupons, +%d Festplatten"),
			TEXT("Wave %d · %.0f s · %d enemies · Core %.0f %% to %.0f %% · +%d Coupons, +%d Hard Drives"),
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
			Spalte(Reihe, UTDEinsatzSubsystem::Anzeige(T.Art), Breiten[0], UTDWellenStatistik::Farbe(T.Art), true, false);
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

// ---------------------------------------------------------------------------------------------------------------------------
// 18.11.: Auswahl "Spielmodus" / "Stufe" im Menue "Neues Spiel"

static FString AuswahlBeschreibung(const FString& Id, int32 Wert)
{
	if (Id == UTDGameInstanceModule::ModusId)
	{
		switch (Wert)
		{
		case ETDModus::Aus: return TDT(TEXT("Normales Satisfactory, der Mod greift nicht ein."), TEXT("Regular Satisfactory, the mod stays out of the way."));
		case ETDModus::Einsatz: return TDT(TEXT("Abwehrkern an einem festen Platz, 50 Wellen in fünf Phasen. Deine Fabrik liefert die Munition."),
			TEXT("Defense Core at a fixed site, 50 waves in five phases. Your factory makes the ammo."));
		case ETDModus::Endlos: return TDT(TEXT("Wie Einsatz, aber Wellen auf Zeit aus einer Richtung, ohne Ende. Wie weit kommst du?"),
			TEXT("Like Operation, but timed waves from one direction that never stop. How far can you get?"));
		case ETDModus::Abwehr: return TDT(TEXT("Normales Satisfactory mit Türmen. Gekämpft wird nur, wenn du an einem Rohstoffknoten eine Bohrung startest."),
			TEXT("Regular Satisfactory with turrets. You only fight when you start a drilling at a resource node."));
		default: return FString();
		}
	}
	switch (Wert)
	{
	case 0: return TDT(TEXT("Grasfelder Erkundung, Felsenwüste Normal, Nördlicher Wald Hart, Dünenwüste Albtraum."),
		TEXT("Grass Fields Exploration, Rocky Desert Normal, Northern Forest Hard, Dune Desert Nightmare."));
	case 1: return TDT(TEXT("Ohne Druck, zum Kennenlernen."), TEXT("No pressure, for getting to know the mod."));
	case 2: return TDT(TEXT("Die vorgesehene Schwierigkeit."), TEXT("The intended difficulty."));
	case 3: return TDT(TEXT("Mehr Fronten, frühere Eliten. Noch nicht fertig eingestellt."), TEXT("More fronts, earlier elites. Not fully balanced yet."));
	default: return TDT(TEXT("Für Erfahrene. Noch nicht fertig eingestellt."), TEXT("For veterans. Not fully balanced yet."));
	}
}

TSharedRef<SWidget> UTDAuswahlKarten::RebuildWidget()
{
	// 18.11. (Marius, Screenshot): die Zeile im Menue hat eine feste Hoehe - eine Liste untereinander wurde abgeschnitten.
	// Deshalb alle Moeglichkeiten nebeneinander in einer Zeile; der Beschreibungssatz steht im Tooltip.
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UHorizontalBox* Reihe = WidgetTree->ConstructWidget<UHorizontalBox>();
		WidgetTree->RootWidget = Reihe;
		mReihe = Reihe;
	}
	return Super::RebuildWidget();
}

/** Kurzname fuer die Schaltflaeche: "Einsatz – Abwehrkern, 50 Wellen" -> "Einsatz", "Hart (ungetestet)" -> "Hart" */
static FString AuswahlKurzname(const FString& Name)
{
	FString Kurz = Name;
	int32 Pos = INDEX_NONE;
	if (Kurz.FindChar(TEXT('('), Pos))
	{
		Kurz = Kurz.Left(Pos);
	}
	Pos = Kurz.Find(TEXT(" –"));
	if (Pos != INDEX_NONE)
	{
		Kurz = Kurz.Left(Pos);
	}
	return Kurz.TrimStartAndEnd();
}

void UTDAuswahlKarten::Fuelle()
{
	const UFGUserSetting_IntSelector* Wahl = mUserSetting ? Cast<UFGUserSetting_IntSelector>(mUserSetting->ValueSelector) : nullptr;
	if (!mReihe || !Wahl || mWerte.Num() > 0)
	{
		return;
	}
	const FString Id = mUserSetting->StrId;
	for (const FIntegerSelection& W : Wahl->IntegerSelectionValues)
	{
		UButton* Knopf = WidgetTree->ConstructWidget<UButton>();
		FButtonStyle Stil;
		Stil.SetNormal(FSlateNoResource());
		Stil.SetHovered(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 30), 3.f));
		Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 50), 3.f));
		Stil.SetNormalPadding(FMargin(0.f));
		Stil.SetPressedPadding(FMargin(0.f));
		Knopf->SetStyle(Stil);
		const FString Satz = AuswahlBeschreibung(Id, W.Value);
		Knopf->SetToolTipText(FText::FromString(Satz.IsEmpty() ? W.Name.ToString() : W.Name.ToString() + TEXT(": ") + Satz));
		UBorder* Feld = WidgetTree->ConstructWidget<UBorder>();
		Feld->SetBrush(FSlateRoundedBoxBrush(TDUI::C(58, 58, 58, 230), 3.f));
		Feld->SetPadding(FMargin(10.f, 1.f));
		Feld->SetHorizontalAlignment(HAlign_Center);
		Feld->SetVerticalAlignment(VAlign_Center);
		UTextBlock* Name = TDUI::Text(WidgetTree, AuswahlKurzname(W.Name.ToString()), 15, TDUI::Hell, true);
		Feld->AddChild(Name);
		Knopf->AddChild(Feld);
		if (UHorizontalBoxSlot* Platz = mReihe->AddChildToHorizontalBox(Knopf))
		{
			Platz->SetPadding(FMargin(2.f, 0.f));
			Platz->SetVerticalAlignment(VAlign_Center);
		}
		UTDAuswahlKlick* Klick = NewObject<UTDAuswahlKlick>(this);
		Klick->mAuswahl = this;
		Klick->mWert = W.Value;
		Knopf->OnClicked.AddDynamic(Klick, &UTDAuswahlKlick::Klick);
		mKlicks.Add(Klick);
		mFelder.Add(Feld);
		mNamen.Add(Name);
		mWerte.Add(W.Value);
	}
	mGezeigt = INDEX_NONE;
}

int32 UTDAuswahlKarten::AktuellerWert() const
{
	const UFGUserSetting_IntSelector* Wahl = mUserSetting ? Cast<UFGUserSetting_IntSelector>(mUserSetting->ValueSelector) : nullptr;
	const int32 Standard = Wahl ? Wahl->DefaultValue : 0;
	if (IFGOptionInterface* Optionen = mOptionInterface.GetInterface())
	{
		const FVariant V = Optionen->GetOptionValue(GetSettingIdentifier(), FVariant(Standard));
		return V.GetType() == EVariantTypes::Int32 ? V.GetValue<int32>() : Standard;
	}
	return Standard;
}

void UTDAuswahlKarten::Waehle(int32 Wert)
{
	if (IFGOptionInterface* Optionen = mOptionInterface.GetInterface())
	{
		Optionen->SetOptionValue(GetSettingIdentifier(), FVariant(Wert));
		UE_LOG(LogFicsitTD, Log, TEXT("Auswahl %s: %d"), *GetSettingIdentifier(), Wert);
	}
	mGezeigt = INDEX_NONE;
}

void UTDAuswahlKarten::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Fuelle();
	mTakt -= InDeltaTime;
	if (mTakt > 0.f && mGezeigt != INDEX_NONE)
	{
		return;
	}
	mTakt = 0.2f;
	const int32 Wert = AktuellerWert();
	if (Wert == mGezeigt)
	{
		return;
	}
	mGezeigt = Wert;
	for (int32 i = 0; i < mWerte.Num(); ++i)
	{
		const bool bGewaehlt = mWerte[i] == Wert;
		mFelder[i]->SetBrush(FSlateRoundedBoxBrush(bGewaehlt ? TDUI::Akzent : TDUI::C(58, 58, 58, 230), 3.f));
		mNamen[i]->SetColorAndOpacity(FSlateColor(bGewaehlt ? TDUI::DunkelText : TDUI::Hell));
	}
}

// ---------------------------------------------------------------------------------------------------------------------------
// 18.11.: Startfenster nach der Landung

static const int32 GTDStartModi[] = { ETDModus::Einsatz, ETDModus::Endlos, ETDModus::Abwehr, ETDModus::Aus };

static FString StartModusName(int32 Modus)
{
	switch (Modus)
	{
	case ETDModus::Einsatz: return TDT(TEXT("Einsatz"), TEXT("Operation"));
	case ETDModus::Endlos: return TDT(TEXT("Endlos-Wellen"), TEXT("Endless waves"));
	case ETDModus::Abwehr: return TDT(TEXT("Freies Spiel mit Abwehr"), TEXT("Free play with defense"));
	default: return TDT(TEXT("Aus"), TEXT("Off"));
	}
}

static FString StartModusText(int32 Modus)
{
	switch (Modus)
	{
	case ETDModus::Einsatz: return TDT(TEXT("Dein Abwehrkern steht an einem festen Platz. 50 Wellen in fünf Phasen mit Bossen. Deine Fabrik liefert die Munition, Gebäude kosten Kontingent, geforscht wird im MAM."),
		TEXT("Your Defense Core stands at a fixed site. 50 waves in five phases with bosses. Your factory makes the ammo, buildings cost quota, research happens in the MAM."));
	case ETDModus::Endlos: return TDT(TEXT("Wie Einsatz, aber die Wellen kommen auf Zeit und hören nie auf, immer aus einer Richtung. Wie weit kommst du?"),
		TEXT("Like Operation, but the waves come on a timer and never stop, always from one direction. How far can you get?"));
	case ETDModus::Abwehr: return TDT(TEXT("Normales Satisfactory mit HUB und Meilensteinen, dazu Türme und Abwehr-Forschung. Gekämpft wird nur, wenn du an einem Rohstoffknoten eine Bohrung startest."),
		TEXT("Regular Satisfactory with HUB and milestones, plus turrets and defense research. You only fight when you start a drilling at a resource node."));
	default: return TDT(TEXT("Normales Satisfactory, der Mod greift nicht ein."), TEXT("Regular Satisfactory, the mod stays out of the way."));
	}
}

static FString StartStufeName(int32 Stufe)
{
	switch (Stufe)
	{
	case 0: return TDT(TEXT("Nach Startort"), TEXT("By start location"));
	case 1: return TDT(TEXT("Erkundung"), TEXT("Exploration"));
	case 2: return TDT(TEXT("Normal"), TEXT("Normal"));
	case 3: return TDT(TEXT("Hart"), TEXT("Hard"));
	default: return TDT(TEXT("Albtraum"), TEXT("Nightmare"));
	}
}

TSharedRef<SWidget> UTDStartfenster::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const USessionSettingsManager* E = World->GetSubsystem<USessionSettingsManager>())
			{
				mModus = E->GetIntOptionValue(UTDGameInstanceModule::ModusId);
				mStufe = E->GetIntOptionValue(UTDGameInstanceModule::StufeId);
			}
		}
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		UBorder* Dunkel = WidgetTree->ConstructWidget<UBorder>();
		Dunkel->SetBrush(FSlateColorBrush(TDUI::C(0, 0, 0, 150)));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Dunkel))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}
		UBorder* Fenster = WidgetTree->ConstructWidget<UBorder>();
		Fenster->SetBrush(FSlateRoundedBoxBrush(TDUI::Fenster, 6.f));
		Fenster->SetPadding(FMargin(0.f));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Fenster))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetAutoSize(true);
		}
		USizeBox* Breite = WidgetTree->ConstructWidget<USizeBox>();
		Breite->SetWidthOverride(1180.f);
		Fenster->AddChild(Breite);
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Breite->AddChild(Spalte);

		// Kopfleiste
		UBorder* Kopf = WidgetTree->ConstructWidget<UBorder>();
		Kopf->SetBrush(FSlateRoundedBoxBrush(TDUI::Kopfleiste, 6.f));
		Kopf->SetPadding(FMargin(20.f, 12.f));
		Kopf->AddChild(TDUI::Text(WidgetTree, TDT(TEXT("FICSIT Tower Defense – Wie willst du spielen?"), TEXT("FICSIT Tower Defense – How do you want to play?")), 22, TDUI::Hell, true));
		Spalte->AddChildToVerticalBox(Kopf);

		UVerticalBox* Inhalt = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Inhalt))
		{
			S->SetPadding(FMargin(20.f, 16.f, 20.f, 20.f));
		}
		UTextBlock* Hinweis = TDUI::Text(WidgetTree, TDT(TEXT("Wähle einen Spielmodus. Du kannst ihn nur jetzt festlegen; für einen anderen Modus startest du später ein neues Spiel."),
			TEXT("Choose a game mode. You set it only now; for another mode, start a new game later.")), 14, TDUI::Grau);
		Hinweis->SetAutoWrapText(true);
		Inhalt->AddChildToVerticalBox(Hinweis);

		auto NeuerKlick = [this](UButton* Knopf, int32 Art, int32 Wert)
		{
			UTDStartKlick* K = NewObject<UTDStartKlick>(this);
			K->mFenster = this;
			K->mArt = Art;
			K->mWert = Wert;
			Knopf->OnClicked.AddDynamic(K, &UTDStartKlick::Klick);
			mKlicks.Add(K);
		};
		auto LeererStil = []()
		{
			FButtonStyle Stil;
			Stil.SetNormal(FSlateNoResource());
			Stil.SetHovered(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 18), 4.f));
			Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 36), 4.f));
			Stil.SetNormalPadding(FMargin(0.f));
			Stil.SetPressedPadding(FMargin(0.f));
			return Stil;
		};

		// vier Modus-Karten
		UHorizontalBox* Karten = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(Karten))
		{
			S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}
		for (const int32 Modus : GTDStartModi)
		{
			UButton* Knopf = WidgetTree->ConstructWidget<UButton>();
			Knopf->SetStyle(LeererStil());
			UBorder* Feld = WidgetTree->ConstructWidget<UBorder>();
			Feld->SetPadding(FMargin(16.f, 14.f));
			Feld->SetVerticalAlignment(VAlign_Top);
			USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
			Hoehe->SetMinDesiredHeight(170.f);
			UVerticalBox* Karte = WidgetTree->ConstructWidget<UVerticalBox>();
			UTextBlock* Name = TDUI::Text(WidgetTree, StartModusName(Modus), 20, TDUI::Hell, true);
			Name->SetAutoWrapText(true);
			Karte->AddChildToVerticalBox(Name);
			UTextBlock* Text = TDUI::Text(WidgetTree, StartModusText(Modus), 14, TDUI::C(210, 210, 210));
			Text->SetAutoWrapText(true);
			if (UVerticalBoxSlot* S = Karte->AddChildToVerticalBox(Text))
			{
				S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			}
			Hoehe->AddChild(Karte);
			Feld->AddChild(Hoehe);
			Knopf->AddChild(Feld);
			if (UButtonSlot* Innen = Cast<UButtonSlot>(Feld->Slot))
			{
				Innen->SetHorizontalAlignment(HAlign_Fill);
				Innen->SetVerticalAlignment(VAlign_Fill);
				Innen->SetPadding(FMargin(0.f));
			}
			if (UHorizontalBoxSlot* S = Karten->AddChildToHorizontalBox(Knopf))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetPadding(FMargin(0.f, 0.f, Modus == ETDModus::Aus ? 0.f : 10.f, 0.f));
				S->SetVerticalAlignment(VAlign_Fill);   // alle Karten gleich hoch
			}
			NeuerKlick(Knopf, 0, Modus);
			mModusFelder.Add(Feld);
			mModusNamen.Add(Name);
		}

		// Stufe
		UVerticalBox* StufenBox = WidgetTree->ConstructWidget<UVerticalBox>();
		mStufenBereich = StufenBox;
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(StufenBox))
		{
			S->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
		}
		StufenBox->AddChildToVerticalBox(TDUI::Text(WidgetTree, TDT(TEXT("Stufe"), TEXT("Difficulty")), 16, TDUI::Akzent, true));
		UHorizontalBox* Stufen = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* S = StufenBox->AddChildToVerticalBox(Stufen))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}
		for (int32 Stufe = 0; Stufe <= 4; ++Stufe)
		{
			UButton* Knopf = WidgetTree->ConstructWidget<UButton>();
			Knopf->SetStyle(LeererStil());
			UBorder* Feld = WidgetTree->ConstructWidget<UBorder>();
			Feld->SetPadding(FMargin(16.f, 6.f));
			UTextBlock* Name = TDUI::Text(WidgetTree, StartStufeName(Stufe), 16, TDUI::Hell, true);
			Feld->AddChild(Name);
			Knopf->AddChild(Feld);
			if (UHorizontalBoxSlot* S = Stufen->AddChildToHorizontalBox(Knopf))
			{
				S->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
			}
			NeuerKlick(Knopf, 1, Stufe);
			mStufenFelder.Add(Feld);
			mStufenNamen.Add(Name);
		}
		mStufenText = TDUI::Text(WidgetTree, FString(), 13, TDUI::Grau);
		mStufenText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* S = StufenBox->AddChildToVerticalBox(mStufenText))
		{
			S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
		}

		// Los
		UButton* Los = WidgetTree->ConstructWidget<UButton>();
		Los->SetStyle(TDUI::KnopfStil(true));
		Los->AddChild(TDUI::Text(WidgetTree, TDT(TEXT("Los geht's"), TEXT("Let's go")), 18, TDUI::DunkelText, true));
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(Los))
		{
			S->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
			S->SetHorizontalAlignment(HAlign_Right);
		}
		NeuerKlick(Los, 2, 0);
		Zeige();
	}
	return Super::RebuildWidget();
}

void UTDStartfenster::Zeige()
{
	for (int32 i = 0; i < mModusFelder.Num(); ++i)
	{
		const bool bGewaehlt = GTDStartModi[i] == mModus;
		mModusFelder[i]->SetBrush(bGewaehlt ? FSlateRoundedBoxBrush(TDUI::C(64, 46, 30), 4.f, TDUI::Akzent, 3.f) : FSlateRoundedBoxBrush(TDUI::Feld, 4.f, TDUI::C(70, 70, 70), 1.f));
		mModusNamen[i]->SetColorAndOpacity(FSlateColor(bGewaehlt ? TDUI::Akzent : TDUI::Hell));
	}
	for (int32 i = 0; i < mStufenFelder.Num(); ++i)
	{
		const bool bGewaehlt = i == mStufe;
		mStufenFelder[i]->SetBrush(FSlateRoundedBoxBrush(bGewaehlt ? TDUI::Akzent : TDUI::Reiter, 3.f));
		mStufenNamen[i]->SetColorAndOpacity(FSlateColor(bGewaehlt ? TDUI::DunkelText : TDUI::Hell));
	}
	// Stufe nur fuer Einsatz und Endlos
	const bool bStufe = mModus == ETDModus::Einsatz || mModus == ETDModus::Endlos;
	if (mStufenBereich)
	{
		mStufenBereich->SetVisibility(bStufe ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (mStufenText)
	{
		mStufenText->SetText(FText::FromString(AuswahlBeschreibung(UTDGameInstanceModule::StufeId, mStufe)));
	}
}

void UTDStartfenster::Waehle(int32 Art, int32 Wert)
{
	if (Art == 0)
	{
		mModus = Wert;
	}
	else if (Art == 1)
	{
		mStufe = Wert;
	}
	else if (mKernplatz)
	{
		mKernplatz->StartfensterGewaehlt(mModus, mStufe);
		return;
	}
	Zeige();
}

void UTDStartfenster::NativeConstruct()
{
	Super::NativeConstruct();
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
		FInputModeUIOnly Modus;
		Modus.SetWidgetToFocus(TakeWidget());
		Modus.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Modus);
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
	}
}

void UTDStartfenster::NativeDestruct()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->ResetIgnoreLookInput();
		PC->ResetIgnoreMoveInput();
	}
	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------------------------------------------------------
// 18.11.: Fortschritt der Vorbereitung (Kernplatz anfliegen, Gelaende, Plattform, Netz)

static FString VorbereitungSchritt(int32 i)
{
	switch (i)
	{
	case 0: return TDT(TEXT("Einsatzort anfliegen"), TEXT("Flying to the operation site"));
	case 1: return TDT(TEXT("Gelände laden"), TEXT("Loading the terrain"));
	case 2: return TDT(TEXT("Plattform und Abwehrkern bauen"), TEXT("Building platform and Defense Core"));
	default: return TDT(TEXT("Wege der Gegner berechnen (bis zu 60 s)"), TEXT("Calculating enemy paths (up to 60 s)"));
	}
}

TSharedRef<SWidget> UTDVorbereitung::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		UBorder* Rahmen = WidgetTree->ConstructWidget<UBorder>();
		Rahmen->SetBrush(FSlateRoundedBoxBrush(TDUI::C(24, 24, 24, 235), 6.f, TDUI::Akzent, 2.f));
		Rahmen->SetPadding(FMargin(22.f, 14.f));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Rahmen))
		{
			S->SetAnchors(FAnchors(0.5f, 0.16f));
			S->SetAlignment(FVector2D(0.5f, 0.f));
			S->SetAutoSize(true);
		}
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Rahmen->AddChild(Spalte);
		mTitel = TDUI::Text(WidgetTree, TDT(TEXT("FICSIT Tower Defense: Einsatzort wird vorbereitet"), TEXT("FICSIT Tower Defense: preparing your operation site")), 20, TDUI::Akzent, true);
		Spalte->AddChildToVerticalBox(mTitel);
		for (int32 i = 0; i < 4; ++i)
		{
			UTextBlock* Zeile = TDUI::Text(WidgetTree, VorbereitungSchritt(i), 16, TDUI::Grau);
			if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Zeile))
			{
				S->SetPadding(FMargin(0.f, i == 0 ? 10.f : 4.f, 0.f, 0.f));
			}
			mZeilen.Add(Zeile);
		}
		UTextBlock* Hinweis = TDUI::Text(WidgetTree, TDT(TEXT("Du kannst dich schon umsehen. Danach meldet sich ADA."), TEXT("Feel free to look around. ADA will brief you afterwards.")), 13, TDUI::Grau);
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Hinweis))
		{
			S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		}
		Zeige();
	}
	return Super::RebuildWidget();
}

void UTDVorbereitung::SetzeSchritt(int32 Schritt)
{
	if (Schritt != mSchritt)
	{
		mSekunden = 0.f;
	}
	mSchritt = Schritt;
	Zeige();
}

void UTDVorbereitung::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	mSekunden += InDeltaTime;
	mGesamt += InDeltaTime;
	mTakt -= InDeltaTime;
	if (mTakt <= 0.f)
	{
		mTakt = 0.25f;
		Zeige();
	}
}

void UTDVorbereitung::Zeige()
{
	if (mZeilen.Num() < 4)
	{
		return;
	}
	const int32 Punkte = 1 + (static_cast<int32>(mGesamt * 2.f) % 3);
	for (int32 i = 0; i < 4; ++i)
	{
		if (i < mSchritt)
		{
			mZeilen[i]->SetText(FText::FromString(TDT(TEXT("Erledigt: "), TEXT("Done: ")) + VorbereitungSchritt(i)));
			mZeilen[i]->SetColorAndOpacity(FSlateColor(TDUI::Gut));
		}
		else if (i == mSchritt)
		{
			mZeilen[i]->SetText(FText::FromString(VorbereitungSchritt(i) + FString(TEXT(" ")) + FString::ChrN(Punkte, TEXT('.'))
				+ FString::Printf(TEXT("  %.0f s"), mSekunden)));
			mZeilen[i]->SetColorAndOpacity(FSlateColor(TDUI::Akzent));
		}
		else
		{
			mZeilen[i]->SetText(FText::FromString(VorbereitungSchritt(i)));
			mZeilen[i]->SetColorAndOpacity(FSlateColor(TDUI::Grau));
		}
	}
	if (mTitel && mSchritt >= 4)
	{
		mTitel->SetText(FText::FromString(TDT(TEXT("Bereit! Dein Einsatz beginnt."), TEXT("Ready! Your operation begins."))));
		mTitel->SetColorAndOpacity(FSlateColor(TDUI::Gut));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// 21.11. (Marius, C11): Bonusfenster nach der Bosswelle
// ---------------------------------------------------------------------------------------------------------------

void UTDBonusFenster::Zeige(UObject* Kontext, int32 Welle, bool bEndlos)
{
	UWorld* World = Kontext ? Kontext->GetWorld() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC || !PC->IsLocalController() || TDBalance::Wert(TEXT("Einsatz.Bonus.Fenster"), 1.f) < 1.f)
	{
		return;
	}
	UTDBonusFenster* Fenster = CreateWidget<UTDBonusFenster>(PC, UTDBonusFenster::StaticClass());
	if (!Fenster)
	{
		return;
	}
	Fenster->mWelle = Welle;
	Fenster->mEndlos = bEndlos;
	Fenster->AddToViewport(60);
	UE_LOG(LogFicsitTD, Log, TEXT("Bonusfenster: geoeffnet nach Bosswelle %d"), Welle);
}

TSharedRef<SWidget> UTDBonusFenster::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr;
		UCanvasPanel* Flaeche = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = Flaeche;
		UBorder* Dunkel = WidgetTree->ConstructWidget<UBorder>();
		Dunkel->SetBrush(FSlateColorBrush(TDUI::C(0, 0, 0, 120)));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Dunkel))
		{
			S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			S->SetOffsets(FMargin(0.f));
		}
		UBorder* Fenster = WidgetTree->ConstructWidget<UBorder>();
		Fenster->SetBrush(FSlateRoundedBoxBrush(TDUI::Fenster, 6.f));
		Fenster->SetPadding(FMargin(0.f));
		if (UCanvasPanelSlot* S = Flaeche->AddChildToCanvas(Fenster))
		{
			S->SetAnchors(FAnchors(0.5f, 0.5f));
			S->SetAlignment(FVector2D(0.5f, 0.5f));
			S->SetAutoSize(true);
		}
		USizeBox* Breite = WidgetTree->ConstructWidget<USizeBox>();
		Breite->SetWidthOverride(980.f);
		Fenster->AddChild(Breite);
		UVerticalBox* Spalte = WidgetTree->ConstructWidget<UVerticalBox>();
		Breite->AddChild(Spalte);

		UBorder* Kopf = WidgetTree->ConstructWidget<UBorder>();
		Kopf->SetBrush(FSlateRoundedBoxBrush(TDUI::Kopfleiste, 6.f));
		Kopf->SetPadding(FMargin(20.f, 12.f));
		Kopf->AddChild(TDUI::Text(WidgetTree, mEndlos
			? TDF(TEXT("Bosswelle %d geschafft – wähle einen Bonus"), TEXT("Boss wave %d cleared – pick a bonus"), mWelle)
			: TDF(TEXT("Phase %d geschafft – wähle einen Bonus"), TEXT("Phase %d complete – pick a bonus"), FMath::Max(1, mWelle / 10)), 22, TDUI::Hell, true));
		Spalte->AddChildToVerticalBox(Kopf);

		UVerticalBox* Inhalt = WidgetTree->ConstructWidget<UVerticalBox>();
		if (UVerticalBoxSlot* S = Spalte->AddChildToVerticalBox(Inhalt))
		{
			S->SetPadding(FMargin(20.f, 14.f, 20.f, 20.f));
		}
		if (Einsatz && !mEndlos)
		{
			for (const FString& Zeile : Einsatz->PhasenBericht(FMath::Max(1, mWelle - 9), mWelle))
			{
				UTextBlock* T = TDUI::Text(WidgetTree, Zeile, 14, TDUI::C(210, 210, 210));
				T->SetAutoWrapText(true);
				Inhalt->AddChildToVerticalBox(T);
			}
		}
		UTextBlock* Hinweis = TDUI::Text(WidgetTree, TDT(TEXT("Dazu gibt es einen Talentpunkt (Kern-Menü, Reiter Pionier)."), TEXT("You also get a Talent Point (Core Menu, Pioneer tab).")), 14, TDUI::Grau);
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(Hinweis))
		{
			S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}

		auto NeuerKlick = [this](UButton* Knopf, int32 Nr)
		{
			UTDBonusKlick* K = NewObject<UTDBonusKlick>(this);
			K->mFenster = this;
			K->mNr = Nr;
			Knopf->OnClicked.AddDynamic(K, &UTDBonusKlick::Klick);
			mKlicks.Add(K);
		};
		static const TCHAR* TexteDe[] = { TEXT("Sofort mehr Geld für Kontingente und Ausrüstung."), TEXT("Der Kern hält dauerhaft mehr aus."), TEXT("Mehr Forschung im MAM.") };
		static const TCHAR* TexteEn[] = { TEXT("More money right away for quotas and gear."), TEXT("The Core permanently takes more damage."), TEXT("More research in the MAM.") };
		UHorizontalBox* Karten = WidgetTree->ConstructWidget<UHorizontalBox>();
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(Karten))
		{
			S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
		}
		for (int32 Nr = 0; Nr < 3; ++Nr)
		{
			UButton* Knopf = WidgetTree->ConstructWidget<UButton>();
			FButtonStyle Stil;
			Stil.SetNormal(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 14), 4.f));
			Stil.SetHovered(FSlateRoundedBoxBrush(TDUI::C(250, 149, 73, 90), 4.f));
			Stil.SetPressed(FSlateRoundedBoxBrush(TDUI::C(250, 149, 73, 150), 4.f));
			Stil.SetNormalPadding(FMargin(0.f));
			Stil.SetPressedPadding(FMargin(0.f));
			Knopf->SetStyle(Stil);
			UVerticalBox* Karte = WidgetTree->ConstructWidget<UVerticalBox>();
			UTextBlock* Name = TDUI::Text(WidgetTree, UTDEinsatzSubsystem::EndlosBonusName(Nr), 20, TDUI::Hell, true);
			Name->SetAutoWrapText(true);
			Karte->AddChildToVerticalBox(Name);
			UTextBlock* Text = TDUI::Text(WidgetTree, TDT(TexteDe[Nr], TexteEn[Nr]), 14, TDUI::C(210, 210, 210));
			Text->SetAutoWrapText(true);
			if (UVerticalBoxSlot* S = Karte->AddChildToVerticalBox(Text))
			{
				S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
			}
			UBorder* Feld = WidgetTree->ConstructWidget<UBorder>();
			Feld->SetBrush(FSlateNoResource());
			Feld->SetPadding(FMargin(16.f, 14.f));
			USizeBox* Hoehe = WidgetTree->ConstructWidget<USizeBox>();
			Hoehe->SetMinDesiredHeight(120.f);
			Hoehe->AddChild(Karte);
			Feld->AddChild(Hoehe);
			Knopf->AddChild(Feld);
			if (UHorizontalBoxSlot* S = Karten->AddChildToHorizontalBox(Knopf))
			{
				S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				S->SetPadding(FMargin(0.f, 0.f, Nr < 2 ? 10.f : 0.f, 0.f));
				S->SetVerticalAlignment(VAlign_Fill);
			}
			NeuerKlick(Knopf, Nr);
		}
		UButton* Spaeter = WidgetTree->ConstructWidget<UButton>();
		FButtonStyle Grau;
		Grau.SetNormal(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 10), 4.f));
		Grau.SetHovered(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 30), 4.f));
		Grau.SetPressed(FSlateRoundedBoxBrush(TDUI::C(255, 255, 255, 50), 4.f));
		Spaeter->SetStyle(Grau);
		Spaeter->AddChild(TDUI::Text(WidgetTree, TDT(TEXT("Später wählen (Kern-Menü)"), TEXT("Pick later (Core Menu)")), 14, TDUI::Grau));
		if (UVerticalBoxSlot* S = Inhalt->AddChildToVerticalBox(Spaeter))
		{
			S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
			S->SetHorizontalAlignment(HAlign_Right);
		}
		NeuerKlick(Spaeter, -1);
	}
	return Super::RebuildWidget();
}

void UTDBonusFenster::NativeConstruct()
{
	Super::NativeConstruct();
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(true);
		FInputModeUIOnly Modus;
		Modus.SetWidgetToFocus(TakeWidget());
		Modus.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Modus);
		PC->SetIgnoreLookInput(true);
		PC->SetIgnoreMoveInput(true);
	}
}

void UTDBonusFenster::NativeDestruct()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetShowMouseCursor(false);
		PC->SetInputMode(FInputModeGameOnly());
		PC->ResetIgnoreLookInput();
		PC->ResetIgnoreMoveInput();
	}
	Super::NativeDestruct();
}

void UTDBonusFenster::Waehle(int32 Nr)
{
	if (Nr >= 0)
	{
		if (UTDEinsatzSubsystem* Einsatz = GetWorld() ? GetWorld()->GetSubsystem<UTDEinsatzSubsystem>() : nullptr)
		{
			Einsatz->WaehleEndlosBonus(Nr);
		}
	}
	UE_LOG(LogFicsitTD, Log, TEXT("Bonusfenster: %s"), Nr >= 0 ? *FString::Printf(TEXT("Bonus %d gewaehlt"), Nr) : TEXT("spaeter"));
	RemoveFromParent();
}
