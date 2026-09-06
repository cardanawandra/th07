#pragma once
#include "Connection.hpp"

#include "inttypes.hpp"

// netplay stub for 2p
#ifdef TWO_PLAYER
    #define TH_BUTTON_SHOOT3
    #define TH_BUTTON_BOMB3
    #define TH_BUTTON_FOCUS3
    #define TH_BUTTON_UP3
    #define TH_BUTTON_DOWN3
    #define TH_BUTTON_LEFT3
    #define TH_BUTTON_RIGHT3
#endif

enum TouhouButton
{
    TH_BUTTON_MENU = 1 << 3,

    // core start
    TH_BUTTON_SHOOT = 1 << 0,
    TH_BUTTON_BOMB = 1 << 1,
    TH_BUTTON_FOCUS = 1 << 2,
    TH_BUTTON_UP = 1 << 4,
    TH_BUTTON_DOWN = 1 << 5,
    TH_BUTTON_LEFT = 1 << 6,
    TH_BUTTON_RIGHT = 1 << 7,
    TH_BUTTON_UP_LEFT = TH_BUTTON_UP | TH_BUTTON_LEFT,
    TH_BUTTON_UP_RIGHT = TH_BUTTON_UP | TH_BUTTON_RIGHT,
    TH_BUTTON_DOWN_LEFT = TH_BUTTON_DOWN | TH_BUTTON_LEFT,
    TH_BUTTON_DOWN_RIGHT = TH_BUTTON_DOWN | TH_BUTTON_RIGHT,
    TH_BUTTON_DIRECTION =
        TH_BUTTON_DOWN | TH_BUTTON_RIGHT | TH_BUTTON_UP | TH_BUTTON_LEFT,
    // core end

    TH_BUTTON_SKIP = 1 << 8,
    TH_BUTTON_Q = 1 << 9,
    TH_BUTTON_S = 1 << 10,
    TH_BUTTON_HOME = 1 << 11,
    TH_BUTTON_ENTER = 1 << 12,
    TH_BUTTON_D = 1 << 13, // only used for cheat code
    TH_BUTTON_RESET = 1 << 14,
    // ^ latest are 14, then next 15 and so on for player 2 and 3
    // player 2
    TH_BUTTON_SHOOT2 = 1 << 15,
    TH_BUTTON_BOMB2 = 1 << 16,
    TH_BUTTON_FOCUS2 = 1 << 17,
    TH_BUTTON_UP2 = 1 << 18,
    TH_BUTTON_DOWN2 = 1 << 19,
    TH_BUTTON_LEFT2 = 1 << 20,
    TH_BUTTON_RIGHT2 = 1 << 21,
    TH_BUTTON_UP_LEFT2 = TH_BUTTON_UP2 | TH_BUTTON_LEFT2,
    TH_BUTTON_UP_RIGHT2 = TH_BUTTON_UP2 | TH_BUTTON_RIGHT2,
    TH_BUTTON_DOWN_LEFT2 = TH_BUTTON_DOWN2 | TH_BUTTON_LEFT2,
    TH_BUTTON_DOWN_RIGHT2 = TH_BUTTON_DOWN2 | TH_BUTTON_RIGHT2,
    TH_BUTTON_DIRECTION2 =
        TH_BUTTON_DOWN2 | TH_BUTTON_RIGHT2 | TH_BUTTON_UP2 | TH_BUTTON_LEFT2,

    //player 3
    #ifndef TWO_PLAYER
    TH_BUTTON_SHOOT3 = 1 << 22,
    TH_BUTTON_BOMB3 = 1 << 23,
    TH_BUTTON_FOCUS3 = 1 << 24,
    TH_BUTTON_UP3 = 1 << 25,
    TH_BUTTON_DOWN3 = 1 << 26,
    TH_BUTTON_LEFT3 = 1 << 27,
    TH_BUTTON_RIGHT3 = 1 << 28,
    TH_BUTTON_UP_LEFT3 = TH_BUTTON_UP3 | TH_BUTTON_LEFT3,
    TH_BUTTON_UP_RIGHT3 = TH_BUTTON_UP3 | TH_BUTTON_RIGHT3,
    TH_BUTTON_DOWN_LEFT3 = TH_BUTTON_DOWN3 | TH_BUTTON_LEFT3,
    TH_BUTTON_DOWN_RIGHT3 = TH_BUTTON_DOWN3 | TH_BUTTON_RIGHT3,
    TH_BUTTON_DIRECTION3 =
        TH_BUTTON_DOWN3 | TH_BUTTON_RIGHT3 | TH_BUTTON_UP3 | TH_BUTTON_LEFT3,
    #endif

    TH_BUTTON_SELECTMENU = TH_BUTTON_ENTER | TH_BUTTON_SHOOT,
    TH_BUTTON_RETURNMENU = TH_BUTTON_MENU | TH_BUTTON_BOMB,
    TH_BUTTON_WRONG_CHEATCODE = TH_BUTTON_SHOOT | TH_BUTTON_BOMB |
                                TH_BUTTON_MENU | TH_BUTTON_Q | TH_BUTTON_S |
                                TH_BUTTON_ENTER,
    TH_BUTTON_ANY = 0xFFFFFFFF,
};

#define IS_PRESSED_RAW(key) ((g_CurFrameRawInput & (key)) != 0)
#define IS_PRESSED_GAME(key) ((g_CurFrameGameInput & (key)) != 0)
#define WAS_PRESSED_RAW(key) (IS_PRESSED_RAW(key) && ((g_CurFrameRawInput & (key)) != (g_LastFrameRawInput & (key))))
#define WAS_PRESSED_GAME(key) (IS_PRESSED_GAME(key) && ((g_CurFrameGameInput & (key)) != (g_LastFrameGameInput & (key))))
#define IS_EIGHTH(key) (((g_CurFrameRawInput & (key)) != 0) && (g_IsEighthFrameOfHeldInput != 0))
#define WAS_PRESSED_RAW_AND_IS_EIGHTH(key) (WAS_PRESSED_RAW(key) || IS_EIGHTH(key))

namespace Controller
{
u32 GetControllerInput(u32 buttons);
u8 *GetControllerState();
u32 GetInput();
u32 GetJoystickCaps();
void ResetKeyboard();
u32 SetButtonFromControllerInputs(u32 *outButtons, i16 controllerButtonToTest,
                                  u32 touhouButton, u32 inputButtons);
u32 SetButtonFromDirectInputJoystate(u32 *outButtons,
                                     i16 controllerButtonToTest,
                                     u32 touhouButton, u8 *inputButtons);

// netplay
bool RcvPacks(int playerType);
void SendKeys(int frame, int playerType);
u32 GetInput_Net(int frame, bool is_in_UI, int &cur_ctrl);
u32 GetInput_Single(int &cur_ctrl);

} // namespace Controller