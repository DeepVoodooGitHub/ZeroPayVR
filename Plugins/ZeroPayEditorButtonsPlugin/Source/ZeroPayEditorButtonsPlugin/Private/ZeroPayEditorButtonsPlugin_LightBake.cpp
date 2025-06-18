#include "ZeroPayEditorButtonsPlugin.h"
#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPluginCommands.h"
#include "EditorLevelUtils.h"
#include "EditorBuildUtils.h"
#include "FileHelpers.h"
#include "TimerManager.h"

void FZeroPayEditorButtonsPluginModule::PerformLightBake()
{
	/* Get persistent world */
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[IsSubLevelVisibleByPath] failed to find persistent level"));
		return ;
	}

	/* Get the path to this level */
	FString PersistentWorldPath = FPackageName::GetLongPackagePath(World->GetOutermost()->GetName()) ;
	/* Strip off "Levels" so we get path to definition file.. */
	if (PersistentWorldPath.EndsWith("/Levels"))
		PersistentWorldPath.LeftChopInline(7);
	/* Add in defition file (we expect) */
	PersistentWorldPath += "/ZeroPayDefinition" ;

	/* Attempt to open the data asset */
	UZeroPayMod_DefinitionDataAsset* DefinitionDataAsset = LoadObject<UZeroPayMod_DefinitionDataAsset>(nullptr, *PersistentWorldPath);
	if (DefinitionDataAsset == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath) ) ) ;
		return;
	}

	/* Get the locations of the levels */
	persistentLeveLightBake = DefinitionDataAsset->Definition.persistentlevel.Get();
	if (persistentLeveLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find Persistent Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}
	pcvrLevelLightBake = DefinitionDataAsset->Definition.pcvrlevel.Get() ;
	if (pcvrLevelLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find PCVR Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}
	quest3LevelLightBake = DefinitionDataAsset->Definition.quest3level.Get();
	if (quest3LevelLightBake == nullptr)
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			FText::FromString("Error, could not find Quest 3 Level as specified in the DefinitionDataAsset at path '{0}'"),
			FText::FromString(PersistentWorldPath)));
		return;
	}

	/* Get paths */
	FString pcvrLevelPath = FPackageName::GetLongPackagePath(pcvrLevelLightBake->GetOutermost()->GetName());
	FString quest3LevelPath = FPackageName::GetLongPackagePath(quest3LevelLightBake->GetOutermost()->GetName());

	/* Now remember sub-level status for return later */
    bPCVRLevel_OriginalVisibility = IsSubLevelVisibleByPath(persistentLeveLightBake, pcvrLevelLightBake);
	bQuest3Level_OriginalVisibility = IsSubLevelVisibleByPath(persistentLeveLightBake, quest3LevelLightBake);

	/* Check already not running */
	GPULightmassSubsystem = persistentLeveLightBake->GetSubsystem<UGPULightmassSubsystem>();
	if (GPULightmassSubsystem)
	{
		if (GPULightmassSubsystem->IsRunning())
		{
			ShowNotification("The GPULight mass system is currently executing, please wait for it to finish.", SNotificationItem::CS_Fail);
			return;
		}
	}
	else
	{
		EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Error, could not find the GPU Lightmass system; please ensure you have the correct plugin enabled and ray cast support turned on for this project"));
		return ;
	}

	/* PCVR first.. */
	SetSpecificSublevelVisible(pcvrLevelLightBake, true);
	SetSpecificSublevelVisible(quest3LevelLightBake, false);

	/* Info */
	ShowNotification("Building PCVR Lighting via GPU Lightmass.", SNotificationItem::CS_Pending);

	/* Spawn the light mass system - using the users existing settings */
	GPULightmassSubsystem->SetRealtime(false) ;
	GPULightmassSubsystem->Launch();

	/* GPU Lightmass's call back is bad.. it's called too early before the lightmass system has been removed (and we can't add another for Quest3 baking) */
	TimerCallback = FTimerDelegate::CreateLambda([this]()
		{
			if (!GPULightmassSubsystem->IsRunning())
			{
				// Stop polling
				persistentLeveLightBake->GetTimerManager().ClearTimer(TimerHandle);

				// Call the user's action
				HandlePCVRLightBuildComplete();
			}
		});

	persistentLeveLightBake->GetTimerManager().SetTimer(
		TimerHandle,
		TimerCallback,
		0.2f,      // Check interval
		true       // Loop
	);
	
}


