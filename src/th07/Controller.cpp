// netplay
#include "Controller.hpp"

#include <dinput.h>

#include "GameErrorContext.hpp"
#include "Supervisor.hpp"
#include "dsutil.hpp"
#include "i18n.hpp"
#include "inttypes.hpp"
#include "utils.hpp"
// netplay
#include "Rng.hpp"
#include <map>
#define BITS_32 Bits<32>
std::map<int, BITS_32> g_ctrl_bits_self;
std::map<int, BITS_32> g_ctrl_bits_rcved[CONTROL_RECEIVER];
std::map<int, int> g_ctrl_rng_rcved[CONTROL_RECEIVER];
std::map<int, int> g_ctrl_rng_self;
std::map<int, InGameCtrlType> g_ctrl_rcved[CONTROL_RECEIVER];
std::map<int, InGameCtrlType> g_ctrl_self;

extern Host g_host;
extern Guest g_guest;
extern int g_delay;
extern bool g_is_host;
extern int g_playerType;
extern bool g_is_connected;
bool g_is_sync = true;
extern bool g_istry_to_reconnect;
extern bool g_is_single_mode;
extern bool g_resync_trigger;
extern int g_resync_stage_frame;
struct CurKeyStates
{
    bool K_F2; // life
    bool K_F3; // bomb
    bool K_F4; // power
    bool K_R;
    bool K_Q;

    bool K_F6; // insane mode

    bool K_N; // add delay
    bool K_M; // dec delay
} g_cur_ctrl_key_state;
// end netplay

// GLOBAL: TH07 0x0049fc88
static JOYCAPSA g_JoystickCaps;

// GLOBAL: TH07 0x0049fe1c
static u16 g_AutoFocusTimer;

i32 TH_BUTTON_SHOOT_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_SHOOT,TH_BUTTON_SHOOT2,TH_BUTTON_SHOOT3};
i32 TH_BUTTON_BOMB_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_BOMB,TH_BUTTON_BOMB2,TH_BUTTON_BOMB3};
i32 TH_BUTTON_FOCUS_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_FOCUS,TH_BUTTON_FOCUS2,TH_BUTTON_FOCUS3};
i32 TH_BUTTON_UP_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_UP,TH_BUTTON_UP2,TH_BUTTON_UP3};
i32 TH_BUTTON_DOWN_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_DOWN,TH_BUTTON_DOWN2,TH_BUTTON_DOWN3};
i32 TH_BUTTON_LEFT_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_LEFT,TH_BUTTON_LEFT2,TH_BUTTON_LEFT3};
i32 TH_BUTTON_RIGHT_MAP[CONTROL_RECEIVER] = {0,TH_BUTTON_RIGHT,TH_BUTTON_RIGHT2,TH_BUTTON_RIGHT3};

#define KEY_PRESSED(scancode, thButton) \
    ((keyboardState[scancode] & 0x80) != 0 ? thButton : 0)
#define JOYSTICK_MIDPOINT(min, max) ((min + max) / 2)

// FUNCTION: TH07 0x00430290
u32 Controller::GetJoystickCaps()
{
    JOYINFOEX joyinfo;

    joyinfo.dwSize = sizeof(JOYINFOEX);
    joyinfo.dwFlags = 255;
    if (joyGetPosEx(0, &joyinfo))
    {
        g_GameErrorContext.Log(TH_CONTROL_NO_USABLE_PADS);
        return 1;
    }
    joyGetDevCapsA(0, &g_JoystickCaps, 0x194);
    return 0;
}

// FUNCTION: TH07 0x004302f0
u32 Controller::SetButtonFromDirectInputJoystate(u32 *outButtons,
                                                 i16 controllerButtonToTest,
                                                 u32 touhouButton,
                                                 u8 *inputButtons)
{
    if (controllerButtonToTest < 0)
    {
        return 0;
    }
    *outButtons |= (inputButtons[controllerButtonToTest] & 0x80) != 0
                       ? (u32)touhouButton
                       : 0;

    return (inputButtons[controllerButtonToTest] & 0x80) != 0 ? (u32)touhouButton
                                                              : 0;
}

// FUNCTION: TH07 0x00430370
u32 Controller::SetButtonFromControllerInputs(u32 *outButtons,
                                              i16 controllerButtonToTest,
                                              u32 touhouButton,
                                              u32 inputButtons)
{
    u32 mask;

    if (controllerButtonToTest < 0)
    {
        return 0;
    }

    mask = 1 << (i32)controllerButtonToTest;
    *outButtons |= (inputButtons & mask) != 0 ? (u32)touhouButton : 0;
    return (inputButtons & mask) != 0 ? (u32)touhouButton : 0;
}

