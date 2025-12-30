import unreal
import re

# ============================================================
# USER SETTINGS (edit these and save the script)
# ============================================================

ACTOR_A = "BP_ZP_Gun_KA47_r1"   # can be Actor Name or Actor Label
ACTOR_B = "BP_ZP_Gun_KA47_r2"   # can be Actor Name or Actor Label

# Match mode: "exact" | "startswith" | "contains" | "regex"
MATCH_MODE = "contains"

# What to match: "name" | "label" | "either"
# - "name"  -> Actor.get_name()
# - "label" -> Actor.get_actor_label() (may be editor-world only)
# - "either"-> try label then name
MATCH_FIELD = "label"

# Require same class?
REQUIRE_SAME_CLASS = True

# How many diffs to print to log before truncating
MAX_DIFF_LINES = 500


# ============================================================
# WORLD + ACTOR FINDING
# ============================================================

def _get_active_world():
    """
    Prefer PIE/Game world when playing, else editor world.
    UE Python API availability varies slightly by version; we try a few options.
    """
    # Try to fetch PIE world (if supported)
    for fn_name in ("get_game_world", "get_play_world", "get_pie_world"):
        if hasattr(unreal.EditorLevelLibrary, fn_name):
            try:
                w = getattr(unreal.EditorLevelLibrary, fn_name)()
                if w:
                    return w
            except Exception:
                pass

    # Fallback: editor world
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        pass

    # Final fallback: first world we can find via engine (least reliable)
    try:
        worlds = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_worlds()
        if worlds:
            return worlds[0]
    except Exception:
        pass

    raise RuntimeError("Could not resolve an active world (PIE or Editor).")


def _match(s: str, pattern: str) -> bool:
    if s is None:
        return False
    if MATCH_MODE == "exact":
        return s == pattern
    if MATCH_MODE == "startswith":
        return s.startswith(pattern)
    if MATCH_MODE == "contains":
        return pattern in s
    if MATCH_MODE == "regex":
        return re.search(pattern, s) is not None
    raise RuntimeError(f"Invalid MATCH_MODE: {MATCH_MODE}")


def _actor_label_safe(actor: unreal.Actor) -> str:
    # get_actor_label is not always available/valid in PIE for spawned actors
    try:
        return actor.get_actor_label()
    except Exception:
        return ""


def _actor_matches(actor: unreal.Actor, token: str) -> bool:
    nm = actor.get_name()
    lb = _actor_label_safe(actor)

    if MATCH_FIELD == "name":
        return _match(nm, token)
    if MATCH_FIELD == "label":
        return _match(lb, token)
    if MATCH_FIELD == "either":
        return _match(lb, token) or _match(nm, token)

    raise RuntimeError(f"Invalid MATCH_FIELD: {MATCH_FIELD}")


def _find_actor_by_token(world: unreal.World, token: str) -> unreal.Actor:
    """
    Searches all actors in the world and returns a unique match.
    """
    matches = []
    # Iterate ALL actors (including BP instances)
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if _actor_matches(actor, token):
            matches.append(actor)

    if not matches:
        raise RuntimeError(
            f"Could not find actor matching '{token}' "
            f"(MATCH_MODE={MATCH_MODE}, MATCH_FIELD={MATCH_FIELD})."
        )

    if len(matches) > 1:
        # Print candidates to help user refine token
        unreal.log_warning(f"[ActorCompare] Multiple matches for '{token}':")
        for a in matches[:30]:
            unreal.log_warning(f"  - Name='{a.get_name()}', Label='{_actor_label_safe(a)}', Class='{a.get_class().get_name()}'")
        raise RuntimeError(
            f"Token '{token}' matched {len(matches)} actors. "
            f"Refine ACTOR_A/ACTOR_B or change MATCH_MODE/MATCH_FIELD."
        )

    return matches[0]


# ============================================================
# PROPERTY COMPARISON
# ============================================================

def _is_simple_value(v) -> bool:
    return isinstance(v, (int, float, bool, str)) or v is None



_ADDR_RE = re.compile(r"\(0x[0-9A-Fa-f]+\)")

def _safe_to_string(v) -> str:
    """
    Stringify values but normalize out volatile address-like tokens that Unreal's
    Python wrappers include for structs (e.g. (0x00000A0E...)).
    """
    try:
        s = str(v)
    except Exception:
        return "<unstringable>"

    # Remove volatile memory/address tokens from struct reprs
    s = _ADDR_RE.sub("(<ADDR>)", s)

    return s


def _iter_editor_properties(obj: unreal.Object):
    """
    Best-effort enumeration of properties accessible via get_editor_property.
    UE Python does not expose a perfect "all FProperty names" iterator in all builds,
    so we probe attributes and keep the ones that succeed.
    """
    names = []
    for name in dir(obj):
        if name.startswith("_"):
            continue
        try:
            attr = getattr(obj, name)
            if callable(attr):
                continue
        except Exception:
            pass

        try:
            obj.get_editor_property(name)
            names.append(name)
        except Exception:
            continue

    return sorted(set(names))


