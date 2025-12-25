#!/usr/bin/env python3
"""
Generate updated .qrc file with Lucide icons included.
"""

import os

def generate_qrc():
    lucide_icons_dir = 'editor/resources/icons/lucide'
    old_icons_dir = 'editor/resources/icons'

    # Get all Lucide icons
    lucide_icons = sorted([f for f in os.listdir(lucide_icons_dir) if f.endswith('.svg')])

    # Get old icons (excluding lucide directory)
    old_icons = sorted([f for f in os.listdir(old_icons_dir)
                       if f.endswith('.svg') and os.path.isfile(os.path.join(old_icons_dir, f))])

    # Generate .qrc content
    qrc_content = ['<RCC>', '    <qresource prefix="/">']

    # Add old icons (for compatibility)
    for icon in old_icons:
        qrc_content.append(f'        <file>icons/{icon}</file>')

    # Add Lucide icons
    for icon in lucide_icons:
        qrc_content.append(f'        <file>icons/lucide/{icon}</file>')

    qrc_content.extend(['    </qresource>', '</RCC>', ''])

    return '\n'.join(qrc_content)

if __name__ == '__main__':
    qrc_content = generate_qrc()

    # Write to file
    output_path = 'editor/resources/editor_resources.qrc'
    with open(output_path, 'w') as f:
        f.write(qrc_content)

    print(f"Generated {output_path}")
    print(f"Total icon entries: {qrc_content.count('<file>')}")