#pragma var_order(pji, distance, DVar1, DVar2, hr, js, retryCount)
// FUNCTION: TH07 0x004303f0
u32 Controller::GetControllerInput(u32 buttons)
{
    i32 retryCount;
    DIJOYSTATE2 js;
    i32 hr;
    u32 DVar2;
    u32 DVar1;
    u32 distance;
    JOYINFOEX pji;

    if (!g_Supervisor.controller)
    {
        memset(&pji, 0, sizeof(JOYINFOEX));
        pji.dwSize = sizeof(JOYINFOEX);
        pji.dwFlags = 255;
        if (joyGetPosEx(0, &pji))
        {
            return buttons;
        }

        DVar1 = SetButtonFromControllerInputs(
            &buttons, g_Supervisor.cfg.controllerMapping.shootButton,
            TH_BUTTON_SHOOT, pji.dwButtons);
        if (g_Supervisor.cfg.shotSlow)
        {
            if (DVar1 != 0)
            {
                if (g_AutoFocusTimer < 20)
                {
                    g_AutoFocusTimer++;
                }
                if (g_AutoFocusTimer >= 10)
                {
                    buttons |= TH_BUTTON_FOCUS;
                }
            }
            else
            {
                if (g_AutoFocusTimer > 10)
                {
                    g_AutoFocusTimer -= 10;
                    buttons |= TH_BUTTON_FOCUS;
                }
                else
                {
                    g_AutoFocusTimer = 0;
                }
            }
        }
        SetButtonFromControllerInputs(&buttons,
                                      g_Supervisor.cfg.controllerMapping.bombButton,
                                      TH_BUTTON_BOMB, pji.dwButtons);
        SetButtonFromControllerInputs(
            &buttons, g_Supervisor.cfg.controllerMapping.focusButton,
            TH_BUTTON_FOCUS, pji.dwButtons);
        SetButtonFromControllerInputs(&buttons,
                                      g_Supervisor.cfg.controllerMapping.menuButton,
                                      TH_BUTTON_MENU, pji.dwButtons);
        SetButtonFromControllerInputs(&buttons,
                                      g_Supervisor.cfg.controllerMapping.upButton,
                                      TH_BUTTON_UP, pji.dwButtons);
        SetButtonFromControllerInputs(&buttons,
                                      g_Supervisor.cfg.controllerMapping.downButton,
                                      TH_BUTTON_DOWN, pji.dwButtons);
        SetButtonFromControllerInputs(&buttons,
                                      g_Supervisor.cfg.controllerMapping.leftButton,
                                      TH_BUTTON_LEFT, pji.dwButtons);
        SetButtonFromControllerInputs(
            &buttons, g_Supervisor.cfg.controllerMapping.rightButton,
            TH_BUTTON_RIGHT, pji.dwButtons);
        SetButtonFromControllerInputs(&buttons,
                                      g_Supervisor.cfg.controllerMapping.skipButton,
                                      TH_BUTTON_SKIP, pji.dwButtons);
        distance = (g_JoystickCaps.wXmax - g_JoystickCaps.wXmin) / 2 / 2;
        buttons |= JOYSTICK_MIDPOINT(g_JoystickCaps.wXmin, g_JoystickCaps.wXmax) +
                               distance <
                           pji.dwXpos
                       ? TH_BUTTON_RIGHT
                       : 0;
        buttons |= pji.dwXpos < JOYSTICK_MIDPOINT(g_JoystickCaps.wXmin,
                                                  g_JoystickCaps.wXmax) -
                                    distance
                       ? TH_BUTTON_LEFT
                       : 0;
        distance = (g_JoystickCaps.wYmax - g_JoystickCaps.wYmin) / 2 / 2;
        buttons |= JOYSTICK_MIDPOINT(g_JoystickCaps.wYmin, g_JoystickCaps.wYmax) +
                               distance <
                           pji.dwYpos
                       ? TH_BUTTON_DOWN
                       : 0;
        buttons |= pji.dwYpos < JOYSTICK_MIDPOINT(g_JoystickCaps.wYmin,
                                                  g_JoystickCaps.wYmax) -
                                    distance
                       ? TH_BUTTON_UP
                       : 0;
        return buttons;
    }
    else
    {
        if (FAILED(hr = g_Supervisor.controller->Poll()))
        {
            retryCount = 0;
            // STRING: TH07 0x00497d80
            DebugPrint("error : DIERR_INPUTLOST\r\n");
            hr = g_Supervisor.controller->Acquire();
            while (hr == DIERR_INPUTLOST)
            {
                hr = g_Supervisor.controller->Acquire();
                // STRING: TH07 0x00497d60
                DebugPrint("error : DIERR_INPUTLOST %d\r\n", retryCount);
                retryCount++;
                if (retryCount >= 400)
                {
                    return buttons;
                }
            }
            return buttons;
        }
        else
        {
            memset(&js, 0, sizeof(DIJOYSTATE2));
            if (FAILED(hr = g_Supervisor.controller->GetDeviceState(0x110, &js)))
            {
                return buttons;
            }

            DVar2 = SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.shootButton, 1,
                js.rgbButtons);
            if (g_Supervisor.cfg.shotSlow)
            {
                if (DVar2 != 0)
                {
                    if (g_AutoFocusTimer < 20)
                    {
                        g_AutoFocusTimer++;
                    }
                    if (g_AutoFocusTimer >= 10)
                    {
                        buttons |= TH_BUTTON_FOCUS;
                    }
                }
                else
                {
                    if (g_AutoFocusTimer > 10)
                    {
                        g_AutoFocusTimer -= 10;
                        buttons |= TH_BUTTON_FOCUS;
                    }
                    else
                    {
                        g_AutoFocusTimer = 0;
                    }
                }
            }
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.bombButton,
                TH_BUTTON_BOMB, js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.focusButton,
                TH_BUTTON_FOCUS, js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.menuButton,
                TH_BUTTON_MENU, js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.upButton, TH_BUTTON_UP,
                js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.downButton,
                TH_BUTTON_DOWN, js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.leftButton,
                TH_BUTTON_LEFT, js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.rightButton,
                TH_BUTTON_RIGHT, js.rgbButtons);
            SetButtonFromDirectInputJoystate(
                &buttons, g_Supervisor.cfg.controllerMapping.skipButton,
                TH_BUTTON_SKIP, js.rgbButtons);
            SetButtonFromDirectInputJoystate(&buttons, 7, TH_BUTTON_D, js.rgbButtons);
            buttons |= js.lX > g_Supervisor.cfg.padAxisX ? TH_BUTTON_RIGHT : 0;
            buttons |= js.lX < -g_Supervisor.cfg.padAxisX ? TH_BUTTON_LEFT : 0;
            buttons |= js.lY > g_Supervisor.cfg.padAxisY ? TH_BUTTON_DOWN : 0;
            buttons |= js.lY < -g_Supervisor.cfg.padAxisY ? TH_BUTTON_UP : 0;
        }
    }
    return buttons;
}

