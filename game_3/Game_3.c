#include "Game_3.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "Buzzer.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include "Joystick.h"
#include "rng.h"
#include "exit.h"

extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;  // Buzzer control
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern RNG_HandleTypeDef hrng;

/**
 * @brief Game 2 Implementation - Student can modify
 * 
 * EXAMPLE: Shows how to use the Buzzer for sound effects
 * This is a placeholder with a bouncing animation.
 * Replace this with your actual game logic!
 */


// Frame rate for this game (in milliseconds) - runs slower than Game 1
#define GAME2_FRAME_TIME_MS 50  // ~20 FPS (different from Game 1!)
#define ENEMY_COUNT 3
#define PLAYER_Y 210
#define PLAYER_SPEED 5
#define ENEMY_FALL_SPEED 4
#define TREASURE_FALL_SPEED 3
#define MYSTERY_FALL_SPEED 3
#define INITIAL_LIVES 3
#define MAX_LIVES 5
#define SLOW_DOWN_FRAMES 60
#define EFFECT_MESSAGE_FRAMES 40

// Game state - customize for your game
typedef enum {
    GAME2_STATE_START = 0,
    GAME2_STATE_PLAYING,
    GAME2_STATE_PAUSE,
    GAME2_STATE_GAME_OVER
} Game2State;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t size;
} Player;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t size;
    int16_t vx;
    int16_t vy;
} Enemy;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t size;
    int16_t vy;
} Treasure;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t size;
    int16_t vy;
} MysteryItem;


static Game2State game_state = GAME2_STATE_START;
static Player player;
static Enemy enemies[ENEMY_COUNT];
static Treasure treasure;
static MysteryItem mystery_item;
static uint32_t score = 0;
static uint32_t animation_counter = 0;
static int lives = INITIAL_LIVES;
static int slow_down_timer = 0;
static char effect_message[24] = "";
static int effect_message_timer = 0;


static int RandomRange(int min, int max) {
    uint32_t r = 0;
    HAL_RNG_GenerateRandomNumber(&hrng, &r);
    return min + (r % (max - min + 1));
}

static void PlayShortBeep(uint32_t freq_hz, uint8_t volume, uint32_t duration_ms) {
    buzzer_tone(&buzzer_cfg, freq_hz, volume);
    HAL_Delay(duration_ms);
    buzzer_off(&buzzer_cfg);
}

static void RespawnTreasure(void) {
    treasure.x = RandomRange(10, 220);
    treasure.y = 35;
    treasure.size = 10;
    treasure.vy = TREASURE_FALL_SPEED;
}

static void RespawnEnemy(int index) {
    enemies[index].x = RandomRange(10, 220);
    enemies[index].y = RandomRange(-120, 20);
    enemies[index].size = 12;
    enemies[index].vx = 0;
    enemies[index].vy = ENEMY_FALL_SPEED + (index % 2);
}

static void RespawnMysteryItem(void) {
    mystery_item.x = RandomRange(10, 220);
    mystery_item.y = RandomRange(-180, -60);
    mystery_item.size = 14;
    mystery_item.vy = MYSTERY_FALL_SPEED;
}


static void ResetGameState(void) {
    int i;

    player.x = 110;
    player.y = PLAYER_Y;
    player.size = 16;

    for (i = 0; i < ENEMY_COUNT; i++) {
        RespawnEnemy(i);
    }

    RespawnTreasure();
    RespawnMysteryItem();

    score = 0;
    lives = INITIAL_LIVES;
    slow_down_timer = 0;
    effect_message_timer = 0;
    effect_message[0] = '\0';
    animation_counter = 0;
}

static uint8_t IsOverlap(int ax, int ay, int as, int bx, int by, int bs) {
    if (ax + as < bx) return 0;
    if (bx + bs < ax) return 0;
    if (ay + as < by) return 0;
    if (by + bs < ay) return 0;
    return 1;
}

static void UpdatePlayer(void) {
    Joystick_Read(&joystick_cfg, &joystick_data);
    UserInput joy = Joystick_GetInput(&joystick_data);

    switch (joy.direction) {
        case E:
        case NE:
        case SE:
            player.x += PLAYER_SPEED;
            break;

        case W:
        case NW:
        case SW:
            player.x -= PLAYER_SPEED;
            break;

        default:
            break;
    }

    if (player.x < 0) player.x = 0;
    if (player.x > 228) player.x = 228;

    // Keep player fixed at the bottom
    player.y = PLAYER_Y;
}

static void UpdateTreasure(void) {
    treasure.y += treasure.vy;

    if (treasure.y > 240) {
        RespawnTreasure();
    }
}

