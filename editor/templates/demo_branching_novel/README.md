# First Day at Academy - Interactive Demo

A complete playable visual novel demonstration showcasing NovelMind engine capabilities.

## Overview

This demo presents a slice-of-life story about a new student's first day at the academy. It features:

- **Branching narrative** with meaningful player choices
- **3 unique endings** (Good, Normal, Lonely)
- **3 characters** with expression variations
- **5 beautifully styled backgrounds**
- **Background music and sound effects**
- **Timeline animations** (fade, slide, dissolve)

## Quick Start

1. Open this project in the NovelMind editor
2. Press **Play** to start the demo
3. Make choices to experience different story paths
4. Try multiple playthroughs to see all endings!

## Story Structure

```
Intro: School Hallway
        |
        v
  First Choice
   /        \
  v          v
Cafeteria   Library
(Social)    (Study)
  |            |
  v            v
Second       Second
Choice       Choice
  |            |
  +-----+------+
        |
   +----+----+----+
   |    |    |    |
   v    v    v    v
 Good Normal Normal Lonely
```

## Characters

| Character | Description | Expressions |
|-----------|-------------|-------------|
| **Alex** | The protagonist - new student | neutral, happy, sad |
| **Sam** | Friendly classmate with glasses | neutral, happy, surprised |
| **Prof. White** | Strict but fair teacher | neutral, happy |

## Locations

1. **School Hallway** - Where the story begins
2. **Cafeteria** - Warm, social atmosphere
3. **Library** - Quiet, studious environment
4. **Rooftop at Sunset** - Good/Normal endings
5. **Empty Classroom** - Lonely ending

## Features Demonstrated

### Story Graph
- Visual branching paths
- Multiple endings with conditions
- Scene-to-scene navigation

### Scene View (WYSIWYG)
- Character positioning (left, center, right)
- Expression changes during dialogue
- Fade in/out animations

### Dialogue System
- Named character dialogue
- Narrator text
- Character expressions tied to dialogue

### Choice System
- 2-option choices at branch points
- Choices affect story variables
- Conditional story paths

### Timeline & Animations
- Fade transitions between scenes
- Dissolve effects for character appearances
- Cross-fade for music transitions

### Audio
- Background music tracks (placeholder)
- Sound effects for actions
- Music transitions between scenes

### Variables & Conditions
- `friendship_points` - Tracks social choices
- `intelligence_points` - Tracks study choices
- `social_route` / `study_route` - Path flags

## Files

```
demo_branching_novel/
├── project.json           # Project configuration
├── template.json          # Template metadata
├── preview.png            # Template preview image
├── README.md              # This file
├── CREDITS.md             # Asset attribution
│
├── assets/
│   ├── characters/        # 8 character sprite PNGs
│   ├── backgrounds/       # 5 background images
│   ├── music/             # Background music (placeholder)
│   ├── sfx/               # Sound effects (placeholder)
│   └── ui/                # UI elements
│
├── scenes/
│   ├── intro_hallway.nmscene
│   ├── cafeteria_social.nmscene
│   ├── library_study.nmscene
│   ├── ending_good.nmscene
│   ├── ending_normal.nmscene
│   └── ending_lonely.nmscene
│
├── scripts/
│   └── main.nms           # Complete story script
│
└── story_graph/
    └── demo_story.nmgraph # Visual story flow
```

## Endings Guide

| Ending | Path | Final Choice |
|--------|------|--------------|
| **Good** | Either | "Play games" / "Take a break" |
| **Normal** | Either | "Study together" / "Study more" |
| **Lonely** | Either | "Go home" / "Focus alone" |

## Playtime

- **Single playthrough**: 3-5 minutes
- **All endings**: 10-15 minutes

## Customization Ideas

This template is designed to be modified. Try:

1. Adding more characters
2. Creating additional story branches
3. Adding voice acting to dialogue
4. Creating new backgrounds
5. Implementing an achievement system

## Technical Notes

- Engine version: 1.0.0+
- Resolution: 1920x1080 (16:9)
- Platforms: Windows, Linux, Web

## License

This demo template is provided under CC0 / MIT license. Feel free to use it as a base for your own projects.

---

*Created by the NovelMind Team*