// GLOBAL: TH07 0x0135e218
static u8 g_ControllerData[32 * 4];

#pragma var_order(joyinfoex, joyButtonBit, joyButtonIndex, hr, dijoystate2, \
                  diRetryCount)
// FUNCTION: TH07 0x004309c0
u8 *Controller::GetControllerState()
{
    HRESULT hr;
    i32 diRetryCount;
    DIJOYSTATE2 dijoystate2;
    u32 joyButtonIndex;
    u32 joyButtonBit;
    JOYINFOEX joyinfoex;

    memset(g_ControllerData, 0, sizeof(g_ControllerData));
    if (!g_Supervisor.controller)
    {
        memset(&joyinfoex, 0, sizeof(JOYINFOEX));
        joyinfoex.dwSize = sizeof(JOYINFOEX);
        joyinfoex.dwFlags = 255;
        if (joyGetPosEx(0, &joyinfoex))
        {
            return g_ControllerData;
        }

        for (joyButtonBit = joyinfoex.dwButtons, joyButtonIndex = 0;
             joyButtonIndex < 32; joyButtonIndex++, joyButtonBit >>= 1)
        {
            if ((joyButtonBit & 1) != 0)
            {
                g_ControllerData[joyButtonIndex] = 0x80;
            }
        }
        return g_ControllerData;
    }
    else
    {
        if (FAILED(hr = g_Supervisor.controller->Poll()))
        {
            diRetryCount = 0;
            DebugPrint("error : DIERR_INPUTLOST\r\n");
            hr = g_Supervisor.controller->Acquire();
            while (hr == DIERR_INPUTLOST)
            {
                hr = g_Supervisor.controller->Acquire();
                diRetryCount++;
                if (diRetryCount >= 400)
                {
                    DebugPrint("error : DIERR_INPUTLOST %d\r\n", diRetryCount);
                    return g_ControllerData;
                }
            }
            return g_ControllerData;
        }
        else
        {
            g_Supervisor.controller->GetDeviceState(sizeof(DIJOYSTATE2),
                                                    &dijoystate2);
            // ZUN landmine: hr holds the result of Poll, not GetDeviceState
            if (FAILED(hr))
            {
                return g_ControllerData;
            }

            memcpy(g_ControllerData, dijoystate2.rgbButtons,
                   sizeof(g_ControllerData));
            return g_ControllerData;
        }
    }
}

