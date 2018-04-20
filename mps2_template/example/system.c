/****************************************************************************
*  Copyright 2017 Gorgon Meducer (Email:embedded_zhuoran@hotmail.com)       *
*                                                                           *
*  Licensed under the Apache License, Version 2.0 (the "License");          *
*  you may not use this file except in compliance with the License.         *
*  You may obtain a copy of the License at                                  *
*                                                                           *
*     http://www.apache.org/licenses/LICENSE-2.0                            *
*                                                                           *
*  Unless required by applicable law or agreed to in writing, software      *
*  distributed under the License is distributed on an "AS IS" BASIS,        *
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. *
*  See the License for the specific language governing permissions and      *
*  limitations under the License.                                           *
*                                                                           *
****************************************************************************/

/*============================ INCLUDES ======================================*/
#include ".\app_platform\app_platform.h"

#include "fce.h"

#include <assert.h>
#include <string.h>
/*============================ MACROS ========================================*/

#ifndef DELAY_OBJ_POOL_SIZE
#   warning No defined DELAY_OBJ_POOL_SIZE, default value 4 is used
#   define DELAY_OBJ_POOL_SIZE                              (4)
#endif

#ifndef IO_STREAM_BLOCK_BUFFER_ITEM_COUNT
#   warning No defined IO_STREAM_BLOCK_BUFFER_ITEM_COUNT, default value 8 is used
#   define IO_STREAM_BLOCK_BUFFER_ITEM_COUNT                (8)
#endif

#ifndef IO_STREAM_BLOCK_BUFFER_ITEM_SIZE
#   warning No defined IO_STREAM_BLOCK_BUFFER_ITEM_SIZE, default value 64 is used
#   define IO_STREAM_BLOCK_BUFFER_ITEM_SIZE                 (64)
#endif

#ifndef IO_TELEGRAPH_BUFFER_ITEM_COUNT
#   warning No defined IO_TELEGRAPH_BUFFER_ITEM_COUNT, default value 4 is used
#   define IO_TELEGRAPH_BUFFER_ITEM_COUNT                   (4)
#endif

#ifndef IO_CHANNEL_BUFFER_ITEM_COUNT
#   warning No defined IO_CHANNEL_BUFFER_ITEM_COUNT, default value 8 is used
#   define IO_CHANNEL_BUFFER_ITEM_COUNT                   (8)
#endif

#if IO_STREAM_BLOCK_BUFFER_ITEM_COUNT < (IO_CHANNEL_BUFFER_ITEM_COUNT * 2 + 1) 
#error No sufficent blocks to support specified numbers of channels, \
        The minimal number should be (IO_CHANNEL_BUFFER_ITEM_COUNT * 2 + 1)
#endif

#ifndef NES_ROM_BUFFER_SIZE
#   warning No defined NES_ROM_BUFFER_SIZE, default value 32 is used
#   define NES_ROM_BUFFER_SIZE                              32
#endif

#ifndef NES_ROM_PATH
#   warning No defined NES_ROM_PATH, default "..\\..\\LiteNES\\ROMS\\Super Mario Bros (E).nes" is used.
#   define NES_ROM_PATH "..\\..\\LiteNES\\ROMS\\Super Mario Bros (E).nes"
#endif

/*============================ MACROFIED FUNCTIONS ===========================*/
/*============================ TYPES =========================================*/
/*============================ GLOBAL VARIABLES ==============================*/
/*============================ LOCAL VARIABLES ===============================*/  
/*============================ PROTOTYPES ====================================*/
/*============================ IMPLEMENTATION ================================*/

/*----------------------------------------------------------------------------
  SysTick / Timer0 IRQ Handler
 *----------------------------------------------------------------------------*/


void SysTick_Handler (void) 
{
    static volatile uint32_t            s_wMSTicks = 0; 
    /*! 1ms timer event handler */
    s_wMSTicks++;
    
    if (!(s_wMSTicks % 1000)) {
        static volatile uint16_t wValue = 0;

        //printf("%s [%08x]\r\n", "Hello world!", wValue++);
        
        //STREAM_OUT.Stream.Flush();
    }
    
    FILE_IO.Dependent.TimerTickService();

    /*! call application platform 1ms event handler */
    app_platform_1ms_event_handler();
}

static void system_init(void)
{
    if (!app_platform_init()) {
        NVIC_SystemReset();
    }

    SysTick_Config(SystemCoreClock  / 1000);  //!< Generate interrupt every 1 ms 
}

