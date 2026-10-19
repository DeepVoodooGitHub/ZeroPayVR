// ZeroPay Blueprint Error Navigator
//
// Turns a LogScript warning such as
//   LogScript: Warning: Accessed None trying to read property CallFunc_Foo_ReturnValue
//     BP_ZP_VRCharacter_r1_C /Game/Maps/...:PersistentLevel.BP_ZP_VRCharacter_r1_C_0
//     Function /ZeroPayMod/.../BP_ZP_VRCharacter_r1.BP_ZP_VRCharacter_r1_C:ExecuteUbergraph_BP_ZP_VRCharacter_r1:3970
// into the Blueprint graph node responsible for it. Two independent lookups are made and cross-checked:
//
// 1. OFFSET LOOKUP
//    FFrame::Serialize (ScriptCore.cpp) prints the location as "%s:%04X": the number is HEXADECIMAL and is the VM
//    instruction pointer when the warning fired, usually mid-instruction. FKismetDebugUtilities::OnScriptException
//    maps it back by subtracting one and taking the first debug-mapped location at or after it; this tool does the
//    same. It is only valid if the bytecode that wrote the log is byte-for-byte what the editor compiles now: any
//    difference earlier in the function (and ExecuteUbergraph contains every event) shifts every later offset.
//
// 2. NAME LOOKUP (does not use the offset)
//    The property named in the warning is the reference that was None. The compiler records which pin created each
//    generated variable (CallFunc_X_ReturnValue, K2Node_DynamicCast_AsY, ...), so that pin is found directly; for
//    ordinary variables the Get nodes in the graphs compiled into the function are found by name.
//
// If the node at the offset does not consume the named property, the bytecode differs and the name lookup wins.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Internationalization/Regex.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/Docking/TabManager.h"
#include "Textures/SlateIcon.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ZeroPayBlueprintErrorNavigator"

DEFINE_LOG_CATEGORY_STATIC(LogZeroPayBPErrorNav, Log, All);

namespace ZeroPayBPErrorNav
{
    static const FName TabId(TEXT("ZeroPayBlueprintErrorNavigator"));

    // Same value as UEdGraphSchema_K2::PC_Exec. Spelled out so this module does not need a BlueprintGraph dependency.
    static const FName ExecPinCategory(TEXT("exec"));

    struct FParsedLocation
    {
        FString ClassPath;
        FString FunctionName;
        FString OffsetText;
        int32 CodeOffset = INDEX_NONE;
    };

    struct FResolvedLocation
    {
        UEdGraphNode* Node = nullptr;
        int32 Offset = INDEX_NONE;
    };

    struct FCandidate
    {
        UEdGraphNode* RawNode = nullptr;     // Node recorded in the debug data for this bytecode.
        UEdGraphNode* VisibleNode = nullptr; // Node to show (macro instance if RawNode lives in a macro library).
        int32 MappedOffset = INDEX_NONE;
        TArray<UEdGraphNode*> ExpansionChain;
    };

    struct FNameLookupResult
    {
        FString How;
        TArray<UEdGraphNode*> Producers; // Nodes whose output is the reference that was None.
        TArray<UEdGraphNode*> Consumers; // Impure nodes that use that output (where the access happened).
    };

    // ------------------------------------------------------------------------------------------------------------
    // Parsing
    // ------------------------------------------------------------------------------------------------------------

    // The number after the last colon is HEXADECIMAL: ":3970" is 0x3970 = 14704, ":03A4" is 0x03A4 = 932.
    static bool ParseLocation(const FString& Input, FParsedLocation& Out)
    {
        static const TCHAR* const Patterns[] =
        {
            // Preferred: the "Function <class path>:<function name>:<hex offset>" line LogScript prints.
            TEXT("Function\\s+(/[^\\s:]+):([^:\\r\\n]+):([0-9A-Fa-f]+)"),
            // Fallback: the same location without the "Function " prefix.
            TEXT("(/[^\\s:]+):([A-Za-z_][^:\\r\\n]*):([0-9A-Fa-f]+)")
        };

        for (const TCHAR* PatternText : Patterns)
        {
            const FRegexPattern Pattern(PatternText);
            FRegexMatcher Matcher(Pattern, Input);
            if (!Matcher.FindNext())
            {
                continue;
            }

            Out.ClassPath = Matcher.GetCaptureGroup(1).TrimStartAndEnd();
            Out.FunctionName = Matcher.GetCaptureGroup(2).TrimStartAndEnd();
            Out.OffsetText = Matcher.GetCaptureGroup(3).ToUpper();

            const int64 Parsed = FCString::Strtoi64(*Out.OffsetText, nullptr, 16);
            if (Out.ClassPath.IsEmpty() || Out.FunctionName.IsEmpty() || Parsed < 0 || Parsed > MAX_int32)
            {
                return false;
            }

            Out.CodeOffset = static_cast<int32>(Parsed);
            return true;
        }

        return false;
    }

