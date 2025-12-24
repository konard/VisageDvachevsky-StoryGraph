# Voice-Over Integration for Dialogue Nodes

This guide explains how to use voice-over integration in dialogue nodes within NovelMind's Story Graph editor.

## Overview

Voice-over integration allows you to:
- Assign voice audio files to dialogue nodes in Visual-First workflow
- Preview voice clips directly in the editor
- Record voice lines using the integrated Recording Studio
- Auto-detect voice files based on localization keys
- View voice binding status with visual indicators

## Features

### 1. Voice Clip Assignment

There are multiple ways to assign voice clips to dialogue nodes:

#### Manual Assignment
1. Right-click on a Dialogue node in the Story Graph
2. Select **"Assign Voice Clip..."** from the context menu
3. Browse and select an audio file (supported formats: OGG, WAV, MP3, FLAC, OPUS)
4. The voice clip is now bound to the dialogue node

#### Drag-and-Drop (Future)
- Drag a voice file from the Asset Browser directly onto a dialogue node
- The file will be automatically assigned

#### Auto-Detection
1. Right-click on a Dialogue node
2. Select **"Auto-Detect Voice"**
3. The system will search for matching voice files based on:
   - Character name
   - Scene ID
   - Localization key pattern

### 2. Voice Clip Status Indicators

Dialogue nodes display visual indicators showing their voice-over status:

#### Status Colors
- **Green**: Voice clip is bound and file exists
- **Red**: Voice clip is assigned but file is missing
- **Blue**: Voice clip was auto-detected
- **Gray**: No voice clip assigned (unbound)

#### Visual Elements
- **Play Button** (triangle icon): Appears when a voice clip is assigned
  - Click to preview the voice audio
- **Record Button** (circle icon): Always visible on dialogue nodes
  - Click to open Recording Studio and record/re-record voice
- **"Voice" Label**: Shown when voice is successfully bound

### 3. Recording Voice Lines

#### Opening Recording Studio
1. Right-click on a Dialogue node
2. Select **"Record Voice..."** (or **"Re-record Voice..."** if already recorded)
3. Recording Studio panel opens with:
   - Dialogue text displayed for reference
   - Speaker name shown
   - Recording controls ready

#### Recording Workflow
1. Set up your microphone in Recording Studio
2. Click **Record** to start capturing audio
3. Speak the dialogue line
4. Click **Stop** when finished
5. Voice clip is automatically generated and assigned to the node

### 4. Voice Clip Preview

To preview a voice clip:

#### From Context Menu
1. Right-click on a Dialogue node with assigned voice
2. Select **"Preview Voice"**
3. Audio plays through your speakers

#### From Play Button
1. Click the **Play button** (triangle icon) on the dialogue node
2. Voice clip plays immediately

#### Stopping Preview
- Preview stops automatically when complete
- Click elsewhere or trigger another action to stop early

### 5. Clearing Voice Assignment

To remove a voice clip from a dialogue node:
1. Right-click on the Dialogue node
2. Select **"Clear Voice Clip"**
3. Voice assignment is removed, node returns to unbound status

## Integration with Voice Manager

The Voice Manager panel provides centralized control over all voice files in your project.

### Syncing with Voice Manager
- Voice assignments made in Story Graph are reflected in Voice Manager
- Voice statistics update automatically
- Missing voice files are flagged in both interfaces

### Import/Export
- Export voice assignments to CSV for voice actors
- Import recorded voice files and assignments in bulk
- Voice Manager tracks all dialogue nodes across all scenes

## Integration with Play-in-Editor

During Play-in-Editor mode:
- Voice clips automatically play when dialogue nodes are executed
- Voice playback is synchronized with dialogue text display
- Dialogue timing can be controlled by voice duration

## File Organization

### Recommended Structure
```
MyProject/
└── Assets/
    └── Voice/
        ├── en/              # English locale
        │   ├── alice/       # Per-character folders
        │   │   ├── scene01_001.ogg
        │   │   └── scene01_002.ogg
        │   └── bob/
        │       ├── scene01_003.ogg
        │       └── scene01_004.ogg
        └── ru/              # Russian locale
            └── alice/
                ├── scene01_001.ogg
                └── scene01_002.ogg
```

### Naming Conventions for Auto-Detection

For auto-detection to work, use consistent naming:
- `{character}_{scene}_{line}.ogg`
- `{scene}_{character}_{number}.ogg`
- Pattern matching can be customized in Voice Manager settings

## Localization Support

Voice-over integration supports multi-language projects:
- Each dialogue node can have different voice files per locale
- Localization keys link dialogue to translated voice files
- Voice Manager handles locale switching automatically

## Technical Details

### Data Storage
- Voice clip paths are stored in IR node properties
- Binding status tracked per node
- Voice metadata cached for performance

### Supported Audio Formats
- **OGG Vorbis** (recommended for web/cross-platform)
- **WAV** (uncompressed, highest quality)
- **MP3** (widely compatible)
- **FLAC** (lossless compression)
- **Opus** (modern, efficient codec)

### Performance Considerations
- Voice duration is cached after first load
- Preview uses streaming playback (no full file load)
- Large voice files are handled efficiently

## Troubleshooting

### Voice File Not Playing
1. Check file exists at specified path
2. Verify audio format is supported
3. Check system audio output settings
4. Look for errors in Console panel

### Auto-Detection Not Finding Files
1. Verify file naming matches expected pattern
2. Check files are in correct Voice assets folder
3. Review auto-detection patterns in Voice Manager
4. Ensure localization keys are set correctly

### Recording Issues
1. Check microphone permissions
2. Verify input device in Recording Studio
3. Test microphone levels before recording
4. Check disk space for saving recordings

## Best Practices

1. **Organize by Character**: Group voice files by character for easy management
2. **Consistent Naming**: Use clear, consistent file naming conventions
3. **Test Early**: Preview voice clips frequently during development
4. **Backup Recordings**: Keep backup copies of recorded voice files
5. **Version Control**: Include voice assignment files in version control
6. **Placeholder Audio**: Use temporary audio during prototyping

## Advanced Usage

### Scripting Voice Assignment
Voice clip properties can be set programmatically via IR nodes:
```cpp
dialogueNode->setProperty("voice_file", "voice/hero/hello.ogg");
dialogueNode->setProperty("voice_localization_key", "hero_intro_001");
dialogueNode->setProperty("voice_duration", 2.5);
```

### Batch Operations
Use Voice Manager for batch operations:
- Auto-map all unbound dialogues at once
- Import voice assignments from spreadsheet
- Export all missing voice lines for recording session

## See Also

- [Voice Manager Panel Documentation](voice_manager.md)
- [Recording Studio Documentation](recording_studio.md)
- [Scene Nodes and Workflows](scene_node_workflows.md)
- [Localization Guide](localization.md)
