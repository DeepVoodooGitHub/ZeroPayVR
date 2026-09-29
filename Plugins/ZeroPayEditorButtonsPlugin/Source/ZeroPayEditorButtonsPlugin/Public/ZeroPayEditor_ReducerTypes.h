#pragma once

#include "CoreMinimal.h"
#include "ZeroPayEditor_ReducerTypes.generated.h"

/* ============================================================================
 * Results
 * ----------------------------------------------------------------------------
 * Returned by value from the reducer entry point. Statistics deliberately live
 * here rather than on the settings asset so that running the reducer does not
 * dirty the settings asset and so that multiple runs can be compared.
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FReducerResults
{
	GENERATED_BODY()

	/* True if the run did not complete. Always check this before trusting counts. */
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results")
	bool bFailed = true;

	/* True if the user cancelled via the slow-task dialog. */
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results")
	bool bCancelled = false;

	/* True if this was a dry run and no changes were written. */
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results")
	bool bDryRun = false;

	/* Human readable failure reason, empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results")
	FString FailureReason;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Before")
	int32 OriginalActorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Before")
	int32 OriginalComponentCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Before")
	int64 OriginalTriangleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Before")
	int64 OriginalVertexCount = 0;

	/* Sum of material slots across all considered components - a draw call proxy. */
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Before")
	int32 OriginalMaterialSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|After")
	int32 ReducedActorCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|After")
	int64 ReducedTriangleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|After")
	int64 ReducedVertexCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|After")
	int32 ReducedMaterialSlotCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 ChunkCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 MergeJobCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 FailedMergeJobCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	float ElapsedSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 AtlasCount = 0;               // atlases created

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 AtlasTileCount = 0;           // texture tiles placed across all atlases

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 DownscaledTileCount = 0;      // tiles shrunk so a chunk fits one atlas

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 UniqueSourceTextureCount = 0; // distinct source textures referenced

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results|Run")
	int32 UnsupportedMaterialCount = 0; // materials whose graph was only partly understood

	/* Non-fatal problems worth surfacing to the user. */
	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer|Results")
	TArray<FString> Warnings;

	void Fail(const FString& InReason)
	{
		bFailed = true;
		FailureReason = InReason;
	}

	void Warn(const FString& InWarning)
	{
		Warnings.Add(InWarning);
		UE_LOG(LogTemp, Warning, TEXT("[ZeroPayReducer] %s"), *InWarning);
	}
};


/* ============================================================================
 * Runtime (per-invocation, not saved) settings
 * ============================================================================
 */

USTRUCT(BlueprintType)
struct FReducerRuntimeSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Runtime")
	bool bStage1_ShowVisualDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Runtime")
	float fStage1_VisualDebugDuration = 15.0f;

	/* Skip the "existing content will be destroyed" confirmation. For automation only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ZeroPay Level Reducer|Runtime")
	bool bSuppressPrompts = false;
};


USTRUCT(BlueprintType)
struct FFoundAssetInformation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 FoundAssets = 0;

	UPROPERTY(BlueprintReadOnly, Category = "ZeroPay Level Reducer")
	int32 FoundInstances = 0;
};
