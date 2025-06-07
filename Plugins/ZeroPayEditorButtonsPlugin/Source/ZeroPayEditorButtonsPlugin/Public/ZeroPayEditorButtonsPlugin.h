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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOperationComplete, bool, bSuccess);

UCLASS(Blueprintable)
class UZeroPayEditorOperationHandle : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FOnOperationComplete OnCompleted;
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
	UZeroPayEditorOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
	
private:
	/* Vars */
	bool bIsOperationRunning;
	FString ClosurePreventationMessage;
	TSharedPtr<class FUICommandList> PluginCommands;
	UEditorUtilityWidget* WidgetInstance;
	/* Cooking vars */
	UZeroPayEditorOperationHandle* Handle;
	FString GlobalUGCValue;
	FString LastMessage ;
	bool bAbortOperation ;

	/* Windows, Menus, Dialogs, etc. */
	TSharedRef<SDockTab> SpawnDockableTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void ShowTemporaryNotification(const FString& Message, float Duration = 2.0f);
	void UpdateUIProgressField() ;

	/* Cooking logic */
	bool CookAndPackWindows(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackAndroid(UZeroPayMod_DefinitionDataAsset* dataAsset);
	bool CookAndPackLinuxServer(UZeroPayMod_DefinitionDataAsset* dataAsset);

	bool ExecuteCookShellCmd(FString Platform, FString UGCID, FString MapName, FString NeverCookMapName);
	bool ExecutePakShellCmd(FString Platform, FString CookedPakLocation_Windows, FString CookedPakListFilePath);
};

UCLASS()
class UZeroPayEditorButtonsFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "MyPlugin")
	static UZeroPayEditorOperationHandle* CookAndUploadPackages(UZeroPayMod_DefinitionDataAsset* dataAsset);
};