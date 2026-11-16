// Erzeugt von Scripts/erzeuge_anbauten.py aus Satisfactory/Modelle/Anbauten.json (Blender: td_anbauten.py, td_klarraum.py).
// Nicht von Hand aendern. Optik-Durchgang 16.11.2026: Statuslampe auf der Konsole im Sockel, Stromanschluss im Anschlusskasten,
// Klarraum = Sockel + Anbauten + Original-Eingang (asymmetrisch, cm, Bauteilraum).
#pragma once

struct FTDAnbau { const TCHAR* Turm; FVector Lampe; FVector Strom; FVector2D KlarraumMin; FVector2D KlarraumMax; };

static const FTDAnbau TDAnbauTabelle[] = {
	{ TEXT("Bolzenturm"), FVector(-85.0f, 174.3f, 214.3f), FVector(-0.0f, -184.3f, 188.0f), FVector2D(-250.0f, -210.0f), FVector2D(210.0f, 210.0f) },
	{ TEXT("Drohnenplatz"), FVector(-300.0f, 148.3f, 209.3f), FVector(250.0f, 324.3f, 98.0f), FVector2D(-390.0f, -310.0f), FVector2D(310.0f, 330.0f) },
	{ TEXT("Elektrozaun"), FVector(-380.0f, -44.3f, 209.3f), FVector(-0.0f, -54.3f, 98.0f), FVector2D(-400.0f, -60.0f), FVector2D(400.0f, 60.0f) },
	{ TEXT("Flammenwerfer"), FVector(-189.3f, 105.0f, 249.3f), FVector(-45.0f, -199.3f, 110.0f), FVector2D(-230.0f, -220.0f), FVector2D(220.0f, 220.0f) },
	{ TEXT("Funkmast"), FVector(-79.3f, 115.0f, 109.2f), FVector(124.3f, 30.0f, 103.0f), FVector2D(-190.0f, -190.0f), FVector2D(190.0f, 190.0f) },
	{ TEXT("Gewehrturm"), FVector(-100.0f, 205.3f, 234.3f), FVector(-85.0f, -209.3f, 110.0f), FVector2D(-280.0f, -230.0f), FVector2D(230.0f, 230.0f) },
	{ TEXT("Himmelsturm"), FVector(-85.0f, 174.3f, 214.3f), FVector(-0.0f, -184.3f, 188.0f), FVector2D(-250.0f, -210.0f), FVector2D(210.0f, 210.0f) },
	{ TEXT("Lockfeuer"), FVector(119.3f, 100.0f, 79.3f), FVector(-70.0f, -148.3f, 98.0f), FVector2D(-200.0f, -150.0f), FVector2D(140.0f, 130.0f) },
	{ TEXT("Nobelisk-Werfer"), FVector(-105.0f, 189.3f, 234.3f), FVector(-45.0f, -199.3f, 110.0f), FVector2D(-270.0f, -220.0f), FVector2D(220.0f, 220.0f) },
	{ TEXT("Oelschlick"), FVector(380.0f, -133.3f, 169.2f), FVector(350.0f, 143.3f, 98.0f), FVector2D(-410.0f, -410.0f), FVector2D(510.0f, 410.0f) },
	{ TEXT("Schildgenerator"), FVector(-314.3f, 290.0f, 169.2f), FVector(-175.0f, -319.3f, 108.0f), FVector2D(-330.0f, -320.0f), FVector2D(310.0f, 310.0f) },
	{ TEXT("Singularitaet"), FVector(-124.3f, 310.0f, 114.3f), FVector(-240.0f, -324.3f, 103.0f), FVector2D(-390.0f, -350.0f), FVector2D(310.0f, 350.0f) },
	{ TEXT("Stoerfeld"), FVector(-85.0f, 174.3f, 214.3f), FVector(-0.0f, -184.3f, 128.0f), FVector2D(-250.0f, -210.0f), FVector2D(220.0f, 250.0f) },
	{ TEXT("Teslaspule"), FVector(-90.0f, -90.0f, 170.3f), FVector(-90.0f, -118.1f, 133.9f), FVector2D(-210.0f, -210.0f), FVector2D(210.0f, 210.0f) },
	{ TEXT("Tor"), FVector(0.0f, -0.0f, 522.0f), FVector(-300.0f, -112.3f, 158.0f), FVector2D(-400.0f, -120.0f), FVector2D(400.0f, 100.0f) },
};