void FZeroPayEditorButtonsPluginModule::HandlePCVRLightBuildComplete()
{
	ShowNotification("Saving PCVR level (with lighting).", SNotificationItem::CS_Pending);

	ULevel* Level = pcvrLevelLightBake->PersistentLevel;
	UPackage* LevelPackage = Level->GetOutermost();
	LevelPackage->SetDirtyFlag(true);
	UEditorLoadingAndSavingUtils::SaveMap(pcvrLevelLightBake, FString());

	/* Quest 3.. */
	SetSpecificSublevelVisible(pcvrLevelLightBake, false);
	SetSpecificSublevelVisible(quest3LevelLightBake, true);

	/* Info */
	ShowNotification("Building Quest3 Lighting via GPU Lightmass.", SNotificationItem::CS_Pending);

	/* Quest 3 lightmass.. use call back as we don't care if the actual lightmass engine is "gone" now.. we're done.. */
	GPULightmassSubsystem->OnLightBuildEnded().AddRaw(this, &FZeroPayEditorButtonsPluginModule::HandleQuest3LightBuildComplete);
	GPULightmassSubsystem->SetRealtime(false);
	GPULightmassSubsystem->Launch();
}

void FZeroPayEditorButtonsPluginModule::HandleQuest3LightBuildComplete()
{
	ShowNotification("Saving Quest3 level (with lighting).", SNotificationItem::CS_Pending);

	ULevel* Level = quest3LevelLightBake->PersistentLevel;
	UPackage* LevelPackage = Level->GetOutermost();
	LevelPackage->SetDirtyFlag(true);
	UEditorLoadingAndSavingUtils::SaveMap(quest3LevelLightBake, FString());

	/* Quest 3.. */
	SetSpecificSublevelVisible(pcvrLevelLightBake, bQuest3Level_OriginalVisibility);
	SetSpecificSublevelVisible(quest3LevelLightBake, bQuest3Level_OriginalVisibility);

	ShowNotification("ZeroPayVR Lightbake and save completed", SNotificationItem::CS_Success);
}


bool FZeroPayEditorButtonsPluginModule::IsSubLevelVisibleByPath(UWorld* World, UWorld* SubWorld)
{

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (!StreamingLevel) continue;

		if (StreamingLevel->GetLoadedLevel() == SubWorld->PersistentLevel)
		{
			return StreamingLevel->IsLevelVisible() ;
		}
	}

	/* Not found */
	UE_LOG(LogTemp, Error, TEXT("[IsSubLevelVisibleByPath] could not find requested sub-level."));
	return false; 
}

void FZeroPayEditorButtonsPluginModule::SetSpecificSublevelVisible(UWorld* SubWorld, bool bVisbility)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();

	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel && StreamingLevel->GetLoadedLevel())
		{
			if (StreamingLevel->GetLoadedLevel() == SubWorld->PersistentLevel)
				EditorLevelUtils::SetLevelVisibility(StreamingLevel->GetLoadedLevel(), bVisbility, true);
		}
	}

}

void FZeroPayEditorButtonsPluginModule::ShowNotification(FString notification, SNotificationItem::ECompletionState State)
{
	FNotificationInfo Info(FText::FromString(notification));
	Info.FadeInDuration = 0.2f;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 5.0f;
	Info.bUseThrobber = true;
	Info.bUseSuccessFailIcons = true;
	Info.bUseLargeFont = false;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
		Notification->SetCompletionState(State);
}