    static FString ReadNameAfter(const FString& Input, const TCHAR* Marker, const TCHAR* Terminator)
    {
        const int32 MarkerIndex = Input.Find(Marker, ESearchCase::IgnoreCase);
        if (MarkerIndex == INDEX_NONE)
        {
            return FString();
        }

        const int32 Start = MarkerIndex + FCString::Strlen(Marker);
        int32 End = Start;
        while (End < Input.Len() && Input[End] != TEXT('\r') && Input[End] != TEXT('\n') && Input[End] != TEXT('"'))
        {
            ++End;
        }

        FString Name = Input.Mid(Start, End - Start);
        if (Terminator)
        {
            const int32 TerminatorIndex = Name.Find(Terminator, ESearchCase::IgnoreCase);
            if (TerminatorIndex != INDEX_NONE)
            {
                Name.LeftInline(TerminatorIndex);
            }
        }

        Name.TrimStartAndEndInline();
        Name.RemoveFromStart(TEXT("'"));
        Name.RemoveFromEnd(TEXT("'"));
        while (Name.EndsWith(TEXT(".")))
        {
            Name.LeftChopInline(1);
        }
        return Name;
    }

    // "Accessed None trying to read property X"
    // "Attempted to access Y via property X, but X is not valid (pending kill or garbage)"
    static FString ExtractNoneReferenceName(const FString& Input)
    {
        FString Name = ReadNameAfter(Input, TEXT("trying to read property "), nullptr);
        if (Name.IsEmpty())
        {
            Name = ReadNameAfter(Input, TEXT("via property "), TEXT(", but"));
        }
        return Name;
    }

    // ------------------------------------------------------------------------------------------------------------
    // Graph helpers
    // ------------------------------------------------------------------------------------------------------------

    // The compiler sanitises generated variable names; mirror that loosely so pin names with spaces still match.
    static FString NormalizeName(const FString& In)
    {
        FString Out;
        Out.Reserve(In.Len());
        for (int32 Index = 0; Index < In.Len(); ++Index)
        {
            const TCHAR Ch = In[Index];
            Out.AppendChar((FChar::IsAlnum(Ch) || Ch == TEXT('_')) ? Ch : TEXT('_'));
        }
        return Out;
    }

    // Generated names get "_<n>" appended when they collide, e.g. CallFunc_GetPlayerPawn_ReturnValue_1.
    static FString StripNumericSuffix(const FString& In)
    {
        int32 UnderscoreIndex = INDEX_NONE;
        if (In.FindLastChar(TEXT('_'), UnderscoreIndex) && UnderscoreIndex > 0 && UnderscoreIndex < In.Len() - 1)
        {
            for (int32 Index = UnderscoreIndex + 1; Index < In.Len(); ++Index)
            {
                if (!FChar::IsDigit(In[Index]))
                {
                    return In;
                }
            }
            return In.Left(UnderscoreIndex);
        }
        return In;
    }

