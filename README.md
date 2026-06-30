# Memory Match

A memory-matching card game built in C using raylib, with multiple grid sizes, themes, and a scoring system.

---

## Overview

Memory Match is a classic card-flip game where players find matching pairs within a limited number of moves. It supports configurable grid sizes, multiple visual themes, difficulty-based scoring, and persistent high scores, wrapped in an animated UI.

## Tech Stack

| Layer | Technology |
|---|---|
| Core logic | C |
| Graphics & input | raylib |
| Asset loading | Windows API (executable-relative paths) |
| Storage | Local file (`highscore.dat`) |

## Module Breakdown

| Component | Responsibility |
|---|---|
| Grid system | 2x2, 4x4, and 6x6 board generation |
| Theme engine | Fruits and Animals (image-based), Colors and Shapes (procedural) |
| Difficulty system | Beginner, Moderate, Expert move/score multipliers |
| Scoring engine | Base score, move efficiency bonus, time bonus, near-perfect bonus |
| Star rating | 1-3 star outcome based on solve efficiency |
| High score storage | Per grid size and difficulty, persisted to disk |
| Pause menu | Resume, restart, or exit to main menu |
| UI/animation layer | Particle effects, background stars, fade transitions |

## Game Flow

```
Main Menu
   |
   v
Select Grid Size + Theme + Difficulty
   |
   v
Gameplay (card flipping, move tracking)
   |
   v
Win / Game Over
   |
   v
Score + Star Rating
   |
   v
High Score Check -> Save if new record
```

## Highlights

- Three grid sizes for varying game length and difficulty
- Four selectable themes, two image-based and two procedurally generated
- Difficulty-scaled move limits and scoring
- Star rating (1-3 stars) based on solve efficiency, with a separate Game Over state
- Persistent high scores tracked per grid size and difficulty combination
- In-game pause menu for resume/restart/exit
- Animated visual feedback: particle bursts, twinkling background, fade transitions

## Controls

- Click a card to flip it
- Match two identical cards to clear them
- Esc pauses the game

## Setup

**1. Requirements**

GCC and raylib, on Windows (uses the Windows API for asset path resolution).

**2. Assets**

Place the following in an `assets/` folder next to the executable:
```
apple.png, banana.png, mango.png, orange.png,
grapes.png, litchi.png, guava.png, watermelon.png,
lion.png, dog.png, cat.png, elephant.png,
penguin.png, rabbit.png, bird.png, tortoise.png,
card_back.png
```

**3. Run**
```
main.exe
```

## Files

- `main.c` — game source code
- `main.exe` — compiled Windows executable
- `highscore.dat` — saved high score data, created automatically

## License

For academic purposes only
