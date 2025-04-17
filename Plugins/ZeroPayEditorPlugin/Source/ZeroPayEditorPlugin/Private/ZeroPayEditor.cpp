// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditor.h"
#include <string>
#include "ZeroPayEditorStyle.h"
#include "ZeroPayEditorCommands.h"
#include "LevelEditor.h"
//#include "Widgets/Docking/SDockableTab.h"
//#include "Widgets/Docking/SDockTabStack.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Runtime/Core/Public/Async/Async.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
//#include "AssetRegistryModule.h"
#include "Serialization/BufferArchive.h"
#include "ImageUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Modules/ModuleManager.h"

static const FName ZeroPayEditorTabName("ZeroPayEditor");

#define LOCTEXT_NAMESPACE "FZeroPayEditorModule"

FZeroPayEditorModule::FZeroPayEditorModule()
{
	UGCValue = FText::FromString("** Not Created Yet **") ;
	GameInstallPath = FText::FromString("C:\\Program Files (x86)\\Steam\\steamapps\\common\\ZeroPay\\ZeroPay\\Content\\Paks") ;

	bInSetupMode = true ;
	bInLocalMode = false ;
	bInDeployMode = false ;
	bIsSettingUp = false;

	sSetupProgress = FText::FromString("Wait for user to generate UGC..");
	sLocalProgress = FText::FromString("Standing by..");
	sDeployProgress = FText::FromString("Standing by..");

	// See if there's an existing UGC and definition
	bSetupIsValid = false;
	bSetupIsBroken = false;
	bSetupIsDeploying = false;
	nUGCValue = 0;

	ValidateUGC();
}

FZeroPayEditorModule::~FZeroPayEditorModule()
{
	// Undo flags
	bIsSettingUp = false;
}

void FZeroPayEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
 	UE_LOG(LogTemp, Warning, TEXT("ZeroPayEditorPlugin module loaded!"));

	FZeroPayEditorStyle::Initialize();
	FZeroPayEditorStyle::ReloadTextures();

	FZeroPayEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FZeroPayEditorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FZeroPayEditorModule::PluginButtonClicked),
		FCanExecuteAction());
		
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	
	{
		TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
		MenuExtender->AddMenuExtension("WindowLayout", EExtensionHook::After, PluginCommands, FMenuExtensionDelegate::CreateRaw(this, &FZeroPayEditorModule::AddMenuExtension));

		LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
	}
	
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		ToolbarExtender->AddToolBarExtension("Settings", EExtensionHook::After, PluginCommands, FToolBarExtensionDelegate::CreateRaw(this, &FZeroPayEditorModule::AddToolbarExtension));
		
		LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(ToolbarExtender);
	}
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ZeroPayEditorTabName, FOnSpawnTab::CreateRaw(this, &FZeroPayEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FZeroPayEditorTabTitle", "ZeroPay Publish"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FZeroPayEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	FZeroPayEditorStyle::Shutdown();

	FZeroPayEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ZeroPayEditorTabName);
}

// get value to display in SNumericEntryBox
FText FZeroPayEditorModule::GetUGCValue() const
{
	return UGCValue;
}

// Set Value
void FZeroPayEditorModule::SetUGCValue(const FText& InText, ETextCommit::Type)
{
	UGCValue = InText;
}

FText FZeroPayEditorModule::GetGameInstallPathValue() const
{
	return GameInstallPath;
}

void FZeroPayEditorModule::SetGameInstallPathValueCommitted(const FText& InText, ETextCommit::Type)
{
	GameInstallPath = InText;
}

bool FZeroPayEditorModule::OnCanPublishOnEnabled() const
{
	return bSetupIsValid;
}


TSharedRef<SDockTab> FZeroPayEditorModule::OnSpawnPluginTab_ZeroPay(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab);

}