    static bool HasExecPins(const UEdGraphNode* Node)
    {
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin && Pin->PinType.PinCategory == ExecPinCategory)
            {
                return true;
            }
        }
        return false;
    }

    // Variable Get and Function Entry nodes name their output pins after the variable/parameter itself.
    static bool IsVariableSourceNode(const UEdGraphNode* Node)
    {
        static const FName VariableGetClassName(TEXT("K2Node_VariableGet"));
        static const FName FunctionEntryClassName(TEXT("K2Node_FunctionEntry"));

        for (const UClass* Class = Node->GetClass(); Class; Class = Class->GetSuperClass())
        {
            if (Class->GetFName() == VariableGetClassName || Class->GetFName() == FunctionEntryClassName)
            {
                return true;
            }
        }
        return false;
    }

    static bool PinProducesName(const UEdGraphPin* OutputPin, const FString& WantedName, const FString& WantedNameNoSuffix)
    {
        const UEdGraphNode* Owner = OutputPin->GetOwningNode();
        if (!Owner)
        {
            return false;
        }

        if (IsVariableSourceNode(Owner) && NormalizeName(OutputPin->PinName.ToString()).Equals(WantedName, ESearchCase::IgnoreCase))
        {
            return true;
        }

        // Compiler temporaries: <DescriptiveCompiledName>_<PinName>, e.g. CallFunc_GetPlayerPawn_ReturnValue,
        // K2Node_DynamicCast_AsBP_Foo, K2Node_CustomEvent_OtherActor.
        const FString CompiledName = NormalizeName(Owner->GetDescriptiveCompiledName() + TEXT("_") + OutputPin->PinName.ToString());
        return CompiledName.Equals(WantedName, ESearchCase::IgnoreCase)
            || CompiledName.Equals(WantedNameNoSuffix, ESearchCase::IgnoreCase);
    }

    // Pure nodes have no bytecode of their own; they run inside the impure node that consumes them. Walk the data
    // inputs of the executing node (through pure nodes, never past an impure one) for whoever produced the None value.
    static void FindNoneReferenceSources(UEdGraphNode* ExecutingNode, const FString& ReferenceName, TArray<UEdGraphNode*>& OutSources)
    {
        if (!ExecutingNode || ReferenceName.IsEmpty())
        {
            return;
        }

        const FString Wanted = NormalizeName(ReferenceName);
        const FString WantedNoSuffix = StripNumericSuffix(Wanted);

        TSet<UEdGraphNode*> Visited;
        TArray<UEdGraphNode*> Queue;
        Queue.Add(ExecutingNode);
        Visited.Add(ExecutingNode);

        for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
        {
            UEdGraphNode* Current = Queue[QueueIndex];
            for (UEdGraphPin* Pin : Current->Pins)
            {
                if (!Pin || Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == ExecPinCategory)
                {
                    continue;
                }

                for (UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    UEdGraphNode* Upstream = Linked ? Linked->GetOwningNode() : nullptr;
                    if (!Upstream)
                    {
                        continue;
                    }

                    if (PinProducesName(Linked, Wanted, WantedNoSuffix))
                    {
                        OutSources.AddUnique(Upstream);
                    }

                    if (!Visited.Contains(Upstream) && !HasExecPins(Upstream))
                    {
                        Visited.Add(Upstream);
                        Queue.Add(Upstream);
                    }
                }
            }
        }
    }

    // Follow an output pin downstream (through pure nodes) to the impure nodes that actually execute with its value.
    static void CollectImpureConsumers(UEdGraphPin* OutputPin, TArray<UEdGraphNode*>& OutConsumers)
    {
        TSet<UEdGraphNode*> Visited;
        TArray<UEdGraphPin*> PinQueue;
        PinQueue.Add(OutputPin);

        for (int32 QueueIndex = 0; QueueIndex < PinQueue.Num(); ++QueueIndex)
        {
            UEdGraphPin* Current = PinQueue[QueueIndex];
            for (UEdGraphPin* Linked : Current->LinkedTo)
            {
                UEdGraphNode* Node = Linked ? Linked->GetOwningNode() : nullptr;
                if (!Node || Visited.Contains(Node))
                {
                    continue;
                }
                Visited.Add(Node);

                if (HasExecPins(Node))
                {
                    OutConsumers.AddUnique(Node);
                    continue;
                }

                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != ExecPinCategory)
                    {
                        PinQueue.Add(Pin);
                    }
                }
            }
        }
    }

    // Graphs whose nodes compile into Function: the event graph pages (and their collapsed sub-graphs) for
    // ExecuteUbergraph, otherwise the function graph with the same name.
    static void GetGraphsCompiledInto(UBlueprint* Blueprint, UFunction* Function, TArray<UEdGraph*>& OutGraphs)
    {
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);

        const bool bUbergraph = Function->HasAnyFunctionFlags(FUNC_UbergraphFunction);
        for (UEdGraph* Graph : AllGraphs)
        {
            UEdGraph* TopLevel = Graph ? FBlueprintEditorUtils::GetTopLevelGraph(Graph) : nullptr;
            if (!TopLevel)
            {
                continue;
            }

            const bool bMatches = bUbergraph
                ? Blueprint->UbergraphPages.Contains(TopLevel)
                : (TopLevel->GetFName() == Function->GetFName());
            if (bMatches)
            {
                OutGraphs.Add(Graph);
            }
        }
    }

    // NAME LOOKUP: independent of bytecode offsets.
    static void FindByReferenceName(UBlueprint* Blueprint, UBlueprintGeneratedClass* OwnerClass, UFunction* Function, FBlueprintDebugData& DebugData, const FString& ReferenceName, FNameLookupResult& Out)
    {
        TArray<UEdGraphPin*> ProducerPins;
        const FName PropertyName(*ReferenceName);

        FProperty* Property = FindFProperty<FProperty>(Function, PropertyName);
        if (!Property)
        {
            Property = FindFProperty<FProperty>(OwnerClass, PropertyName);
        }

        // Generated variables remember the pin that created them.
        if (Property)
        {
            UEdGraphPin* CreatorPin = DebugData.FindPinThatCreatedProperty(Property);
            UEdGraphNode* CreatorNode = CreatorPin ? CreatorPin->GetOwningNode() : nullptr;
            if (CreatorNode && (!Blueprint || FBlueprintEditorUtils::FindBlueprintForNode(CreatorNode) == Blueprint))
            {
                ProducerPins.Add(CreatorPin);
                Out.How = TEXT("exact, from the compiler's debug data");
            }
        }

        // Ordinary variables (and anything the debug data does not know): match names in the relevant graphs.
        if (ProducerPins.Num() == 0 && Blueprint)
        {
            const FString Wanted = NormalizeName(ReferenceName);
            const FString WantedNoSuffix = StripNumericSuffix(Wanted);

            TArray<UEdGraph*> Graphs;
            GetGraphsCompiledInto(Blueprint, Function, Graphs);

            for (UEdGraph* Graph : Graphs)
            {
                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (!Node)
                    {
                        continue;
                    }

                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != ExecPinCategory
                            && Pin->LinkedTo.Num() > 0 && PinProducesName(Pin, Wanted, WantedNoSuffix))
                        {
                            ProducerPins.Add(Pin);
                        }
                    }
                }
            }

            if (ProducerPins.Num() > 0)
            {
                Out.How = TEXT("by name, in the graphs compiled into this function");
            }
        }

        for (UEdGraphPin* Pin : ProducerPins)
        {
            Out.Producers.AddUnique(Pin->GetOwningNode());
            CollectImpureConsumers(Pin, Out.Consumers);
        }
    }

    // ------------------------------------------------------------------------------------------------------------
    // Offset lookup helpers
    // ------------------------------------------------------------------------------------------------------------

    // Same search FBlueprintDebugData::FindSourceNodeFromCodeLocation does with bAllowImpreciseHit = true,
    // but it also reports which offset matched (needed for the macro expansion lookup).
    static FResolvedLocation FindMappingAtOrAfter(FBlueprintDebugData& DebugData, UFunction* Function, int32 StartOffset)
    {
        FResolvedLocation Result;
        for (int32 Trial = FMath::Max(0, StartOffset); Trial < Function->Script.Num(); ++Trial)
        {
            if (UEdGraphNode* Node = DebugData.FindSourceNodeFromCodeLocation(Function, Trial, false))
            {
                Result.Node = Node;
                Result.Offset = Trial;
                break;
            }
        }
        return Result;
    }

    static FResolvedLocation FindMappingBefore(FBlueprintDebugData& DebugData, UFunction* Function, int32 StartOffset)
    {
        FResolvedLocation Result;
        for (int32 Trial = FMath::Min(StartOffset, Function->Script.Num() - 1); Trial >= 0; --Trial)
        {
            if (UEdGraphNode* Node = DebugData.FindSourceNodeFromCodeLocation(Function, Trial, false))
            {
                Result.Node = Node;
                Result.Offset = Trial;
                break;
            }
        }
        return Result;
    }

    // Bytecode expanded from a macro (ForEachLoop, etc.) is recorded against the node inside the macro graph,
    // which may live in another asset such as StandardMacros. Prefer the macro instance in the Blueprint being debugged.
    static FCandidate MakeCandidate(FBlueprintDebugData& DebugData, UFunction* Function, const FResolvedLocation& Resolved, UBlueprint* OwnerBlueprint)
    {
        FCandidate Candidate;
        Candidate.RawNode = Resolved.Node;
        Candidate.VisibleNode = Resolved.Node;
        Candidate.MappedOffset = Resolved.Offset;

        if (!Resolved.Node)
        {
            return Candidate;
        }

        if (const TArray<TWeakObjectPtr<UEdGraphNode>>* Expansion = DebugData.FindExpansionSourceNodesFromCodeLocation(Function, Resolved.Offset))
        {
            for (const TWeakObjectPtr<UEdGraphNode>& WeakNode : *Expansion)
            {
                if (UEdGraphNode* Node = WeakNode.Get())
                {
                    Candidate.ExpansionChain.Add(Node);
                }
            }
        }

        if (!OwnerBlueprint || FBlueprintEditorUtils::FindBlueprintForNode(Resolved.Node) == OwnerBlueprint)
        {
            return Candidate;
        }

        UEdGraphNode* Fallback = nullptr;
        for (UEdGraphNode* Node : Candidate.ExpansionChain)
        {
            if (FBlueprintEditorUtils::FindBlueprintForNode(Node) != OwnerBlueprint)
            {
                continue;
            }

            // Prefer the instance placed in an ordinary graph over one inside a local macro graph.
            if (!OwnerBlueprint->MacroGraphs.Contains(Node->GetGraph()))
            {
                Candidate.VisibleNode = Node;
                return Candidate;
            }

            if (!Fallback)
            {
                Fallback = Node;
            }
        }

        if (Fallback)
        {
            Candidate.VisibleNode = Fallback;
        }
        return Candidate;
    }

    // ------------------------------------------------------------------------------------------------------------
    // Output helpers
    // ------------------------------------------------------------------------------------------------------------

    static FString DescribeNode(const UEdGraphNode* Node)
    {
        if (!Node)
        {
            return TEXT("<none>");
        }

        const UEdGraph* Graph = Node->GetGraph();
        return FString::Printf(TEXT("'%s' (graph '%s')"),
            *Node->GetNodeTitle(ENodeTitleType::ListView).ToString(),
            Graph ? *Graph->GetName() : TEXT("?"));
    }

    static FString DescribeNodes(const TArray<UEdGraphNode*>& Nodes, int32 MaxCount = 6)
    {
        TArray<FString> Parts;
        for (int32 Index = 0; Index < Nodes.Num() && Index < MaxCount; ++Index)
        {
            Parts.Add(DescribeNode(Nodes[Index]));
        }
        if (Nodes.Num() > MaxCount)
        {
            Parts.Add(FString::Printf(TEXT("... and %d more"), Nodes.Num() - MaxCount));
        }
        return FString::Join(Parts, TEXT(", "));
    }

    static void FocusGraphNode(const UEdGraphNode* Node)
    {
        if (Node)
        {
            FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(Node, false);
        }
    }
}