def _normalize_obj_ref(o: unreal.Object) -> str:
    # Compare object references by path (more stable than transient object pointer)
    try:
        return o.get_path_name()
    except Exception:
        return _safe_to_string(o)


def _compare_objects(a: unreal.Object, b: unreal.Object, path: str, diffs: list[str], visited: set[tuple[int, int]]):
    if a is None and b is None:
        return
    if (a is None) != (b is None):
        diffs.append(f"{path}: one is None, other is not")
        return

    key = (id(a), id(b))
    if key in visited:
        return
    visited.add(key)

    if a.get_class() != b.get_class():
        diffs.append(f"{path}: class mismatch {a.get_class().get_name()} vs {b.get_class().get_name()}")
        return

    for p in _iter_editor_properties(a):
        try:
            va = a.get_editor_property(p)
            vb = b.get_editor_property(p)
        except Exception:
            continue

        ppath = f"{path}.{p}"

        # Object refs
        if isinstance(va, unreal.Object) or isinstance(vb, unreal.Object):
            if (va is None) != (vb is None):
                diffs.append(f"{ppath}: one is None, other is not")
                continue
            if va is None and vb is None:
                continue
            ra = _normalize_obj_ref(va)
            rb = _normalize_obj_ref(vb)
            if ra != rb:
                diffs.append(f"{ppath}: object ref differs\n  A: {ra}\n  B: {rb}")
            continue

        # Lists/tuples: string compare baseline (adequate for most UPROPERTY arrays)
        if isinstance(va, (list, tuple)) or isinstance(vb, (list, tuple)):
            sa = _safe_to_string(va)
            sb = _safe_to_string(vb)
            if sa != sb:
                diffs.append(f"{ppath}: sequence differs\n  A: {sa}\n  B: {sb}")
            continue

        # Struct-like or other: string compare
        if not _is_simple_value(va) or not _is_simple_value(vb):
            sa = _safe_to_string(va)
            sb = _safe_to_string(vb)
            if sa != sb:
                diffs.append(f"{ppath}: value differs\n  A: {sa}\n  B: {sb}")
            continue

        # Simple values
        if va != vb:
            diffs.append(f"{ppath}: {va} != {vb}")


def _components_by_stable_key(actor: unreal.Actor):
    """
    Index components by a stable key: prefer name, but also include class to reduce collisions.
    """
    comps = actor.get_components_by_class(unreal.ActorComponent)
    out = {}
    for c in comps:
        key = f"{c.get_name()}|{c.get_class().get_name()}"
        out[key] = c
    return out


def compare_two_named_actors():
    world = _get_active_world()
    unreal.log(f"[ActorCompare] Using world: {world.get_name()}")

    a = _find_actor_by_token(world, ACTOR_A)
    b = _find_actor_by_token(world, ACTOR_B)

    unreal.log(f"[ActorCompare] A: Name='{a.get_name()}', Label='{_actor_label_safe(a)}', Class='{a.get_class().get_name()}'")
    unreal.log(f"[ActorCompare] B: Name='{b.get_name()}', Label='{_actor_label_safe(b)}', Class='{b.get_class().get_name()}'")

    if REQUIRE_SAME_CLASS and a.get_class() != b.get_class():
        raise RuntimeError(
            f"Actors are different classes:\n"
            f"  A: {a.get_class().get_name()}\n"
            f"  B: {b.get_class().get_name()}"
        )

    diffs = []
    visited = set()

    # Actor-level properties
    _compare_objects(a, b, path="Actor", diffs=diffs, visited=visited)

    # Component-level properties (including inherited ones)
    a_comps = _components_by_stable_key(a)
    b_comps = _components_by_stable_key(b)

    all_keys = sorted(set(a_comps.keys()) | set(b_comps.keys()))
    for k in all_keys:
        ca = a_comps.get(k)
        cb = b_comps.get(k)
        if (ca is None) != (cb is None):
            diffs.append(f"Component[{k}]: present in one actor only")
            continue
        _compare_objects(ca, cb, path=f"Component[{k}]", diffs=diffs, visited=visited)

    if not diffs:
        unreal.log(f"[ActorCompare] MATCH: '{ACTOR_A}' == '{ACTOR_B}'")
        return True

    unreal.log_warning(f"[ActorCompare] MISMATCH: '{ACTOR_A}' != '{ACTOR_B}'  (diffs={len(diffs)})")
    for line in diffs[:MAX_DIFF_LINES]:
        unreal.log_warning(line)
    if len(diffs) > MAX_DIFF_LINES:
        unreal.log_warning(f"[ActorCompare] Diff truncated. Total diffs: {len(diffs)}")

    return False


# Run
compare_two_named_actors()