TSharedRef<SDockTab> FZeroPayEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{	
	FString RelativePath = FPaths::ProjectContentDir();
	FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*RelativePath);
	
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
		[
			// Main Vertical Box Container
			SNew(SVerticalBox)
				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).MaxHeight(32)
				[
					SNew(STextBlock)
					.ShadowColorAndOpacity(FLinearColor::Black)
					.ColorAndOpacity(FLinearColor::White)
					.ShadowOffset(FIntPoint(-1, 1))
					.TextStyle(&ZeroPayHeading)
					.Text(FText::FromString("The KillerJim Mod Publishing Tool"))
				]
				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoHeight()
				[
					SNew(SSpacer).Size(FVector2D(1, 16))
				]

				// Button Section
				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).MaxHeight(32)
				[
					SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						[
							SNew(SSpacer).Size(FVector2D(32, 1))
						]
						+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoWidth().AutoWidth()
						[
							// Initial set-up button
							SNew(SButton)
								.Text(FText::FromString("Initial set-up"))
								.ContentPadding(6)
								.OnClicked_Raw(this, &FZeroPayEditorModule::OnClick_ShowSetup)
								.ButtonColorAndOpacity_Raw(this, &FZeroPayEditorModule::OnGetButtonSetupBackground)
						]
						+ SHorizontalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						[
							SNew(SSpacer).Size(FVector2D(32, 1))
						]
						+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoWidth()
						[
							// Play offline
							SNew(SButton)
								.Text(FText::FromString("Play local (offline)"))
								.ContentPadding(6)
								.OnClicked_Raw(this, &FZeroPayEditorModule::OnClick_ShowLocal)
								.IsEnabled_Raw(this, &FZeroPayEditorModule::OnCanPublishOnEnabled)
							.ButtonColorAndOpacity_Raw(this, &FZeroPayEditorModule::OnGetButtonLocalBackground)
						]
						+ SHorizontalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left)
						[
							SNew(SSpacer).Size(FVector2D(32, 1))
						]
						+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoWidth()
						[
							// Deploy online
							SNew(SButton)
								.Text(FText::FromString("Deploy (online)"))
								.ContentPadding(6)
								.OnClicked_Raw(this, &FZeroPayEditorModule::OnClick_ShowDeploy)
								.IsEnabled_Raw(this, &FZeroPayEditorModule::OnCanPublishOnEnabled)
								.ButtonColorAndOpacity_Raw(this, &FZeroPayEditorModule::OnGetButtonDeployBackground)
						]
				]

				// Spacer
				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).MaxHeight(16)
					
				// Thin horizontal border
				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).MaxHeight(32)
				[
					SNew(SBorder)
					[
						SNew(SSpacer).Size(FVector2D(2048, 1))
					]
				]

				//
				// Main Content, in 3 vertical boxes
				//

				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoHeight()
				[
					// >>> SET UP - VERTICAL BOX CONTAINER <<<
					SNew(SVerticalBox).Visibility_Raw(this, &FZeroPayEditorModule::OnVisible_Setup)
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor::White)
								.ShadowOffset(FIntPoint(-1, 1))
								.TextStyle(&ZeroPaySubHeading)
								.Text(FText::FromString("> Initial Set-Up"))
						]
						// Spacer
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
						[	
							SNew(SSpacer).Size(FVector2D(2048, 1))
						]
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
								.ShadowOffset(FIntPoint(-1, 1))
								.WrapTextAt(1024)
								.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
								.Text(FText::FromString("This editor plugin helps you set-up your project, test it locally and then deploy it on-line. The first step is to get the project configured for upload to "
									"steam as a UGC (User Generated Content) package. This process can only be completed once, and will assign you a unique UGC number from steam. Remember that content is 'hidden' "
									"by default and you need to make it visible on your UGC's Steam Workshop page (Under visibility, right side)."))
						]
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size( FVector2D( 32,32 ) ) 
						]

						// Setup - UGC Information
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight() 
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoWidth()
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text(FText::FromString("Unique UGC Number: "))
							]
						+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text_Raw(this, &FZeroPayEditorModule::GetUGCValue)              // display this value
							]
						]

						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(32, 32))
						]

						// Setup - "Generate UGC"
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(SButton)
								.Text(FText::FromString("Generate UGC"))
								.ContentPadding(8)
								.OnClicked_Raw(this, &FZeroPayEditorModule::OnClick_GenerateUGC)
								.IsEnabled_Raw(this, &FZeroPayEditorModule::OnEnabled_GenerateUGC)
						]

						// Setup - Information
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AutoWidth()
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text(FText::FromString("Set-up Status: "))
							]
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
									.Text_Raw(this, &FZeroPayEditorModule::GetText_SetupProgress)
							]
						]
				]

				+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoHeight()
				[
					// >>> SET UP - LOCAL <<<
					SNew(SVerticalBox).Visibility_Raw(this, &FZeroPayEditorModule::OnVisible_Local)
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor::White)
								.ShadowOffset(FIntPoint(-1, 1))
								.TextStyle(&ZeroPaySubHeading)
								.Text(FText::FromString("> Play off-line (locally)"))
						]
						// Spacer
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
						[
							SNew(SSpacer).Size(FVector2D(2048, 1))
						]
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
								.ShadowOffset(FIntPoint(-1, 1))
								.WrapTextAt(1024)
								.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
								.Text(FText::FromString("This will allow you to cook and install your level or mod into the local ZeroPay game."))
						]
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(32, 32))
						]

						// Local - "Cook and Install PAK"
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(SButton)
								.Text(FText::FromString("Publish locally"))
								.ContentPadding(8)
								.OnClicked_Raw(this, &FZeroPayEditorModule::OnClick_GenerateUGC)
								.IsEnabled_Raw(this, &FZeroPayEditorModule::OnEnabled_GenerateUGC)
						]
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(16, 16))
						]

						// Local - Progress TEXT BOX
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AutoWidth()
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text(FText::FromString("Local build process: "))
							]
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
									.Text_Raw(this, &FZeroPayEditorModule::GetText_LocalProgress)
							]
						]
				]

				+SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoHeight()
				[
					// >>> DEPLOY <<<
					SNew(SVerticalBox).Visibility_Raw(this, &FZeroPayEditorModule::OnVisible_Deploy)
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor::White)
								.ShadowOffset(FIntPoint(-1, 1))
								.TextStyle(&ZeroPaySubHeading)
								.Text(FText::FromString("> Deploy to workshop"))
						]
						// Spacer
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top)
						[
							SNew(SSpacer).Size(FVector2D(2048, 1))
						]
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f))
								.ShadowOffset(FIntPoint(-1, 1))
								.WrapTextAt(1024)
								.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
								.Text(FText::FromString("Submit your level or mod to the workshop; providing an optional change note"))
						]
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(32, 32))
						]

						// Deploy - change note
						+ SVerticalBox::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoWidth()
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text(FText::FromString("Change Note: "))
							]
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
							[
								SNew(SEditableTextBox)
									.MinDesiredWidth(512)
									.OnTextCommitted_Raw(this, &FZeroPayEditorModule::OnTextCommitted_ChangeNote) 
							]
						]

						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(8, 8))
						]	

						// Deploy - change note
						+ SVerticalBox::Slot().VAlign(VAlign_Bottom).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).AutoWidth()
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text(FText::FromString("Mark as 'beta' release (recommended for development mods): "))
							]
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
							[
								SNew(SCheckBox)
									.OnCheckStateChanged_Raw(this, &FZeroPayEditorModule::OnCheckBoxStateChange_Beta)
							]
						]

						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(8,8))
						]

						// Deploy - "Cook and Upload PAK"
						+ SVerticalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(SButton)
								.Text(FText::FromString("Publish to workshop..."))
								.ContentPadding(8)
								.OnClicked_Raw(this, &FZeroPayEditorModule::OnClick_Deploy)
								.IsEnabled_Raw(this, &FZeroPayEditorModule::OnEnable_Deploy)
						]
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).AutoHeight()
						[
							SNew(SSpacer).Size(FVector2D(16, 16))
						]

						// Local - Progress TEXT BOX
						+ SVerticalBox::Slot().VAlign(VAlign_Center).HAlign(HAlign_Left).VAlign(VAlign_Center).AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).AutoWidth()
							[
								SNew(STextBlock)
									.ShadowColorAndOpacity(FLinearColor::Black)
									.ColorAndOpacity(FLinearColor::White)
									.ShadowOffset(FIntPoint(-1, 1))
									.TextStyle(&ZeroPayBoldStyle)
									.Text(FText::FromString("Deployment progress: "))
							]
						+ SHorizontalBox::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
						[
							SNew(STextBlock)
								.ShadowColorAndOpacity(FLinearColor::Black)
								.ColorAndOpacity(FLinearColor::White)
								.ShadowOffset(FIntPoint(-1, 1))
								.TextStyle(&FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
								.Text_Raw(this, &FZeroPayEditorModule::GetText_DeployProgress)
						]
					]
				]
		];
}

void FZeroPayEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ZeroPayEditorTabName);
}

void FZeroPayEditorModule::AddMenuExtension(FMenuBuilder& Builder)
{
	Builder.AddMenuEntry(FZeroPayEditorCommands::Get().OpenPluginWindow);
}

