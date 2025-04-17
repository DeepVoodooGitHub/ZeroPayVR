// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FZeroPayEditorButtonsPluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command. */
	void ShowQuest3View_Clicked();
	void ShowPCVRView_Clicked();
	void BakeMap_Clicked();
	
private:

	void RegisterMenus();

	void ShowTemporaryNotification(const FString& Message, float Duration = 2.0f);


private:
	TSharedPtr<class FUICommandList> PluginCommands;
};
