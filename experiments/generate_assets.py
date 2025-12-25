#!/usr/bin/env python3
"""
Asset Generator for Demo Branching Novel Template
Generates minimalist silhouette character sprites and placeholder images.
"""

import os
from pathlib import Path

# Try to import image libraries
try:
    from PIL import Image, ImageDraw, ImageFont
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("PIL not available, using SVG only")

# Base path for assets
BASE_PATH = Path("/tmp/gh-issue-solver-1766665309860/editor/templates/demo_branching_novel/assets")

# Ensure directories exist
for subdir in ["characters", "backgrounds", "music", "sfx", "voice", "ui"]:
    (BASE_PATH / subdir).mkdir(parents=True, exist_ok=True)


def create_character_svg(name: str, color: str, expression: str, accent_color: str = None) -> str:
    """Generate SVG for a character silhouette with expression."""

    # Expression modifiers
    eye_shapes = {
        "neutral": ('ellipse cx="170" cy="280" rx="15" ry="12"', 'ellipse cx="230" cy="280" rx="15" ry="12"'),
        "happy": ('path d="M155 280 Q170 270 185 280"', 'path d="M215 280 Q230 270 245 280"'),
        "sad": ('path d="M155 275 Q170 285 185 275"', 'path d="M215 275 Q230 285 245 275"'),
        "surprised": ('ellipse cx="170" cy="280" rx="18" ry="18"', 'ellipse cx="230" cy="280" rx="18" ry="18"'),
    }

    mouth_shapes = {
        "neutral": 'path d="M175 340 L225 340"',
        "happy": 'path d="M170 335 Q200 360 230 335"',
        "sad": 'path d="M170 350 Q200 335 230 350"',
        "surprised": 'ellipse cx="200" cy="345" rx="15" ry="20"',
    }

    left_eye, right_eye = eye_shapes.get(expression, eye_shapes["neutral"])
    mouth = mouth_shapes.get(expression, mouth_shapes["neutral"])

    # Accent feature based on character
    accent = ""
    if accent_color:
        if name == "sam":
            # Sam has glasses
            accent = f'''
    <circle cx="170" cy="280" r="25" fill="none" stroke="{accent_color}" stroke-width="3"/>
    <circle cx="230" cy="280" r="25" fill="none" stroke="{accent_color}" stroke-width="3"/>
    <line x1="195" y1="280" x2="205" y2="280" stroke="{accent_color}" stroke-width="3"/>
    <line x1="145" y1="280" x2="130" y2="270" stroke="{accent_color}" stroke-width="3"/>
    <line x1="255" y1="280" x2="270" y2="270" stroke="{accent_color}" stroke-width="3"/>
            '''
        elif name == "professor":
            # Professor has a tie
            accent = f'''
    <polygon points="200,500 185,520 200,700 215,520" fill="{accent_color}"/>
            '''

    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg width="400" height="1080" viewBox="0 0 400 1080" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="bodyGrad" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" style="stop-color:{color};stop-opacity:1" />
      <stop offset="100%" style="stop-color:{color};stop-opacity:0.8" />
    </linearGradient>
  </defs>

  <!-- Body silhouette -->
  <ellipse cx="200" cy="300" rx="100" ry="120" fill="url(#bodyGrad)"/>

  <!-- Shoulders and torso -->
  <path d="M100 380 Q100 450 120 550 L120 900 Q120 1000 200 1080 Q280 1000 280 900 L280 550 Q300 450 300 380 Q250 420 200 420 Q150 420 100 380"
        fill="url(#bodyGrad)"/>

  <!-- Neck -->
  <rect x="175" y="380" width="50" height="60" fill="{color}"/>

  <!-- Face features (white/light for contrast) -->
  <{left_eye} fill="white"/>
  <{right_eye} fill="white"/>
  <{mouth} fill="none" stroke="white" stroke-width="4" stroke-linecap="round"/>

  <!-- Accent features -->
  {accent}
