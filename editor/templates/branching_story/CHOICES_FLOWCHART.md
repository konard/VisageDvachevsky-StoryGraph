# Branching Story - Choices Flowchart

This document describes the story flow and how player choices affect the narrative.

## Story Overview

The story follows a protagonist who awakens at a mystical crossroads and must choose their path through a world of light and shadow.

## Story Flow Diagram

```
                    ┌─────────────┐
                    │   PROLOGUE  │
                    │  (Start)    │
                    └──────┬──────┘
                           │
                           ▼
                    ┌─────────────┐
                    │ ACT 1       │
                    │ SCENE 1     │
                    │             │
                    │ Choice:     │
                    │ "What       │
                    │ drives you?"│
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
              ▼            ▼            ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │Knowledge │ │Help      │ │Survive   │
        │+1 aff_a  │ │+1 aff_b  │ │(neutral) │
        └────┬─────┘ └────┬─────┘ └────┬─────┘
              │            │            │
              └────────────┼────────────┘
                           │
                           ▼
                    ┌─────────────┐
                    │ ACT 1       │
                    │ CHOICE 1    │
                    │             │
                    │ "Choose     │
                    │ your path"  │
                    └──────┬──────┘
                           │
         ┌─────────────────┼─────────────────┐
         │                 │                 │
         ▼                 ▼                 ▼
   ┌───────────┐     ┌───────────┐     ┌───────────┐
   │ PATH OF   │     │ PATH OF   │     │ PATH OF   │
   │ LIGHT     │     │ SHADOWS   │     │ MYSTERY   │
   │ (Meadow)  │     │ (Forest)  │     │ (Mountain)│
   │ +2 aff_a  │     │ +2 aff_b  │     │ (neutral) │
   └─────┬─────┘     └─────┬─────┘     └─────┬─────┘
         │                 │                 │
         ▼                 ▼                 ▼
   ┌───────────┐     ┌───────────┐     ┌───────────┐
   │ ACT 2     │     │ ACT 2     │     │ ACT 2     │
   │ ROUTE A   │     │ ROUTE B   │     │ ROUTE C   │
   │           │     │           │     │           │
   │ Choice:   │     │ Choice:   │     │ Choice:   │
   │ "Accept   │     │ "Face or  │     │ "What do  │
   │ balance?" │     │ flee?"    │     │ you seek?"│
   └─────┬─────┘     └─────┬─────┘     └─────┬─────┘
         │                 │                 │
    ┌────┴────┐       ┌────┴────┐       ┌────┴────┐
    │         │       │         │       │    │    │
    ▼         ▼       ▼         ▼       ▼    ▼    ▼
┌───────┐ ┌───────┐ ┌───────┐ ┌───────┐ (3 options)
│Yes    │ │No     │ │Face   │ │Learn  │
│+1 a/b │ │       │ │+1 b   │ │+1 a   │
│       │ │       │ │secret │ │       │
└───┬───┘ └───┬───┘ └───┬───┘ └───┬───┘
    │         │         │         │
    └────┬────┘         └────┬────┘
         │                   │
         └─────────┬─────────┘
                   │
                   ▼
            ┌─────────────┐
            │ ENDING      │
            │ CHECK       │
            └──────┬──────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
    ▼              ▼              ▼
┌────────┐   ┌───────────┐   ┌────────┐
│ GOOD   │   │ NEUTRAL   │   │ BAD    │
│ ENDING │   │ (Light or │   │ ENDING │
│        │   │  Shadow)  │   │        │
│secret  │   │           │   │        │
│+ a>=2  │   │ a>b or    │   │ else   │
│+ b>=2  │   │ b>a       │   │        │
└────────┘   └───────────┘   └────────┘
```

## Variables

| Variable | Type | Description |
|----------|------|-------------|
| `affinity_a` | Integer | Tracks alignment with the light/wisdom path |
| `affinity_b` | Integer | Tracks alignment with the shadow/action path |
| `secret_found` | Boolean | Whether the player discovered the hidden truth |

## Endings

### Good Ending (True Ending)
**Requirements:** `secret_found == true` AND `affinity_a >= 2` AND `affinity_b >= 2`

The protagonist achieves balance, understanding both light and shadow, and transcends the crossroads to become a guide themselves.

### Light Ending (Neutral)
**Requirements:** `affinity_a > affinity_b`

The protagonist finds peace in the light but feels something is missing. Encourages replay to discover other paths.

### Shadow Ending (Neutral)
**Requirements:** `affinity_b > affinity_a`

The protagonist gains power through darkness but feels hollow. Encourages replay to discover other paths.

### Bad Ending
**Requirements:** Default (when no other conditions are met)

The protagonist becomes lost in uncertainty, unable to find their way. Encourages replay with more decisive choices.

## How to Achieve Each Ending

### Good Ending
1. In Act 1 Scene 1, choose either "Knowledge" or "Help others"
2. In Act 1 Choice, take the Path of Shadows (Forest)
3. In Act 2 Route B, choose "Face whatever comes"
   - This sets `secret_found = true` and increases both affinities

### Light Ending
1. Choose options that increase `affinity_a`
2. Avoid options that set `secret_found = true`
3. End with `affinity_a > affinity_b`

### Shadow Ending
1. Choose options that increase `affinity_b`
2. Avoid options that set `secret_found = true`
3. End with `affinity_b > affinity_a`

### Bad Ending
1. Make neutral choices throughout
2. End with `affinity_a == affinity_b` without finding the secret
