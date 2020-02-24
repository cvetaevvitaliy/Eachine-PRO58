#include <stdint.h>

#include "buttons.h"
#include "settings.h"
#include "settings_eeprom.h"
#include "stm32f1xx_hal.h"
#ifdef USE_BUZZER
	#include "beeper.h"
#endif

struct Buttons::ButtonState states[BUTTON_COUNT];
static Buttons::ChangeFunc changeFuncs[BUTTON_HOOKS_MAX] = { nullptr };


namespace Buttons {
    static void runChangeFuncs(Button button, PressType pressType);
    static void updateButton(
        const Button button,
        struct Buttons::ButtonState &state,
		GPIO_TypeDef *port,
        uint16_t pin
    );


    uint32_t lastChangeTime = 0;


    void update() {
/*        #define UPDATE_BUTTON(button) \
            updateButton( \
                Button::button, \
                states[static_cast<__SIZE_TYPE__>(Button::button)], \
                BUTTON_ ## button ## _GPIO_Port \
				BUTTON_ ## button ## _Pin \
            );

        UPDATE_BUTTON(UP);
        UPDATE_BUTTON(DOWN);
        UPDATE_BUTTON(MODE);
        UPDATE_BUTTON(EB0);
        UPDATE_BUTTON(EB1);
        UPDATE_BUTTON(EB2);
        #undef UPDATE_BUTTON
*/
    	if(!EepromSettings.screenFlip){
			updateButton(Button::UP, states[static_cast<uint8_t>(Button::UP)], BUTTON_UP_GPIO_Port, BUTTON_UP_Pin);
			updateButton(Button::DOWN, states[static_cast<uint8_t>(Button::DOWN)], BUTTON_DOWN_GPIO_Port, BUTTON_DOWN_Pin);
#ifndef HB5808
			if(!EepromSettings.FSPinsMode){
				updateButton(Button::UP, states[static_cast<uint8_t>(Button::FS_UP)], FS_PIN_EB0_GPIO_Port, FS_PIN_EB0_Pin);
				updateButton(Button::DOWN, states[static_cast<uint8_t>(Button::FS_DOWN)], FS_PIN_EB2_GPIO_Port, FS_PIN_EB2_Pin);
			}
#endif
    	}
    	else{
			updateButton(Button::UP, states[static_cast<uint8_t>(Button::UP)], BUTTON_DOWN_GPIO_Port, BUTTON_DOWN_Pin);
			updateButton(Button::DOWN, states[static_cast<uint8_t>(Button::DOWN)], BUTTON_UP_GPIO_Port, BUTTON_UP_Pin);
#ifndef HB5808
			if(!EepromSettings.FSPinsMode){
				updateButton(Button::UP, states[static_cast<uint8_t>(Button::FS_UP)], FS_PIN_EB2_GPIO_Port, FS_PIN_EB2_Pin);
				updateButton(Button::DOWN, states[static_cast<uint8_t>(Button::FS_DOWN)], FS_PIN_EB0_GPIO_Port, FS_PIN_EB0_Pin);
			}
#endif
    	}
		updateButton(Button::MODE, states[static_cast<uint8_t>(Button::MODE)], BUTTON_MODE_GPIO_Port, BUTTON_MODE_Pin);
#ifndef HB5808
		if(!EepromSettings.FSPinsMode){
			updateButton(Button::MODE, states[static_cast<uint8_t>(Button::FS_MODE)], FS_PIN_EB1_GPIO_Port, FS_PIN_EB1_Pin);
		}
#endif
    }

    const ButtonState *get(Button button) {
        return &states[static_cast<size_t>(button)];
    }

    const bool any() {
        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            if (states[i].pressed)
                return true;
        }

        return false;
    }

    void registerChangeFunc(ChangeFunc func) {
        for (uint8_t i = 0; i < BUTTON_HOOKS_MAX; i++) {
            if (changeFuncs[i] == nullptr) {
                changeFuncs[i] = func;
                return;
            }
        }
    }

    void deregisterChangeFunc(ChangeFunc func) {
        for (uint8_t i = 0; i < BUTTON_HOOKS_MAX; i++) {
            if (changeFuncs[i] == func) {
                changeFuncs[i] = nullptr;
                return;
            }
        }
    }

    static void runChangeFuncs(Button button, PressType pressType) {
#ifdef USE_BUZZER
    	Beeper::beepHigh(50);
#endif
        for (uint8_t i = 0; i < BUTTON_HOOKS_MAX; i++) {
            if (changeFuncs[i] != nullptr) {
                changeFuncs[i](button, pressType);
            }
        }
    }

    static void updateButton(
        const Button button,
        struct ButtonState &state,
		GPIO_TypeDef *port,
        uint16_t pin
    ) {
    	bool pinStatus = HAL_GPIO_ReadPin(port,pin) == GPIO_PIN_SET ? false : true;

        if (pinStatus != state.lastReading) {
            state.lastDebounceTime = HAL_GetTick();
        }

        state.lastReading = pinStatus;

        if (
			pinStatus != state.pressed &&
            (HAL_GetTick() - state.lastDebounceTime) >= BUTTON_DEBOUNCE_DELAY
        ) {
            state.pressed = pinStatus;

            uint32_t prevChangeTime = state.changeTime;
            state.changeTime = HAL_GetTick();
            lastChangeTime = state.changeTime;

            if (!state.pressed) {
                uint32_t duration = state.changeTime - prevChangeTime;

                if (duration < 500)
                    runChangeFuncs(button, PressType::SHORT);
                else if (duration < 1500)
                    runChangeFuncs(button, PressType::LONG);
            }
        }

        if (state.pressed) {
            uint32_t duration = HAL_GetTick() - state.changeTime;

            if (duration >= 1500)
                runChangeFuncs(button, PressType::HOLDING);
        }
    }
}
