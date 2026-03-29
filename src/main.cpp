#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Wire.h>

// --- Configuration ---
#define CELL_SIZE   2
#define SCREEN_W    320
#define SCREEN_H    240
#define GRID_W      (SCREEN_W / CELL_SIZE)
#define GRID_H      (SCREEN_H / CELL_SIZE)

// Touch pins (CYD XPT2046)
#define TOUCH_CS    33
#define TOUCH_IRQ   36
#define TOUCH_CLK   25
#define TOUCH_MISO  39
#define TOUCH_MOSI  32

// MPU I2C
#define MPU_ADDR    0x68
#define MPU_SDA     22
#define MPU_SCL     27

// Touch calibration (adjust if touch is offset)
#define TOUCH_X_MIN 300
#define TOUCH_X_MAX 3800
#define TOUCH_Y_MIN 300
#define TOUCH_Y_MAX 3800

// Sand brush radius in grid cells
#define BRUSH_RADIUS    3
#define OBSTACLE_BRUSH  1  // Obstacle brush radius in grid cells

// Max lateral scan distance for liquid leveling (cells)
#define LIQUID_FLOW     10

// UI - Hamburger menu button (top-left corner, always visible)
#define MENU_BTN_X    2
#define MENU_BTN_Y    2
#define MENU_BTN_W    28
#define MENU_BTN_H    28

// Menu panel items (shown below hamburger when open)
#define MENU_PANEL_X  2
#define MENU_PANEL_Y  (MENU_BTN_Y + MENU_BTN_H + 4)
#define MENU_ITEM_W   90
#define MENU_ITEM_H   24
#define MENU_ITEM_GAP 3

// --- Objects ---
TFT_eSPI tft;
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

// --- Grid (0 = empty, 1-4 = sand color variant) ---
uint8_t grid[GRID_H][GRID_W];

// --- Gravity ---
float gravX = 0.0f;
float gravY = 1.0f;
float gravMag = 1.0f;  // filtered tilt magnitude

// Threshold below which the sand is fully frozen (flat position)
#define GRAVITY_DEADZONE 0.12f
// Threshold above which the sand moves at full speed
#define GRAVITY_FULL     0.40f

// 8 directions: S, SE, E, NE, N, NW, W, SW
const int8_t DIR_DX[] = { 0,  1,  1,  1,  0, -1, -1, -1};
const int8_t DIR_DY[] = { 1,  1,  0, -1, -1, -1,  0,  1};
int primaryDir = 0;

// Sand color palette (RGB565)
#define RGB565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
const uint16_t SAND_COLORS[] = {
    RGB565(236, 204, 104),
    RGB565(214, 182, 86),
    RGB565(246, 220, 130),
    RGB565(200, 168, 72),
};
#define NUM_COLORS 4
#define BG_COLOR TFT_BLACK

#define OBSTACLE_VAL    5
#define FIXED_VAL       6   // Reserved: button zone cells (never overwritten)
#define OBSTACLE_COLOR  RGB565(110, 110, 110)

enum DrawMode : uint8_t { MODE_SAND, MODE_OBSTACLE, MODE_OBSTACLE_ERASE };

bool mpuAvailable = false;
DrawMode drawMode = MODE_SAND;
bool menuOpen = false;

// --- Setup Functions ---
void setupDisplay() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(BG_COLOR);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
}

void setupTouch() {
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    ts.setRotation(1);
}

void setupMPU() {
    Wire.begin(MPU_SDA, MPU_SCL);
    Wire.setClock(400000);

    Wire.beginTransmission(MPU_ADDR);
    if (Wire.endTransmission() != 0) {
        mpuAvailable = false;
        return;
    }
    mpuAvailable = true;

    // Wake up MPU
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0x00);
    Wire.endTransmission(true);
    delay(50);

    // Accelerometer range ±2g
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1C);
    Wire.write(0x00);
    Wire.endTransmission(true);
}

// --- Draw a single grid cell ---
inline void drawCell(int gx, int gy, uint8_t val) {
    if (val == FIXED_VAL) return; // Never overwrite button zone
    uint16_t color = (val == 0)           ? BG_COLOR
                   : (val == OBSTACLE_VAL) ? OBSTACLE_COLOR
                                           : SAND_COLORS[val - 1];
    tft.fillRect(gx * CELL_SIZE, gy * CELL_SIZE, CELL_SIZE, CELL_SIZE, color);
}