class SZeroPayBlueprintErrorNavigator : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SZeroPayBlueprintErrorNavigator) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        ChildSlot
            [
                SNew(SBorder)
                    .Padding(12.0f)
                    [
                        SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(STextBlock)
                                    .Text(LOCTEXT("Instructions", "Paste the complete LogScript warning, including the 'Accessed None trying to read property ...' line: the property name enables a lookup that still works when the build that wrote the log has different bytecode from the editor."))
                                    .AutoWrapText(true)
                            ]
                            + SVerticalBox::Slot().FillHeight(1.0f).Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SAssignNew(InputBox, SMultiLineEditableTextBox)
                                    .HintText(LOCTEXT("InputHint", "LogScript: Warning: Accessed None trying to read property MyRef\n    BP_X_C /Game/Maps/Map.Map:PersistentLevel.BP_X_C_0\n    Function /ZeroPayMod/Blueprints/.../BP_X.BP_X_C:ExecuteUbergraph_BP_X:3970"))
                                    .AutoWrapText(true)
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SButton)
                                    .HAlign(HAlign_Center)
                                    .Text(LOCTEXT("LocateButton", "Locate Node in Blueprint Editor"))
                                    .OnClicked(this, &SZeroPayBlueprintErrorNavigator::OnLocateClicked)
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
                            [
                                SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 4.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                            .HAlign(HAlign_Center)
                                            .Text(LOCTEXT("ShowExecutingButton", "Show Executing Node"))
                                            .ToolTipText(LOCTEXT("ShowExecutingTooltip", "The impure node that was running when the warning fired (best available answer after cross-checking the offset against the property name)."))
                                            .IsEnabled_Lambda([this]() { return LastExecutingNode.IsValid(); })
                                            .OnClicked_Lambda([this]()
                                                {
                                                    ZeroPayBPErrorNav::FocusGraphNode(LastExecutingNode.Get());
                                                    return FReply::Handled();
                                                })
                                    ]
                                + SHorizontalBox::Slot().FillWidth(1.0f).Padding(4.0f, 0.0f, 0.0f, 0.0f)
                                    [
                                        SNew(SButton)
                                            .HAlign(HAlign_Center)
                                            .Text_Lambda([this]()
                                                {
                                                    const FText Base = LastPropertyName.IsEmpty()
                                                        ? LOCTEXT("ShowSourceGeneric", "Show Source of None Value")
                                                        : FText::Format(LOCTEXT("ShowSourceFmt", "Show '{0}' Source"), FText::FromString(LastPropertyName));

                                                    if (SourceNodes.Num() > 1)
                                                    {
                                                        return FText::Format(LOCTEXT("ShowSourceCountFmt", "{0} ({1} of {2})"), Base, FText::AsNumber(SourceIndex + 1), FText::AsNumber(SourceNodes.Num()));
                                                    }
                                                    return Base;
                                                })
                                            .ToolTipText(LOCTEXT("ShowSourceTooltip", "The node that produced the reference named in the warning. Click again to step through multiple candidates."))
                                            .IsEnabled_Lambda([this]() { return SourceNodes.Num() > 0; })
                                            .OnClicked_Lambda([this]()
                                                {
                                                    if (SourceNodes.Num() > 0)
                                                    {
                                                        SourceIndex = FMath::Clamp(SourceIndex, 0, SourceNodes.Num() - 1);
                                                        ZeroPayBPErrorNav::FocusGraphNode(SourceNodes[SourceIndex].Get());
                                                        SourceIndex = (SourceIndex + 1) % SourceNodes.Num();
                                                    }
                                                    return FReply::Handled();
                                                })
                                    ]
                            ]
                        + SVerticalBox::Slot().AutoHeight()
                            [
                                SAssignNew(StatusText, STextBlock)
                                    .Text(LOCTEXT("InitialStatus", "Ready."))
                                    .AutoWrapText(true)
                            ]
                    ]
            ];
    }

