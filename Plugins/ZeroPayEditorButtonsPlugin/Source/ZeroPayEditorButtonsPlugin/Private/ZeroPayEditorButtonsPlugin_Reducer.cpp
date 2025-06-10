// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZeroPayEditorButtonsPlugin.h"
#include "ZeroPayEditorButtonsPluginStyle.h"
#include "ZeroPayEditorButtonsPluginCommands.h"

UZeroPayEditorReduceOperationHandle* FZeroPayEditorButtonsPluginModule::ReduceLevel(UZeroPayMod_DefinitionDataAsset* dataAsset, UZeroPayEditor_ReducerSettingsAsset* reducerSettings)
{
	/* Find the settings file and validate it's correct */
	bAbortOperation = false;
	ReduceHandle = NewObject<UZeroPayEditorReduceOperationHandle>();

	Async(EAsyncExecution::Thread, [this, dataAsset, reducerSettings]()
		{
#if 0
			/* >>> Pack PCVR <<< */
			if (!CookAndPackWindows(dataAsset))
				bAbortOperation = true;

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
					{
						Handle->OnCompleted.Broadcast(false, dataAsset->Definition.UGCID);
					});
				return;
			}
			/* >>> Pack Quest 3 <<< */
			if (!CookAndPackAndroid(dataAsset))
				bAbortOperation = true;

			/* Aborted? */
			if (bAbortOperation)
			{
				// Simulate some logic, then notify later
				AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
					{
						Handle->OnCompleted.Broadcast(false, dataAsset->Definition.UGCID);
					});
				return;
			}
			/* >>> Pack Linux Server <<< */
			if (!CookAndPackLinuxServer(dataAsset))
				bAbortOperation = true;

#endif
			/* All Good! */
			if (!bAbortOperation)
				LastMessage = "Reduction operation completed successfully!";

			UpdateQuest3ReducerUIProgressField();
			FPlatformProcess::Sleep(1.0f);

			// Simulate some logic, then notify later
			AsyncTask(ENamedThreads::GameThread, [this, dataAsset]()
				{
					ReduceHandle->OnCompleted.Broadcast(!bAbortOperation, dataAsset->Definition.UGCID);
				});
		});

	return ReduceHandle;
}



void FZeroPayEditorButtonsPluginModule::UpdateQuest3ReducerUIProgressField()
{
	// Post result back to main thread safely
	Async(EAsyncExecution::TaskGraphMainThread, [this]()
		{
			/* During development, changes to the editor utility BP can cause the instance to disappear */
			if (WidgetQuest3ReducerInstance == nullptr)
				return;
			if (!IsValid(WidgetQuest3ReducerInstance))
				return;

			UFunction* Func = WidgetQuest3ReducerInstance->FindFunction("UpdateUIProgressField");
			if (!Func)
			{
				UE_LOG(LogTemp, Error, TEXT("Function UpdateUIProgressField not found on %s"), *WidgetQuest3ReducerInstance->GetName());
				return;
			}

			// Match the parameter layout: 1 FString
			struct FMyParams
			{
				FString InputString;
			};

			FMyParams Params;
			Params.InputString = LastMessage;

			WidgetQuest3ReducerInstance->ProcessEvent(Func, &Params);
		});
}

