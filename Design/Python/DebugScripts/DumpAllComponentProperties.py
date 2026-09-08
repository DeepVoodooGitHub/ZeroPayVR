import unreal

def safe_get(obj, name):
    try:
        return obj.get_editor_property(name)
    except:
        return "<not available>"


selected_actors = unreal.EditorLevelLibrary.get_selected_level_actors()

if not selected_actors:
    unreal.log_warning("NO ACTOR SELECTED")
else:
    for actor in selected_actors:

        unreal.log("")
        unreal.log("=" * 120)
        unreal.log("ACTOR")
        unreal.log("=" * 120)
        unreal.log(f"Name: {actor.get_name()}")
        unreal.log(f"Class: {actor.get_class().get_name()}")
        unreal.log(f"Path: {actor.get_path_name()}")

        components = actor.get_components_by_class(unreal.ActorComponent)

        for c in components:

            unreal.log("")
            unreal.log("-" * 120)
            unreal.log(f"COMPONENT: {c.get_name()}")
            unreal.log(f"CLASS:     {c.get_class().get_name()}")
            unreal.log(f"PATH:      {c.get_path_name()}")
            unreal.log("-" * 120)

            property_names = [
                "active",
                "auto_activate",
                "can_ever_affect_navigation",
                "component_tags",
                "editable_when_inherited",
                "is_editor_only",

                # SceneComponent
                "visible",
                "hidden_in_game",
                "mobility",
                "relative_location",
                "relative_rotation",
                "relative_scale3d",

                # PrimitiveComponent
                "cast_shadow",
                "owner_no_see",
                "only_owner_see",
                "render_in_main_pass",
                "render_in_depth_pass",
                "receives_decals",
                "use_attach_parent_bound",
                "bounds_scale",

                # SkeletalMeshComponent
                "skeletal_mesh",
                "animation_mode",
                "animation_data",
                "anim_class",
                "enable_update_rate_optimizations",
                "update_animation_in_editor",
                "visibility_based_anim_tick_option",
                "component_space_transforms_double_buffering",
                "disable_post_process_blueprint",
                "hide_skin",
                "forced_lod_model",
                "min_lod_model",
                "predicted_lod_level",

                # Collision / physics
                "collision_enabled",
                "collision_profile_name",
                "generate_overlap_events",
                "simulate_physics",
                "enable_gravity",
            ]

            for name in property_names:
                value = safe_get(c, name)

                if value != "<not available>":
                    unreal.log(f"{name} = {value}")

        unreal.log("")
        unreal.log("=" * 120)
        unreal.log("END")
        unreal.log("=" * 120)