</svg>
'''
    return svg


def create_background_svg(name: str, primary: str, secondary: str, description: str) -> str:
    """Generate a simple gradient background with visual elements."""

    elements = ""

    if "hallway" in name:
        # School hallway with lockers
        elements = '''
    <rect x="50" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="200" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="350" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="500" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="1260" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="1410" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="1560" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <rect x="1710" y="400" width="120" height="500" fill="#555" stroke="#333" stroke-width="2"/>
    <!-- Floor -->
    <rect x="0" y="900" width="1920" height="180" fill="#8B7355"/>
        '''
    elif "cafeteria" in name:
        # Cafeteria with tables
        elements = '''
    <rect x="100" y="600" width="300" height="20" fill="#8B4513" rx="5"/>
    <rect x="150" y="620" width="10" height="200" fill="#654321"/>
    <rect x="350" y="620" width="10" height="200" fill="#654321"/>
    <rect x="500" y="600" width="300" height="20" fill="#8B4513" rx="5"/>
    <rect x="550" y="620" width="10" height="200" fill="#654321"/>
    <rect x="750" y="620" width="10" height="200" fill="#654321"/>
    <rect x="1100" y="600" width="300" height="20" fill="#8B4513" rx="5"/>
    <rect x="1500" y="600" width="300" height="20" fill="#8B4513" rx="5"/>
    <!-- Counter -->
    <rect x="0" y="300" width="1920" height="100" fill="#DEB887"/>
    <!-- Floor -->
    <rect x="0" y="820" width="1920" height="260" fill="#D2B48C"/>
        '''
    elif "library" in name:
        # Library with bookshelves
        elements = '''
    <rect x="50" y="200" width="200" height="600" fill="#654321"/>
    <rect x="60" y="210" width="180" height="80" fill="#8B0000"/>
    <rect x="60" y="300" width="180" height="80" fill="#006400"/>
    <rect x="60" y="390" width="180" height="80" fill="#00008B"/>
    <rect x="60" y="480" width="180" height="80" fill="#8B4513"/>
    <rect x="60" y="570" width="180" height="80" fill="#4B0082"/>
    <rect x="60" y="660" width="180" height="80" fill="#B8860B"/>

    <rect x="300" y="200" width="200" height="600" fill="#654321"/>
    <rect x="310" y="210" width="180" height="80" fill="#2F4F4F"/>
    <rect x="310" y="300" width="180" height="80" fill="#8B0000"/>
    <rect x="310" y="390" width="180" height="80" fill="#556B2F"/>
    <rect x="310" y="480" width="180" height="80" fill="#483D8B"/>
    <rect x="310" y="570" width="180" height="80" fill="#8B4513"/>
    <rect x="310" y="660" width="180" height="80" fill="#2F4F4F"/>

    <rect x="1420" y="200" width="200" height="600" fill="#654321"/>
    <rect x="1670" y="200" width="200" height="600" fill="#654321"/>
    <!-- Floor -->
    <rect x="0" y="800" width="1920" height="280" fill="#DEB887"/>
        '''
    elif "rooftop" in name or "sunset" in name:
        # Sunset sky with clouds
        elements = '''
    <ellipse cx="300" cy="200" rx="150" ry="50" fill="#FFB347" opacity="0.6"/>
    <ellipse cx="700" cy="150" rx="200" ry="60" fill="#FFB347" opacity="0.5"/>
    <ellipse cx="1400" cy="180" rx="180" ry="55" fill="#FFB347" opacity="0.6"/>
    <!-- Sun -->
    <circle cx="1600" cy="400" r="120" fill="#FFD700"/>
    <circle cx="1600" cy="400" r="100" fill="#FFA500"/>
    <!-- Railing -->
    <rect x="0" y="850" width="1920" height="20" fill="#444"/>
    <rect x="100" y="750" width="10" height="120" fill="#444"/>
    <rect x="300" y="750" width="10" height="120" fill="#444"/>
    <rect x="500" y="750" width="10" height="120" fill="#444"/>
    <rect x="700" y="750" width="10" height="120" fill="#444"/>
    <!-- Floor -->
    <rect x="0" y="870" width="1920" height="210" fill="#555"/>
        '''
    elif "classroom" in name or "empty" in name:
        # Empty classroom
        elements = '''
    <!-- Desks -->
    <rect x="200" y="600" width="150" height="100" fill="#8B4513"/>
    <rect x="400" y="600" width="150" height="100" fill="#8B4513"/>
    <rect x="600" y="600" width="150" height="100" fill="#8B4513"/>
    <rect x="200" y="750" width="150" height="100" fill="#8B4513"/>
    <rect x="400" y="750" width="150" height="100" fill="#8B4513"/>
    <rect x="600" y="750" width="150" height="100" fill="#8B4513"/>
    <rect x="1100" y="600" width="150" height="100" fill="#8B4513"/>
    <rect x="1300" y="600" width="150" height="100" fill="#8B4513"/>
    <rect x="1500" y="600" width="150" height="100" fill="#8B4513"/>
    <!-- Blackboard -->
    <rect x="700" y="150" width="500" height="300" fill="#2F4F4F"/>
    <rect x="710" y="160" width="480" height="280" fill="#1a1a2e"/>
    <!-- Floor -->
    <rect x="0" y="850" width="1920" height="230" fill="#D2B48C"/>
        '''

    svg = f'''<?xml version="1.0" encoding="UTF-8"?>
