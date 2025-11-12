#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ZeroPay_ModExtractSettings.generated.h"

/**
 * One row describing a property of a struct.
 */
USTRUCT(BlueprintType)
struct FStructPropertyReportRow
{
	GENERATED_BODY()

	/** Internal or display-clean property name */
	UPROPERTY(BlueprintReadOnly, Category = "StructReport")
	FName Name;

	/** Human-friendly type (e.g., int32, float, TArray<FString>, struct MyData) */
	UPROPERTY(BlueprintReadOnly, Category = "StructReport")
	FString Type;

	/** Default or current value (ExportText form) */
	UPROPERTY(BlueprintReadOnly, Category = "StructReport")
	FString DefaultValue;

	/** Tooltip or description (from metadata/comments if available) */
	UPROPERTY(BlueprintReadOnly, Category = "StructReport")
	FText ToolTip;

	/** Min (from ClampMin/UIMin/SliderMin) for numeric types; empty if not specified */
	UPROPERTY(BlueprintReadOnly, Category = "StructReport")
	FString MinValue;

	/** Max (from ClampMax/UIMax/SliderMax) for numeric types; empty if not specified */
	UPROPERTY(BlueprintReadOnly, Category = "StructReport")
	FString MaxValue;
};

/**
 * BP utilities for inspecting native & Blueprint-defined structs.
 * Build note: Add "StructUtils" to your module's Build.cs dependencies for FInstancedStruct.
 */
UCLASS()
class ZEROPAYMOD_API UZeroPay_ModExtractSettings : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Inspect a struct TYPE (reads default values). Works with native USTRUCTs and UserDefinedStruct assets. */
	UFUNCTION(BlueprintPure, Category = "ZeroPay|ModTools", meta = (DisplayName = "Inspect Struct (Defaults)"))
	static TArray<FStructPropertyReportRow> InspectStructDefaults(const UScriptStruct* StructType);

	/** Inspect a live instance via InstancedStruct (reads current values). */
	UFUNCTION(BlueprintPure, Category = "ZeroPay|ModTools", meta = (DisplayName = "Inspect Instanced Struct"))
	static TArray<FStructPropertyReportRow> InspectInstancedStruct(const struct FInstancedStruct& Instance);
};