// --- UI: Hamburger menu button ---
void drawMenuButton() {
    uint16_t bg = menuOpen ? RGB565(80, 80, 80) : RGB565(40, 40, 40);
    tft.fillRect(MENU_BTN_X, MENU_BTN_Y, MENU_BTN_W, MENU_BTN_H, bg);
    tft.drawRect(MENU_BTN_X, MENU_BTN_Y, MENU_BTN_W, MENU_BTN_H, TFT_WHITE);
    int lx = MENU_BTN_X + 5, lw = MENU_BTN_W - 10;
    tft.drawFastHLine(lx, MENU_BTN_Y + 8,  lw, TFT_WHITE);
    tft.drawFastHLine(lx, MENU_BTN_Y + 14, lw, TFT_WHITE);
    tft.drawFastHLine(lx, MENU_BTN_Y + 20, lw, TFT_WHITE);
}

// --- UI: Menu panel (4 items) ---
void drawMenuPanel() {
    static const char* labels[] = { "Sand", "Clear", "Wall", "Erase" };
    static const uint16_t bgs[] = { SAND_COLORS[0], SAND_COLORS[0], OBSTACLE_COLOR, OBSTACLE_COLOR };
    for (int i = 0; i < 4; i++) {
        int iy = MENU_PANEL_Y + i * (MENU_ITEM_H + MENU_ITEM_GAP);
        bool active = (i == 0 && drawMode == MODE_SAND)
                   || (i == 2 && drawMode == MODE_OBSTACLE)
                   || (i == 3 && drawMode == MODE_OBSTACLE_ERASE);
        uint16_t border = active ? TFT_WHITE : TFT_DARKGREY;
        tft.fillRect(MENU_PANEL_X + 1, iy + 1, MENU_ITEM_W - 2, MENU_ITEM_H - 2, bgs[i]);
        tft.drawRect(MENU_PANEL_X, iy, MENU_ITEM_W, MENU_ITEM_H, border);
        if (i == 1) {
            tft.drawLine(MENU_PANEL_X + 4, iy + 4, MENU_PANEL_X + MENU_ITEM_W - 5, iy + MENU_ITEM_H - 5, TFT_BLACK);
            tft.drawLine(MENU_PANEL_X + MENU_ITEM_W - 5, iy + 4, MENU_PANEL_X + 4, iy + MENU_ITEM_H - 5, TFT_BLACK);
        } else if (i == 3) {
            tft.drawLine(MENU_PANEL_X + 4, iy + 4, MENU_PANEL_X + MENU_ITEM_W - 5, iy + MENU_ITEM_H - 5, TFT_WHITE);
            tft.drawLine(MENU_PANEL_X + MENU_ITEM_W - 5, iy + 4, MENU_PANEL_X + 4, iy + MENU_ITEM_H - 5, TFT_WHITE);
        }
        uint16_t textColor = (bgs[i] == OBSTACLE_COLOR) ? TFT_WHITE : TFT_BLACK;
        tft.setTextColor(textColor, bgs[i]);
        tft.setTextSize(1);
        int tx = MENU_PANEL_X + (MENU_ITEM_W - (int)strlen(labels[i]) * 6) / 2;
        int ty = iy + (MENU_ITEM_H - 8) / 2;
        tft.setCursor(tx, ty);
        tft.print(labels[i]);
    }
}

// Close menu and restore grid cells that were painted over
void closeMenu() {
    menuOpen = false;
    int gx0 = MENU_PANEL_X / CELL_SIZE;
    int gy0 = MENU_PANEL_Y / CELL_SIZE;
    int gx1 = (MENU_PANEL_X + MENU_ITEM_W) / CELL_SIZE + 1;
    int gy1 = (MENU_PANEL_Y + 4 * (MENU_ITEM_H + MENU_ITEM_GAP)) / CELL_SIZE + 1;
    if (gx1 >= GRID_W) gx1 = GRID_W - 1;
    if (gy1 >= GRID_H) gy1 = GRID_H - 1;
    tft.startWrite();
    for (int gy = gy0; gy <= gy1; gy++)
        for (int gx = gx0; gx <= gx1; gx++)
            drawCell(gx, gy, grid[gy][gx]);
    tft.endWrite();
    drawMenuButton();
}

// Mark hamburger button zone as FIXED so sand never enters it
void markButtonZone() {
    int maxGX = (MENU_BTN_X + MENU_BTN_W + CELL_SIZE) / CELL_SIZE;
    int maxGY = (MENU_BTN_Y + MENU_BTN_H + CELL_SIZE) / CELL_SIZE;
    for (int gy = 0; gy < maxGY && gy < GRID_H; gy++)
        for (int gx = 0; gx < maxGX && gx < GRID_W; gx++)
            grid[gy][gx] = FIXED_VAL;
}

