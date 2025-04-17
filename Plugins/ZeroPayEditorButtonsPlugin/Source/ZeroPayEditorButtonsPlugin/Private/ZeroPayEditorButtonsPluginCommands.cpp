// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPluginCommands.h"

#define LOCTEXT_NAMESPACE "FZeroPayEditorButtonsPluginModule"

void FZeroPayEditorButtonsPluginCommands::RegisterCommands()
{
	UI_COMMAND(ShowQuest3View, "ShowQuest3View", "Show Quest3 data layer only", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ShowPCVRView, "ShowPCVRView", "Show PCVR data layer only", EUserInterfaceActionType::Button, FInputChord());	
	UI_COMMAND(BakeMap, "BakeMap", "Bakes lighting and HLOD correctly", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
