// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ZeroPayMod_DefinitionDataAsset.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "Modules/ModuleManager.h"
#include "ZeroPayEditorButtonsPlugin.generated.h"

class FToolBarBuilder;
class FMenuBuilder;

struct FBPResultParams
{
	bool ReturnValue;   // ← for return value
	FString Message;    // ← for out parameter
};

class FZeroPayEditorButtonsPluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void ShowQuest3View_Clicked();
	void ShowPCVRView_Clicked();
	void OpenModIOWindow_Clicked();

	/* Cooking logic */
	bool CookThings(UZeroPayMod_DefinitionDataAsset* dataAsset);
	
private:
	/* Vars */
	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;
	UEditorUtilityWidget* WidgetInstance;
	/* Cooking vars */
	FString GlobalUGCValue;

	/* Windows, Menus, Dialogs, etc. */
	TSharedRef<SDockTab> SpawnDockableTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void ShowTemporaryNotification(const FString& Message, float Duration = 2.0f);
	void UpdateUIProgressField(const FString& Message) ;

	/* Cooking logic */
	bool PackWindows(FString mapName);
	bool PackAndroid(FString mapName);
	bool PackWindowsServer(FString mapName);
	bool PackLinuxServer(FString mapName);
};

UCLASS()
class UZeroPayEditorButtonsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MyPlugin")
	static void CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
};