// --- Accelerometer ---
void readAccelerometer() {
    if (!mpuAvailable) return;

    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);

    int bytesRead = Wire.requestFrom(MPU_ADDR, 4);
    if (bytesRead < 4) return;

    uint8_t axH = Wire.read(), axL = Wire.read();
    uint8_t ayH = Wire.read(), ayL = Wire.read();
    int16_t ax = (int16_t)((axH << 8) | axL);
    int16_t ay = (int16_t)((ayH << 8) | ayL);

    // Map MPU axes to screen gravity
    // Adjust signs or swap axes to match your MPU mounting orientation
    float rawGX = -(float)ay / 16384.0f;
    float rawGY =  (float)ax / 16384.0f;

    // Low-pass filter
    gravX = gravX * 0.7f + rawGX * 0.3f;
    gravY = gravY * 0.7f + rawGY * 0.3f;
    gravMag = sqrtf(gravX * gravX + gravY * gravY);
}

void updateGravityDir() {
    if (gravMag < GRAVITY_DEADZONE) return;
    float angle = atan2f(gravX, gravY);
    int dir = (int)roundf(angle / (M_PI / 4.0f));
    primaryDir = ((dir % 8) + 8) % 8;
}

// --- Touch input ---
void handleTouch() {
    static bool wasTouched    = false;
    static bool touchIsButton = false;

    bool isTouched = ts.touched();

    if (!isTouched) {
        wasTouched    = false;
        touchIsButton = false;
        return;
    }

    TS_Point p = ts.getPoint();
    int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
    int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);
    sx = constrain(sx, 0, SCREEN_W - 1);
    sy = constrain(sy, 0, SCREEN_H - 1);

    // --- Rising edge: detect hamburger / menu item press ---
    if (!wasTouched) {
        touchIsButton = false;
        bool inHamburger = (sx >= MENU_BTN_X && sx < MENU_BTN_X + MENU_BTN_W &&
                            sy >= MENU_BTN_Y && sy < MENU_BTN_Y + MENU_BTN_H);
        if (inHamburger) {
            menuOpen = !menuOpen;
            if (menuOpen) { drawMenuButton(); drawMenuPanel(); }
            else          { closeMenu(); }
            touchIsButton = true;
        } else if (menuOpen) {
            bool hitItem = false;
            if (sx >= MENU_PANEL_X && sx < MENU_PANEL_X + MENU_ITEM_W) {
                for (int i = 0; i < 4 && !hitItem; i++) {
                    int iy = MENU_PANEL_Y + i * (MENU_ITEM_H + MENU_ITEM_GAP);
                    if (sy >= iy && sy < iy + MENU_ITEM_H) {
                        if (i == 0) {
                            drawMode = MODE_SAND;
                            closeMenu();
                        } else if (i == 1) {
                            memset(grid, 0, sizeof(grid));
                            markButtonZone();
                            tft.fillScreen(BG_COLOR);
                            menuOpen = false;
                            drawMenuButton();
                        } else if (i == 2) {
                            drawMode = MODE_OBSTACLE;
                            closeMenu();
                        } else {
                            drawMode = MODE_OBSTACLE_ERASE;
                            closeMenu();
                        }
                        hitItem = true;
                    }
                }
            }
            if (!hitItem) closeMenu();  // tap outside menu closes it
            touchIsButton = true;       // consume touch regardless
        }
        wasTouched = true;
    }

    if (touchIsButton) return;

    // --- Drawing ---
    int gx = sx / CELL_SIZE;
    int gy = sy / CELL_SIZE;

    if (drawMode == MODE_SAND) {
        for (int dy = -BRUSH_RADIUS; dy <= BRUSH_RADIUS; dy++) {
            for (int dx = -BRUSH_RADIUS; dx <= BRUSH_RADIUS; dx++) {
                if (dx * dx + dy * dy > BRUSH_RADIUS * BRUSH_RADIUS) continue;
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                    if (random(100) < 60) {
                        uint8_t c = random(1, NUM_COLORS + 1);
                        grid[ny][nx] = c;
                        drawCell(nx, ny, c);
                    }
                }
            }
        }
    } else if (drawMode == MODE_OBSTACLE) {
        for (int dy = -OBSTACLE_BRUSH; dy <= OBSTACLE_BRUSH; dy++) {
            for (int dx = -OBSTACLE_BRUSH; dx <= OBSTACLE_BRUSH; dx++) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H
                    && grid[ny][nx] != OBSTACLE_VAL && grid[ny][nx] != FIXED_VAL) {
                    grid[ny][nx] = OBSTACLE_VAL;
                    drawCell(nx, ny, OBSTACLE_VAL);
                }
            }
        }
    } else { // MODE_OBSTACLE_ERASE
        for (int dy = -OBSTACLE_BRUSH; dy <= OBSTACLE_BRUSH; dy++) {
            for (int dx = -OBSTACLE_BRUSH; dx <= OBSTACLE_BRUSH; dx++) {
                int nx = gx + dx;
                int ny = gy + dy;
                if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H
                    && grid[ny][nx] == OBSTACLE_VAL) {
                    grid[ny][nx] = 0;
                    drawCell(nx, ny, 0);
                }
            }
        }
    }
}

