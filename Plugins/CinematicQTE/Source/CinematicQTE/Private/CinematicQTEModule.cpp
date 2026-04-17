// Copyright Cinematic QTE System. All Rights Reserved.

#include "CinematicQTEModule.h"
#include "CinematicQTETypes.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogCinematicQTE);

// Console variables for debug
TAutoConsoleVariable<int32> CVarQTEDebugShow(
	TEXT("qte.Debug.Show"),
	0,
	TEXT("Display current QTE debug info on screen. 0=Off, 1=On"),
	ECVF_Cheat);

TAutoConsoleVariable<FString> CVarQTEDebugForceResult(
	TEXT("qte.Debug.ForceResult"),
	TEXT(""),
	TEXT("Force the next QTE to finish with a specified result. Values: Success, Failure, Cancelled, Timeout, (empty)=disabled"),
	ECVF_Cheat);

void FCinematicQTEModule::StartupModule()
{
	UE_LOG(LogCinematicQTE, Log, TEXT("CinematicQTE module started."));
}

void FCinematicQTEModule::ShutdownModule()
{
	UE_LOG(LogCinematicQTE, Log, TEXT("CinematicQTE module shutdown."));
}

IMPLEMENT_MODULE(FCinematicQTEModule, CinematicQTE);
