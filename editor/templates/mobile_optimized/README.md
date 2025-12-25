# Mobile Optimized Template

## Overview

This template is specifically designed for creating visual novels that run smoothly on mobile devices. It includes optimized settings for portrait orientation, touch-friendly UI, and compressed assets.

## Key Features

- **Portrait Orientation**: 9:16 aspect ratio (1080x1920) for natural mobile viewing
- **Touch-Optimized**: Large tap targets (48px minimum) for comfortable interaction
- **Asset Optimization**: Built-in settings for compressed textures and audio
- **Mobile Build Settings**: Pre-configured for Android and iOS export

## Mobile-Specific Settings

### Resolution
- **Width**: 1080 pixels
- **Height**: 1920 pixels
- **Aspect Ratio**: 9:16 (Portrait)

### Asset Limits
- **Max Texture Size**: 1024x1024
- **Image Quality**: 80%
- **Audio Quality**: Medium (compressed)

### UI Configuration
- **Touch Target Size**: 48px minimum
- **Button Padding**: 16px
- **Dialogue Box**: Bottom-third of screen

## Project Structure

```
mobile_optimized/
├── project.json          # Mobile-optimized configuration
├── template.json         # Template metadata
├── README.md             # This file
├── assets/
│   ├── characters/       # Character sprites (keep small!)
│   ├── backgrounds/      # 1080x1920 backgrounds
│   ├── music/            # Compressed audio
│   ├── sfx/              # Short sound effects
│   └── ui/               # Touch-friendly UI elements
├── scenes/
├── scripts/
│   └── main.nms
└── story_graph/
```

## Asset Guidelines

### Characters
- **Resolution**: 512x1024 or smaller
- **Format**: PNG with transparency
- **Size**: Under 500KB each

### Backgrounds
- **Resolution**: 1080x1920 (portrait)
- **Format**: JPG (80% quality) or PNG
- **Size**: Under 1MB each

### Audio
- **Format**: OGG (recommended)
- **Music**: 128kbps stereo
- **SFX**: 96kbps mono
- **Size**: Keep tracks under 3MB

## Writing for Mobile

### Short Lines
```nms
// Good for mobile
say Character "Keep lines short."
say Character "One thought per line."

// Bad for mobile
say Character "Long paragraphs of text are hard to read on small mobile screens and should be broken up into smaller chunks."
```

### Clear Choices
```nms
// Good - large, distinct options
choice {
    "Yes, continue" -> goto next
    "No, go back" -> goto previous
}
```

### Touch-Friendly Timing
```nms
// Allow time for touch input
wait 0.3
// Don't rush transitions
transition fade 0.5
```

## Testing Checklist

- [ ] Test on actual mobile devices
- [ ] Check text readability
- [ ] Verify touch targets are large enough
- [ ] Test in portrait orientation
- [ ] Check load times on slower devices
- [ ] Verify audio plays correctly
- [ ] Test offline functionality

## Build Targets

This template is configured for:
- Android 8.0+
- iOS 13.0+
- Mobile web browsers

## Need Help?

- [Mobile Development Guide](https://novelmind.dev/docs/mobile)
- [Asset Optimization Tips](https://novelmind.dev/docs/optimization)
- [Community Forum](https://novelmind.dev/community)
