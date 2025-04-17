// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPlugin.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FZeroPayEditorButtonsPluginStyle::StyleInstance = nullptr;

void FZeroPayEditorButtonsPluginStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FZeroPayEditorButtonsPluginStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FZeroPayEditorButtonsPluginStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("ZeroPayEditorButtonsPluginStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon42x42(42.0f, 42.0f);

TSharedRef< FSlateStyleSet > FZeroPayEditorButtonsPluginStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("ZeroPayEditorButtonsPluginStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("ZeroPayEditorButtonsPlugin")->GetBaseDir() / TEXT("Resources"));

	Style->Set("ZeroPayEditorButtonsPlugin.ShowQuest3View", new IMAGE_BRUSH(TEXT("Quest3_EyeIcon"), Icon20x20));
	Style->Set("ZeroPayEditorButtonsPlugin.ShowPCVRView", new IMAGE_BRUSH(TEXT("PCVR_EyeIcon"), Icon20x20));
	Style->Set("ZeroPayEditorButtonsPlugin.BakeMap", new IMAGE_BRUSH(TEXT("BakeMap_Icon"), Icon20x20));
	return Style;
}

void FZeroPayEditorButtonsPluginStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FZeroPayEditorButtonsPluginStyle::Get()
{
	return *StyleInstance;
}
