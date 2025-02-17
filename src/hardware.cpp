/* LibreSolar charge controller firmware
 * Copyright (c) 2016-2019 Martin Jäger (www.libre.solar)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UNIT_TEST

#include "hardware.h"
#include "pcb.h"
#include "load.h"

#include "mbed.h"
#include "half_bridge.h"
#include "us_ticker_data.h"
#include "leds.h"

#if defined(STM32F0)

void control_timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM16 clock
    RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;

    // Set timer clock to 10 kHz
    TIM16->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM16->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM16->ARR = 10000 / freq_Hz - 1;

    // 1 = second-highest priority of STM32L0/F0
    NVIC_SetPriority(TIM16_IRQn, 1);
    NVIC_EnableIRQ(TIM16_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM16->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM16_IRQHandler(void)
{
    TIM16->SR &= ~TIM_SR_UIF;       // clear update interrupt flag to restart timer
    system_control();
}

#elif defined(STM32L0)

void control_timer_start(int freq_Hz)   // max. 10 kHz
{
    // Enable TIM7 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM7EN;

    // Set timer clock to 10 kHz
    TIM7->PSC = SystemCoreClock / 10000 - 1;

    // Interrupt on timer update
    TIM7->DIER |= TIM_DIER_UIE;

    // Auto Reload Register sets interrupt frequency
    TIM7->ARR = 10000 / freq_Hz - 1;

    // 1 = second-highest priority of STM32L0/F0
    NVIC_SetPriority(TIM7_IRQn, 1);
    NVIC_EnableIRQ(TIM7_IRQn);

    // Control Register 1
    // TIM_CR1_CEN =  1: Counter enable
    TIM7->CR1 |= TIM_CR1_CEN;
}

extern "C" void TIM7_IRQHandler(void)
{
    TIM7->SR &= ~(1 << 0);
    system_control();
}

#endif


// Reset the watchdog timer
void feed_the_dog()
{
    IWDG->KR = 0xAAAA;
}

// configures IWDG (timeout in seconds)
void init_watchdog(float timeout) {

    // TODO for STM32L0
    #define LSI_FREQ 40000     // approx. 40 kHz according to RM0091

    uint16_t prescaler;

    IWDG->KR = 0x5555; // Disable write protection of IWDG registers

    // set prescaler
    if ((timeout * (LSI_FREQ/4)) < 0x7FF) {
        IWDG->PR = IWDG_PRESCALER_4;
        prescaler = 4;
    }
    else if ((timeout * (LSI_FREQ/8)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_8;
        prescaler = 8;
    }
    else if ((timeout * (LSI_FREQ/16)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_16;
        prescaler = 16;
    }
    else if ((timeout * (LSI_FREQ/32)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_32;
        prescaler = 32;
    }
    else if ((timeout * (LSI_FREQ/64)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_64;
        prescaler = 64;
    }
    else if ((timeout * (LSI_FREQ/128)) < 0xFF0) {
        IWDG->PR = IWDG_PRESCALER_128;
        prescaler = 128;
    }
    else {
        IWDG->PR = IWDG_PRESCALER_256;
        prescaler = 256;
    }

    // set reload value (between 0 and 0x0FFF)
    IWDG->RLR = (uint32_t)(timeout * (LSI_FREQ/prescaler));

    //float calculated_timeout = ((float)(prescaler * IWDG->RLR)) / LSI_FREQ;
    //printf("WATCHDOG set with prescaler:%d reload value: 0x%X - timeout:%f\n", prescaler, (unsigned int)IWDG->RLR, calculated_timeout);

    IWDG->KR = 0xAAAA;  // reload
    IWDG->KR = 0xCCCC;  // start

    feed_the_dog();
}

// this function is called by mbed if a serious error occured: error() function called
void mbed_die(void)
{
    half_bridge_stop();
    load_switch_set(false);
    load_usb_set(false);

    leds_init(false);

    while (1) {

        leds_toggle_error();
        wait_ms(100);

        // stay here to indicate something was wrong
        feed_the_dog();
    }
}

#if defined(STM32F0)
#define SYS_MEM_START       0x1FFFC800
#define SRAM_END            0x20003FFF  // 16k
#elif defined(STM32L0)
#define SYS_MEM_START       0x1FF00000
#define SRAM_END            0X20004FFF  // 20k
#define FLASH_LAST_PAGE     0x0802FF80  // start address of last page (192 kbyte cat 5 devices)
#endif

#define MAGIC_CODE_ADDR     (SRAM_END - 0xF)    // where the magic code is stored

void start_dfu_bootloader()
{
    // place magic code at end of RAM and initiate reset
    *((uint32_t *)(MAGIC_CODE_ADDR)) = 0xDEADBEEF;
    NVIC_SystemReset();
}

// This function should be called at the beginning of SystemInit in system_clock.c (mbed target directory)
// As mbed does not provide this functionality, you have to patch the file manually
extern "C" void system_init_hook()
{
    if (*((uint32_t *)(MAGIC_CODE_ADDR)) == 0xDEADBEEF) {

        uint32_t jump_address;

#ifdef STM32L0
        // Trying to implement this solution, but doesn't work properly...
        // https://stackoverflow.com/questions/42020893/stm32l073rz-rev-z-iap-jump-to-bootloader-system-memory
        if (*((uint32_t *)(MAGIC_CODE_ADDR + 4)) == 0) {      // first jump

            *((uint32_t *)(MAGIC_CODE_ADDR + 4)) = 0xAAAAAAAA;

            // Reinitialize the Stack pointer and jump to application address
            jump_address = *((uint32_t *) (SYS_MEM_START + 4));
        }
        else {
            *((uint32_t *)(MAGIC_CODE_ADDR)) =  0x00000000;  // reset the first trigger
            *((uint32_t *)(MAGIC_CODE_ADDR + 4)) =  0x00000000;  // reset the second trigger

            // Reinitialize the Stack pointer and jump to application address
            jump_address = (*((uint32_t *) (0x1FF00369)));
        }
#else // STM32F0 and possibly others
        *((uint32_t *)(MAGIC_CODE_ADDR)) =  0x00000000;  // reset the trigger

        jump_address = (*((uint32_t *) (SYS_MEM_START + 4)));
#endif

        // reinitialize stack pointer
        __set_MSP(SYS_MEM_START);

        // jump to specified address
        void (*jump)(void);
        jump = (void (*)(void)) jump_address;
        jump();

        while (42); // should never be reached
    }
}

/*
// alternative approach suggested by STM (without reset and magic bytes)

void BootLoaderInit(uint32_t BootLoaderStatus)
{
    void (*SysMemJump)(void);
    SysMemJump = (void (*)(void)) (*((uint32_t *) (SYS_MEM_START + 4))); // for STM32F0

    if (BootLoaderStatus == 1) {

        printf("Trying to jump to bootloader!\n");

        // shut down any tasks running
        HAL_RCC_DeInit();
        SysTick->CTRL = 0;  // reset Systick Timer
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        //__set_PRIMASK(1);   // disable interrupts

        __set_MSP(SYS_MEM_START);

        SysMemJump();

        while(42);
    }
}
*/

#else

// dummy functions for unit tests
void hw_load_switch(bool enabled) {;}
void hw_usb_out(bool enabled) {;}
void init_watchdog(float timeout) {;}
void feed_the_dog() {;}
void control_timer_start(int freq_Hz) {;}
void system_control() {;}
void start_dfu_bootloader() {;}

#endif /* UNIT_TEST */
