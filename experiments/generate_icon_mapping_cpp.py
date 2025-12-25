#!/usr/bin/env python3
"""
Generate C++ code for icon mapping in NMIconManager.
"""

import json

def generate_cpp_mapping():
    # Load the mapping
    with open('/tmp/icon-mapping.json', 'r') as f:
        icon_mapping = json.load(f)

    lines = []
    lines.append("  // Icon name mapping to Lucide icon files")
    lines.append("  // Format: m_iconFilePaths[\"icon-name\"] = \":/icons/lucide/icon-file.svg\";")
    lines.append("")

    # Group by category (based on original icon names)
    categories = {
        "File Operations": ["file-"],
        "Edit Operations": ["edit-", "copy", "delete"],
        "Playback Controls": ["play", "pause", "stop", "step-"],
        "Panel Icons": ["panel-"],
        "Zoom and View Controls": ["zoom-"],
        "Utility Icons": ["settings", "help", "search", "filter", "refresh", "add", "remove", "warning", "error", "info"],
        "Arrow and Navigation": ["arrow-", "chevron-"],
        "Node Type Icons": ["node-"],
        "Scene Object Icons": ["object-"],
        "Transform Icons": ["transform-"],
        "Asset Type Icons": ["asset-"],
        "Tool Icons": ["tool-"],
        "Status Icons": ["status-", "breakpoint", "execute"],
        "Layout Icons": ["layout-"],
        "Visibility and Lock Icons": ["visible", "hidden", "locked", "unlocked"],
        "Audio Icons": ["audio-", "microphone"],
        "Localization Icons": ["language", "translate", "locale-"],
        "Timeline Icons": ["keyframe", "snap", "loop", "easing-"],
        "Inspector Icons": ["property-"],
        "System Icons": ["external-", "folder-open", "import", "export", "pin", "unpin"],
        "Welcome Icons": ["welcome-"],
        "Template Icons": ["template-"],
    }

    def get_category(icon_name):
        for category, prefixes in categories.items():
            for prefix in prefixes:
                if icon_name.startswith(prefix) or icon_name == prefix:
                    return category
        return "Other"

    # Group icons by category
    grouped = {}
    for icon_name in sorted(icon_mapping.keys()):
        category = get_category(icon_name)
        if category not in grouped:
            grouped[category] = []
        grouped[category].append(icon_name)

    # Generate code for each category
    for category in sorted(grouped.keys()):
        lines.append(f"  // {category}")
        for icon_name in grouped[category]:
            lucide_name = icon_mapping[icon_name]
            lines.append(f'  m_iconFilePaths["{icon_name}"] = ":/icons/lucide/{lucide_name}.svg";')
        lines.append("")

    return '\n'.join(lines)

if __name__ == '__main__':
    cpp_code = generate_cpp_mapping()
    print(cpp_code)
    print(f"\n// Total icons mapped: {cpp_code.count('m_iconFilePaths')}")
