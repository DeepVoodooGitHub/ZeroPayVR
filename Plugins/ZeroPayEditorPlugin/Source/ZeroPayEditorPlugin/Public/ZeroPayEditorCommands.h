// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "ZeroPayEditorStyle.h"

class FZeroPayEditorCommands : public TCommands<FZeroPayEditorCommands>
{
public:

	FZeroPayEditorCommands()
		: TCommands<FZeroPayEditorCommands>(TEXT("ZeroPayEditor"), NSLOCTEXT("Contexts", "ZeroPayEditor", "ZeroPayEditor Plugin"), NAME_None, FZeroPayEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};