// FUNCTION: TH07 0x00430b50
u32 Controller::GetInput()
{
    u8 keyboardState[256];

    // netplay, not sure
    memset(keyboardState,0,sizeof(keyboardState));

    u32 buttons = 0;

    if (!g_Supervisor.keyboard)
    {
        GetKeyboardState(keyboardState);

        buttons |= KEY_PRESSED(VK_UP, TH_BUTTON_UP);
        buttons |= KEY_PRESSED(VK_DOWN, TH_BUTTON_DOWN);
        buttons |= KEY_PRESSED(VK_LEFT, TH_BUTTON_LEFT);
        buttons |= KEY_PRESSED(VK_RIGHT, TH_BUTTON_RIGHT);
        buttons |= KEY_PRESSED(VK_NUMPAD8, TH_BUTTON_UP);
        buttons |= KEY_PRESSED(VK_NUMPAD2, TH_BUTTON_DOWN);
        buttons |= KEY_PRESSED(VK_NUMPAD4, TH_BUTTON_LEFT);
        buttons |= KEY_PRESSED(VK_NUMPAD6, TH_BUTTON_RIGHT);
        buttons |= KEY_PRESSED(VK_NUMPAD7, TH_BUTTON_UP_LEFT);
        buttons |= KEY_PRESSED(VK_NUMPAD9, TH_BUTTON_UP_RIGHT);
        buttons |= KEY_PRESSED(VK_NUMPAD1, TH_BUTTON_DOWN_LEFT);
        buttons |= KEY_PRESSED(VK_NUMPAD3, TH_BUTTON_DOWN_RIGHT);
        buttons |= KEY_PRESSED(VK_HOME, TH_BUTTON_HOME);
        buttons |= KEY_PRESSED('D', TH_BUTTON_D);
        buttons |= KEY_PRESSED('Z', TH_BUTTON_SHOOT);
        buttons |= KEY_PRESSED('X', TH_BUTTON_BOMB);
        buttons |= KEY_PRESSED(VK_SHIFT, TH_BUTTON_FOCUS);
        buttons |= KEY_PRESSED(VK_ESCAPE, TH_BUTTON_MENU);
        buttons |= KEY_PRESSED(VK_CONTROL, TH_BUTTON_SKIP);
        buttons |= KEY_PRESSED('Q', TH_BUTTON_Q);
        buttons |= KEY_PRESSED('S', TH_BUTTON_S);
        buttons |= KEY_PRESSED('R', TH_BUTTON_RESET);
        buttons |= KEY_PRESSED(VK_RETURN, TH_BUTTON_ENTER);

        if(g_is_single_mode){
            #ifdef TWO_PLAYER
            buttons |= KEY_PRESSED('F', TH_BUTTON_SHOOT2);
            buttons |= KEY_PRESSED('G', TH_BUTTON_BOMB2);
            buttons |= KEY_PRESSED('D', TH_BUTTON_FOCUS2);
            buttons |= KEY_PRESSED('I', TH_BUTTON_UP2);
            buttons |= KEY_PRESSED('K', TH_BUTTON_DOWN2);
            buttons |= KEY_PRESSED('J', TH_BUTTON_LEFT2);
            buttons |= KEY_PRESSED('L', TH_BUTTON_RIGHT2);
            #else
            //Player 2
            buttons |= KEY_PRESSED('W', TH_BUTTON_SHOOT2);
            buttons |= KEY_PRESSED('E', TH_BUTTON_BOMB2);
            buttons |= KEY_PRESSED('Q', TH_BUTTON_FOCUS2);
            buttons |= KEY_PRESSED('T', TH_BUTTON_UP2);
            buttons |= KEY_PRESSED('G', TH_BUTTON_DOWN2);
            buttons |= KEY_PRESSED('F', TH_BUTTON_LEFT2);
            buttons |= KEY_PRESSED('H', TH_BUTTON_RIGHT2);

            //Player 3
            buttons |= KEY_PRESSED('V', TH_BUTTON_SHOOT3);
            buttons |= KEY_PRESSED('B', TH_BUTTON_BOMB3);
            buttons |= KEY_PRESSED('C', TH_BUTTON_FOCUS3);
            buttons |= KEY_PRESSED('I', TH_BUTTON_UP3);
            buttons |= KEY_PRESSED('K', TH_BUTTON_DOWN3);
            buttons |= KEY_PRESSED('J', TH_BUTTON_LEFT3);
            buttons |= KEY_PRESSED('L', TH_BUTTON_RIGHT3);
            #endif
        }
    }
    else
    {
        HRESULT hr = g_Supervisor.keyboard->GetDeviceState(sizeof(keyboardState),
                                                           keyboardState);
        buttons = 0;
        if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
        {
            g_Supervisor.keyboard->Acquire();
            return GetControllerInput(buttons);
        }
        buttons |= KEY_PRESSED(DIK_UP, TH_BUTTON_UP);
        buttons |= KEY_PRESSED(DIK_DOWN, TH_BUTTON_DOWN);
        buttons |= KEY_PRESSED(DIK_LEFT, TH_BUTTON_LEFT);
        buttons |= KEY_PRESSED(DIK_RIGHT, TH_BUTTON_RIGHT);
        buttons |= KEY_PRESSED(DIK_NUMPAD8, TH_BUTTON_UP);
        buttons |= KEY_PRESSED(DIK_NUMPAD2, TH_BUTTON_DOWN);
        buttons |= KEY_PRESSED(DIK_NUMPAD4, TH_BUTTON_LEFT);
        buttons |= KEY_PRESSED(DIK_NUMPAD6, TH_BUTTON_RIGHT);
        buttons |= KEY_PRESSED(DIK_NUMPAD7, TH_BUTTON_UP_LEFT);
        buttons |= KEY_PRESSED(DIK_NUMPAD9, TH_BUTTON_UP_RIGHT);
        buttons |= KEY_PRESSED(DIK_NUMPAD1, TH_BUTTON_DOWN_LEFT);
        buttons |= KEY_PRESSED(DIK_NUMPAD3, TH_BUTTON_DOWN_RIGHT);
        buttons |= KEY_PRESSED(DIK_HOME, TH_BUTTON_HOME);
        buttons |= KEY_PRESSED(DIK_D, TH_BUTTON_D);
        buttons |= KEY_PRESSED(DIK_Z, TH_BUTTON_SHOOT);
        buttons |= KEY_PRESSED(DIK_X, TH_BUTTON_BOMB);
        buttons |= KEY_PRESSED(DIK_LSHIFT, TH_BUTTON_FOCUS);
        buttons |= KEY_PRESSED(DIK_RSHIFT, TH_BUTTON_FOCUS);
        buttons |= KEY_PRESSED(DIK_ESCAPE, TH_BUTTON_MENU);
        buttons |= KEY_PRESSED(DIK_LCONTROL, TH_BUTTON_SKIP);
        // buttons |= KEY_PRESSED(DIK_RCONTROL, TH_BUTTON_SKIP);
        buttons |= KEY_PRESSED(DIK_Q, TH_BUTTON_Q);
        buttons |= KEY_PRESSED(DIK_S, TH_BUTTON_S);
        buttons |= KEY_PRESSED(DIK_RETURN, TH_BUTTON_ENTER);
        buttons |= KEY_PRESSED(DIK_R, TH_BUTTON_RESET);

        if(g_is_single_mode){
            #ifdef TWO_PLAYER
            buttons |= KEY_PRESSED(DIK_F, TH_BUTTON_SHOOT2);
            buttons |= KEY_PRESSED(DIK_G, TH_BUTTON_BOMB2);
            buttons |= KEY_PRESSED(DIK_D, TH_BUTTON_FOCUS2);
            buttons |= KEY_PRESSED(DIK_I, TH_BUTTON_UP2);
            buttons |= KEY_PRESSED(DIK_K, TH_BUTTON_DOWN2);
            buttons |= KEY_PRESSED(DIK_J, TH_BUTTON_LEFT2);
            buttons |= KEY_PRESSED(DIK_L, TH_BUTTON_RIGHT2);
            #else
            //Player 2
            buttons |= KEY_PRESSED(DIK_W, TH_BUTTON_SHOOT2);
            buttons |= KEY_PRESSED(DIK_E, TH_BUTTON_BOMB2);
            buttons |= KEY_PRESSED(DIK_Q, TH_BUTTON_FOCUS2);
            buttons |= KEY_PRESSED(DIK_T, TH_BUTTON_UP2);
            buttons |= KEY_PRESSED(DIK_G, TH_BUTTON_DOWN2);
            buttons |= KEY_PRESSED(DIK_F, TH_BUTTON_LEFT2);
            buttons |= KEY_PRESSED(DIK_H, TH_BUTTON_RIGHT2);

            //Player 3
            buttons |= KEY_PRESSED(DIK_V, TH_BUTTON_SHOOT3);
            buttons |= KEY_PRESSED(DIK_B, TH_BUTTON_BOMB3);
            buttons |= KEY_PRESSED(DIK_C, TH_BUTTON_FOCUS3);
            buttons |= KEY_PRESSED(DIK_I, TH_BUTTON_UP3);
            buttons |= KEY_PRESSED(DIK_K, TH_BUTTON_DOWN3);
            buttons |= KEY_PRESSED(DIK_J, TH_BUTTON_LEFT3);
            buttons |= KEY_PRESSED(DIK_L, TH_BUTTON_RIGHT3);
            #endif
        }
    }
    
    return buttons;
    return GetControllerInput(buttons);
}

