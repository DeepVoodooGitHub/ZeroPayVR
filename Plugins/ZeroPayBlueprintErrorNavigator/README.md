# ZeroPay Blueprint Error Navigator (UE 5.5.4)

Editor-only C++ plugin. Adds **Tools > Blueprint Error Navigator**. Paste a Blueprint `LogScript` warning (or the `Function ...:...:OFFSET` portion), click **Locate Node in Blueprint Editor** to open the mapped node or pin.

## Install

1. Close Unreal Editor.
2. Copy this `ZeroPayBlueprintErrorNavigator` folder into `<YourProject>/Plugins/`, alongside your existing plugins. The `.uplugin` file must end up at `<YourProject>/Plugins/ZeroPayBlueprintErrorNavigator/ZeroPayBlueprintErrorNavigator.uplugin`.
3. From the `.uproject` context menu choose **Generate Visual Studio project files** (if needed).
4. Build the **<ProjectName>Editor** target in Visual Studio / UnrealBuildTool for your Unreal Engine **5.5.4** installation. In an existing C++ project, opening the project and accepting the module-build prompt may also work.
5. Start the editor. If disabled, enable **ZeroPay Blueprint Error Navigator** under Edit > Plugins and restart.
6. Open **Tools > Blueprint Error Navigator**. Drag the tab to dock it.

## Example

Paste:

```
Function /ZeroPayMod/Blueprints/GameLogic/Character/BP_ZP_VRCharacter_r1.BP_ZP_VRCharacter_r1_C:ExecuteUbergraph_BP_ZP_VRCharacter_r1:3975
```

Or paste the complete LogScript warning and call stack. The parser uses the first location with an explicit `:integer` bytecode offset.

## Results

- **PIN MATCH**: Navigate to source pin if the compiler mapped it.
- **EXACT NODE MATCH**: Navigate to exact recorded source node.
- **APPROXIMATE MATCH**: Navigate to a best-effort source node, explicitly labelled approximate.
- **No source-node mapping**: No mapping in the currently loaded compiled Blueprint. Verify the Blueprint was not recompiled since the log was generated and the source is present in the editor.

## Requirements and caveats

- Editor-only; does not run in packaged games.
- Bytecode offsets belong to the *specific compiled Blueprint version* that logged the error; recompiling, hot reload, reinstancing, cooking or loading a different build can invalidate them.
- The source Blueprint and generated class must be available, and compiler debug mappings must be retained.
- Opening the editor Blueprint later than an error from a different compiled version cannot reconstruct the original mapping.
- This code was cross-checked against the public Unreal Engine **5.5 API** references for FBlueprintDebugData, FKismetEditorUtilities, FGlobalTabmanager and ToolMenus. **It was not compiled/tested in a running UE 5.5.4 installation** in this environment.
