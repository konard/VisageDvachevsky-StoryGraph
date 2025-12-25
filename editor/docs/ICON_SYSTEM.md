# NovelMind Editor Icon System

## Overview

The NovelMind Editor uses a professional icon pack integration system that provides consistent, high-quality icons throughout the user interface.

## Icon Pack

- **Name**: [Lucide Icons](https://lucide.dev)
- **License**: ISC License (permissive open-source)
- **Total Icons**: 1,666+ available icons
- **Used Icons**: 111 unique Lucide icons mapped to 138 editor icon names
- **Format**: SVG (Scalable Vector Graphics)
- **Color**: Dynamic theming support (icons adapt to theme colors)

## Architecture

### Icon Manager (`NMIconManager`)

The `NMIconManager` class provides centralized icon management with:

- **Resource-based loading**: Icons are loaded from Qt resource files (`.qrc`)
- **Runtime SVG rendering**: Icons are rendered at runtime with proper color theming
- **Caching**: Rendered icons are cached for performance
- **Fallback system**: Graceful handling of missing icons

### File Structure

```
editor/
├── resources/
│   ├── icons/
│   │   ├── lucide/           # Lucide icon SVG files (111 icons)
│   │   │   ├── play.svg
│   │   │   ├── pause.svg
│   │   │   └── ...
│   │   └── *.svg             # Legacy icons (for compatibility)
│   └── editor_resources.qrc  # Qt resource file
├── src/qt/
│   └── nm_icon_manager.cpp   # Icon manager implementation
└── include/NovelMind/editor/qt/
    └── nm_icon_manager.hpp   # Icon manager interface
```

## Icon Naming Conventions

### Current System (Editor Icon Names)

The editor uses semantic icon names organized by category:

#### File Operations
- `file-new`, `file-open`, `file-save`, `file-close`

#### Edit Operations
- `edit-undo`, `edit-redo`, `edit-cut`, `edit-copy`, `edit-paste`, `edit-delete`

#### Playback Controls
- `play`, `pause`, `stop`, `step-forward`, `step-backward`

#### Panel Icons
- `panel-scene`, `panel-graph`, `panel-inspector`, `panel-console`
- `panel-hierarchy`, `panel-assets`, `panel-timeline`, `panel-curve`
- `panel-voice`, `panel-localization`, `panel-diagnostics`, `panel-build`

#### Navigation
- `arrow-up`, `arrow-down`, `arrow-left`, `arrow-right`
- `chevron-up`, `chevron-down`, `chevron-left`, `chevron-right`

#### Node Types (Story Graph)
- `node-dialogue`, `node-choice`, `node-event`, `node-condition`
- `node-random`, `node-start`, `node-end`, `node-jump`, `node-variable`

#### Scene Objects
- `object-character`, `object-background`, `object-prop`, `object-effect`, `object-ui`

#### Tools
- `tool-select`, `tool-hand`, `tool-zoom`, `tool-frame`

#### Status Indicators
- `status-success`, `status-warning`, `status-error`, `status-info`

#### Other Categories
- Zoom controls: `zoom-in`, `zoom-out`, `zoom-fit`
- Utility: `settings`, `help`, `search`, `filter`, `refresh`, `add`, `remove`
- Transform: `transform-move`, `transform-rotate`, `transform-scale`
- Assets: `asset-image`, `asset-audio`, `asset-video`, `asset-font`, `asset-script`
- Audio: `audio-record`, `audio-mute`, `microphone`, etc.
- Timeline: `keyframe`, `keyframe-add`, `loop`, `easing-*`
- And more...

### Lucide Icon Mapping

Each editor icon name is mapped to a Lucide icon file. The mapping is defined in `nm_icon_manager.cpp`:

```cpp
m_iconFilePaths["play"] = ":/icons/lucide/play.svg";
m_iconFilePaths["pause"] = ":/icons/lucide/pause.svg";
m_iconFilePaths["panel-scene"] = ":/icons/lucide/image.svg";
// ... etc
```

## Usage

### C++ Code

```cpp
#include "NovelMind/editor/qt/nm_icon_manager.hpp"

// Get icon instance (singleton)
auto& iconMgr = NMIconManager::instance();

// Get an icon (16x16 with default theme color)
QIcon icon = iconMgr.getIcon("play");

// Get an icon with custom size and color
QIcon largeIcon = iconMgr.getIcon("pause", 32, QColor(255, 100, 100));

// Get a pixmap instead of QIcon
QPixmap pixmap = iconMgr.getPixmap("file-save", 24);

// Set default color for theme
iconMgr.setDefaultColor(QColor(220, 220, 220));
```

### Adding Icons to UI Elements

```cpp
// QPushButton
QPushButton* playButton = new QPushButton();
playButton->setIcon(iconMgr.getIcon("play", 16));

// QAction (menu items, toolbar)
QAction* saveAction = new QAction("Save", this);
saveAction->setIcon(iconMgr.getIcon("file-save"));

// QTreeWidget items
QTreeWidgetItem* item = new QTreeWidgetItem();
item->setIcon(0, iconMgr.getIcon("asset-image"));
```

## Adding New Icons

### Step 1: Find the Lucide Icon

Browse available icons at: https://lucide.dev

### Step 2: Add Icon File

1. Download the SVG file from Lucide repository
2. Place it in `editor/resources/icons/lucide/`

### Step 3: Update Resource File

Add the icon to `editor/resources/editor_resources.qrc`:

```xml
<file>icons/lucide/new-icon.svg</file>
```

### Step 4: Add Mapping

In `nm_icon_manager.cpp`, add the mapping in `initializeIcons()`:

```cpp
m_iconFilePaths["my-new-icon"] = ":/icons/lucide/new-icon.svg";
```

### Step 5: Use the Icon

```cpp
QIcon icon = iconMgr.getIcon("my-new-icon");
```

## Color Theming

Icons automatically adapt to the current theme color:

- **Default Color**: Light gray (`#dcdcdc`) for dark themes
- **Custom Colors**: Can be specified per icon usage
- **SVG Color Replacement**: The system replaces `currentColor` and stroke attributes in the SVG with the requested color

### Changing Theme Color

```cpp
// Update default color for all icons
iconMgr.setDefaultColor(QColor(180, 180, 180));

// This automatically clears the icon cache to ensure new color is applied
```

## Performance

- **Caching**: Rendered icons are cached based on (name, size, color)
- **On-Demand Loading**: SVG files are loaded from resources only when needed
- **Cache Clearing**: Cache is automatically cleared when default color changes

## Troubleshooting

### Icon Not Appearing

1. Check that the icon name is correctly mapped in `initializeIcons()`
2. Verify the SVG file exists in `editor/resources/icons/lucide/`
3. Ensure the file is listed in `editor_resources.qrc`
4. Rebuild the project to regenerate Qt resources

### Wrong Color

- Lucide icons use `currentColor` for stroke
- The system replaces this with the theme color
- If an icon doesn't recolor properly, check the SVG structure

### Icon Looks Blurry

- Icons are rendered as vector graphics and should scale smoothly
- If blurry, ensure the size parameter is the actual display size
- High-DPI displays are handled automatically by Qt

## License & Attribution

### Lucide Icons
- **License**: ISC License
- **Copyright**: © 2024 Lucide Contributors
- **Website**: https://lucide.dev
- **Repository**: https://github.com/lucide-icons/lucide

The ISC License allows free use, modification, and distribution of the icons in both personal and commercial projects.

## Migration Notes

### From Old System

The previous system used hardcoded SVG strings in C++. The new system:

- ✅ Loads icons from resource files (cleaner, more maintainable)
- ✅ Uses professional Lucide icon pack (consistent design)
- ✅ Maintains backward compatibility (icon names unchanged)
- ✅ Reduces code size (330 lines vs 1,118 lines)
- ✅ Easier to update icons (just replace SVG files)

All existing icon names continue to work - no changes needed in calling code.