// FUNCTION: TH07 0x004312c0
void Controller::ResetKeyboard()
{
    u8 key_states[256];

    GetKeyboardState(key_states);
    for (i32 i = 0; i < ARRAY_SIZE_SIGNED(key_states); i++)
    {
        key_states[i] = key_states[i] & 0x7f;
    }
    SetKeyboardState(key_states);
}

// netplay

bool Controller::RcvPacks(int playerType)
{
    bool hasdata_all = false;
    bool hasdata;
    do
    {
        Pack pack;
        if (g_is_host)
        {
            g_host.PollReceive(pack, hasdata);
        }
        else
        {
            g_guest.PollReceive(pack, hasdata);
        }
        hasdata_all |= hasdata;
        if (!hasdata)
            return hasdata_all;
        if (pack.ctrl.ctrl_type == Ctrl_Key)
        {
            int frame = pack.ctrl.frame;
            for (int i = 0; i < KeyPackFrameNum; i++)
            {
                g_ctrl_bits_rcved[pack.playerType][frame - i] = pack.ctrl.keys[i];
                g_ctrl_rng_rcved[pack.playerType][frame - i] = pack.ctrl.rng_seed[i];
                g_ctrl_rcved[pack.playerType][frame - i] = pack.ctrl.igc_type[i];
            }
        }
        else if (pack.ctrl.ctrl_type == Ctrl_Try_Resync)
        {
            if ((pack.ctrl.resync_setting.frame_to_re_sync > g_Supervisor.calcCount) &&
                (pack.ctrl.resync_setting.frame_to_re_sync <= g_Supervisor.calcCount + g_delay * 2 + 2))
            {
                g_resync_trigger = true;
                g_resync_stage_frame = pack.ctrl.resync_setting.frame_to_re_sync;
            }
        }
    } while (hasdata);
    return hasdata_all;
}