// --- Liquid physics ---
void simulateSand() {
    int dx_p = DIR_DX[primaryDir];
    int dy_p = DIR_DY[primaryDir];
    // Perpendicular directions (+90° and -90° from primary gravity, for liquid leveling)
    int perp1  = (primaryDir + 2) % 8;
    int perp2  = (primaryDir + 6) % 8;

    // Continuous gravity: interpolate stochastically between the two bracketing 45°-directions
    // to avoid visible snapping to 8 discrete angles.
    float gravAngle = atan2f(gravX, gravY);
    float sectorF   = gravAngle / (M_PI / 4.0f);
    int   dirFloor  = (((int)floorf(sectorF)) % 8 + 8) % 8;
    int   dirCeil   = (dirFloor + 1) % 8;
    int   blendT    = (int)((sectorF - floorf(sectorF)) * 256.0f); // 0..255

    // Process grains opposite to gravity direction
    int yStart, yEnd, yStep;
    if (dy_p >= 0) { yStart = GRID_H - 1; yEnd = -1; yStep = -1; }
    else            { yStart = 0; yEnd = GRID_H; yStep = 1; }

    int xStart, xEnd, xStep;
    if (dx_p > 0)       { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    else if (dx_p < 0)  { xStart = 0; xEnd = GRID_W; xStep = 1; }
    else {
        if (random(2)) { xStart = 0; xEnd = GRID_W; xStep = 1; }
        else           { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    }

    // Friction: probability of a grain moving this frame (0-100)
    int moveChance;
    if (gravMag < GRAVITY_DEADZONE) {
        return;
    } else if (gravMag >= GRAVITY_FULL) {
        moveChance = 100;
    } else {
        float t = (gravMag - GRAVITY_DEADZONE) / (GRAVITY_FULL - GRAVITY_DEADZONE);
        moveChance = (int)(t * t * 100.0f);
    }

    tft.startWrite();

    for (int y = yStart; y != yEnd; y += yStep) {
        for (int x = xStart; x != xEnd; x += xStep) {
            uint8_t val = grid[y][x];
            if (val == 0 || val > NUM_COLORS) continue;
            if (moveChance < 100 && (int)random(100) >= moveChance) continue;

            // Per-grain: stochastically pick between the two directions bracketing the
            // continuous gravity angle, giving smooth motion at any orientation.
            int fallDir = ((int)random(256) < blendT) ? dirCeil : dirFloor;
            int fdx = DIR_DX[fallDir], fdy = DIR_DY[fallDir];
            int fs1 = (fallDir + 1) % 8, fs2 = (fallDir + 7) % 8;

            // 1. Try primary gravity direction
            int nx = x + fdx, ny = y + fdy;
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            // 2. Try diagonal slides (random order)
            int s1 = fs1, s2 = fs2;
            if (random(2)) { s1 = fs2; s2 = fs1; }

            nx = x + DIR_DX[s1]; ny = y + DIR_DY[s1];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            nx = x + DIR_DX[s2]; ny = y + DIR_DY[s2];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val; grid[y][x] = 0;
                drawCell(x, y, 0); drawCell(nx, ny, val);
                continue;
            }

            // 3. Liquid leveling: scan laterally (perpendicular to gravity) and move
            //    to the first spot that has an open fall path below it.
            int p1 = perp1, p2 = perp2;
            if (random(2)) { p1 = perp2; p2 = perp1; }

            bool moved = false;
            for (int pass = 0; pass < 2 && !moved; pass++) {
                int pd = (pass == 0) ? p1 : p2;
                for (int dist = 1; dist <= LIQUID_FLOW && !moved; dist++) {
                    int cx = x + DIR_DX[pd] * dist;
                    int cy = y + DIR_DY[pd] * dist;
                    if (cx < 0 || cx >= GRID_W || cy < 0 || cy >= GRID_H) break;
                    if (grid[cy][cx] != 0) break;
                    // Move only if destination has an open fall path (true lower ground)
                    int bx = cx + fdx, by = cy + fdy;
                    if (bx >= 0 && bx < GRID_W && by >= 0 && by < GRID_H && grid[by][bx] == 0) {
                        grid[cy][cx] = val; grid[y][x] = 0;
                        drawCell(x, y, 0); drawCell(cx, cy, val);
                        moved = true;
                    }
                }
            }
        }
    }

    tft.endWrite();
}

// --- Main ---
void setup() {
    Serial.begin(115200);
    randomSeed(analogRead(34));
    memset(grid, 0, sizeof(grid));

    setupDisplay();
    setupTouch();
    setupMPU();
    markButtonZone();
    drawMenuButton();
}

void loop() {
    readAccelerometer();
    updateGravityDir();
    handleTouch();
    simulateSand();
}