void FZeroPayEditorModule::AddToolbarExtension(FToolBarBuilder& Builder)
{
	Builder.AddToolBarButton(FZeroPayEditorCommands::Get().OpenPluginWindow);
}

FReply FZeroPayEditorModule::OnTestOnClicked()
{	 
	
	return FReply::Handled();
}

//
// ValidateUGC
//
void FZeroPayEditorModule::ValidateUGC()
{

	//LogTemp: Display: UnrealBinary: D: / UnrealEngineGitHub / UnrealEngine / Engine / Binaries / Win64 / UnrealEditor.exe
	//LogTemp : Display: UnrealPakBinary: D: / UnrealEngineGitHub / UnrealEngine / Engine / Binaries / Win64 / UnrealPak.exe
	//LogTemp : Display: ProjectFullFilePath: D: / DVG / ZeroPay / ZeroPay.uproject
	//LogTemp : Display: ProjectCookedPath_Windows: D: / DVG / ZeroPay / Saved / Cooked / Windows / SampleProject_UE24C / Content
	//LogTemp : Display: PakFileName_Windows: UGC** Not Created Yet **-Windows.pak
	//LogTemp : Display: CookedPakLocation_Windows: D: / DVG / ZeroPay / Saved / Workshop / UGC * * Not Created Yet **-Windows.pak
	//LogTemp : Display: CookedPakListFilePath: D: / DVG / ZeroPay / Saved / custommap_paklist.txt
	//LogTemp : Display: GameInstallationPakPath: D: / DVG / KModOutput / Windows / ZeroPay / Content / Paks /

	/* Write Stream ID out */
	FString StreamID = "1281150";
	bool bSuccess = FFileHelper::SaveStringToFile(StreamID, ANSI_TO_TCHAR(".//steam_appid.txt"), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		sSetupProgress = FText::FromString("Failed to write appID.");
		bIsSettingUp = false;
		bSetupIsBroken = true;
		return ;
	}

	if (!SteamAPI_Init())
	{
		sSetupProgress = FText::FromString("Failed to initialise steam! Is steam running and do you have ZeroPay?");
		bIsSettingUp = false;
		bSetupIsBroken = true;
		return ;
	}


	char sFolderPath[512] = { 0 } ;
	SteamApps()->GetAppInstallDir(1281150, sFolderPath, 512);
	GameInstallPath = FText::FromString(FString::Printf(TEXT("%s\\Content\\Paks"), sFolderPath));
	
	// All build paths, names, etc
	FString ProjectPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) + FString("CustomContent/UGC*");
	//UE_LOG(LogTemp, Display, TEXT("--ProjectPath: %s"), *ProjectPath);

	if (ProjectPath.Find(TEXT(" ")) != -1)
	{
		sSetupProgress = FText::FromString("You have one or more spaces in your project path, the directory path for your project must not have any spaces in it");
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "Modiverse Problem", "This plugin does not support spaces in your projects path; you must place your UE4 project on a path that does not contain spaces!"));
		bSetupIsValid = false;
		return;
	}

	TArray<FString> Directories;
	IFileManager::Get().FindFiles(Directories, *ProjectPath, false, true);

	/* Scan for UGC */ 
	//UE_LOG(LogTemp, Display, TEXT("--Dir Total: %d"), Directories.Num());

	/* Nothing? */
	if (Directories.Num() == 0)
	{
		sSetupProgress = FText::FromString("Project has not yet been set-up..");
		bSetupIsValid = false;
		return ;
	}

	/* Look for the one with a definition in it.. */
	bool bFoundDefinition = false;
	FString sUGCValue = "" ;
	for (int nCounter = 0; nCounter < Directories.Num(); nCounter++)
	{
		sUGCValue = Directories[nCounter].RightChop(3);

		/* Test for validity */
		if (sUGCValue.Len() <= 4)
		{
			sSetupProgress = FText::FromString("Found a folder that was UGCxxx, but xxx was not an numerical value.. cannot continue!");
			bSetupIsValid = false;
			bSetupIsBroken = true;
			return;
		}

		/* Look for definition */

		// Validate the definitions (must be done in main game thread, so the debugger says - still works tbh)
		FString sAssetFullPath = FString("/Game/CustomContent/UGC") + sUGCValue + FString("/Definition.uasset");
		FString sAssetName = FString("Definition");

		// Open definition
		auto * package = LoadPackage(nullptr, *sAssetFullPath, LOAD_None);
		dataAsset = FindObject<UZeroPayModDefinitionDataAsset>(package, *sAssetName);
		if (dataAsset != nullptr)
		{
			bFoundDefinition = true;
			break;
		}
	}

	/* OK? */
	if (!bFoundDefinition)
	{
		sSetupProgress = FText::FromString("Found one or more UGCxxxxx folders but there was no ZeroPay definition asset inside any of them");
		bSetupIsValid = false;
		bSetupIsBroken = true;
		return ;
	}

	/* Convert */
	nUGCValue = FCString::Atoi64(*sUGCValue);

	/* Show to user.. */
	UGCValue = FText::FromString(FString::Printf(TEXT("UGC%llu"), nUGCValue));

	sSetupProgress = FText::FromString("Project is set-up correctly.");

	bSetupIsValid = true ;

	return;
}

//
// Top Section (buttons, colours, etc.) 
//

FReply FZeroPayEditorModule::OnClick_ShowSetup()
{
	bInSetupMode = true ;
	bInLocalMode = false;
	bInDeployMode = false;

	return FReply::Handled();
}

FReply FZeroPayEditorModule::OnClick_ShowLocal()
{
	bInSetupMode = false;
	bInLocalMode = true;
	bInDeployMode = false;

	return FReply::Handled();
}

FReply FZeroPayEditorModule::OnClick_ShowDeploy()
{
	bInSetupMode = false;
	bInLocalMode = false;
	bInDeployMode = true;

	return FReply::Handled();
}

FSlateColor FZeroPayEditorModule::OnGetButtonSetupBackground() const
{
	return bInSetupMode ? FSlateColor(FLinearColor(0.1f, 1.0f, 0.1f, 1.0f)) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
}

FSlateColor FZeroPayEditorModule::OnGetButtonLocalBackground() const
{
	return bInLocalMode ? FSlateColor(FLinearColor(0.1f, 1.0f, 0.1f, 1.0f)) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
}

FSlateColor FZeroPayEditorModule::OnGetButtonDeployBackground() const
{
	return bInDeployMode ? FSlateColor(FLinearColor(0.1f, 1.0f, 0.1f, 1.0f)) : FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
}

EVisibility FZeroPayEditorModule::OnVisible_Setup() const
{
	return bInSetupMode ? EVisibility::Visible : EVisibility::Collapsed ;
}