void Controller::SendKeys(int frame, int playerType)
{
    Pack pack;
    pack.echoTick = 0;
    pack.sendTick = 0;
    pack.seq = 0;
    pack.type = 4;

    pack.ctrl.ctrl_type = Ctrl_Key;
    pack.ctrl.frame = frame;
    for (int i = 0; i < KeyPackFrameNum; i++)
    {
        std::map<int, BITS_32>::iterator find_res = g_ctrl_bits_self.find(frame - i);
        if (find_res == g_ctrl_bits_self.end())
            ReadFromInt(pack.ctrl.keys[i], 0);
        else
            pack.ctrl.keys[i] = find_res->second;

        std::map<int, int>::iterator find_res2 = g_ctrl_rng_self.find(frame - i);
        if (find_res2 == g_ctrl_rng_self.end())
            pack.ctrl.rng_seed[i] = 0;
        else
            pack.ctrl.rng_seed[i] = find_res2->second;

        std::map<int, InGameCtrlType>::iterator find_res3 = g_ctrl_self.find(frame - i);
        if (find_res3 == g_ctrl_self.end())
            pack.ctrl.igc_type[i] = IGC_NONE;
        else
            pack.ctrl.igc_type[i] = find_res3->second;
    }
    if (g_is_host)
    {
        g_host.SendPack(pack, playerType);
    }
    else
    {
        g_guest.SendPack(pack);
    }
}

// todo, get rid of it
void HandleControlKeys(int frame)
{
    InGameCtrlType igctrl = IGC_NONE;
    if (g_cur_ctrl_key_state.K_F2)
        igctrl = Inf_Life;
    else if (g_cur_ctrl_key_state.K_F3)
        igctrl = Inf_Bomb;
    else if (g_cur_ctrl_key_state.K_F4)
        igctrl = Inf_Power;
    else if (g_cur_ctrl_key_state.K_Q)
        igctrl = Quick_Quit;
    else if (g_cur_ctrl_key_state.K_R)
        igctrl = Quick_Restart;
    else if (g_cur_ctrl_key_state.K_M)
        igctrl = Add_Delay;
    else if (g_cur_ctrl_key_state.K_N)
        igctrl = Dec_Delay;
    else if (g_cur_ctrl_key_state.K_F6)
        igctrl = Insane_Mode;
    g_ctrl_self[frame] = igctrl;
}

#define TH_ISDOWN(a,mask,b) ((a)&(mask)?(b):0)

