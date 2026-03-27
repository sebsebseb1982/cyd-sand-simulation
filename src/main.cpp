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
#define BRUSH_RADIUS 3

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

bool mpuAvailable = false;

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
    uint16_t color = (val == 0) ? BG_COLOR : SAND_COLORS[val - 1];
    tft.fillRect(gx * CELL_SIZE, gy * CELL_SIZE, CELL_SIZE, CELL_SIZE, color);
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
    if (gravMag < GRAVITY_DEADZONE) {
        // Screen is flat: keep current direction but sand won't move (handled in simulateSand)
        return;
    }
    float angle = atan2f(gravX, gravY);
    int dir = (int)roundf(angle / (M_PI / 4.0f));
    primaryDir = ((dir % 8) + 8) % 8;
}

// --- Touch input ---
void handleTouch() {
    if (!ts.touched()) return;

    TS_Point p = ts.getPoint();

    int sx = map(p.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, SCREEN_W - 1);
    int sy = map(p.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, SCREEN_H - 1);
    sx = constrain(sx, 0, SCREEN_W - 1);
    sy = constrain(sy, 0, SCREEN_H - 1);

    int gx = sx / CELL_SIZE;
    int gy = sy / CELL_SIZE;

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
}

// --- Sand physics ---
void simulateSand() {
    int dx_p = DIR_DX[primaryDir];
    int dy_p = DIR_DY[primaryDir];
    int slide1 = (primaryDir + 1) % 8;
    int slide2 = (primaryDir + 7) % 8;

    // Process grains opposite to gravity direction
    int yStart, yEnd, yStep;
    if (dy_p >= 0) { yStart = GRID_H - 1; yEnd = -1; yStep = -1; }
    else            { yStart = 0; yEnd = GRID_H; yStep = 1; }

    int xStart, xEnd, xStep;
    if (dx_p > 0)       { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    else if (dx_p < 0)  { xStart = 0; xEnd = GRID_W; xStep = 1; }
    else {
        // Alternate scan direction for symmetry
        if (random(2)) { xStart = 0; xEnd = GRID_W; xStep = 1; }
        else           { xStart = GRID_W - 1; xEnd = -1; xStep = -1; }
    }

    // Friction: probability of a grain moving this frame (0-100)
    // Below deadzone: frozen. Between deadzone and full: gradual. Above full: always moves.
    int moveChance;
    if (gravMag < GRAVITY_DEADZONE) {
        return; // fully frozen, skip simulation entirely
    } else if (gravMag >= GRAVITY_FULL) {
        moveChance = 100;
    } else {
        float t = (gravMag - GRAVITY_DEADZONE) / (GRAVITY_FULL - GRAVITY_DEADZONE);
        moveChance = (int)(t * t * 100.0f); // quadratic ramp for natural feel
    }

    tft.startWrite();

    for (int y = yStart; y != yEnd; y += yStep) {
        for (int x = xStart; x != xEnd; x += xStep) {
            uint8_t val = grid[y][x];
            if (val == 0) continue;
            if (moveChance < 100 && (int)random(100) >= moveChance) continue;

            // Try primary gravity direction
            int nx = x + dx_p;
            int ny = y + dy_p;
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val;
                grid[y][x] = 0;
                drawCell(x, y, 0);
                drawCell(nx, ny, val);
                continue;
            }

            // Try diagonal slides (random order for natural look)
            int s1 = slide1, s2 = slide2;
            if (random(2)) { s1 = slide2; s2 = slide1; }

            nx = x + DIR_DX[s1];
            ny = y + DIR_DY[s1];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val;
                grid[y][x] = 0;
                drawCell(x, y, 0);
                drawCell(nx, ny, val);
                continue;
            }

            nx = x + DIR_DX[s2];
            ny = y + DIR_DY[s2];
            if (nx >= 0 && nx < GRID_W && ny >= 0 && ny < GRID_H && grid[ny][nx] == 0) {
                grid[ny][nx] = val;
                grid[y][x] = 0;
                drawCell(x, y, 0);
                drawCell(nx, ny, val);
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
}

void loop() {
    readAccelerometer();
    updateGravityDir();
    handleTouch();
    simulateSand();
}