EVisibility FZeroPayEditorModule::OnVisible_Local() const
{
	return bInLocalMode ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FZeroPayEditorModule::OnVisible_Deploy() const
{
	return bInDeployMode ? EVisibility::Visible : EVisibility::Collapsed;
}

//
// Set-Up / Generate UGC
//

FText FZeroPayEditorModule::GetText_SetupProgress() const
{
	return sSetupProgress;
}


FReply FZeroPayEditorModule::OnClick_GenerateUGC()
{
	/* Disable UI */
	bIsSettingUp = true;
	bUGCOperationCompleted = false;
	nUGCOperationPublishedFieldID = 0;

	auto Future = Async(EAsyncExecution::ThreadPool, [&]
	{
		// "i" has been captured by reference! By the time this thing runs on another thread who knows what it will be
		// We could instead use [=] but that would copy the SomeStuff array which we probably don't want.
		// We could use [i,&SomeStuff] to capture i by value and SomeStuff by reference. That works as long as we guarantee that the array and its contents won't change while these async operations are in-flight.
		return Perform_GenerateUGC();
	});

	/* Poll steam until complete */
	int nTimeout = 5000;
	while (!bUGCOperationCompleted && (nTimeout >= 0))
	{
		SteamAPI_RunCallbacks();
		FPlatformProcess::Sleep(0.001f);
		nTimeout--;
	}

	if (bUGCOperationSuccess)
		WriteUGCFiles() ;

	bIsSettingUp = false;

	return FReply::Handled();

}

void FZeroPayEditorModule::Perform_GenerateUGC()
{
	// Create an UGC Item
	SteamAPICall_t m_callCreateItem = SteamUGC()->CreateItem(1281150, k_EWorkshopFileTypeCommunity);
	m_callResultCreateItem.Set(m_callCreateItem, this, &FZeroPayEditorModule::OnCreateItemCallBack);

//	int32 itemCount = SteamUGC()->GetNumSubscribedItems();
//	outputText = FText::FromString(FString::Printf(TEXT("[Info] | Spawning blueprint item '%d'"), itemCount));

	sSetupProgress = FText::FromString("Creating new on-line steam UGC item... (5 second timeout)") ;

	/* Poll steam until complete */
	int nTimeout = 5000 ;
	while (bIsSettingUp && (nTimeout >= 0))
	{
		SteamAPI_RunCallbacks();
		FPlatformProcess::Sleep(0.001f);
		nTimeout--;
	}

	if (nTimeout <= 0)
	{
		sSetupProgress = FText::FromString("[ERROR] Steam timedout creating an UGC item for you. Check your internet connection, steam client and steam status...");
		bIsSettingUp = false;
		return ;
	}

	/* Disable UI */
	bIsSettingUp = false ;

	return ;
}

void FZeroPayEditorModule::OnCreateItemCallBack(CreateItemResult_t* pResult, bool bIOFailure)
{
	bUGCOperationCompleted = true;
	if (pResult->m_eResult == k_EResultOK)
	{
		bUGCOperationSuccess = true;
		nUGCOperationPublishedFieldID = pResult->m_nPublishedFileId;
	}
	else
	{
		bUGCOperationSuccess = false;
	}
}

void FZeroPayEditorModule::WriteUGCFiles()
{
	// Store as string and number
	UGCValue = FText::FromString(FString::Printf(TEXT("UGC%llu"), nUGCOperationPublishedFieldID));
	nUGCValue = nUGCOperationPublishedFieldID ;

	sSetupProgress = FText::FromString(FString::Printf(TEXT("SUCCESS!! UGC%llu was created and assigned by steamworks."), nUGCOperationPublishedFieldID));

	/* Valid.. create directory */
	FString UGCPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) + FString("CustomContent/") + FString::Printf(TEXT("UGC%llu"), nUGCOperationPublishedFieldID) ;
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectory(*UGCPath);

	CreateAndSaveDataDefinition(UGCPath, FText::FromString(FString::Printf(TEXT("%llu"), nUGCOperationPublishedFieldID)).ToString());
	//UE_LOG(LogTemp, Display, TEXT("--ProjectPath: %s"), *ProjectPath);

	bIsSettingUp = false;
	bSetupIsValid = true;

} ;

void FZeroPayEditorModule::CreateAndSaveDataDefinition(FString UGCPath, FString SteamWorks_UGCID_)
{
	//FPackageName::RegisterMountPoint("/PlayerData/Levels/", relative_dir);

	FString sFullPath = UGCPath + FString("/Definition");
	FString sAssetFullPath = FString("/Game/ZeroPayMod/Definition") ;
	FString sAssetName = FString("Definition");

	auto * package = CreatePackage(*sAssetFullPath);

	UZeroPayModDefinitionDataAsset* level_asset = NewObject< UZeroPayModDefinitionDataAsset >(package, UZeroPayModDefinitionDataAsset::StaticClass(), *sAssetName, RF_Public | RF_Standalone);

	level_asset->Definition.SteamWorks_UGCID = SteamWorks_UGCID_;

	if (ensure(level_asset != nullptr))
	{
		const auto file_name = FString::Printf(TEXT("%s%s"), *sFullPath, *FPackageName::GetAssetPackageExtension());

		UPackage::SavePackage(package, /*level_asset*/ nullptr, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *file_name);

		//FAssetRegistryModule::AssetCreated(level_asset);
	}
}


bool FZeroPayEditorModule::OnEnabled_GenerateUGC() const
{
	return !bSetupIsValid && !bSetupIsBroken && !bIsSettingUp;
}

//
// Local
//

FText FZeroPayEditorModule::GetText_LocalProgress() const
{
	return sLocalProgress ;
}

//
// Deploy
//

FText FZeroPayEditorModule::GetText_DeployProgress() const
{
	return sDeployProgress;
}

void FZeroPayEditorModule::OnTextCommitted_ChangeNote(const FText& InText, ETextCommit::Type)
{
	sChangeNote = InText ;
}

void FZeroPayEditorModule::OnCheckBoxStateChange_Beta(ECheckBoxState state)
{
	bIsBetaRelease = (state == ECheckBoxState::Checked) ;
}


bool FZeroPayEditorModule::OnEnable_Deploy() const
{
	return bSetupIsValid && !bSetupIsDeploying ;
}

bool string_is_valid(const std::string &str)
{
	return find_if(str.begin(), str.end(),
		[](char c) { return !(isalnum(c) || (c == ' ')); }) == str.end();
}


