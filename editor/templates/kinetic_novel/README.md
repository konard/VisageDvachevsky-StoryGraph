# Kinetic Novel Template

## Overview

This template provides a starting point for creating linear visual novels - stories that progress without player choices. It includes sample scenes, characters, and demonstrates best practices for narrative-driven experiences.

## What's Included

- **5 Demo Scenes**: Introduction, Chapter 1 (2 parts), Chapter 2, and Epilogue
- **3 Sample Characters**: Protagonist (Alex), Friend (Sam), and Mentor (Professor Chen)
- **Complete Story Flow**: A working linear narrative you can study and modify
- **Music/SFX Placeholders**: Ready-to-use audio configuration
- **Scene Transitions**: Examples of fade, dissolve, and slide transitions

## Project Structure

```
kinetic_novel/
├── project.json              # Project configuration
├── template.json             # Template metadata
├── README.md                 # This file
├── assets/
│   ├── characters/           # Character sprites
│   │   └── .gitkeep
│   ├── backgrounds/          # Background images (5 needed)
│   │   └── .gitkeep
│   ├── music/                # Background music (4 tracks)
│   │   └── .gitkeep
│   ├── sfx/                  # Sound effects
│   │   └── .gitkeep
│   ├── voice/                # Voice acting (optional)
│   │   └── .gitkeep
│   └── ui/                   # UI customization
│       └── .gitkeep
├── scenes/
│   ├── intro.nmscene
│   ├── chapter1_part1.nmscene
│   ├── chapter1_part2.nmscene
│   ├── chapter2.nmscene
│   └── epilogue.nmscene
├── scripts/
│   └── main.nms              # All dialogue and scene logic
└── story_graph/
    └── main_story.nmgraph    # Visual story flow
```

## Story Flow

```
intro → chapter1_part1 → chapter1_part2 → chapter2 → epilogue
```

The demo story follows Alex, a new student, on their first day at school. They meet Sam, attend Professor Chen's class, and learn about the power of storytelling.

## Getting Started

### 1. Replace Placeholder Assets

Add your own images to the asset folders:

**Characters** (in `assets/characters/`):
- `protagonist_silhouette.png` → Your protagonist sprite
- `friend_silhouette.png` → Your friend character
- `mentor_silhouette.png` → Your mentor character

**Backgrounds** (in `assets/backgrounds/`):
- `school_entrance.jpg`
- `school_hallway.jpg`
- `classroom.jpg`
- `library.jpg`
- `sunset_campus.jpg`

**Music** (in `assets/music/`):
- `main_theme.ogg`
- `casual_theme.ogg`
- `contemplative_theme.ogg`
- `emotional_theme.ogg`

### 2. Customize the Dialogue

Edit `scripts/main.nms` to:
- Change character names
- Modify dialogue
- Add new scenes
- Adjust timing

### 3. Modify the Story Graph

Use the Story Graph editor to:
- Add new scenes
- Rearrange the flow
- Visualize your narrative

## Script Features Demonstrated

### Character Definitions
```nms
character Protagonist(name="Alex", color="#4A90D9")
character Friend(name="Sam", color="#7ED321")
```

### Scene Transitions
```nms
transition fade 1.5      // Fade to black
transition dissolve 0.8  // Cross-dissolve
```

### Music and Sound
```nms
play music "main_theme" fadeIn 2.0
play sfx "footsteps"
stop music fadeOut 3.0
```

### Character Positioning
```nms
show Protagonist at center
show Friend at right
hide Friend
```

## Tips for Linear Novels

1. **Focus on Pacing**: Without choices, pacing is crucial. Use `wait` commands strategically.
2. **Visual Variety**: Change backgrounds and character positions to keep scenes dynamic.
3. **Audio Atmosphere**: Music greatly enhances emotional moments.
4. **Scene Transitions**: Match transition types to the mood of scene changes.

## Need Help?

- Check the [NovelMind Documentation](https://novelmind.dev/docs)
- View the [Script Reference](https://novelmind.dev/docs/scripting)
- Visit the [Community Forum](https://novelmind.dev/community)
