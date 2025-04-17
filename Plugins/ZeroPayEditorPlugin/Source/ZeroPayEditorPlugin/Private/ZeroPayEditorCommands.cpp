// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorCommands.h"

#define LOCTEXT_NAMESPACE "FZeroPayEditorModule"

void FZeroPayEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "ZeroPayEditor", "Upload your mods to ZeroPay", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
