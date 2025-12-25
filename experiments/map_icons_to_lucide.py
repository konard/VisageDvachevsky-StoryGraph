#!/usr/bin/env python3
"""
Map existing NovelMind editor icon names to Lucide icon names.
This script creates a mapping between the current hardcoded icon names
and the corresponding Lucide icons that will be used.
"""

import os
import json

# Mapping of current icon names to Lucide icon file names
# Format: "current-name": "lucide-icon-name"
ICON_MAPPING = {
    # File Operations
    "file-new": "file-plus",
    "file-open": "folder-open",
    "file-save": "save",
    "file-close": "x",

    # Edit Operations
    "edit-undo": "undo",
    "edit-redo": "redo",
    "edit-cut": "scissors",
    "edit-copy": "copy",
    "edit-paste": "clipboard",
    "edit-delete": "trash-2",
    "copy": "copy",
    "delete": "trash-2",

    # Playback Controls
    "play": "play",
    "pause": "pause",
    "stop": "square",
    "step-forward": "step-forward",
    "step-backward": "step-back",

    # Panel Icons
    "panel-scene": "image",
    "panel-graph": "git-graph",
    "panel-inspector": "sliders-horizontal",
    "panel-console": "terminal",
    "panel-hierarchy": "list-tree",
    "panel-assets": "folder",
    "panel-timeline": "film",
    "panel-curve": "trending-up",
    "panel-voice": "mic",
    "panel-localization": "globe",
    "panel-diagnostics": "activity",
    "panel-build": "hammer",

    # Zoom and View Controls
    "zoom-in": "zoom-in",
    "zoom-out": "zoom-out",
    "zoom-fit": "maximize",

    # Utility Icons
    "settings": "settings",
    "help": "circle-question-mark",
    "search": "search",
    "filter": "list-filter",
    "refresh": "refresh-cw",
    "add": "plus",
    "remove": "minus",
    "warning": "triangle-alert",
    "error": "circle-x",
    "info": "info",

    # Arrow and Navigation
    "arrow-up": "arrow-up",
    "arrow-down": "arrow-down",
    "arrow-left": "arrow-left",
    "arrow-right": "arrow-right",
    "chevron-up": "chevron-up",
    "chevron-down": "chevron-down",
    "chevron-left": "chevron-left",
    "chevron-right": "chevron-right",

    # Node Type Icons (for Story Graph)
    "node-dialogue": "message-square",
    "node-choice": "git-branch",
    "node-event": "zap",
    "node-condition": "diamond",
    "node-random": "shuffle",
    "node-start": "circle-play",
    "node-end": "circle-stop",
    "node-jump": "corner-down-right",
    "node-variable": "variable",

    # Scene Object Type Icons
    "object-character": "user",
    "object-background": "image",
    "object-prop": "box",
    "object-effect": "sparkles",
    "object-ui": "layout-dashboard",

    # Transform and Manipulation Icons
    "transform-move": "move",
    "transform-rotate": "rotate-cw",
    "transform-scale": "maximize-2",

    # Asset Type Icons
    "asset-image": "image",
    "asset-audio": "music",
    "asset-video": "video",
    "asset-font": "type",
    "asset-script": "file-code",
    "asset-folder": "folder",

    # Action and Tool Icons
    "tool-select": "mouse-pointer",
    "tool-hand": "hand",
    "tool-zoom": "zoom-in",
    "tool-frame": "frame",

    # Status and Indicator Icons
    "status-success": "circle-check",
    "status-warning": "triangle-alert",
    "status-error": "circle-x",
    "status-info": "info",
    "breakpoint": "circle-dot",
    "execute": "circle-play",

    # Grid and Layout Icons
    "layout-grid": "grid-3x3",
    "layout-list": "list",
    "layout-tree": "list-tree",

    # Visibility and Lock Icons
    "visible": "eye",
    "hidden": "eye-off",
    "locked": "lock",
    "unlocked": "lock-open",

    # Audio Icons (Voice Manager, Recording, Playback)
    "audio-record": "circle",
    "audio-waveform": "audio-waveform",
    "audio-mute": "volume-x",
    "audio-unmute": "volume-2",
    "audio-volume-low": "volume-1",
    "audio-volume-high": "volume-2",
    "microphone": "mic",
    "microphone-off": "mic-off",

    # Localization Icons (Languages, Translation, Keys)
    "language": "languages",
    "translate": "languages",
    "locale-missing": "circle-alert",
    "locale-key": "key",
    "locale-add": "circle-plus",

    # Timeline Icons (Keyframes, Playback, Snapping)
    "keyframe": "diamond",
    "keyframe-add": "square-plus",
    "keyframe-remove": "square-minus",
    "snap": "magnet",
    "snap-off": "magnet",  # Will need custom handling
    "loop": "repeat",
    "easing-linear": "trending-up",
    "easing-ease-in": "trending-up",
    "easing-ease-out": "trending-up",
    "easing-ease-in-out": "trending-up",

    # Inspector Icons (Properties, Types, Actions)
    "property-vector": "arrow-up-right",
    "property-color": "palette",
    "property-number": "hash",
    "property-text": "type",
    "property-bool": "toggle-right",
    "property-link": "link",
    "property-reset": "rotate-ccw",
    "property-override": "circle-dot",

    # External/System Icons
    "external-link": "external-link",
    "folder-open": "folder-open",
    "import": "download",
    "export": "upload",
    "pin": "pin",
    "unpin": "pin-off",

    # Welcome Page Icons
    "welcome-new": "file-plus",
    "welcome-open": "folder-open",
    "welcome-examples": "layout-grid",
    "welcome-docs": "book-open",
    "welcome-recent": "clock",

    # Project Template Icons
    "template-blank": "file",
    "template-visual-novel": "book-open",
    "template-dating-sim": "heart",
    "template-mystery": "search",
    "template-rpg": "swords",  # May need alternative
    "template-horror": "skull",  # May need alternative
}

