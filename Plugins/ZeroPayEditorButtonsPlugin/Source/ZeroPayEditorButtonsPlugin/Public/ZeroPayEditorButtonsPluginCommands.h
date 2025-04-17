// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "ZeroPayEditorButtonsPluginStyle.h"

class FZeroPayEditorButtonsPluginCommands : public TCommands<FZeroPayEditorButtonsPluginCommands>
{
public:

	FZeroPayEditorButtonsPluginCommands()
		: TCommands<FZeroPayEditorButtonsPluginCommands>(TEXT("ZeroPayEditorButtonsPlugin"), NSLOCTEXT("Contexts", "ZeroPayEditorButtonsPlugin", "ZeroPayEditorButtonsPlugin Plugin"), NAME_None, FZeroPayEditorButtonsPluginStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > ShowQuest3View ;
	TSharedPtr< FUICommandInfo > ShowPCVRView;
	TSharedPtr< FUICommandInfo > BakeMap;
};
