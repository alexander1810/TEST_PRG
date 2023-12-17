/**
 * @file    main.c
 * @date    2023-12-17
 * @author  Alexander Osanadze
 * @brief   This file is used for whole program execution, start tasks, 
 *          work with queue and timer, logging and etc
 *
 *    file goals:
 *      test for demonstration of work with tasks, queues, timers
 *       
 *
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

//-------------global struct of counters---------------//
typedef struct{
    uint16_t queue_send_cnt;
    uint16_t queue_reseve_cnt;

    uint16_t ms_cnt;
    uint16_t prev_ms_cnt;
    uint16_t prev_message_ms_cnt;
}global_cnt;

//-------------global enum of second names---------------//
typedef enum{
    TIME_1S =   1,
    TIME_2S =   2,
    TIME_3S =   3,
    TIME_4S =   4,
    TIME_5S =   5,
    TIME_6S =   6,
    TIME_7S =   7,
    TIME_8S =   8,
    TIME_9S =   9,
    TIME_10S =  10
}time_in_sec;

//-------------global enum of miliseconds names---------------//
typedef enum{
    TIME_1MS =      1,
    TIME_10MS =     10,
    TIME_50MS =     50,
    TIME_100MS =    100,
    TIME_500MS =    500,
    TIME_1000MS =   1000,
}time_in_ms;

//-------------global enum of error types---------------//
typedef enum{
    TIMER_FAULT,
    TASK_FAULT,
    QUEUE_FAULT,
}error_type;

//-------------Defines for configuration---------------//
#define TIMER_INTERRUPT_PERIOD                  (TIME_1MS)
#define CONVERT_INTERRUPTS_NUMBER_TO_MS(TIME)   (TIME*(configTICK_RATE_HZ/TIMER_INTERRUPT_PERIOD))
#define TIME_TO_INCREMENT                       CONVERT_INTERRUPTS_NUMBER_TO_MS(TIME_5S)
  
#define QUEUE_WRITER_TASK_PERIOD                (TIME_500MS)
#define QUEUE_READER_TASK_PERIOD                (TIME_100MS)

static global_cnt gbl_cnt = {0};
static global_cnt * ptr_gbl_cnt = &gbl_cnt;
static const char* TAG = "TEST";
TimerHandle_t xTimer;
QueueHandle_t xQueue;

/**************************************************************************//**
* @fn           void ErrorHandler(error_type errorType)
* @brief        Error handler for rebooting esp32 by watchdog timer.
* @details      Function logs the type of fault.
* @warning      
* @todo
* @param[in]    error_type errorType - error type(range: TIMER_FAULT, TASK_FAULT, QUEUE_FAULT) 
* @return       none
*****************************************************************************/
void ErrorHandler(error_type errorType)
{
    switch(errorType){
        case TIMER_FAULT:   ESP_LOGE(TAG, "TIMER FAULT!!!");break; 
        case TASK_FAULT:    ESP_LOGE(TAG, "TASK FAULT!!!");break; 
        case QUEUE_FAULT:   ESP_LOGE(TAG, "QUEUE FAULT!!!");break;
        default:            ESP_LOGE(TAG, "UNKNOWN FAULT!!!");break;
    }
    ESP_LOGE(TAG, "REBOOT BY WATCHBOG AFTER %d milliseconds", CONFIG_BOOTLOADER_WDT_TIME_MS);
    while(1){}
}
/**************************************************************************//**
* @fn           uint16_t Get_Diff_CntValue(uint16_t curr_counter, 
                                           uint16_t prev_counter )
* @brief        Function finds a difference between current and previous counters 
* @details      It is not nessesary current counter higher then previous counter or vise versa
* @warning      
* @todo
* @param[in]    uint16_t curr_counter - current counter (range: 0 - 0xFFFF)
* @param[in]    uint16_t prev_counter - previous counter (range: 0 - 0xFFFF)
* @return       difference of counters (range: 0 - 0xFFFF)
*****************************************************************************/
uint16_t Get_Diff_CntValue(uint16_t curr_counter, 
                           uint16_t prev_counter )
{
    int16_t diff_cnt = curr_counter - prev_counter;
    diff_cnt = (diff_cnt >= 0) ? 
        diff_cnt : (0xFFFF + diff_cnt);
    return diff_cnt;
}
/**************************************************************************//**
* @fn           void vTimerCallback( TimerHandle_t pxTimer )
* @brief        Timer Callback 
* @details      Execution of this callback calls every TIMER_INTERRUPT_PERIOD. 
                Increment of queue_send_cnt counter every TIME_TO_INCREMENT for sending by queue 
* @warning      
* @todo
* @param[in]    TimerHandle_t pxTimer - timer handler
* @return       none
*****************************************************************************/
void vTimerCallback( TimerHandle_t pxTimer )
{
    ptr_gbl_cnt->ms_cnt++;

    int16_t diff_cnt = Get_Diff_CntValue(ptr_gbl_cnt->ms_cnt,
                                         ptr_gbl_cnt->prev_ms_cnt);
    
    if(diff_cnt >= TIME_TO_INCREMENT){
        ESP_LOGI(TAG, "TIMER = %d, %d", (int)diff_cnt, 
                                        (int)ptr_gbl_cnt->ms_cnt);
        ptr_gbl_cnt->queue_send_cnt++;
        ptr_gbl_cnt->prev_ms_cnt = ptr_gbl_cnt->ms_cnt;
    }
}
/**************************************************************************//**
* @fn           static void queue_writer_task(void *pvParameter)
* @brief        Task for sending global counter to the queue
* @details      Task executes every QUEUE_WRITER_TASK_PERIOD
* @warning      
* @todo
* @param[in]    void *pvParameter - pointer to parameters that can be used in this task
* @return       none
*****************************************************************************/
static void queue_writer_task(void *pvParameter)
{
    while(1)
    {
        BaseType_t send_status = 
            xQueueSendToBack(xQueue,(uint16_t*)&(ptr_gbl_cnt->queue_send_cnt), 0);

        if (send_status != pdTRUE)
            continue;

        vTaskDelay(QUEUE_WRITER_TASK_PERIOD / portTICK_PERIOD_MS);
    }
}
/**************************************************************************//**
* @fn           static void queue_reader_task(void *pvParameter)
* @brief        Task for receiving global counter from the queue and for logging if with difference time
* @details      Task executes every QUEUE_READER_TASK_PERIOD
* @warning      
* @todo
* @param[in]    void *pvParameter - pointer to parameters that can be used in this task
* @return       none
*****************************************************************************/
static void queue_reader_task(void *pvParameter)
{
    while(1)
    {
        BaseType_t receive_status = 
            xQueueReceive(xQueue,(uint16_t*)&(ptr_gbl_cnt->queue_reseve_cnt), 0);

        if (receive_status != pdTRUE)
            continue;

        int16_t diff_cnt = Get_Diff_CntValue(ptr_gbl_cnt->ms_cnt, 
                                             ptr_gbl_cnt->prev_message_ms_cnt);
        ptr_gbl_cnt->prev_message_ms_cnt = ptr_gbl_cnt->ms_cnt;

        ESP_LOGI(TAG, "counter = %d, diff time in ms = %d", 
                            (int)ptr_gbl_cnt->queue_reseve_cnt, 
                            (int)diff_cnt);
        vTaskDelay(QUEUE_READER_TASK_PERIOD / portTICK_PERIOD_MS);
    }
}
/**************************************************************************//**
* @fn           void app_main(void)
* @brief        Main function of whole program. It is a start point of execution 
* @details      Function creates all tasks, timer, queue. Base check
* @warning      
* @todo
* @param[in]    none
* @return       none
*****************************************************************************/
void app_main(void)
{
    
    if(xQueue == NULL)
        xQueue = xQueueCreate(1, sizeof(uint16_t));

    BaseType_t xReturned;
    TaskHandle_t xHandle = NULL;

    xReturned = xTaskCreate(&queue_writer_task, "queue_writer_task", 4096, NULL, 5, &xHandle);
    if( xReturned != pdPASS )
    {
        vTaskDelete( xHandle );
        ErrorHandler(TASK_FAULT);
    }

    xReturned = xTaskCreate(&queue_reader_task, "queue_reader_task", 4096, NULL, 5, &xHandle);
    if( xReturned != pdPASS )
    {
        vTaskDelete( xHandle );
        ErrorHandler(TASK_FAULT);
    }

    xTimer = xTimerCreate(    
                        "Timer",       
                        (TIMER_INTERRUPT_PERIOD * portTICK_PERIOD_MS),   
                        pdTRUE,        
                        (void *) NULL,  
                        vTimerCallback 
                        );
    if(xTimer == NULL)
        ErrorHandler(TIMER_FAULT);
    else{
        if(xTimerStart(xTimer, 0) != pdPASS)
            ErrorHandler(TIMER_FAULT);
    }
}


