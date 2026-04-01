## ADDED Requirements

### Requirement: Wood is a static, flammable material
Wood (WOOD_VAL = 5, formerly OBSTACLE_VAL) SHALL behave as a static obstacle that does not move under gravity, identical to the previous obstacle behavior.

#### Scenario: Wood does not move
- **WHEN** the simulation runs each frame
- **THEN** WOOD_VAL cells do not change position due to gravity

### Requirement: Wood becomes a fire source when ignited
A WOOD_VAL cell (value 5) that is set to FIRE_SRC by the fire ignition logic SHALL immediately behave as a FIRE_SRC cell from the next frame onward.

#### Scenario: Wood ignition
- **WHEN** a fire particle's ignition check finds a WOOD_VAL neighbor
- **THEN** that cell's value changes from 5 (WOOD_VAL) to 7 (FIRE_SRC)

#### Scenario: Ignited wood burns and propagates
- **WHEN** a WOOD_VAL cell has become FIRE_SRC
- **THEN** it emits fire particles and can ignite further adjacent WOOD_VAL cells, creating chain combustion

### Requirement: Wood is drawn in brown
WOOD_VAL cells SHALL be rendered with color RGB565(101, 67, 33) (dark brown).

#### Scenario: Wood cell color
- **WHEN** drawCell is called for a cell with value WOOD_VAL
- **THEN** the pixel is filled with RGB565(101, 67, 33)