FReply FZeroPayEditorModule::OnClick_Deploy()
{
	bSetupIsDeploying = true;

	// Does map folder exist?
	bool MapFolderExists = FPaths::DirectoryExists(FPaths::ProjectContentDir() + FString("CustomContent/") + UGCValue.ToString() + FString("/Maps")) ;
	if (!MapFolderExists)
	{
		sDeployProgress = FText::FromString("Failed - no /maps folder under UGC folder - Maps must be placed in the maps folder!");
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing FAILED", "Could not find the 'maps' folder under your UGCxxxxxx folder. All maps must be placed in there!"));
		bSetupIsDeploying = false;
		return FReply::Handled();
	}

	// Validate the definitions (must be done in main game thread, so the debugger says - still works tbh)
	FString sAssetFullPath = FString("/Game/CustomContent/") + UGCValue.ToString() + FString("/Definition.uasset");
	FString sAssetName = FString("Definition");

	// Open definition
	auto * package = LoadPackage(nullptr, *sAssetFullPath, LOAD_None);
	dataAsset = FindObject<UZeroPayModDefinitionDataAsset>(package, *sAssetName);

	if (dataAsset == nullptr)
	{
		sDeployProgress = FText::FromString("Failed - could not find definition!");
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing FAILED", "ERROR - Installation failure, could not find the Definition's file that should be under your UGCxxxxxx folder"));
		bSetupIsDeploying = false;
		return FReply::Handled() ;
	}

	/* Name length */
	if (dataAsset->Definition.modName.Len() < 3)
	{
		sDeployProgress = FText::FromString("Failed - Your must supply 3 or more characters as a mod description!");
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing FAILED", "ERROR - You need to supply a 3 or more characters for the mod name in the definition file (under /Game/CustomContent/UGCxxxxxx/Definition)"));
		bSetupIsDeploying = false;
		return FReply::Handled();
	}

	/* Preview image? */
	if (dataAsset->Definition.workshopImage != nullptr)
	{
		if ((dataAsset->Definition.workshopImage->GetSurfaceWidth() != 512) || (dataAsset->Definition.workshopImage->GetSurfaceHeight() != 512))
		{
			sDeployProgress = FText::FromString("Failed - The preview image specified in definition is not 512 x 512.");
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing FAILED", "ERROR - Invalid preview image provided.. "));
			bSetupIsDeploying = false;
			return FReply::Handled();
		}

		// Disable mipmap/compression
		if ((dataAsset->Definition.workshopImage->CompressionSettings != TextureCompressionSettings::TC_VectorDisplacementmap) || (dataAsset->Definition.workshopImage->MipGenSettings != TextureMipGenSettings::TMGS_NoMipmaps))
		{
			TextureCompressionSettings prevCompression = dataAsset->Definition.workshopImage->CompressionSettings;
			TextureMipGenSettings prevMipSettings = dataAsset->Definition.workshopImage->MipGenSettings;
			dataAsset->Definition.workshopImage->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
			dataAsset->Definition.workshopImage->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
			dataAsset->Definition.workshopImage->UpdateResource();
		}
	}

#if 0
	if (dataAsset->Definition.modType == StandaloneLevel)
	{
		/* Find the ZeroPayProxy that will be inthe world, or should be! */
		TArray<AActor*> FoundActors;
		UWorld * currentWorld = GEditor->GetEditorWorldContext().World();
		UGameplayStatics::GetAllActorsOfClass(currentWorld, ANavMeshBoundsVolume::StaticClass(), FoundActors);
		/* Ensure we find only one.. */
		if (FoundActors.Num() == 0)
		{
			sDeployProgress = FText::FromString("Failed - You are publishing a level and have no 'Nav Mesh Bounding Volume'; AI and NPCs will not be able to navigate your level - Even if you don't care, other's might want to fight zombies in your map - Please add one.");
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing FAILED", "You're level has no 'Nav Mesh Bounding Volume', AI's and NPC's are unable to navigate your level - please add one."));
			bSetupIsDeploying = false;
			return FReply::Handled();
		}
	}
#endif

	auto Future = Async(EAsyncExecution::ThreadPool, [&]
	{
		return Perform_Deploy();
	});	

	return FReply::Handled(); 
}

void FZeroPayEditorModule::Perform_Deploy()
{
	// Cook both desktop and server
	if (CookThings())
	{
		// Async, returns immediately
		UploadUGC();
	}
	else
	{
		/* Failed */
		bSetupIsDeploying = false ;
	}
}

bool FZeroPayEditorModule::CookThings()
{
	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	//
	// Build paths
	//

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString PakFileName_Windows = "Windows.pak";
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";
	FString GameInstallationPakPath = GameInstallPath.ToString();

	UE_LOG(LogTemp, Display, TEXT("UnrealBinary: %s"), *UnrealBinary);
	UE_LOG(LogTemp, Display, TEXT("UnrealPakBinary: %s"), *UnrealPakBinary);
	UE_LOG(LogTemp, Display, TEXT("ProjectFullFilePath: %s"), *ProjectFullFilePath);
	UE_LOG(LogTemp, Display, TEXT("PakFileName_Windows: %s"), *PakFileName_Windows);
	UE_LOG(LogTemp, Display, TEXT("CookedPakListFilePath: %s"), *CookedPakListFilePath);
	//UE_LOG(LogTemp, Display, TEXT("GameInstallationPakPath: %s"), *GameInstallationPakPath);

	// Debug
	//UnrealBinary = "D:\\Program Files (x86)\\UE_4.24\\Engine\\Binaries\\Win64\\UnrealEditor.exe";
	//UnrealPakBinary = "D:\\Program Files (x86)\\UE_4.24\\Engine\\Binaries\\Win64\\UnrealPak.exe";
	//ProjectFullFilePath = "D:\\DVG\\SampleProject_UE24C\\SampleProject_UE24C.uproject";
	//PakFileName_Windows = "UGC" + UGCValue.ToString() + "-Windows.pak";
	//CookedPakLocation_Windows = "D:\\DVG\\SampleProject_UE24C\\Saved\\Cooked\\Workshop\\" + PakFileName_Windows;
	//CookedPakListFilePath = "D:\\DVG\\SampleProject_UE24C\\Saved\\custommap_paklist.txt";
	//GameInstallationPakPath = "D:\\DVG\\KModOutput\\Windows\\ZeroPay\\Content\\Paks\\";

	/* Read the level grabbing tha name */
	FString mapName = dataAsset->Definition.persistentlevel.GetAssetName();
	FString pcvrName = dataAsset->Definition.pcvrlevel.GetAssetName(); 

	//
	// >>>>>>>>>>>>>> COOKING >>>>>>>>>>>>>> 
	//

	sDeployProgress = FText::FromString("Cooking content game (desktop and server)...");


	// JBH Windows+WindowsServer+LinuxServer+
	FString Command = "cmd.exe";
	FString CommandArgs = "cmd.exe /k \" \"" + UnrealBinary + "\" " + ProjectFullFilePath + " -run=cook -targetplatform=Android -versioned -map=/Game/CustomContent/" + UGCValue.ToString() + "/Maps/" + *mapName + "";
	FString CommandWorkingDirectory = "C:\\";

	FString AdditionalArgs = " -NeverCookDir=/Game/CustomContent/" + UGCValue.ToString() + "/Maps/Sublevels/PCVR/" + *pcvrName + "";
	CommandArgs += AdditionalArgs;
#if 0
	if (dataAsset->bCookEverythingEvenIfThatsBads)
	{
		FString AdditionalArgs = " -CookDir=\"" ;
		AdditionalArgs += *FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()) + FString("CustomContent/") + UGCValue.ToString() ;
		AdditionalArgs += "/\"";

		CommandArgs += AdditionalArgs ;
	}
