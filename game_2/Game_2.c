#include "Game_2.h"
#include "InputHandler.h"
#include "Menu.h"
#include "LCD.h"
#include "PWM.h"
#include "Buzzer.h"
#include "Joystick.h"
#include "exit.h"
#include "stm32l4xx_hal.h"
#include <stdio.h>
#include <stdlib.h>

// External hardware configurations
extern ST7789V2_cfg_t cfg0;
extern Buzzer_cfg_t buzzer_cfg;

// Joystick input data
extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;
extern InputState current_input;

// Frame update time in milliseconds
#define GAME2_FRAME_TIME_MS 30

// Platform configuration values
#define MAX_PLATFORMS 8
#define PLATFORM_MIN_W 30
#define PLATFORM_MAX_W 50
#define PLATFORM_GAP 30
#define MAX_DYNAMIC_GAP 55   // Maximum difficulty limit

// Coin configuration values
#define MAX_COINS 5

// Spring configuration values
#define MAX_SPRINGS 4

MenuState Game2_Run(void)
{
    int player_x;
    int player_y;
    int velocity_y = 0;

    int plat_x[MAX_PLATFORMS];
    int plat_y[MAX_PLATFORMS];
    int plat_w[MAX_PLATFORMS];

    int plat_dx[MAX_PLATFORMS];
    int plat_move[MAX_PLATFORMS];
    int move_counter = 0;

    int coin_x[MAX_COINS];
    int coin_y[MAX_COINS];
    int coin_active[MAX_COINS];
    int coin_owner[MAX_COINS];

    int spring_x[MAX_SPRINGS];
    int spring_y[MAX_SPRINGS];
    int spring_active[MAX_SPRINGS];

    int coin_counter = 0;
    int spring_counter = 0;

    // Initialise platforms
    for (int i = 0; i < MAX_PLATFORMS; i++) {
        plat_x[i] = rand() % 180;
        plat_y[i] = 220 - i * PLATFORM_GAP;
        plat_w[i] = PLATFORM_MIN_W + rand() % (PLATFORM_MAX_W - PLATFORM_MIN_W);

        plat_dx[i] = (rand() % 2 == 0) ? 1 : -1;
        plat_move[i] = 0;
    }

    // Starting platform
    plat_y[0] = 220;
    plat_x[0] = 90;
    plat_w[0] = 60;

    player_x = plat_x[0] + plat_w[0] / 2;
    player_y = plat_y[0] - 32;

    int score = 0;
    int safe_time = 30;
    int first_jump = 1;
    int game_over = 0;

    // Initialise coins
    for (int i = 0; i < MAX_COINS; i++) {
        coin_active[i] = 0;
        coin_owner[i] = -1;
    }

    // Initialise springs
    for (int i = 0; i < MAX_SPRINGS; i++) {
        spring_active[i] = 0;
    }

    while (1)
    {
        uint32_t frame_start = HAL_GetTick();
        Input_Read();

        // Dynamic difficulty based on score
        int dynamic_gap = PLATFORM_GAP + score / 200;
        if (dynamic_gap > MAX_DYNAMIC_GAP)
            dynamic_gap = MAX_DYNAMIC_GAP;

        // Game over screen
        if (game_over)
        {
            LCD_Fill_Buffer(0);

            LCD_printString("You lose the game!", 15, 100, 1, 2);

            char buf[30];
            sprintf(buf, "Score:%d", score);
            LCD_printString(buf, 65, 130, 1, 2);

            LCD_printString("Press BT3 to Exit", 55, 180, 1, 1);

            LCD_Refresh(&cfg0);

            if (Check_BT3_Exit(0))
                return MENU_STATE_HOME;

            continue;
        }

        if (Check_BT3_Exit(0))
            return MENU_STATE_HOME;

        // Read joystick
        Joystick_Read(&joystick_cfg, &joystick_data);
        UserInput input = Joystick_GetInput(&joystick_data);

        // Move player horizontally
        switch (input.direction)
        {
            case E: case NE: case SE: player_x += 8; break;
            case W: case NW: case SW: player_x -= 8; break;
            default: break;
        }

        // Wrap player across screen
        if (player_x < 0) player_x = 224;
        if (player_x > 224) player_x = 0;

        // Initial jump
        if (first_jump) {
            velocity_y = -15;
            first_jump = 0;
        }

        // Apply gravity
        velocity_y += 2;
        player_y += velocity_y;

        // Move platforms
        for (int i = 0; i < MAX_PLATFORMS; i++)
        {
            if (plat_move[i])
            {
                plat_x[i] += plat_dx[i];

                if (plat_x[i] <= 0 || plat_x[i] + plat_w[i] >= 240)
                {
                    plat_dx[i] = -plat_dx[i];
                }
            }
        }

        // Move coins with platforms
        for (int i = 0; i < MAX_COINS; i++)
        {
            if (coin_active[i])
            {
                int p = coin_owner[i];
                if (p >= 0 && plat_move[p])
                {
                    coin_x[i] += plat_dx[p];
                }
            }
        }

        // Collision detection
        if (velocity_y > 0)
        {
            int foot_prev = player_y + 32 - velocity_y;
            int foot_now  = player_y + 32;

            for (int i = 0; i < MAX_PLATFORMS; i++)
            {
                if (foot_prev <= plat_y[i] && foot_now >= plat_y[i])
                {
                    if (player_x + 16 >= plat_x[i] &&
                        player_x <= plat_x[i] + plat_w[i])
                    {
                        int spring_hit = 0;

                        for (int s = 0; s < MAX_SPRINGS; s++)
                        {
                            if (spring_active[s])
                            {
                                if (abs(plat_y[i] - spring_y[s]) < 3)
                                {
                                    if (player_x + 16 >= spring_x[s] &&
                                        player_x <= spring_x[s] + 16)
                                    {
                                        velocity_y = -44;
                                        spring_hit = 1;
                                        break;
                                    }
                                }
                            }
                        }

                        if (!spring_hit)
                        {
                            velocity_y = -22;

                            if (plat_move[i])
                            {
                                player_x += plat_dx[i];
                            }
                        }

                        break;
                    }
                }
            }
        }

        // Scroll screen
        if (player_y < 120)
        {
            int diff = 120 - player_y;
            player_y = 120;

            for (int i = 0; i < MAX_PLATFORMS; i++)
                plat_y[i] += diff;

            for (int i = 0; i < MAX_COINS; i++)
                if (coin_active[i]) coin_y[i] += diff;

            for (int i = 0; i < MAX_SPRINGS; i++)
                if (spring_active[i]) spring_y[i] += diff;

            score += diff;
        }

        // Platform regeneration with dynamic gap
        for (int i = 0; i < MAX_PLATFORMS; i++)
        {
            if (plat_y[i] > 240)
            {
                int highest = plat_y[0];
                for (int j = 1; j < MAX_PLATFORMS; j++)
                    if (plat_y[j] < highest) highest = plat_y[j];

                plat_y[i] = highest - dynamic_gap;

                plat_x[i] = rand() % 180;
                plat_w[i] = PLATFORM_MIN_W + rand() % (PLATFORM_MAX_W - PLATFORM_MIN_W);
                plat_dx[i] = (rand() % 2 == 0) ? 1 : -1;

                move_counter++;
                plat_move[i] = (move_counter >= 12);
                if (plat_move[i]) move_counter = 0;

                coin_counter++;
                spring_counter++;

                // Coins
                if (coin_counter >= 7)
                {
                    coin_counter = 0;
                    int k = rand() % MAX_COINS;

                    coin_active[k] = 1;
                    coin_x[k] = plat_x[i] + plat_w[i] / 2;
                    coin_y[k] = plat_y[i] - 10;
                    coin_owner[k] = i;
                }

                // Springs
                if (spring_counter >= 8 && plat_move[i] == 0)
                {
                    spring_counter = 0;
                    int s = rand() % MAX_SPRINGS;

                    spring_active[s] = 1;
                    spring_x[s] = plat_x[i] + plat_w[i] / 2;
                    spring_y[s] = plat_y[i] - 2;
                }
            }
        }

        // Coin collision
        for (int i = 0; i < MAX_COINS; i++)
        {
            if (coin_active[i])
            {
                if (abs(player_x - coin_x[i]) < 16 &&
                    abs(player_y - coin_y[i]) < 20)
                {
                    coin_active[i] = 0;
                    coin_owner[i] = -1;
                    score += 50;
                }
            }
        }

        if (safe_time > 0) safe_time--;

        if (player_y > 240 && safe_time == 0)
        {
            game_over = 1;
            buzzer_tone(&buzzer_cfg, 400, 200);
            HAL_Delay(200);
            buzzer_off(&buzzer_cfg);
        }

        // Render
        LCD_Fill_Buffer(0);

        char buf[20];
        sprintf(buf, "Score:%d", score);
        LCD_printString(buf, 5, 5, 1, 1);

        for (int i = 0; i < MAX_PLATFORMS; i++)
            for (int w = 0; w < plat_w[i]; w += 6)
                LCD_printString("=", plat_x[i] + w, plat_y[i], 1, 1);

        for (int i = 0; i < MAX_COINS; i++)
            if (coin_active[i])
                LCD_printString("($)", coin_x[i]-6, coin_y[i], 6, 1);

        for (int i = 0; i < MAX_SPRINGS; i++)
            if (spring_active[i])
                LCD_printString("S", spring_x[i], spring_y[i], 2, 1);

        LCD_printString("o", player_x, player_y, 2, 2);
        LCD_printString("w", player_x, player_y + 16, 2, 2);

        LCD_Refresh(&cfg0);

        uint32_t frame_time = HAL_GetTick() - frame_start;
        if (frame_time < GAME2_FRAME_TIME_MS)
            HAL_Delay(GAME2_FRAME_TIME_MS - frame_time);
    }
}