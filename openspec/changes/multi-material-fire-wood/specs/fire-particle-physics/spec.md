## ADDED Requirements

### Requirement: Fire source emits fire particles
A FIRE_SRC cell (value 7) SHALL emit a fire particle (value 8-11, chosen at random) into the adjacent cell in the anti-gravity direction each frame with ~40% probability, if that cell is empty (value 0).

#### Scenario: Emission into free cell
- **WHEN** a FIRE_SRC cell exists and the adjacent anti-gravity cell is empty
- **THEN** a fire particle (random value 8-11) is placed in that cell with ~40% probability per frame

#### Scenario: Blocked emission
- **WHEN** a FIRE_SRC cell exists and the adjacent anti-gravity cell is occupied
- **THEN** no particle is emitted (no error, no side effect)

### Requirement: Fire source dies probabilistically
A FIRE_SRC cell SHALL have a ~0.5% probability per frame of becoming empty (value 0), independent of emission.

#### Scenario: Natural extinction
- **WHEN** a FIRE_SRC cell is processed each frame
- **THEN** it becomes empty with ~0.5% probability regardless of whether it emitted a particle

### Requirement: Fire particles move in the anti-gravity direction
Fire particles (values 8-11) SHALL move in the direction opposite to the current gravity vector each frame, using the same directional logic as sand (primary direction, then two diagonal slides).

#### Scenario: Free path upward
- **WHEN** a fire particle exists and the anti-gravity cell is empty
- **THEN** the particle moves to that cell

#### Scenario: Diagonal slide when blocked
- **WHEN** a fire particle exists and the primary anti-gravity cell is occupied
- **THEN** the particle tries one of the two diagonal anti-gravity cells (random order); it moves to the first empty one found

#### Scenario: Fully blocked particle
- **WHEN** a fire particle exists and all three anti-gravity cells are occupied
- **THEN** the particle does not move this frame

### Requirement: Fire particles die probabilistically
Fire particles SHALL have a ~2-3% probability per frame of becoming empty (value 0).

#### Scenario: Natural death
- **WHEN** a fire particle is processed each frame
- **THEN** it becomes empty with ~2-3% probability

### Requirement: Fire particles ignite adjacent wood
When a fire particle (values 8-11) moves, it SHALL check the four orthogonal neighbors of its current position before moving. Any neighbor with WOOD_VAL (value 5) SHALL become FIRE_SRC (value 7).

#### Scenario: Wood adjacent to moving fire particle
- **WHEN** a fire particle is about to move and a WOOD_VAL cell is in one of its four orthogonal neighbors
- **THEN** that WOOD_VAL cell becomes FIRE_SRC

#### Scenario: No adjacent wood
- **WHEN** a fire particle moves and no orthogonal neighbor is WOOD_VAL
- **THEN** no cells are modified by ignition logic