#endif
	CommandArgs += "\"";

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Cooking information"))
	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

	/* Success?? */
	int nSuccess;
	nSuccess = StdOut.Find("Success - 0 error(s)", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess <= 0)
	{
		UE_LOG(LogTemp, Display, TEXT("ERROR RECORDED AS:\n%s"), *StdErr)
		sDeployProgress = FText::FromString(">> ERROR >> ERROR > ERROR --- Cook failed. Please look in the 'output window' to see what went wrong");
		return false ;
	}

	sDeployProgress = FText::FromString("Cooked with no errors...");
	FPlatformProcess::Sleep(1.0f) ;

	//
	// >>>>>>>>>>>>>> PACKING LIST >>>>>>>>>>>>>> 
	//
	
	//if (PackWindows(mapName))
		return (PackAndroid(mapName));
//			if (PackWindowsServer(mapName))
	//		return PackLinuxServer(mapName) ;

	//
	// >>>>>>>>>>>>>> COPYING >>>>>>>>>>>>>> 
	//

	//sDeployProgress = FText::FromString("Moving to location file system...");

	//FString To = GameInstallationPakPath + PakFileName_Windows;
	//FString From = CookedPakLocation_Windows;
	//bool Result = PlatformFile.CopyFile(*To, *From, EPlatformFileRead::None, EPlatformFileWrite::None);	

	return false;
}