private:
    TSharedPtr<SMultiLineEditableTextBox> InputBox;
    TSharedPtr<STextBlock> StatusText;

    TWeakObjectPtr<UEdGraphNode> LastExecutingNode;
    TArray<TWeakObjectPtr<UEdGraphNode>> SourceNodes;
    int32 SourceIndex = 0;
    FString LastPropertyName;

    void SetStatus(const FString& Message, bool bError = false)
    {
        StatusText->SetText(FText::FromString(Message));

        if (bError)
        {
            UE_LOG(LogZeroPayBPErrorNav, Warning, TEXT("%s"), *Message);
        }
        else
        {
            UE_LOG(LogZeroPayBPErrorNav, Display, TEXT("%s"), *Message);
        }
    }

    void SetSourceNodes(const TArray<UEdGraphNode*>& Nodes)
    {
        SourceNodes.Reset();
        SourceIndex = 0;
        for (UEdGraphNode* Node : Nodes)
        {
            SourceNodes.Add(Node);
        }
    }

    FReply OnLocateClicked()
    {
        using namespace ZeroPayBPErrorNav;

        LastExecutingNode.Reset();
        SourceNodes.Reset();
        SourceIndex = 0;
        LastPropertyName.Reset();

        const FString Input = InputBox->GetText().ToString();

        FParsedLocation Location;
        if (!ParseLocation(Input, Location))
        {
            if (Input.Contains(TEXT("Script Msg")))
            {
                SetStatus(TEXT("This is a 'Script Msg:' warning. Those are logged with a call stack but without a bytecode offset, so they cannot be mapped to a node. Only warnings that include a 'Function /..._C:Name:XXXX' line can."), true);
            }
            else
            {
                SetStatus(TEXT("No Blueprint code location found. Paste a 'Function /..._C:Name:XXXX' line (XXXX is hexadecimal)."), true);
            }
            return FReply::Handled();
        }

        UBlueprintGeneratedClass* GeneratedClass = LoadObject<UBlueprintGeneratedClass>(nullptr, *Location.ClassPath);
        if (!GeneratedClass)
        {
            SetStatus(FString::Printf(TEXT("Could not load generated Blueprint class: %s"), *Location.ClassPath), true);
            return FReply::Handled();
        }

        const FName FunctionName(*Location.FunctionName);
        UFunction* Function = GeneratedClass->FindFunctionByName(FunctionName, EIncludeSuperFlag::ExcludeSuper);
        if (!Function)
        {
            Function = GeneratedClass->FindFunctionByName(FunctionName, EIncludeSuperFlag::IncludeSuper);
        }
        if (!Function)
        {
            SetStatus(FString::Printf(TEXT("Function '%s' not found in %s"), *Location.FunctionName, *GeneratedClass->GetName()), true);
            return FReply::Handled();
        }

        // Debug data is keyed per UFunction and stored on the class that owns the function
        // (the same class FKismetDebugUtilities::FindClassForNode uses).
        UBlueprintGeneratedClass* OwnerClass = Cast<UBlueprintGeneratedClass>(Function->GetOwnerClass());
        if (!OwnerClass)
        {
            SetStatus(FString::Printf(TEXT("'%s' is not owned by a Blueprint generated class."), *Function->GetName()), true);
            return FReply::Handled();
        }

        UBlueprint* OwnerBlueprint = UBlueprint::GetBlueprintFromClass(OwnerClass);
        FBlueprintDebugData& DebugData = OwnerClass->GetDebugData();
        const int32 ScriptSize = Function->Script.Num();
        LastPropertyName = ExtractNoneReferenceName(Input);

        TArray<FString> Lines;
        Lines.Add(FString::Printf(TEXT("%s : %s   offset 0x%s = %d   (editor's bytecode for this function is %d bytes)"),
            *OwnerClass->GetName(), *Function->GetName(), *Location.OffsetText, Location.CodeOffset, ScriptSize));

        // ---- 1. Offset lookup ---------------------------------------------------------------------------------
        FCandidate Focus;
        FCandidate Alternative;
        TArray<UEdGraphNode*> OffsetSources;

        if (Location.CodeOffset > ScriptSize)
        {
            Lines.Add(TEXT("OFFSET LOOKUP: skipped - the offset is past the end of the editor's bytecode, so the build that wrote the log has different bytecode."));
        }
        else
        {
            const int32 LookupOffset = FMath::Max(0, Location.CodeOffset - 1);
            FResolvedLocation Primary = FindMappingAtOrAfter(DebugData, Function, LookupOffset);
            const FResolvedLocation Previous = FindMappingBefore(DebugData, Function, LookupOffset - 1);

            if (!Primary.Node && !Previous.Node)
            {
                Lines.Add(TEXT("OFFSET LOOKUP: no node/bytecode mapping exists for this function. Compile the Blueprint (without changing it) and try again."));
            }
            else
            {
                const bool bUsedPreviousAsPrimary = (Primary.Node == nullptr);
                if (bUsedPreviousAsPrimary)
                {
                    Primary = Previous;
                }

                Focus = MakeCandidate(DebugData, Function, Primary, OwnerBlueprint);
                if (!bUsedPreviousAsPrimary && Previous.Node)
                {
                    Alternative = MakeCandidate(DebugData, Function, Previous, OwnerBlueprint);
                    if (Alternative.VisibleNode == Focus.VisibleNode)
                    {
                        Alternative = FCandidate();
                    }
                }

                bool bSwappedToAlternative = false;
                if (!LastPropertyName.IsEmpty())
                {
                    FindNoneReferenceSources(Focus.VisibleNode, LastPropertyName, OffsetSources);
                    if (OffsetSources.Num() == 0 && Alternative.VisibleNode)
                    {
                        FindNoneReferenceSources(Alternative.VisibleNode, LastPropertyName, OffsetSources);
                        if (OffsetSources.Num() > 0)
                        {
                            Swap(Focus, Alternative);
                            bSwappedToAlternative = true;
                        }
                    }
                }

                Lines.Add(FString::Printf(TEXT("OFFSET LOOKUP: %s   [debug mapping at 0x%04X]"), *DescribeNode(Focus.VisibleNode), Focus.MappedOffset));

                if (Focus.VisibleNode != Focus.RawNode)
                {
                    Lines.Add(FString::Printf(TEXT("   The bytecode belongs to %s inside a macro; showing the macro instance in this Blueprint."), *DescribeNode(Focus.RawNode)));
                }
                else if (Focus.ExpansionChain.Num() > 0)
                {
                    Lines.Add(FString::Printf(TEXT("   Expanded from: %s"), *DescribeNodes(Focus.ExpansionChain)));
                }

                if (bUsedPreviousAsPrimary)
                {
                    Lines.Add(TEXT("   (No mapping after the offset - used the nearest one before it.)"));
                }
                else if (bSwappedToAlternative)
                {
                    Lines.Add(FString::Printf(TEXT("   Chosen over %s because '%s' is wired into this node, not that one."), *DescribeNode(Alternative.VisibleNode), *LastPropertyName));
                }
                else if (Alternative.VisibleNode)
                {
                    Lines.Add(FString::Printf(TEXT("   Neighbouring node just before the offset: %s   [0x%04X]"), *DescribeNode(Alternative.VisibleNode), Alternative.MappedOffset));
                }
            }
        }

        // ---- 2. Name lookup (independent of the offset) -------------------------------------------------------
        FNameLookupResult NameResult;
        if (!LastPropertyName.IsEmpty())
        {
            FindByReferenceName(OwnerBlueprint, OwnerClass, Function, DebugData, LastPropertyName, NameResult);

            if (NameResult.Producers.Num() > 0)
            {
                Lines.Add(FString::Printf(TEXT("NAME LOOKUP: '%s' (the None value) comes from %s   [%s]"),
                    *LastPropertyName, *DescribeNodes(NameResult.Producers), *NameResult.How));

                if (NameResult.Consumers.Num() > 0)
                {
                    Lines.Add(FString::Printf(TEXT("   Used by: %s"), *DescribeNodes(NameResult.Consumers)));
                }
            }
            else
            {
                Lines.Add(FString::Printf(TEXT("NAME LOOKUP: nothing in the graphs compiled into %s produces '%s'."), *Function->GetName(), *LastPropertyName));
            }
        }
        else if (Input.Contains(TEXT("Accessed None")))
        {
            Lines.Add(TEXT("NAME LOOKUP: not possible - 'Accessed None' without a property name means the None object came straight from a function return value or a literal."));
        }
        else
        {
            Lines.Add(TEXT("NAME LOOKUP: not possible - paste the whole warning so the property name is included."));
        }

        // ---- 3. Verdict ---------------------------------------------------------------------------------------
        const bool bHaveOffsetNode = (Focus.VisibleNode != nullptr);
        const bool bHaveNameResult = (NameResult.Producers.Num() > 0);
        const bool bOffsetAgrees = bHaveOffsetNode
            && (OffsetSources.Num() > 0 || NameResult.Consumers.Contains(Focus.VisibleNode));

        UEdGraphNode* ExecutingNode = nullptr;

        if (bHaveNameResult && bOffsetAgrees)
        {
            ExecutingNode = Focus.VisibleNode;
            SetSourceNodes(OffsetSources.Num() > 0 ? OffsetSources : NameResult.Producers);
            Lines.Add(TEXT("VERDICT: offset and property name agree."));
        }
        else if (bHaveNameResult)
        {
            ExecutingNode = (NameResult.Consumers.Num() == 1) ? NameResult.Consumers[0] : nullptr;
            SetSourceNodes(NameResult.Producers);

            if (bHaveOffsetNode)
            {
                Lines.Add(FString::Printf(TEXT("VERDICT: MISMATCH - the node at the offset does not use '%s'. The bytecode that wrote the log is not the same as the editor's compile of this function, so ignore the offset and trust the name lookup."), *LastPropertyName));
            }
            else
            {
                Lines.Add(TEXT("VERDICT: using the name lookup."));
            }

            if (!ExecutingNode && NameResult.Consumers.Num() > 1)
            {
                Lines.Add(TEXT("   Several nodes use that value; step through the sources with the right-hand button."));
            }
        }
        else
        {
            ExecutingNode = Focus.VisibleNode;
            SetSourceNodes(OffsetSources);
            if (bHaveOffsetNode)
            {
                Lines.Add(TEXT("VERDICT: offset lookup only (could not be cross-checked) - treat it as unverified."));
            }
        }

        if (OwnerBlueprint && OwnerBlueprint->Status == BS_Dirty)
        {
            Lines.Add(TEXT("Note: this Blueprint has uncompiled changes; offsets refer to its last compile."));
        }
        if (OwnerBlueprint && OwnerBlueprint->GetPackage() && OwnerBlueprint->GetPackage()->IsDirty())
        {
            Lines.Add(TEXT("Note: this Blueprint has unsaved changes, so it cannot be the version that was cooked."));
        }

        UEdGraphNode* NodeToFocus = ExecutingNode;
        if (!NodeToFocus && SourceNodes.Num() > 0)
        {
            NodeToFocus = SourceNodes[0].Get();
            SourceIndex = SourceNodes.Num() > 1 ? 1 : 0;
        }

        LastExecutingNode = ExecutingNode;

        if (!NodeToFocus)
        {
            SetStatus(FString::Join(Lines, TEXT("\n")), true);
            return FReply::Handled();
        }

        FocusGraphNode(NodeToFocus);
        SetStatus(FString::Join(Lines, TEXT("\n")), false);
        return FReply::Handled();
    }
};

class FZeroPayBlueprintErrorNavigatorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ZeroPayBPErrorNav::TabId, FOnSpawnTab::CreateRaw(this, &FZeroPayBlueprintErrorNavigatorModule::OnSpawnTab))
            .SetDisplayName(LOCTEXT("TabTitle", "Blueprint Error Navigator"))
            .SetMenuType(ETabSpawnerMenuType::Hidden);

        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FZeroPayBlueprintErrorNavigatorModule::RegisterMenus));
    }

    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ZeroPayBPErrorNav::TabId);
    }

private:
    TSharedRef<SDockTab> OnSpawnTab(const FSpawnTabArgs&)
    {
        return SNew(SDockTab)
            .TabRole(ETabRole::NomadTab)
            [
                SNew(SZeroPayBlueprintErrorNavigator)
            ];
    }

    void OpenTab()
    {
        FGlobalTabmanager::Get()->TryInvokeTab(ZeroPayBPErrorNav::TabId);
    }

    void RegisterMenus()
    {
        FToolMenuOwnerScoped OwnerScoped(this);
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
        if (!Menu) return;

        FToolMenuSection& Section = Menu->FindOrAddSection(TEXT("ZeroPayBlueprintErrorNavigator"));
        Section.AddMenuEntry(
            TEXT("OpenZeroPayBlueprintErrorNavigator"),
            LOCTEXT("MenuLabel", "Blueprint Error Navigator"),
            LOCTEXT("MenuTooltip", "Find the Blueprint graph node corresponding to a LogScript bytecode offset."),
            FSlateIcon(),
            FUIAction(FExecuteAction::CreateRaw(this, &FZeroPayBlueprintErrorNavigatorModule::OpenTab))
        );
    }
};

IMPLEMENT_MODULE(FZeroPayBlueprintErrorNavigatorModule, ZeroPayBlueprintErrorNavigator)

#undef LOCTEXT_NAMESPACE