def check_icons_exist(lucide_icons_path="/tmp/lucide-icons/icons"):
    """Check which mapped Lucide icons actually exist"""
    missing = []
    found = []

    for current_name, lucide_name in ICON_MAPPING.items():
        icon_file = os.path.join(lucide_icons_path, f"{lucide_name}.svg")
        if os.path.exists(icon_file):
            found.append((current_name, lucide_name))
        else:
            missing.append((current_name, lucide_name))

    return found, missing

def suggest_alternatives(missing_icons, lucide_icons_path="/tmp/lucide-icons/icons"):
    """Suggest alternative Lucide icons for missing mappings"""
    alternatives = {}
    all_lucide_icons = [f.replace('.svg', '') for f in os.listdir(lucide_icons_path) if f.endswith('.svg')]

    for current_name, lucide_name in missing_icons:
        # Try to find similar names
        candidates = [icon for icon in all_lucide_icons if lucide_name.split('-')[0] in icon]
        if candidates:
            alternatives[current_name] = candidates[:5]  # Top 5 suggestions

    return alternatives

if __name__ == "__main__":
    print("=" * 80)
    print("NovelMind Editor Icon Mapping to Lucide Icons")
    print("=" * 80)
    print(f"\nTotal icons to map: {len(ICON_MAPPING)}")

    found, missing = check_icons_exist()

    print(f"\nFound: {len(found)} icons")
    print(f"Missing: {len(missing)} icons")

    if missing:
        print("\n" + "=" * 80)
        print("Missing Icons (need alternative mappings):")
        print("=" * 80)
        for current_name, lucide_name in missing:
            print(f"  {current_name} -> {lucide_name}.svg [NOT FOUND]")

        print("\n" + "=" * 80)
        print("Suggested Alternatives:")
        print("=" * 80)
        alternatives = suggest_alternatives(missing)
        for current_name, suggestions in alternatives.items():
            print(f"\n  {current_name}:")
            for alt in suggestions:
                print(f"    - {alt}")

    # Write mapping to JSON file
    output_file = "/tmp/icon-mapping.json"
    with open(output_file, 'w') as f:
        json.dump(ICON_MAPPING, f, indent=2)

    print(f"\n{'=' * 80}")
    print(f"Mapping saved to: {output_file}")
    print("=" * 80)
