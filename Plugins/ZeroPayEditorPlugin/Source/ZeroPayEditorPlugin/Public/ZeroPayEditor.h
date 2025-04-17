// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Misc/Optional.h"
#include "ThirdParty/Steamworks/Steamv146/sdk/public/steam/steam_api.h"
#include "ZeroPayMod/Public/ZeroPayModDefinitionDataAsset.h"

#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

class FToolBarBuilder;
class FMenuBuilder;

class FZeroPayEditorModule : public IModuleInterface
{
public:
	FZeroPayEditorModule();
	~FZeroPayEditorModule();

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();
	
private:
	UZeroPayModDefinitionDataAsset* dataAsset ;

	void AddToolbarExtension(FToolBarBuilder& Builder);
	void AddMenuExtension(FMenuBuilder& Builder);

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);
	TSharedRef<class SDockTab> OnSpawnPluginTab_ZeroPay(const class FSpawnTabArgs& SpawnTabArgs);	

	const FTextBlockStyle ZeroPayHeading = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Regular", 18))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D(1.0f, 1.0f))
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f));	

	const FTextBlockStyle ZeroPaySubHeading = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Regular", 14))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D(1.0f, 1.0f))
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f));

	const FTextBlockStyle ZeroPayBoldStyle = FTextBlockStyle()
		.SetFont(DEFAULT_FONT("Bold", 11))
		.SetColorAndOpacity(FSlateColor::UseForeground())
		.SetShadowOffset(FVector2D(1.0f, 1.0f))
		.SetShadowColorAndOpacity(FLinearColor::Black)
		.SetHighlightColor(FLinearColor(0.02f, 0.3f, 0.0f));

	// UGC
	FText GetUGCValue() const ;    
	void SetUGCValue(const FText& InText, ETextCommit::Type);
	FText UGCValue ;

	FText GetGameInstallPathValue() const;
	void SetGameInstallPathValueCommitted(const FText& InText, ETextCommit::Type);
	FText GameInstallPath ;

	bool bShowEnableLocalPublishButton;

	FReply OnTestOnClicked();

	// Validation
	void ValidateUGC();

	// Setup complete
	bool bSetupIsValid;
	bool bSetupIsBroken;
	bool bSetupIsDeploying;
	bool bIsSettingUp;
	uint64 nUGCValue;
	bool OnCanPublishOnEnabled() const;
	bool bIsBetaRelease;

	void OnCreateItemCallBack(CreateItemResult_t* pResult, bool bIOFailure);
	CCallResult< FZeroPayEditorModule, CreateItemResult_t> m_callResultCreateItem;

	uint64 nUGCOperationPublishedFieldID;
	bool bUGCOperationCompleted ;
	bool bUGCOperationSuccess ;
	void WriteUGCFiles() ;

	void CreateAndSaveDataDefinition(FString UGCPath, FString SteamWorks_UGCID_);

	// Mode
	bool bInSetupMode;
	bool bInLocalMode;
	bool bInDeployMode;
	FReply OnClick_ShowSetup();
	FReply OnClick_ShowLocal();
	FReply OnClick_ShowDeploy();

	FSlateColor OnGetButtonSetupBackground() const;
	FSlateColor OnGetButtonLocalBackground() const;
	FSlateColor OnGetButtonDeployBackground() const;

	EVisibility OnVisible_Setup() const ;
	EVisibility OnVisible_Local() const ;
	EVisibility OnVisible_Deploy() const ;

	// Set-up : Generate UGC
	FReply OnClick_GenerateUGC();
	void Perform_GenerateUGC();

	bool OnEnabled_GenerateUGC() const;
	FText sSetupProgress ;
	FText GetText_SetupProgress() const;

	// Local 	
	FText sLocalProgress ;
	FText GetText_LocalProgress() const;

	// Deploy	
	FText sChangeNote;
	FText sDeployProgress; 
	FText GetText_DeployProgress() const;
	bool OnEnable_Deploy() const;
	void OnTextCommitted_ChangeNote(const FText& InText, ETextCommit::Type);

	void OnCheckBoxStateChange_Beta(ECheckBoxState state);

	FReply OnClick_Deploy();
	void Perform_Deploy();

	bool CookThings();
	bool PackWindows(FString mapName);
	bool PackAndroid(FString mapName);
	bool PackWindowsServer(FString mapName);
	bool PackLinuxServer(FString mapName);
	bool SavePreviewImage(UTexture2D * texture, FString file) ;
	bool UploadUGC();

	bool bWaitingOnResult;
	void OnCallBack_StartItemUpdate(SubmitItemUpdateResult_t* pResult, bool bIOFailure);
	CCallResult< FZeroPayEditorModule, SubmitItemUpdateResult_t> m_callResultStartItemUpdate;


private:
	TSharedPtr<class FUICommandList> PluginCommands;

};
