/* Copyright (C) 2021 Hugo ATTAL - All Rights Reserved
* This plugin is downloadable from the UE4 Marketplace
*/

#include "ElectronicNodes.h"
#include "ENConnectionDrawingPolicy.h"
#include "ENCommands.h"
#include "Editor.h"
#include "Engine/World.h"
#include "NodeFactory.h"
#include "Interfaces/IPluginManager.h"
#include "Lib/HotPatch.h"
#include "Interfaces/IMainFrameModule.h"
#include "Patch/NodeFactoryPatch.h"
#include "Popup/ENUpdatePopup.h"
#include "ISettingsEditorModule.h"
#include "RigVMEditorAsset.h"
#include "RigVMHost.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "FElectronicNodesModule"

void FElectronicNodesModule::StartupModule()
{
	const TSharedPtr<FENConnectionDrawingPolicyFactory> ENConnectionFactory = MakeShareable(new FENConnectionDrawingPolicyFactory);
	FEdGraphUtilities::RegisterVisualPinConnectionFactory(ENConnectionFactory);

	auto const CommandBindings = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame").GetMainFrameCommandBindings();
	ENCommands::Register();

	CommandBindings->MapAction(
		ENCommands::Get().ToggleMasterActivation,
		FExecuteAction::CreateRaw(this, &FElectronicNodesModule::ToggleMasterActivation)
	);

	PluginDirectory = IPluginManager::Get().FindPlugin(TEXT("ElectronicNodes"))->GetBaseDir();
	GlobalSettingsFile = PluginDirectory + "/Settings.ini";

	ElectronicNodesSettings = GetMutableDefault<UElectronicNodesSettings>();
	ElectronicNodesSettings->OnSettingChanged().AddRaw(this, &FElectronicNodesModule::ReloadConfiguration);

	if (ElectronicNodesSettings->UseGlobalSettings)
	{
		if (FPaths::FileExists(GlobalSettingsFile))
		{
			ElectronicNodesSettings->LoadConfig(nullptr, *GlobalSettingsFile);
		}
	}

	if (ElectronicNodesSettings->UseHotPatch && ElectronicNodesSettings->MasterActivate)
	{
#if PLATFORM_WINDOWS && !UE_BUILD_SHIPPING
		FHotPatch::Hook(&FNodeFactory::CreateConnectionPolicy, &FNodeFactoryPatch::CreateConnectionPolicy_Hook);
#endif
	}

	if (ElectronicNodesSettings->ActivatePopupOnUpdate)
	{
		ENUpdatePopup::Register();
	}

	FEditorDelegates::PrePIEEnded.AddRaw(this, &FElectronicNodesModule::HandlePrePIEEnded);
}

#if ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 25
void FElectronicNodesModule::ReloadConfiguration(FName PropertyName)
#else
void FElectronicNodesModule::ReloadConfiguration(UObject* Object, struct FPropertyChangedEvent& Property)
#endif
{
#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION > 25) || (ENGINE_MAJOR_VERSION == 5)
	const FName PropertyName = Property.GetPropertyName();
#endif

	if (PropertyName == "UseGlobalSettings")
	{
		if (ElectronicNodesSettings->UseGlobalSettings)
		{
			if (FPaths::FileExists(GlobalSettingsFile))
			{
				ElectronicNodesSettings->LoadConfig(nullptr, *GlobalSettingsFile);
			}
			else
			{
				ElectronicNodesSettings->SaveConfig(CPF_Config, *GlobalSettingsFile);
			}
		}
	}

	if (PropertyName == "UseHotPatch")
	{
		ISettingsEditorModule* SettingsEditorModule = FModuleManager::GetModulePtr<ISettingsEditorModule>("SettingsEditor");
		if (SettingsEditorModule)
		{
			SettingsEditorModule->OnApplicationRestartRequired();
		}
	}

	if (ElectronicNodesSettings->LoadGlobalSettings)
	{
		if (FPaths::FileExists(GlobalSettingsFile))
		{
			ElectronicNodesSettings->LoadConfig(nullptr, *GlobalSettingsFile);
		}
		ElectronicNodesSettings->LoadGlobalSettings = false;
	}

	ElectronicNodesSettings->SaveConfig();

	if (ElectronicNodesSettings->UseGlobalSettings)
	{
		ElectronicNodesSettings->SaveConfig(CPF_Config, *GlobalSettingsFile);
	}
}

void FElectronicNodesModule::ShutdownModule()
{
	FEditorDelegates::PrePIEEnded.RemoveAll(this);
}

void FElectronicNodesModule::HandlePrePIEEnded(bool /*bIsSimulating*/)
{
	if (GEditor == nullptr)
	{
		return;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem == nullptr)
	{
		return;
	}

	// Drop PIE Control Rig debug targets before CleanupWorld. Engine wire / node paint
	// will otherwise Cast the dying host and call GetRigVMExtendedExecuteContext().
	for (UObject* EditedAsset : AssetEditorSubsystem->GetAllEditedAssets())
	{
		FRigVMEditorAssetInterfacePtr RigAsset;
		if (EditedAsset && EditedAsset->Implements<URigVMEditorAssetInterface>())
		{
			RigAsset = FRigVMEditorAssetInterfacePtr(EditedAsset);
		}
		else
		{
			RigAsset = IRigVMEditorAssetInterface::GetInterfaceOuter(EditedAsset);
		}

		if (RigAsset.GetObject() == nullptr)
		{
			continue;
		}

		URigVMHost* Host = Cast<URigVMHost>(RigAsset->GetObjectBeingDebugged(true));
		if (Host == nullptr)
		{
			continue;
		}

		const UWorld* World = Host->GetWorld();
		if (World && (World->WorldType == EWorldType::PIE || World->IsPlayInEditor()))
		{
			RigAsset->SetObjectBeingDebugged(nullptr);
		}
	}
}

void FElectronicNodesModule::ToggleMasterActivation() const
{
	ElectronicNodesSettings->ToggleMasterActivation();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FElectronicNodesModule, ElectronicNodes)