static void UpdateEnemies(void) {
    int i;
    int extra_speed = score / 5;
    int speed_modifier = 0;

    if (slow_down_timer > 0) {
        speed_modifier = -2;
        slow_down_timer--;
    }

    for (i = 0; i < ENEMY_COUNT; i++) {
        int fall_speed = enemies[i].vy + extra_speed + speed_modifier;

        if (fall_speed < 1) {
            fall_speed = 1;
        }

        enemies[i].y += fall_speed;

        if (enemies[i].y > 240) {
            RespawnEnemy(i);
        }
    }
}

static void UpdateMysteryItem(void) {
    mystery_item.y += mystery_item.vy;

    if (mystery_item.y > 240) {
        RespawnMysteryItem();
    }
}

static void ApplyMysteryEffect(void) {
    int effect = RandomRange(0, 2);   // 0, 1, 2

    switch (effect) {
        case 0:   // +1 life
            if (lives < MAX_LIVES) {
                lives++;
            }
            sprintf(effect_message, "+1 LIFE");
            effect_message_timer = EFFECT_MESSAGE_FRAMES;
            PlayShortBeep(1600, 30, 20);
            break;

        case 1:   // -1 life
            lives--;
            sprintf(effect_message, "-1 LIFE");
            effect_message_timer = EFFECT_MESSAGE_FRAMES;
            PlayShortBeep(500, 35, 40);

            if (lives <= 0) {
                lives = 0;
                game_state = GAME2_STATE_GAME_OVER;
            }
            break;

        case 2:   // slow down enemies
            slow_down_timer = SLOW_DOWN_FRAMES;
            sprintf(effect_message, "SLOW DOWN");
            effect_message_timer = EFFECT_MESSAGE_FRAMES;
            PlayShortBeep(1200, 30, 30);
            break;

        default:
            break;
    }
}

static void UpdatePlayingLogic(void) {
    int i;

    UpdatePlayer();
    UpdateEnemies();
    UpdateTreasure();
    UpdateMysteryItem();
    animation_counter++;

    if (effect_message_timer > 0) {
        effect_message_timer--;
    }

    // collect treasure
    if (IsOverlap(player.x, player.y, player.size,
                  treasure.x, treasure.y, treasure.size)) {
        score++;
        RespawnTreasure();
        PlayShortBeep(1500, 30, 20);
    }

    // collect mystery item
    if (IsOverlap(player.x, player.y, player.size,
                  mystery_item.x, mystery_item.y, mystery_item.size)) {
        RespawnMysteryItem();
        ApplyMysteryEffect();

        if (game_state == GAME2_STATE_GAME_OVER) {
            return;
        }
    }

    // hit enemies
    for (i = 0; i < ENEMY_COUNT; i++) {
        if (IsOverlap(player.x, player.y, player.size,
                      enemies[i].x, enemies[i].y, enemies[i].size)) {
            lives--;
            RespawnEnemy(i);
            PlayShortBeep(400, 35, 50);

            if (lives <= 0) {
                lives = 0;
                game_state = GAME2_STATE_GAME_OVER;
            }
            return;
        }
    }
}




static void DrawPlayerSprite(int16_t x, int16_t y) {
    LCD_printString("^_^", x, y, 1, 2);   // player
}

static void DrawEnemySprite(int16_t x, int16_t y) {
    LCD_printString("[X]", x, y, 2, 2);    // obstacle / enemy
}

static void DrawTreasureSprite(int16_t x, int16_t y) {
    LCD_printString("$", x, y, 6, 3);
}

static void DrawMysterySprite(int16_t x, int16_t y) {
    LCD_printString("?", x, y, 5, 3);
}


static void RenderStartScreen(void) {
    LCD_Fill_Buffer(0);
    LCD_printString("TREASURE ESCAPE", 20, 25, 1, 2);
    LCD_printString("Move left / right", 30, 80, 1, 1);
    LCD_printString("Catch treasure", 48, 100, 1, 1);
    LCD_printString("Avoid obstacles", 40, 120, 1, 1);
    LCD_printString("Catch ? for random effect", 8, 135, 1, 1);
    LCD_printString("BT3: Start", 60, 170, 1, 1);
    LCD_printString("BT2: Menu", 60, 190, 1, 1);
    LCD_Refresh(&cfg0);
}