<svg width="1920" height="1080" viewBox="0 0 1920 1080" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="bgGrad" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" style="stop-color:{primary};stop-opacity:1" />
      <stop offset="100%" style="stop-color:{secondary};stop-opacity:1" />
    </linearGradient>
  </defs>

  <!-- Background gradient -->
  <rect width="1920" height="1080" fill="url(#bgGrad)"/>

  <!-- Scene elements -->
  {elements}

  <!-- Description text (for preview) -->
  <text x="960" y="50" font-family="Arial" font-size="24" fill="white" text-anchor="middle" opacity="0.5">{description}</text>
</svg>
'''
    return svg


def svg_to_png(svg_content: str, output_path: Path, width: int = None, height: int = None):
    """Convert SVG content to PNG file."""
    try:
        import cairosvg
        cairosvg.svg2png(bytestring=svg_content.encode('utf-8'),
                         write_to=str(output_path),
                         output_width=width,
                         output_height=height)
        return True
    except Exception as e:
        print(f"Error converting to PNG: {e}")
        # Save SVG as fallback
        svg_path = output_path.with_suffix('.svg')
        with open(svg_path, 'w') as f:
            f.write(svg_content)
        return False


def generate_characters():
    """Generate all character sprites."""
    characters = {
        "alex": {
            "color": "#4A90D9",
            "accent": None,
            "expressions": ["neutral", "happy", "sad"]
        },
        "sam": {
            "color": "#F5A623",
            "accent": "#333333",  # Glasses color
            "expressions": ["neutral", "happy", "surprised"]
        },
        "professor": {
            "color": "#2C3E50",
            "accent": "#7ED321",  # Tie color
            "expressions": ["neutral", "happy"]
        }
    }

    for char_name, config in characters.items():
        for expr in config["expressions"]:
            svg = create_character_svg(
                char_name,
                config["color"],
                expr,
                config["accent"]
            )
            output_path = BASE_PATH / "characters" / f"{char_name}_{expr}.png"
            if svg_to_png(svg, output_path, width=400, height=1080):
                print(f"Created: {output_path}")
            else:
                print(f"Created SVG fallback for: {char_name}_{expr}")


def generate_backgrounds():
    """Generate all background images."""
    backgrounds = {
        "school_hallway": {
            "primary": "#E8E8E8",
            "secondary": "#B0B0B0",
            "description": "School Hallway"
        },
        "cafeteria": {
            "primary": "#FFF8DC",
            "secondary": "#F5DEB3",
            "description": "Cafeteria - Warm and Inviting"
        },
        "library": {
            "primary": "#F5F5DC",
            "secondary": "#D2B48C",
            "description": "Library - Quiet Study Area"
        },
        "rooftop_sunset": {
            "primary": "#FF6B6B",
            "secondary": "#4ECDC4",
            "description": "Rooftop at Sunset"
        },
        "classroom_empty": {
            "primary": "#DDD",
            "secondary": "#888",
            "description": "Empty Classroom - Evening"
        }
    }

    for bg_name, config in backgrounds.items():
        svg = create_background_svg(
            bg_name,
            config["primary"],
            config["secondary"],
            config["description"]
        )
        output_path = BASE_PATH / "backgrounds" / f"{bg_name}.jpg"
        # For JPG, we'll save as PNG first then the engine can handle it
        output_path = output_path.with_suffix('.png')
        if svg_to_png(svg, output_path, width=1920, height=1080):
            print(f"Created: {output_path}")
        else:
            print(f"Created SVG fallback for: {bg_name}")


def create_placeholder_audio():
    """Create placeholder audio files (empty but valid)."""
    # Create minimal WAV header for placeholder files
    import struct

    def create_silent_wav(path: Path, duration_ms: int = 100):
        """Create a silent WAV file."""
        sample_rate = 44100
        num_samples = int(sample_rate * duration_ms / 1000)

        # WAV header
        header = struct.pack(
            '<4sI4s4sIHHIIHH4sI',
            b'RIFF',
            36 + num_samples * 2,  # file size - 8
            b'WAVE',
            b'fmt ',
            16,  # fmt chunk size
            1,   # audio format (PCM)
            1,   # num channels
            sample_rate,
            sample_rate * 2,  # byte rate
            2,   # block align
            16,  # bits per sample
            b'data',
            num_samples * 2  # data size
        )

        # Silent samples
        samples = b'\x00\x00' * num_samples

        with open(path, 'wb') as f:
            f.write(header)
            f.write(samples)

        print(f"Created placeholder: {path}")

    # Sound effects
    sfx_files = ["door_open.wav", "footsteps.wav", "ui_click.wav"]
    for sfx in sfx_files:
        create_silent_wav(BASE_PATH / "sfx" / sfx, 500)

    # Music files (longer duration placeholder)
    music_files = ["main_theme.ogg", "emotional_theme.ogg"]
    for music in music_files:
        # Create as WAV with .ogg extension (engine should handle)
        path = BASE_PATH / "music" / music
        create_silent_wav(path.with_suffix('.wav'), 1000)
        print(f"Note: {music} created as WAV placeholder. Replace with actual OGG files.")


def create_preview_image():
    """Create a preview image for the template."""
    svg = '''<?xml version="1.0" encoding="UTF-8"?>
