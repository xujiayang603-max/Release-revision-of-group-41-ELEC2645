#include "exit.h"
#include "InputHandler.h"
#include <stdint.h>

uint8_t Check_BT3_Exit(uint8_t game_state)
{
    if (current_input.btn3_pressed ==1)
    {
        return 1;
    }
    return 0;
}