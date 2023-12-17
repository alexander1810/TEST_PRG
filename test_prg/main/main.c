#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/timers.h"
#include "freertos/queue.h"

static const char* TAG = "TEST";
TimerHandle_t xTimer;
QueueHandle_t xQueue;

typedef struct{
    uint16_t queue_send_cnt;
    uint16_t queue_reseve_cnt;

    uint16_t ms_cnt;
    uint16_t prev_ms_cnt;
    uint16_t prev_message_ms_cnt;
}global_cnt;

static global_cnt gbl_cnt = {0};
static global_cnt * ptr_gbl_cnt = &gbl_cnt;

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
typedef enum{
    TIME_1MS =      1,
    TIME_10MS =     10,
    TIME_50MS =     50,
    TIME_100MS =    100,
    TIME_500MS =    500,
    TIME_1000MS =   1000,
}time_in_ms;

#define TIMER_INTERRUPT_PERIOD                  (TIME_1MS)
#define CONVERT_INTERRUPTS_NUMBER_TO_MS(TIME)   (TIME*(configTICK_RATE_HZ/TIMER_INTERRUPT_PERIOD))
#define TIME_TO_INCREMENT                       CONVERT_INTERRUPTS_NUMBER_TO_MS(TIME_5S)
  
#define QUEUE_WRITER_TASK_PERIOD                (TIME_500MS)
#define QUEUE_READER_TASK_PERIOD                (TIME_100MS)

uint16_t Get_Diff_CntValue(uint16_t curr_counter, 
                           uint16_t prev_counter )
{
    int16_t diff_cnt = curr_counter - prev_counter;
    diff_cnt = (diff_cnt >= 0) ? 
        diff_cnt : (0xFFFF + diff_cnt);
    return diff_cnt;
}

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

void app_main(void)
{
    xQueue = xQueueCreate(1, sizeof(uint16_t));

    xTaskCreate(&queue_writer_task, "queue_writer_task", 4096, NULL, 5, NULL);
    xTaskCreate(&queue_reader_task, "queue_reader_task", 4096, NULL, 5, NULL);

    xTimer = xTimerCreate(    
                        "Timer",       
                        (TIMER_INTERRUPT_PERIOD * portTICK_PERIOD_MS),   
                        pdTRUE,        
                        (void *) NULL,  
                        vTimerCallback 
                        );
    xTimerStart(xTimer, 0);
}
