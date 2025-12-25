# Branching Story Template

## Overview

This template demonstrates how to create interactive visual novels with player choices, branching paths, and multiple endings. It showcases the full power of NovelMind's choice system and variable tracking.

## What's Included

- **11 Demo Scenes**: Prologue, 2 acts with 3 routes each, and 4 endings
- **4 Sample Characters**: Protagonist, Guide, Rival, and Mentor
- **Multiple Endings**: Good, Light, Shadow, and Bad endings
- **Variable System**: Affinity tracking that affects the story outcome
- **Choice Flowchart**: Visual documentation of all story branches

## Project Structure

```
branching_story/
├── project.json                  # Project configuration
├── template.json                 # Template metadata
├── README.md                     # This file
├── CHOICES_FLOWCHART.md          # Visual guide to story branches
├── assets/
│   ├── characters/               # Character sprites (4 needed)
│   ├── backgrounds/              # Background images (10 needed)
│   ├── music/                    # Background music (9 tracks)
│   ├── sfx/                      # Sound effects
│   ├── voice/                    # Voice acting (optional)
│   └── ui/
│       └── choice_backgrounds/   # Custom choice menu styling
├── scenes/
│   ├── prologue.nmscene
│   ├── act1_scene1.nmscene
│   └── act1_choice1.nmscene
├── scripts/
│   └── main.nms                  # All dialogue and logic
└── story_graph/
    └── branching_story.nmgraph   # Visual story flow with branches
```

## Story Structure

```
          PROLOGUE
              │
         ACT 1 SCENE 1
              │
         ACT 1 CHOICE ──┬──── Path of Light ──── ACT 2 ROUTE A
              │         │
              │         ├──── Path of Shadows ── ACT 2 ROUTE B
              │         │
              │         └──── Path of Mystery ── ACT 2 ROUTE C
              │
         ENDING CHECK
              │
    ┌─────────┼─────────┬─────────┐
    │         │         │         │
  GOOD     LIGHT     SHADOW     BAD
 ENDING   ENDING    ENDING    ENDING
```

## Variables and Endings

### Tracked Variables

| Variable | Purpose |
|----------|---------|
| `affinity_a` | Tracks alignment with light/wisdom |
| `affinity_b` | Tracks alignment with shadow/action |
| `secret_found` | Whether player discovered the hidden truth |

### Ending Requirements

| Ending | Requirements |
|--------|--------------|
| **Good** | `secret_found` AND `affinity_a >= 2` AND `affinity_b >= 2` |
| **Light** | `affinity_a > affinity_b` |
| **Shadow** | `affinity_b > affinity_a` |
| **Bad** | None of the above |

## Getting Started

### 1. Study the Flow

Before modifying, play through the demo to understand:
- How choices affect variables
- How endings are determined
- The rhythm of choice placement

### 2. Replace Assets

**Characters** (in `assets/characters/`):
- Protagonist, Guide, Rival, Mentor sprites

**Backgrounds** (in `assets/backgrounds/`):
- mysterious_crossroads.jpg
- enchanted_forest.jpg
- three_paths.jpg
- sunlit_meadow.jpg
- dark_forest.jpg
- misty_mountain.jpg
- sunrise_vista.jpg
- peaceful_ending.jpg
- twilight_ending.jpg
- empty_void.jpg

### 3. Customize the Story

Edit `scripts/main.nms` to:
- Change the narrative
- Add/remove choices
- Modify variable logic
- Create new branches

## Script Features Demonstrated

### Variable Declaration
```nms
var affinity_a = 0
var affinity_b = 0
var secret_found = false
```

### Choice Menus
```nms
choice {
    "Option A" -> {
        set affinity_a = affinity_a + 1
        say Protagonist "I chose A!"
    }
    "Option B" -> {
        set affinity_b = affinity_b + 1
        goto different_scene
    }
}
```

### Conditional Branches
```nms
if secret_found == true && affinity_a >= 2 {
    goto ending_good
} else {
    goto ending_bad
}
```

### Inline Choice Actions
```nms
choice {
    "Simple choice" -> goto next_scene
    "Complex choice" -> {
        set variable = value
        say Character "Response"
        goto target_scene
    }
}
```

## Tips for Branching Narratives

1. **Plan First**: Sketch your story flow before writing. Use `CHOICES_FLOWCHART.md` as a template.
2. **Balance Branches**: Ensure all paths have meaningful content.
3. **Track Variables Carefully**: Document what each variable affects.
4. **Meaningful Choices**: Each choice should have visible consequences.
5. **Merge Points**: Not every choice needs a permanent split - branches can reconverge.

## Testing Your Story

1. **Play All Paths**: Systematically test every branch
2. **Check Variables**: Add debug output during development
3. **Verify Endings**: Ensure all endings are reachable
4. **Edge Cases**: Test what happens with equal affinities

## Need Help?

- See `CHOICES_FLOWCHART.md` for visual story documentation
- Check the [NovelMind Documentation](https://novelmind.dev/docs)
- Visit the [Community Forum](https://novelmind.dev/community)