bool FZeroPayEditorModule::PackWindows(FString mapName)
{

	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/Windows/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "Windows.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";
	FString GameInstallationPakPath = GameInstallPath.ToString();

	FString Command = "cmd.exe";
	FString CommandArgs = "cmd.exe /k \"" + UnrealBinary + "\" " + ProjectFullFilePath + " -run=cook -targetplatform=Windows+WindowsServer -SkipCookingEditorOnlyData -versioned -map=/Game/CustomContent/" + UGCValue.ToString() + "/Maps/" + mapName + "";
	FString CommandWorkingDirectory = "";

	sDeployProgress = FText::FromString("Generate packing list..");
	FPlatformProcess::Sleep(0.5f);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
		return false;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;
		bool foundUFont = AssetFiles[Index].Find(".ufont") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp || foundUFont)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Windows, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Asset registry */
	FString ProjectAssetRegistryPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectAssetRegistryPath += "Cooked/Windows/" + FString(FApp::GetProjectName()) + "/AssetRegistry.bin";
	FString realignedFilePath = ProjectAssetRegistryPath.Replace(TEXT("\\"), TEXT("/"));
	generatedPakListLine += "\"" + realignedFilePath + "\"   \"../../../AssetRegistry.bin\" \n";

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (WINDOWS) >>>>>>>>>>>>>> 
	//

	sDeployProgress = FText::FromString(FString::Printf(TEXT("Packing desktop content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Windows\" -UTF8Output -multiprocess -patchpaddingalign=2048";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing Information"))
		UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
		UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

		/* Success?? */
		int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false;
	}

	return true;

}


bool FZeroPayEditorModule::PackAndroid(FString mapName)
{

	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/Android/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "Android.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";
	FString GameInstallationPakPath = GameInstallPath.ToString();

	FString Command = "cmd.exe";
	FString CommandArgs = "cmd.exe /k \"" + UnrealBinary + "\" " + ProjectFullFilePath + " -run=cook -targetplatform=Android -SkipCookingEditorOnlyData -versioned -map=/Game/CustomContent/" + UGCValue.ToString() + "/Maps/" + mapName + "";
	FString CommandWorkingDirectory = "";

	sDeployProgress = FText::FromString("Generate Android Quest 3 packing list..");
	FPlatformProcess::Sleep(0.5f);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
		return false ;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;
		bool foundUFont = AssetFiles[Index].Find(".ufont") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp || foundUFont)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Windows, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}		
	}

	/* Asset registry */
	FString ProjectAssetRegistryPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectAssetRegistryPath += "Cooked/Android/" + FString(FApp::GetProjectName()) + "/AssetRegistry.bin";
	FString realignedFilePath = ProjectAssetRegistryPath.Replace(TEXT("\\"), TEXT("/"));
	generatedPakListLine += "\"" + realignedFilePath + "\"   \"../../../AssetRegistry.bin\" \n";

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false ;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (WINDOWS) >>>>>>>>>>>>>> 
	//

	sDeployProgress = FText::FromString(FString::Printf(TEXT("Packing Android / Quest3 content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Android\" -compress -compressionformat=Zlib";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing Information"))
	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

	/* Success?? */
	int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false ;
	}

	return true;
	
}

bool FZeroPayEditorModule::PackWindowsServer(FString mapName)
{
	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Windows += "Cooked/WindowsServer/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Windows = "WindowsServer.pak";
	FString CookedPakLocation_Windows = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Windows += "Workshop/" + PakFileName_Windows;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";
	FString GameInstallationPakPath = GameInstallPath.ToString();

	FString Command;
	FString CommandArgs;
	FString CommandWorkingDirectory = "";

	sDeployProgress = FText::FromString("Generate packing list...");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Windows, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
		return false ;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Windows, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false ;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (WINDOWS) >>>>>>>>>>>>>> 
	//

	sDeployProgress = FText::FromString(FString::Printf(TEXT("Packing windows server content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Windows + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Windows\" -UTF8Output -multiprocess -patchpaddingalign=2048";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing Information"))
	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

	/* Success?? */
	int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false ;
	}

	return true;
}

//-------

bool FZeroPayEditorModule::PackLinuxServer(FString mapName)
{
	FString StdOut;
	FString StdErr;
	int32 ReturnCode = 0;

	// All build paths, names, etc
	FString UnrealBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealBinary += "Engine/Binaries/Win64/UnrealEditor.exe";
	FString UnrealPakBinary = *FPaths::ConvertRelativePathToFull(FPaths::RootDir());
	UnrealPakBinary += "Engine/Binaries/Win64/UnrealPak.exe";
	FString ProjectFullFilePath = *FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	FString ProjectCookedPath_Linux = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	ProjectCookedPath_Linux += "Cooked/LinuxServer/" + FString(FApp::GetProjectName()) + "/Content";
	FString PakFileName_Linux = "LinuxServer.pak";
	FString CookedPakLocation_Linux = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakLocation_Linux += "Workshop/" + PakFileName_Linux;
	FString CookedPakListFilePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	CookedPakListFilePath += "custommap_paklist.txt";
	FString GameInstallationPakPath = GameInstallPath.ToString();

	FString Command;
	FString CommandArgs;
	FString CommandWorkingDirectory = "";

	sDeployProgress = FText::FromString("Generate packing list...");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> AssetFiles;
	// Find all files with uasset extension in the cooked path 
	PlatformFile.FindFilesRecursively(AssetFiles, *ProjectCookedPath_Linux, NULL);
	// Open PAK list for writing
	bool first = true;

	// Nothing? WTF
	if (AssetFiles.Num() <= 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Found nothing to add to pak list!");
		return false;
	}

	// Remove old pak list
	PlatformFile.DeleteFile(*CookedPakListFilePath);

	// Iterate all assets
	FString generatedPakListLine;
	int nTotalFiles = 0;
	for (int32 Index = 0; Index != AssetFiles.Num(); ++Index)
	{
		/* Make sure it's a filetype we care about.. */
		bool foundUAsset = AssetFiles[Index].Find(".uasset") >= 0;
		bool foundUBulk = AssetFiles[Index].Find(".ubulk") >= 0;
		bool foundUMap = AssetFiles[Index].Find(".umap") >= 0;
		bool foundUExp = AssetFiles[Index].Find(".uexp") >= 0;

		if (foundUAsset || foundUBulk || foundUMap || foundUExp)
		{
			// Generate file such as \"FULLPATH\"SPACE\"RELATIVE PATH"
			FString realignedFilePath = AssetFiles[Index].Replace(TEXT("\\"), TEXT("/"));
			FString relativePakFilePath = AssetFiles[Index].Replace(*ProjectCookedPath_Linux, TEXT("../../.."));
			FString realignedrelativePakFilePath = relativePakFilePath.Replace(TEXT("\\"), TEXT("/"));

			generatedPakListLine += "\"" + realignedFilePath + "\"   \"" + realignedrelativePakFilePath + "\" \n";
			nTotalFiles++;
		}
	}

	/* Write all lines.. */
	bool bSuccess = FFileHelper::SaveStringToFile(generatedPakListLine, *CookedPakListFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSuccess)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - Could not write to PAK list file.");
		return false;
	}

	//
	// >>>>>>>>>>>>>> PACKING DESKTOP (LINUX) >>>>>>>>>>>>>> 
	//

	sDeployProgress = FText::FromString(FString::Printf(TEXT("Packing linux server content (%d assets)..."), nTotalFiles));

	Command = UnrealPakBinary;
	CommandArgs = "\"" + CookedPakLocation_Linux + "\" -create=\"" + CookedPakListFilePath + "\" -platform = \"Linux\" -UTF8Output -multiprocess -patchpaddingalign=2048";
	CommandWorkingDirectory = "C:\\";

	FPlatformProcess::ExecProcess(*Command, *CommandArgs, &ReturnCode, &StdOut, &StdErr, *CommandWorkingDirectory);

	UE_LOG(LogTemp, Display, TEXT("CommandArgs -- %s"), *CommandArgs);

	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT(" KillerJim Mod - Packing (Linux Server) Information"))
	UE_LOG(LogTemp, Display, TEXT("--------------------------------------------------------------------------------"))
	UE_LOG(LogTemp, Display, TEXT("\n%s"), *StdOut)

	/* Success?? */
	int nSuccess = StdOut.Find("Unreal pak executed", ESearchCase::IgnoreCase, ESearchDir::FromStart, INDEX_NONE);
	if (nSuccess == 0)
	{
		sDeployProgress = FText::FromString(">>> ERROR >> ERROR > ERROR - PAKing failed. Please look in the 'output window' for more information.");
		return false;
	}

	return true;
}


bool FZeroPayEditorModule::SavePreviewImage(UTexture2D * texture, FString file)
{
	// Disable mipmap/compression
	FTexture2DMipMap* MM = &texture->GetPlatformData()->Mips[0];

	// Get bitmap data
	TArray<FColor> OutBMP;
	int w = MM->SizeX;
	int h = MM->SizeY;

	OutBMP.SetNumUninitialized(w*h);

	FByteBulkData* RawImageData = &MM->BulkData;

	FColor* FormatedImageData = static_cast<FColor*>(RawImageData->Lock(LOCK_READ_ONLY));
	if (FormatedImageData)
	{
		for (int i = 0; i < (w*h); ++i)
		{
			OutBMP[i] = FormatedImageData[i];
			OutBMP[i].A = 255;
		}

		// Save
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
		TSharedPtr<IImageWrapper>PngImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		PngImageWrapper->SetRaw(OutBMP.GetData(), w*h * 4, 512, 512, ERGBFormat::BGRA, 8);

		const TArray64<uint8> PNGData = PngImageWrapper->GetCompressed(100);
		bool success = FFileHelper::SaveArrayToFile(PNGData, *file) ;

		RawImageData->Unlock();
		return success ;
	}
	else
	{
		RawImageData->Unlock();
		return false;
	}

	RawImageData->Unlock();
	return false ;
}


bool FZeroPayEditorModule::UploadUGC()
{
	sDeployProgress = FText::FromString("Uploading...");
	FPlatformProcess::Sleep(0.5f);

	/* Read UGC Value */
	if (nUGCValue == 0)
	{
		sDeployProgress = FText::FromString("Failed, could not find the UGC");
		/* Failed */
		bSetupIsDeploying = false;
		return false;
	}

	// >> STTEAM UGC COMMANDS
	UGCUpdateHandle_t hStartItemUpdate = SteamUGC()->StartItemUpdate(1281150, nUGCValue);
	SteamUGC()->SetItemTitle(hStartItemUpdate, TCHAR_TO_ANSI(*dataAsset->Definition.modName) );
	SteamUGC()->SetItemDescription(hStartItemUpdate, TCHAR_TO_ANSI(*dataAsset->Definition.modDescription));

	FString UGCContentPath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	UGCContentPath += "Workshop";
	SteamUGC()->SetItemContent(hStartItemUpdate, TCHAR_TO_ANSI(*UGCContentPath));

	// item tags

	SteamParamStringArray_t ItemTagsStringArray;
	std::string ItemTags = "";
#if 0
	const char* buffers[4];

	// Tag 1 = ModType
	switch (dataAsset->Definition.modType)
	{
		case StandaloneLevel:
		{
			ItemTags = "StandaloneLevel";
			break;
		}
		case StandaloneMod:
		{
			ItemTags += "StandaloneMod";
			break;
		}
		case ModWithLevel:
		{
			ItemTags += "ModWithLevel";
			break;
		}
		case LobbyLevel:
		{
			ItemTags += "LobbyLevel";
			break;
		}
		case SpawnableAssets:
		{
			ItemTags += "SpawnableAssets";
			break;
		}
		case GamemodeAssets:
		{
			ItemTags += "GamemodeAssets";
			break;
		}
	}
	buffers[0] = ItemTags.c_str() ;

	// Tag 2 = Version
	std::string Version = "1" ;
	buffers[1] = Version.c_str() ;

	// Tag 3 = Release Type
	std::string releaseType;
	if (bIsBetaRelease)
		releaseType = "beta";
	else
		releaseType = "production";
	buffers[2] = releaseType.c_str();

	// Tag 4 = GameModeCustomLabel
	std::string sGameModeCustomLabel = std::string(TCHAR_TO_UTF8(*dataAsset->Definition.GameModeCustomLabel));
	// Should be something or none for CSV
	if (sGameModeCustomLabel == "")
		sGameModeCustomLabel = "none";
	buffers[3] = sGameModeCustomLabel.c_str();
#endif

	// Write Tags
	ItemTagsStringArray.m_nNumStrings = 0;
//	ItemTagsStringArray.m_ppStrings = &buffers[0] ;
//	SteamUGC()->SetItemTags(hStartItemUpdate, &ItemTagsStringArray);

	// Add previewimage
	if (dataAsset->Definition.workshopImage != nullptr)
	{
		sDeployProgress = FText::FromString("Generating preview image...");
		FPlatformProcess::Sleep(0.5f);

		// Preview is saved under cooked directory..
		FString previewImagePath = *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
		previewImagePath += "WorkshopPreviewImage.png" ;

		// If we created it, add it
		if (SavePreviewImage(dataAsset->Definition.workshopImage, previewImagePath))
		{
			SteamUGC()->SetItemPreview(hStartItemUpdate, TCHAR_TO_ANSI(*previewImagePath));
		}
		else
		{
			sDeployProgress = FText::FromString("Warning! Preview image is invalid, ignoring it and continuing...");
			FPlatformProcess::Sleep(4.0f);
		}
	}

	/* Wait.. */
	for (int nCounter = 0; nCounter < 100; nCounter++)
	{
		SteamAPI_RunCallbacks();
		FPlatformProcess::Sleep(0.01f);
	}
	
	// Submit Item...
	bWaitingOnResult = true;

	SteamAPICall_t m_callSubmitItemUpdate = SteamUGC()->SubmitItemUpdate(hStartItemUpdate, TCHAR_TO_ANSI(*sChangeNote.ToString()));
	m_callResultStartItemUpdate.Set(m_callSubmitItemUpdate, this, &FZeroPayEditorModule::OnCallBack_StartItemUpdate);

	/* Wait.. */
	while (bWaitingOnResult)
	{
		SteamAPI_RunCallbacks();
		FPlatformProcess::Sleep(0.1f);

		// Check progress
		uint64 nBytesProcessed ;
		uint64 nBytesTotal ;
		EItemUpdateStatus status = SteamUGC()->GetItemUpdateProgress(hStartItemUpdate, &nBytesProcessed, &nBytesTotal);

		switch (status)
		{
			case k_EItemUpdateStatusInvalid:
			{
				break ;
			}
			case k_EItemUpdateStatusPreparingConfig:
			{
				sDeployProgress = FText::FromString("[Uploading...] Preparing configuration...");
				break;
			}
			case k_EItemUpdateStatusPreparingContent:
			{
				sDeployProgress = FText::FromString("[Uploading...] Preparing content...");
				break;
			}
			case k_EItemUpdateStatusUploadingContent:
			{
				sDeployProgress = FText::FromString(FString::Printf(TEXT("[Uploading...] Uploading (%dKb/%dKb)"), (nBytesProcessed / 1024), (nBytesTotal / 1024)));
				break;
			}
			case k_EItemUpdateStatusUploadingPreviewFile:
			{
				sDeployProgress = FText::FromString("[Uploading...] Uploading preview image...");
				break;
			}
			case k_EItemUpdateStatusCommittingChanges:
			{
				sDeployProgress = FText::FromString("[Uploading...] Comitting all changes...");
				break;
			}
		}
	}

	bSetupIsDeploying = false;

	return true;
}


void FZeroPayEditorModule::OnCallBack_StartItemUpdate(SubmitItemUpdateResult_t* pResult, bool bIOFailure)
{
	if (pResult->m_eResult != k_EResultOK)
	{
		switch (pResult->m_eResult)
		{
			case 9:
			{
				/* File not found */
				sDeployProgress = FText::FromString(FString::Printf(TEXT("FAILED - The upload process failed, could not find the preview image OR UGC deleted on workshop (bad!)."), pResult->m_eResult));
				break;
			}
			case 25:
			{
				/* File too big */
				sDeployProgress = FText::FromString(FString::Printf(TEXT("FAILED - The upload process failed, the preview image needs to be < 1Mb (or something else is broken)"), pResult->m_eResult));
				break;
			}
			default:
			{
				sDeployProgress = FText::FromString(FString::Printf(TEXT("FAILED - The upload process failed, error %d."), pResult->m_eResult));
				break;
			}
		}
		FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing FAILED", "ERROR - Your content was not uploaded correctly..."));
		bWaitingOnResult = false;
		return ;
	}

	sDeployProgress = FText::FromString("SUCCESS - Uploaded content to steam workshop.");
	FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("WindowsDynamicRHI", "ZeroPay Publishing Completed", "Success - Your content has been uploaded correctly!"));
	bWaitingOnResult = false;
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FZeroPayEditorModule, ZeroPayEditorPlugin)