#ifdef TWO_PLAYER
#define CONTROL_RECEIVER_IGC_NONE {IGC_NONE, IGC_NONE, IGC_NONE}
#define CONTROL_RECEIVER_0 {0, 0, 0}
#define CONTROL_RECEIVER_FALSE {false,false,false};
#else
#define CONTROL_RECEIVER_IGC_NONE {IGC_NONE, IGC_NONE, IGC_NONE, IGC_NONE}
#define CONTROL_RECEIVER_0 {0,0,0,0}
#define CONTROL_RECEIVER_FALSE {false,false,false,false};
#endif

u32 GetKeys(int frame, bool is_in_UI, int &out_ctrl)
{
    InGameCtrlType self_ctrl = IGC_NONE;
    InGameCtrlType rcv_ctrl[CONTROL_RECEIVER] = CONTROL_RECEIVER_IGC_NONE;

    out_ctrl = IGC_NONE;
    if (frame - g_delay < 0)
        return 0;

    u32 self_key = 0;
    std::map<int, BITS_32>::iterator res = g_ctrl_bits_self.find(frame - g_delay);
    if (res != g_ctrl_bits_self.end())
        WriteToInt(res->second, self_key);

    std::map<int, InGameCtrlType>::iterator res2 = g_ctrl_self.find(frame - g_delay);
    if (res2 != g_ctrl_self.end())
        self_ctrl = res2->second;

    u32 rcv_key[CONTROL_RECEIVER] = CONTROL_RECEIVER_0;
    bool is_sync = true;

    bool has_rcv_data[CONTROL_RECEIVER] = CONTROL_RECEIVER_FALSE;

    static bool inited = false;
    bool hasFail = false;
    static LARGE_INTEGER freq;
    LARGE_INTEGER cur;
    LARGE_INTEGER ping_key_time;
    LARGE_INTEGER max_wait_to_time;
    if (!inited)
    {
        inited = true;
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&cur);
    max_wait_to_time.QuadPart = cur.QuadPart + freq.QuadPart * 5.0; // 5.0s
    ping_key_time.QuadPart = cur.QuadPart + freq.QuadPart * 0.1;    // 0.1s
    do
    {
        hasFail = false;
        for(int i=1;i<CONTROL_RECEIVER;i++){
            if(i==g_playerType||has_rcv_data[i]) continue;
            res = g_ctrl_bits_rcved[i].find(frame - g_delay);
            if (res != g_ctrl_bits_rcved[i].end())
            {
                WriteToInt(res->second, rcv_key[i]);
                is_sync = is_sync && (g_ctrl_rng_rcved[i][frame - g_delay] == g_ctrl_rng_self[frame - g_delay]);
                rcv_ctrl[i] = g_ctrl_rcved[i][frame - g_delay];
                has_rcv_data[i] = true;
            }else{
                hasFail = true;
            }
        }
        if(!hasFail){
            break;
        }else{
            int n_transfer = 1;
            while (cur.QuadPart < max_wait_to_time.QuadPart)
            {
                bool packeds = true;
                for(int i=1;i<CONTROL_RECEIVER;i++){
                    if(i==g_playerType||has_rcv_data[i]) continue;
                    packeds |= Controller::RcvPacks(i);
                }
                if (packeds)
                {
                    Sleep(1);
                    break;
                }
                Sleep(1);
                QueryPerformanceCounter(&cur);
                // send key to another player to avoid lock
                if (cur.QuadPart > ping_key_time.QuadPart)
                {
                    ping_key_time.QuadPart = cur.QuadPart + freq.QuadPart * 0.1; // 0.1s
                    if(g_is_host){
                        for(int i=1;i<CONTROL_RECEIVER;i++){
                            if(i==g_playerType||has_rcv_data[i]) continue;
                            Controller::SendKeys(frame, 2);
                        }
                    }else{
                        Controller::SendKeys(frame, 1);
                    }
                }
            }
        }
    } while (cur.QuadPart < max_wait_to_time.QuadPart);

    hasFail = false;
    for(int i=1;i<CONTROL_RECEIVER;i++){
        if(i==g_playerType || has_rcv_data[i]) continue;
        hasFail = true;
    }
    if (hasFail)
    {
        for(int i=1;i<CONTROL_RECEIVER;i++){
            rcv_key[i] = 0;
            rcv_ctrl[i] = IGC_NONE;
        }
        self_key = 0;
        self_ctrl = IGC_NONE;
        g_is_connected = false;
        g_istry_to_reconnect = false;
    }
    g_is_sync = is_sync;


    // TODO : remove all controls
    // if (self_ctrl != IGC_NONE && rcv_ctrl != IGC_NONE)
    // {
    //     out_ctrl = g_is_host ? self_ctrl : rcv_ctrl;
    // }
    // else
    // {
    //     out_ctrl = (self_ctrl == IGC_NONE) ? rcv_ctrl : self_ctrl;
    // }

    u32 finres = 0;
    if (is_in_UI){
        for(int i=1;i<CONTROL_RECEIVER;i++){
            if(i==g_playerType) finres |= self_key;
            else finres |= rcv_key[i];
        }
        return finres;
    }
    for(int i=1;i<CONTROL_RECEIVER;i++){
        if(i==g_playerType){
            finres |= TH_ISDOWN(self_key, TH_BUTTON_LEFT, TH_BUTTON_LEFT_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_RIGHT, TH_BUTTON_RIGHT_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_UP, TH_BUTTON_UP_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_DOWN, TH_BUTTON_DOWN_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_SHOOT, TH_BUTTON_SHOOT_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_BOMB, TH_BUTTON_BOMB_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_FOCUS, TH_BUTTON_FOCUS_MAP[i]);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_MENU, TH_BUTTON_MENU);
            finres |= TH_ISDOWN(self_key, TH_BUTTON_SKIP, TH_BUTTON_SKIP);
        }else{
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_LEFT, TH_BUTTON_LEFT_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_RIGHT, TH_BUTTON_RIGHT_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_UP, TH_BUTTON_UP_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_DOWN, TH_BUTTON_DOWN_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_SHOOT, TH_BUTTON_SHOOT_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_BOMB, TH_BUTTON_BOMB_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_FOCUS, TH_BUTTON_FOCUS_MAP[i]);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_MENU, TH_BUTTON_MENU);
            finres |= TH_ISDOWN(rcv_key[i], TH_BUTTON_SKIP, TH_BUTTON_SKIP);
        }
    }
    return finres;
}

