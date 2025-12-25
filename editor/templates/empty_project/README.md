# Empty Project Template

## Overview

This template provides a minimal starting point for creating visual novels in NovelMind. It's designed for experienced users who want a clean slate to build upon.

## What's Included

- Basic `project.json` configuration with all available settings
- Simple `main.nms` script with examples in comments
- Empty folder structure for assets
- Basic Story Graph with single node
- Empty scene file ready for editing

## Project Structure

```
empty_project/
├── project.json          # Project configuration
├── template.json         # Template metadata
├── README.md             # This file
├── assets/
│   ├── characters/       # Character sprites
│   ├── backgrounds/      # Background images
│   ├── music/            # Background music
│   ├── sfx/              # Sound effects
│   ├── voice/            # Voice acting files
│   └── ui/               # UI elements
├── scenes/
│   └── main.nmscene      # Main scene file
├── scripts/
│   └── main.nms          # Main script
└── story_graph/
    └── main.nmgraph      # Story flow graph
```

## Getting Started

1. **Add Characters**: Place character sprites in `assets/characters/`
2. **Add Backgrounds**: Place background images in `assets/backgrounds/`
3. **Define Characters**: Edit `scripts/main.nms` to define your characters
4. **Write Your Story**: Add scenes and dialogue to `scripts/main.nms`
5. **Configure Audio**: Add music and sound effects to the audio folders

## Configuration

The `project.json` file contains all project settings:

- **Metadata**: Name, version, author, description
- **Runtime**: Start scene, resolution, aspect ratio
- **Localization**: Supported languages
- **Audio**: Volume settings for music, voice, and SFX
- **UI**: Theme and font settings
- **Build**: Output settings for export

## Script Basics

```nms
// Define a character
character Hero(name="Hero Name", color="#4A90D9")

// Create a scene
scene my_scene {
    show background "bg_name"
    say Hero "Hello, world!"
    goto next_scene
}
```

## Need Help?

- Check the [NovelMind Documentation](https://novelmind.dev/docs)
- Visit the [Community Forum](https://novelmind.dev/community)
- Report issues on [GitHub](https://github.com/NovelMind/editor)
