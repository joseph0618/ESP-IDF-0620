#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define SIZE 4

int M1[SIZE][SIZE] = {
    {1, 2, 3, 4},
    {5, 6, 7, 8},
    {9, 10, 11, 12},
    {13, 14, 15, 16}
};

int M2[SIZE][SIZE] = {
    {16, 15, 14, 13},
    {12, 11, 10, 9},
    {8, 7, 6, 5},
    {4, 3, 2, 1}
};

int M3[SIZE][SIZE] = {0};
int sum = 0;
int current_i = 0;
int current_j = 0;
int row = 0;
int column = 0;
int multiplication_done = 0;  
int tasks_finished = 0;       

SemaphoreHandle_t mutex; 

void get_next_position(int *i, int *j) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    *i = current_i;
    *j = current_j;
    current_j++;
    if (current_j == SIZE) {
        current_j = 0;
        current_i++;
    }
    xSemaphoreGive(mutex);
}

void combined_task(void *arg) {
    const char* task_name = pcTaskGetName(NULL);
    
    // Matrix Multiplication Phase
    printf("%s starting multiplication phase\n", task_name);
    
    while (1) {
        int i, j;
        get_next_position(&i, &j);

        if (i >= SIZE) break;

        int core_id = esp_cpu_get_core_id();
        printf("%s is processing M3[%d][%d] on Core %d\n",
               task_name, i, j, core_id);

        int val = 0;
        for (int k = 0; k < SIZE; k++) {
            val += M1[i][k] * M2[k][j];
        }   

        M3[i][j] = val;
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    printf("%s finished multiplication phase\n", task_name);

    xSemaphoreTake(mutex, portMAX_DELAY);
    tasks_finished++;
    if (tasks_finished == 2) {
        multiplication_done = 1;
        printf("All multiplication tasks completed, starting sum phase\n");
    }
    xSemaphoreGive(mutex);

    while (1) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        int mult_done = multiplication_done;
        xSemaphoreGive(mutex);
        
        if (mult_done) break;
        vTaskDelay(1);
    }
    
    // Matrix Sum Phase
    printf("%s starting sum phase\n", task_name);
    
    while (1) {
        xSemaphoreTake(mutex, portMAX_DELAY);       
        if (row >= SIZE) {
            xSemaphoreGive(mutex);
            break;
        }

        sum += M3[row][column];

        int core_id = esp_cpu_get_core_id();
        printf("%s is summing M3[%d][%d] on Core %d\n",
                task_name, row, column, core_id);

        column++;
        if(column >= SIZE) {
            row++;
            column = 0;
        }
        xSemaphoreGive(mutex);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    printf("%s finished sum phase\n", task_name);
    

    xSemaphoreTake(mutex, portMAX_DELAY);
    tasks_finished++;
    if (tasks_finished == 4) {  // 2 tasks × 2 phases
        printf("\nFinal M3:\n");
        for (int i = 0; i < SIZE; i++) {
            for (int j = 0; j < SIZE; j++) {
                printf("%4d ", M3[i][j]);
            }
            printf("\n");
        }
        printf("Sum of elements of M3: %d\n", sum);
    }
    xSemaphoreGive(mutex);
    
    vTaskDelete(NULL);
}

void app_main(void) {
    printf("Matrix Calculation Starting...\n");

    mutex = xSemaphoreCreateMutex();
    
    xTaskCreatePinnedToCore(combined_task, "Task A", 2048, NULL, 1, NULL, tskNO_AFFINITY);
    xTaskCreatePinnedToCore(combined_task, "Task B", 2048, NULL, 1, NULL, tskNO_AFFINITY);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
}