<svg width="480" height="270" viewBox="0 0 480 270" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="previewBg" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" style="stop-color:#FF6B6B;stop-opacity:1" />
      <stop offset="100%" style="stop-color:#4ECDC4;stop-opacity:1" />
    </linearGradient>
  </defs>

  <!-- Background -->
  <rect width="480" height="270" fill="url(#previewBg)"/>

  <!-- Silhouette characters -->
  <ellipse cx="120" cy="180" rx="30" ry="40" fill="#4A90D9"/>
  <ellipse cx="360" cy="180" rx="30" ry="40" fill="#F5A623"/>

  <!-- Title -->
  <text x="240" y="50" font-family="Arial" font-size="18" fill="white" text-anchor="middle" font-weight="bold">First Day at Academy</text>
  <text x="240" y="75" font-family="Arial" font-size="12" fill="white" text-anchor="middle">Interactive Demo</text>

  <!-- Features -->
  <text x="240" y="250" font-family="Arial" font-size="10" fill="white" text-anchor="middle">3 Endings | Branching Story | Ready to Play</text>
</svg>
'''
    output_path = BASE_PATH.parent / "preview.png"
    if svg_to_png(svg, output_path, width=480, height=270):
        print(f"Created: {output_path}")
    else:
        print("Created SVG fallback for preview")


if __name__ == "__main__":
    print("Generating assets for Demo Branching Novel template...")
    print("=" * 50)

    print("\n[1/4] Generating character sprites...")
    generate_characters()

    print("\n[2/4] Generating background images...")
    generate_backgrounds()

    print("\n[3/4] Creating placeholder audio files...")
    create_placeholder_audio()

    print("\n[4/4] Creating preview image...")
    create_preview_image()

    print("\n" + "=" * 50)
    print("Asset generation complete!")
    print("\nNote: Audio files are placeholders. For production use,")
    print("replace with royalty-free music from:")
    print("- https://incompetech.com (Kevin MacLeod - CC BY)")
    print("- https://freepd.com (Public Domain)")
    print("- https://freesound.org (CC0 sound effects)")
