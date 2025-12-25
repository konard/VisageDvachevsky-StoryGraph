# Interactive Tutorial Project

## Welcome to NovelMind!

This tutorial project will teach you everything you need to know to create your own visual novel. It's designed for complete beginners with no prior experience.

## How to Use This Tutorial

1. **Open the Script**: Look at `scripts/main.nms` to see the code
2. **Read the Comments**: Every feature is explained in detail
3. **Run the Project**: Play through to see the code in action
4. **Experiment**: Modify the code and see what happens
5. **Check This README**: Find additional resources and tips

## Lessons Covered

### Lesson 1: Characters & Dialogue
Learn how to:
- Define characters with names and colors
- Write dialogue for different characters
- Use the narrator for scene descriptions

```nms
character Hero(name="Hero", color="#4A90D9")

scene example {
    say Hero "This is dialogue!"
    say Narrator "This is narration."
}
```

### Lesson 2: Scenes & Navigation
Learn how to:
- Create scenes (the building blocks of your story)
- Navigate between scenes with `goto`
- Structure your narrative

```nms
scene first_scene {
    say Narrator "This is scene 1."
    goto second_scene
}

scene second_scene {
    say Narrator "This is scene 2."
}
```

### Lesson 3: Backgrounds & Sprites
Learn how to:
- Display background images
- Show and hide characters
- Position characters on screen

```nms
scene example {
    show background "classroom"
    show Hero at center
    say Hero "Hello!"
    hide Hero
}
```

### Lesson 4: Choices & Branching
Learn how to:
- Create choice menus
- Branch to different scenes
- Make interactive stories

```nms
scene example {
    say Hero "What should I do?"
    choice {
        "Go left" -> goto left_path
        "Go right" -> goto right_path
    }
}
```

### Lesson 5: Variables & Conditions
Learn how to:
- Store values in variables
- Track player decisions
- Create conditional story paths

```nms
var relationship = 0

scene example {
    set relationship = relationship + 1
    if relationship >= 5 {
        goto good_ending
    } else {
        goto bad_ending
    }
}
```

### Lesson 6: Audio & Music
Learn how to:
- Play background music
- Add sound effects
- Fade and crossfade audio

```nms
scene example {
    play music "happy_theme" fadeIn 2.0
    play sfx "door_open"
    stop music fadeOut 1.0
}
```

### Lesson 7: Transitions & Timing
Learn how to:
- Use visual transitions
- Control pacing with wait
- Create dramatic moments

```nms
scene example {
    transition fade 1.0
    wait 0.5
    transition dissolve 0.5
}
```

## Quick Reference

### Character Definition
```nms
character Name(name="Display Name", color="#HexColor")
```

### Scene Definition
```nms
scene scene_name {
    // content here
}
```

### Common Commands
| Command | Description |
|---------|-------------|
| `say Character "text"` | Display dialogue |
| `show Character at position` | Show sprite |
| `hide Character` | Hide sprite |
| `show background "name"` | Change background |
| `goto scene_name` | Jump to scene |
| `choice { ... }` | Create choice menu |
| `play music "name"` | Start music |
| `play sfx "name"` | Play sound effect |
| `transition type duration` | Visual transition |
| `wait seconds` | Pause execution |

### Positions
- `left` - Left side of screen
- `center` - Center of screen
- `right` - Right side of screen

### Transition Types
- `fade` - Fade to black and back
- `dissolve` - Cross-dissolve between scenes

## Next Steps

After completing this tutorial:

1. **Create Your Own Project**: Use the Empty Project template
2. **Study Other Templates**: Look at Kinetic Novel and Branching Story
3. **Read the Documentation**: Visit [novelmind.dev/docs](https://novelmind.dev/docs)
4. **Join the Community**: Get help at [novelmind.dev/community](https://novelmind.dev/community)

## Tips for Success

1. **Start Simple**: Begin with a short story
2. **Test Often**: Run your project frequently
3. **Read Error Messages**: They tell you what's wrong
4. **Use Comments**: Document your code
5. **Back Up Your Work**: Save frequently

## Need Help?

- **Documentation**: [novelmind.dev/docs](https://novelmind.dev/docs)
- **Script Reference**: [novelmind.dev/docs/scripting](https://novelmind.dev/docs/scripting)
- **Community Forum**: [novelmind.dev/community](https://novelmind.dev/community)
- **Bug Reports**: [github.com/NovelMind/editor](https://github.com/NovelMind/editor)

Happy creating!