u32 Controller::GetInput_Single(int &cur_ctrl)
{
    u32 input = GetInput();
    HandleControlKeys(0);
    cur_ctrl = g_ctrl_self[0];
    return input;
}

u32 Controller::GetInput_Net(int frame, bool is_in_UI, int &cur_ctrl)
{
    if (!g_is_connected)
    {
        u32 input = GetInput();
        HandleControlKeys(frame);
        cur_ctrl = g_ctrl_self[frame];
        return input;
    }
    u32 btn = GetInput();
    BITS_32 cur_btn_bits;
    ReadFromInt(cur_btn_bits, btn);
    g_ctrl_bits_self[frame] = cur_btn_bits;
    g_ctrl_rng_self[frame] = g_Rng.seed;

    // remove frames
    int frame_rem = 80;
    std::map<int, BITS_32>::iterator last_res;
    std::map<int, int>::iterator last_res_seed;
    std::map<int, InGameCtrlType>::iterator last_res_ctrl;

    last_res = g_ctrl_bits_self.find(frame - frame_rem);
    if (last_res != g_ctrl_bits_self.end())
        g_ctrl_bits_self.erase(last_res);

    last_res_seed = g_ctrl_rng_self.find(frame - frame_rem);
    if (last_res_seed != g_ctrl_rng_self.end())
        g_ctrl_rng_self.erase(last_res_seed);

    last_res_ctrl = g_ctrl_self.find(frame - frame_rem);
    if (last_res_ctrl != g_ctrl_self.end())
        g_ctrl_self.erase(last_res_ctrl);

    for(int i=0;i<CONTROL_RECEIVER;i++){
        last_res = g_ctrl_bits_rcved[i].find(frame - frame_rem);
        if (last_res != g_ctrl_bits_rcved[i].end())
            g_ctrl_bits_rcved[i].erase(last_res);
        last_res_seed = g_ctrl_rng_rcved[i].find(frame - frame_rem);
        if (last_res_seed != g_ctrl_rng_rcved[i].end())
            g_ctrl_rng_rcved[i].erase(last_res_seed);
        last_res_ctrl = g_ctrl_rcved[i].find(frame - frame_rem);
        if (last_res_ctrl != g_ctrl_rcved[i].end())
            g_ctrl_rcved[i].erase(last_res_ctrl);
    }

    HandleControlKeys(frame);
    if(g_is_host){
        for(int i=1;i<CONTROL_RECEIVER;i++){
            if(i!=g_playerType){
                Controller::SendKeys(frame,i);
                RcvPacks(i);
            }
        }
    }else{
        Controller::SendKeys(frame,1);
        for(int i=1;i<CONTROL_RECEIVER;i++){
            if(i!=g_playerType){
                RcvPacks(i);
            }
        }
    }

    u32 res = GetKeys(frame, is_in_UI, cur_ctrl);
    return res;
}