static void file_io_service_init(void)
{
    NO_INIT static file_io_delay_item_t s_tDelayObjPool[DELAY_OBJ_POOL_SIZE];
    NO_INIT static struct {
        file_io_block_t tHeader;
        uint8_t chBuffer[IO_STREAM_BLOCK_BUFFER_ITEM_SIZE+8];
    } s_tBlockBuffer[IO_STREAM_BLOCK_BUFFER_ITEM_COUNT];
    
    NO_INIT static 
        file_io_telegraph_t s_tTelegraphBuffer[IO_TELEGRAPH_BUFFER_ITEM_COUNT];
    NO_INIT static 
        file_io_stream_t s_tChannelBuffer[IO_CHANNEL_BUFFER_ITEM_COUNT];


    FILE_IO_CFG (
        {   (STREAM_IN.Stream.ReadByte),
            (STREAM_OUT.Stream.WriteByte),
            (STREAM_OUT.Stream.Flush)
        },
        (mem_block_t){  (uint8_t *)s_tChannelBuffer,  
                        sizeof(s_tChannelBuffer)
                     },
        (mem_block_t){  (uint8_t *)s_tBlockBuffer,  
                        sizeof(s_tBlockBuffer)
                     },
                     
        IO_STREAM_BLOCK_BUFFER_ITEM_SIZE+8,
        (mem_block_t){  (uint8_t *)s_tTelegraphBuffer,  
                        sizeof(s_tTelegraphBuffer)
                     },
                     
        (mem_block_t){  (uint8_t *)s_tDelayObjPool,  
                        sizeof(s_tDelayObjPool)
                     },
        
        ((( IO_STREAM_BLOCK_BUFFER_ITEM_COUNT 
            - 1                                                                 //! receive buffer
            - MIN(  IO_TELEGRAPH_BUFFER_ITEM_COUNT,                             //! transmit buffer
                    IO_CHANNEL_BUFFER_ITEM_COUNT)
            ) / IO_CHANNEL_BUFFER_ITEM_COUNT))
                    
    );
}

static void app_init(void)
{   
    file_io_service_init();
}

/*
private void show_progress(void)
{
    static const uint8_t c_chIcons[] = {"-\\|/-\\|/"};
    static uint8_t s_chCount = 0;
    static uint8_t s_chIndex = 0;
    
    s_chCount++;
    if (!(s_chCount & 0x07)) {
        log("\b%c", c_chIcons[s_chIndex++]);
        s_chIndex &= 0x07;
    }
}
*/
/*----------------------------------------------------------------------------
  Main function
 *----------------------------------------------------------------------------*/

NO_INIT static uint8_t s_cROMBuffer[NES_ROM_BUFFER_SIZE * 1024];

static int_fast32_t load_nes_rom(uint8_t *pchBuffer, uint32_t wSize)
{
    if (NULL == pchBuffer || wSize < (32*1024)) {
        return false;
    }
    uint32_t wTotalSize = 0;
    log_info("Loading NES ROM...");
    file_io_stream_t * ptInput = FILE_IO.Channel.Open(
                            "Input",
                            "PATH:" NES_ROM_PATH,
                            FILE_IO_INPUT          |
                            FILE_IO_BINARY_STREAM
                         );

    if (NULL == ptInput) {
        return -1;
    }
    
    while (!FILE_IO.Channel.EndOfStream(ptInput)) {
        
        //! read command output
        int32_t nSize = FILE_IO.Channel.Read(ptInput, pchBuffer, wSize);
        
        if (nSize == -1) {
            log_info("Error\r\n");
            break;
        } else if (0 == nSize) {
            continue;
        }
        
        pchBuffer+=nSize;
        wSize -= nSize;
        wTotalSize += nSize;
    }
    
    FILE_IO.Channel.Close(ptInput);
    log_info("OK\r\n ROM SIZE: %d Bytes\r\n", wTotalSize);
    
    
    return wTotalSize;
}

int main (void) 
{
    system_init();
    app_init();
              
    file_io_stream_t *ptLog = FILE_IO.Channel.Open(
                            "Log",
                            "STDOUT",
                            FILE_IO_OUTPUT          |
                            FILE_IO_TEXT_STREAM
                         );
    assert(NULL != ptLog);
    retarget_stdout(ptLog);
    
    do {
        int_fast32_t nSize = load_nes_rom(s_cROMBuffer, sizeof(s_cROMBuffer));
        if (nSize < 0) {
            break;
        }
        
        if (fce_load_rom((char *)s_cROMBuffer, nSize) != 0){
            break;
        }
        
        log_info("Initialise NES Simulator...\r\n")
        fce_init();
        log_info("Game Start...\r\n")
        fce_run();
        
        FILE_IO.Channel.Close(ptLog);
        while(1);
    } while(false);
    
    log_info("Error: Invalid or unsupported rom.\r\n")
    
    FILE_IO.Channel.Close(ptLog);

    while (true) {
    }

}
