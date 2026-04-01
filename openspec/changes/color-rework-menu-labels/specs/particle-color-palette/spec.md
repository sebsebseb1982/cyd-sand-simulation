## ADDED Requirements

### Requirement: Sand uses cream palette
The system SHALL display sand particles using a cream-toned color palette centered on `#ffedd1` (RGB 255, 237, 209), with 4 micro-variations spread across warm and cool brightness shifts.

#### Scenario: Sand particles render in cream tones
- **WHEN** a sand particle is placed or updated on the grid
- **THEN** it SHALL be drawn in one of four cream variants near `#ffedd1`

### Requirement: Wood uses amber color
The system SHALL display wood particles using the color `#a36500` (RGB 163, 101, 0) both in the simulation and in the menu.

#### Scenario: Wood cell rendered in simulation
- **WHEN** a wood cell (`WOOD_VAL`) is drawn
- **THEN** its color SHALL be `RGB565(163, 101, 0)`

### Requirement: Fire uses red-orange palette
The system SHALL display fire particles using 4 color variants anchored at `#ff2f00` (RGB 255, 47, 0), ranging from deep red to orange. The fire source cell SHALL use the same base red.

#### Scenario: Fire particles render in red-orange range
- **WHEN** a fire particle (FIRE_MIN..FIRE_MAX) is drawn
- **THEN** each variant SHALL be a distinct shade in the red-to-orange range anchored at `#ff2f00`

#### Scenario: Fire source uses base red
- **WHEN** a fire source cell (FIRE_SRC) is drawn
- **THEN** its color SHALL match the base fire color `RGB565(255, 47, 0)`
