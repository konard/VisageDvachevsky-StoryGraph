#!/usr/bin/env python3
"""
Copy Lucide icon SVG files to the NovelMind editor resources directory.
"""

import json
import os
import shutil

def copy_icons():
    # Load the mapping
    with open('/tmp/icon-mapping.json', 'r') as f:
        icon_mapping = json.load(f)

    lucide_icons_dir = '/tmp/lucide-icons/icons'
    target_dir = 'editor/resources/icons/lucide'

    # Create target directory if it doesn't exist
    os.makedirs(target_dir, exist_ok=True)

    # Get unique Lucide icon names (some current icons map to the same Lucide icon)
    lucide_icons = set(icon_mapping.values())

    copied = []
    failed = []

    for lucide_icon in sorted(lucide_icons):
        source_file = os.path.join(lucide_icons_dir, f'{lucide_icon}.svg')
        target_file = os.path.join(target_dir, f'{lucide_icon}.svg')

        if os.path.exists(source_file):
            shutil.copy2(source_file, target_file)
            copied.append(lucide_icon)
        else:
            failed.append(lucide_icon)

    print(f"Copied {len(copied)} unique Lucide icons to {target_dir}")

    if failed:
        print(f"\nFailed to copy {len(failed)} icons:")
        for icon in failed:
            print(f"  - {icon}")

    return copied, failed

if __name__ == '__main__':
    copied, failed = copy_icons()

    if not failed:
        print("\n✓ All icons copied successfully!")
    else:
        print(f"\n✗ {len(failed)} icons missing")