static void RenderPlayingScreen(void) {
    char text[32];
    int i;

    LCD_Fill_Buffer(0);

    LCD_printString("GAME 2", 75, 5, 1, 2);

    sprintf(text, "Score: %lu", score);
    LCD_printString(text, 10, 20, 1, 1);

    sprintf(text, "Lives: %d", lives);
    LCD_printString(text, 150, 20, 1, 1);

    if (effect_message_timer > 0) {
        LCD_printString(effect_message, 80, 40, 5, 2);
    }

    DrawPlayerSprite(player.x, player.y);
    DrawTreasureSprite(treasure.x, treasure.y);
    DrawMysterySprite(mystery_item.x, mystery_item.y);

    for (i = 0; i < ENEMY_COUNT; i++) {
        DrawEnemySprite(enemies[i].x, enemies[i].y);
    }

    LCD_printString("BT2 Pause", 10, 225, 1, 1);
    LCD_printString("BT3 Menu", 145, 225, 1, 1);

    LCD_Refresh(&cfg0);
}


static void RenderPauseScreen(void) {
    LCD_Fill_Buffer(0);
    LCD_printString("PAUSED", 70, 70, 1, 3);
    LCD_printString("BT2: Resume", 55, 130, 1, 1);
    LCD_printString("BT3: Menu", 60, 150, 1, 1);
    LCD_Refresh(&cfg0);
}

static void RenderGameOverScreen(void) {
    char text[32];

    LCD_Fill_Buffer(0);
    LCD_printString("GAME OVER", 45, 50, 1, 3);

    sprintf(text, "Final Score: %lu", score);
    LCD_printString(text, 45, 110, 1, 1);

    LCD_printString("BT3: Restart", 50, 155, 1, 1);
    LCD_printString("BT2: Menu", 60, 175, 1, 1);
    LCD_Refresh(&cfg0);
}




MenuState Game3_Run(void) {
    // Initialize game state
    animation_counter = 0;
    game_state = GAME2_STATE_START;
    ResetGameState();
    
    // Play a brief startup sound
    buzzer_tone(&buzzer_cfg, 1200, 30);  // 1.2kHz at 30% volume
    HAL_Delay(50);  // Brief beep duration
    buzzer_off(&buzzer_cfg);  // Stop the buzzer
    
    MenuState exit_state = MENU_STATE_HOME;  // Default: return to menu
    
    // Game's own loop - runs until exit condition
    while (1) {
        uint32_t frame_start = HAL_GetTick();
        
        // Read input
        Input_Read();
        
        // Check if button was pressed to return to menu
        if (game_state == GAME2_STATE_START && current_input.btn2_pressed) {
            exit_state = MENU_STATE_HOME;
            break;  // Exit game loop
        }

        if (game_state == GAME2_STATE_PLAYING && Check_BT3_Exit(0)) {
            exit_state = MENU_STATE_HOME;
            break;
        }

        if (game_state == GAME2_STATE_PAUSE && Check_BT3_Exit(0)) {
            exit_state = MENU_STATE_HOME;
            break;
        }

        if (game_state == GAME2_STATE_GAME_OVER && current_input.btn2_pressed) {
            exit_state = MENU_STATE_HOME;
            break;  // Exit game loop
        }
        
        // UPDATE: Game logic
        switch (game_state) {
            case GAME2_STATE_START:
                if (current_input.btn3_pressed) {
                    ResetGameState();
                    game_state = GAME2_STATE_PLAYING;
                    PlayShortBeep(1000, 30, 30);
                }
                break;

            case GAME2_STATE_PLAYING:
                if (current_input.btn2_pressed) {
                    game_state = GAME2_STATE_PAUSE;
                } else {
                    UpdatePlayingLogic();
                }
                break;

            case GAME2_STATE_PAUSE:
                if (current_input.btn2_pressed) {
                    game_state = GAME2_STATE_PLAYING;
                }
                break;

            case GAME2_STATE_GAME_OVER:
                if (current_input.btn3_pressed) {
                    ResetGameState();
                    game_state = GAME2_STATE_PLAYING;
                    PlayShortBeep(900, 30, 30);
                }
                break;

            default:
                exit_state = MENU_STATE_HOME;
                break;
        }
        
        // RENDER: Draw to LCD
        switch (game_state) {
            case GAME2_STATE_START:
                RenderStartScreen();
                break;

            case GAME2_STATE_PLAYING:
                RenderPlayingScreen();
                break;

            case GAME2_STATE_PAUSE:
                RenderPauseScreen();
                break;

            case GAME2_STATE_GAME_OVER:
                RenderGameOverScreen();
                break;

            default:
                RenderStartScreen();
                break;
        }
        
        // Frame timing - wait for remainder of frame time
        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME2_FRAME_TIME_MS) {
            HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
        }
    }
    
    return exit_state;  // Tell main where to go next
}