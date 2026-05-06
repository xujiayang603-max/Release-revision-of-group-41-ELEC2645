#include "Game_1.h"
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
#include <math.h>

extern ST7789V2_cfg_t cfg0;
extern PWM_cfg_t pwm_cfg;
extern Buzzer_cfg_t buzzer_cfg;

extern Joystick_cfg_t joystick_cfg;
extern Joystick_t joystick_data;

#define GAME1_FRAME_TIME_MS 30

MenuState Game1_Run(void)
{

    //Cursor

    int cursor_x = 120;
    int cursor_y = 120;
    int MOVE_SPEED = 3;

    //Fish

    int fish_x = rand() % 160 + 20;
    int fish_y = rand() % 120 + 40;

    int fish_dx = 1;
    int fish_dy = 1;

    //Score

    int score = 0;

    //Progress bar

    int bar_x = 60;
    int bar_width = 120;

    int catch_width = 30;
    int catch_x = bar_x + (bar_width - catch_width)/2;

    int marker_x = 60;
    int marker_speed = 4;

    //Game state

    int game_state = 0;

    MenuState exit_state = MENU_STATE_HOME;

    buzzer_tone(&buzzer_cfg, 1200, 40);
    HAL_Delay(80);
    buzzer_off(&buzzer_cfg);

    Input_Read();

    while (1)
    {

        uint32_t frame_start = HAL_GetTick();

        Input_Read();

        if (Check_BT3_Exit(game_state) && game_state == 0) 
        {
            exit_state = MENU_STATE_HOME;
            break;
        }

        Joystick_Read(&joystick_cfg, &joystick_data);
        UserInput input = Joystick_GetInput(&joystick_data);

        switch (input.direction)
        {
            case N:  cursor_y -= MOVE_SPEED; break;
            case S:  cursor_y += MOVE_SPEED; break;
            case E:  cursor_x += MOVE_SPEED; break;
            case W:  cursor_x -= MOVE_SPEED; break;
            case NE: cursor_x += MOVE_SPEED; cursor_y -= MOVE_SPEED; break;
            case NW: cursor_x -= MOVE_SPEED; cursor_y -= MOVE_SPEED; break;
            case SE: cursor_x += MOVE_SPEED; cursor_y += MOVE_SPEED; break;
            case SW: cursor_x -= MOVE_SPEED; cursor_y += MOVE_SPEED; break;

            default: break;
        }

        if (cursor_x < 50) cursor_x = 50;
        if (cursor_x > 190) cursor_x = 190;
        if (cursor_y < 70) cursor_y = 70;
        if (cursor_y > 170) cursor_y = 170;

        //STATE 0 FIND FISH

        if (game_state == 0)
        {

            fish_x += fish_dx;
            fish_y += fish_dy;

            if (fish_x < 50) {
                fish_x = 50;
                fish_dx = -fish_dx;
            }
            if (fish_x > 190) {
                fish_x = 190;
                fish_dx = -fish_dx;
            }

            if (fish_y < 70) {
                fish_y = 70;
                fish_dy = -fish_dy;
            }
            if (fish_y > 170) {
                fish_y = 170;
                fish_dy = -fish_dy;
            }

            if (abs(cursor_x - fish_x) < 12 && abs(cursor_y - fish_y) < 12)
            {

                game_state = 1;

                marker_x = bar_x;
                marker_speed = 4;

                buzzer_tone(&buzzer_cfg, 1800, 40);
                HAL_Delay(50);
                buzzer_off(&buzzer_cfg);
            }
        }

        //STATE 1 FISHING

    if (game_state == 1){
        if (current_input.btn3_pressed)
        {

            if (marker_x >= catch_x && marker_x <= catch_x + catch_width)
            {

                score++;

                buzzer_tone(&buzzer_cfg, 2200, 80);
                HAL_Delay(120);
                buzzer_off(&buzzer_cfg);

                fish_x = rand() % 140 + 20;
                fish_y = rand() % 100 + 40;
            }

            game_state = 0;
        }

        marker_x += marker_speed;

        if (marker_x < 20 || marker_x > 220)
            marker_speed = -marker_speed;
    }

        LCD_Fill_Buffer(0);

        //SCORE
        char score_text[20];
        sprintf(score_text, "Score:%d", score);
        LCD_printString(score_text, 5, 5, 1, 1);

        //CAT
        LCD_printString(" /\\_/\\", 5, 25, 1, 1);
        LCD_printString("= >.< =", 5, 35, 1, 1);
        LCD_printString(" \\ m / ", 5, 45, 1, 1);

        LCD_printString("Fishing Game", 70, 10, 1, 2);

        if (game_state == 0)
        {
            LCD_Draw_Rect(40, 60, 160, 120, 14, 1);
            if (fish_dx >= 0) {
                LCD_printString("><>", fish_x, fish_y, 0, 2);
            } else {
                LCD_printString("<><", fish_x, fish_y, 0, 2);
            }
            LCD_printString("|>", cursor_x, cursor_y, 0, 2);
            LCD_printString("BT3: Exit", 80, 225, 1, 1);
        }

        if (game_state == 1)
        {

            LCD_printString("Press BT3!", 80, 80, 1, 2);

            LCD_printString("[            ]", bar_x, 200, 1, 2);
            LCD_printString("###", catch_x, 200, 1, 2);
            LCD_printString("^", marker_x, 185, 1, 2);
            LCD_printString("BT3: Catch", 80, 225, 1, 1);
        }

        LCD_Refresh(&cfg0);

        uint32_t frame_time = HAL_GetTick() - frame_start;

        if (frame_time < GAME1_FRAME_TIME_MS)
        {
            HAL_Delay(GAME1_FRAME_TIME_MS - frame_time);
        }
    }

    return exit_state;
}