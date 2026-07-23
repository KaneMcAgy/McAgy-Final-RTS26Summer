/* Final Capstone Project 
Made changes to the startup to look more polished
changed variable names to Avionics tasks rather than app3 jargon
Added health monitor and startup banner as well
 * ============================================================
 * Theme: Avionics 
 * ============================================================
 */

#ifndef WITH_LOAD
#define WITH_LOAD 1
#endif

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_attr.h"

#define BUTTON_GPIO   GPIO_NUM_18      /* input — button to GND */
#define ISR_PULSE_GPIO GPIO_NUM_19     /* output — scope this for latency */
#define DEBOUNCE_US   200

#define CONFIG_LOG_DEFAULT_LEVEL_INFO 1
#define CONFIG_LOG_MAXIMUM_LEVEL  5

static const char *TAG = "AEGIS";

/* Signaling primitives */
static SemaphoreHandle_t btn_sem;            /* binary semaphore path */
static TaskHandle_t      task_notif_handle;  /* direct notification path */

/* Latency telemetry */
static volatile int64_t isr_entry_time_us;
static volatile uint32_t presses_observed;
static volatile uint64_t latency_max_sem_us;
static volatile uint64_t latency_max_notif_us;

/* Debounce — track time of last accepted edge */
static volatile int64_t last_edge_us;

/* ============================================================
 *  ISR — runs in interrupt context. IRAM_ATTR avoids the
 *  first-execution cache-fill penalty from flash.
 * ============================================================ */
static void IRAM_ATTR button_isr(void *arg)
{
    int64_t now = esp_timer_get_time();

    /* Debounce: drop edges within DEBOUNCE_US of last accepted one. */
    if (now - last_edge_us < DEBOUNCE_US) return;
    last_edge_us = now;

    /* 1. Toggle the scope output HIGH so the logic analyzer can see ISR entry. */
    gpio_set_level(ISR_PULSE_GPIO, 1);

    isr_entry_time_us = now;
    presses_observed++;

    BaseType_t higher_woken = pdFALSE;

    /* 2. Signal via binary semaphore.
     *    Multiple presses while taken can be LOST — binary sem has no count. */
    xSemaphoreGiveFromISR(btn_sem, &higher_woken);

    /* 3. Signal via direct task notification.
     *    Faster than the semaphore on most ports; one-to-one. */
    vTaskNotifyGiveFromISR(task_notif_handle, &higher_woken);

    /* 4. Toggle scope output LOW — ISR is about to return. */
    gpio_set_level(ISR_PULSE_GPIO, 0);

    /* 5. Request a context switch on ISR exit if a higher-priority task is ready. */
    portYIELD_FROM_ISR(higher_woken); // I took this out for my failure test and restored it for submission as told to do in the directions.
}

/* ============================================================
 *  Bottom-half task: binary-semaphore path
 * ============================================================ */
static void btn_task_sem(void *arg)
{
    for (;;) {
        if (xSemaphoreTake(btn_sem, portMAX_DELAY) == pdTRUE) {
            int64_t wake = esp_timer_get_time();
            int64_t lat = wake - isr_entry_time_us;
            if ((uint64_t)lat > latency_max_sem_us) latency_max_sem_us = (uint64_t)lat;

            /* TODO(YOU): Do the actual themed work here.
             *
             * Avionics:   "RADAR pulse received -> increment hit counter"
             * Medical:    "patient call button —> set alarm bit"
             * Industrial: "E-STOP pressed —> assert safe state"
             * Space:      "ground command —> log + acknowledge"
             * Security:   "tamper sensor —> record event, raise integrity flag"
             *
             * For the scaffold we just log:
             */
            ESP_LOGI(TAG,
         "[COMMAND-SEM] Cockpit command received | command=%lu | latency=%lld us | worst=%llu us",
         (unsigned long)presses_observed,
         (long long)lat,
         (unsigned long long)latency_max_sem_us);
        }
    }
}

/* ============================================================
 *  Bottom-half task: direct-notification path
 * ============================================================ */
static void btn_task_notif(void *arg)
{
    for (;;) {
        uint32_t count = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (count == 0) continue;

        int64_t wake = esp_timer_get_time();
        int64_t lat = wake - isr_entry_time_us;
        if ((uint64_t)lat > latency_max_notif_us) latency_max_notif_us = (uint64_t)lat;

        /* TODO(YOU): same theme-appropriate work as the sem task.
         * (For your final design, you'd use ONE path — but the scaffold gives
         * you both so you can compare wake latency in your README.) */
        ESP_LOGI(TAG,
         "[EMERGENCY-NOTIFY] Flight response activated | event=%lu | latency=%lld us | worst=%llu us | pending=%lu",
         (unsigned long)presses_observed,
         (long long)lat,
         (unsigned long long)latency_max_notif_us,
         (unsigned long)count);
    }
}

#if WITH_LOAD
/* ============================================================
 *  BACKGROUND LOAD  (WITH_LOAD = 1)
 * ============================================================
 *
 * These four tasks are based on App 2's scheduler demo: four
 * periodic tasks pinned to Core 1 on the rate-monotonic ladder. 
 * Here they exist to put Core 1 under realistic contention while you measure ISR
 * response latency. 
 * You are not studying these bodies in App 3 — you are studing what
 * their presence does to your wake latency.
 *
 * Why these default code segments (the same rules App 2 fixed on):
 *   (1) DEAD-CODE ELIMINATION. Each kernel ends by writing a `volatile` sink
 *       and seeds itself from that sink, so -O2/-Os cannot delete the work.
 *   (2) INITIALIZE BUFFERS ONCE. Large buffers are static and filled a single
 *       time in load_init_buffers(), never inside the period.
 *   (3) float, NOT double. The ESP32 FPU is single-precision; double is
 *       software-emulated and runs with data-dependent timing.
 *   (4) WOKWI != SILICON. The *_ITERS / *_N / *_LEN knobs are 240 MHz ballpark.
 *       Tune them if you want a specific load level; the defaults give a light,
 *       comfortably schedulable set (~15-20% utilization).
 *
 * Per-task heartbeat counters and a WCET-max helper are included so you can
 * confirm the load is actually running (heartbeats climbing) and, if you want,
 * report the load's own WCET. Single 32-bit reads are atomic on Xtensa, so the
 * heartbeats need no mutex yet (App 6 changes that).
 */
static volatile uint32_t hb_a, hb_b, hb_c, hb_d;
static uint64_t wcet_a_max_us, wcet_b_max_us, wcet_c_max_us, wcet_d_max_us;

#define MEASURE_WCET(_max_var, _body) do {                       \
    int64_t _t0 = esp_timer_get_time();                          \
    _body;                                                        \
    int64_t _dt = esp_timer_get_time() - _t0;                    \
    if ((uint64_t)_dt > (_max_var)) (_max_var) = (uint64_t)_dt;  \
} while (0)

/* ---- Load Task A  priority 15  period 10 ms : xorshift32 churn (integer) ---- */
#define A_ITERS 300
static volatile uint32_t a_sink;
static void attitude_sensor_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10);
    for (;;) {
        MEASURE_WCET(wcet_a_max_us, {
            uint32_t x = a_sink ? a_sink : 0xACE1u;   /* seed from sink (observable) */
            for (int i = 0; i < A_ITERS; i++) {
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
            }
            a_sink = x;
        });
        hb_a++;
        vTaskDelayUntil(&last, period);
    }
}

/* ---- Load Task B  priority 10  period 20 ms : single-precision FIR ---- */
#define B_SAMP 64                       /* power of two for the index mask */
#define B_TAPS 16                       /* <= B_SAMP */
static float b_buf[B_SAMP];
static float b_coef[B_TAPS];
static volatile float b_sink;
static void flight_control_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(20);
    for (;;) {
        MEASURE_WCET(wcet_b_max_us, {
            float acc = b_sink;          /* seed from sink (observable) */
            for (int n = 0; n < B_SAMP; n++)
                for (int k = 0; k < B_TAPS; k++)
                    acc += b_buf[(n + B_SAMP - k) & (B_SAMP - 1)] * b_coef[k];
            b_sink = acc;
        });
        hb_b++;
        vTaskDelayUntil(&last, period);
    }
}

/* ---- Load Task C  priority 5  period 50 ms : CRC-32 over a buffer ---- */
#define C_LEN 2048                     /* bytes; raise toward 49152 to lengthen */
static uint8_t c_pkt[C_LEN];
static volatile uint32_t c_sink;
static void telemetry_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(50);
    for (;;) {
        MEASURE_WCET(wcet_c_max_us, {
            uint32_t crc = 0xFFFFFFFFu ^ c_sink;     /* seed from sink */
            for (int n = 0; n < C_LEN; n++) {
                crc ^= c_pkt[n];
                for (int b = 0; b < 8; b++)
                    crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
            }
            c_sink = crc ^ 0xFFFFFFFFu;
        });
        hb_c++;
        vTaskDelayUntil(&last, period);
    }
}

/* ---- Load Task D  priority 2  period 100 ms : insertion sort, forced worst case ---- */
#define D_N 200
static int d_arr[D_N];
static volatile int d_sink;
static void health_monitor_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(100);

    uint32_t report_counter = 0;

    for (;;) {
        MEASURE_WCET(wcet_d_max_us, {
            for (int i = 0; i < D_N; i++) {
                d_arr[i] = D_N - i + (d_sink & 1);
            }

            for (int i = 1; i < D_N; i++) {
                int key = d_arr[i];
                int j = i - 1;

                while (j >= 0 && d_arr[j] > key) {
                    d_arr[j + 1] = d_arr[j];
                    j--;
                }

                d_arr[j + 1] = key;
            }

            d_sink = d_arr[D_N / 2];
        });

        hb_d++;
        report_counter++;

        /*
         * Health Monitor runs every 100 ms.
         * Print one report every 10 executions, or approximately once per second.
         */
        if (report_counter >= 10) {
            report_counter = 0;

            ESP_LOGI(TAG,
                     "[HEALTH] Mode=NORMAL | Heartbeats A=%lu B=%lu C=%lu D=%lu",
                     (unsigned long)hb_a,
                     (unsigned long)hb_b,
                     (unsigned long)hb_c,
                     (unsigned long)hb_d);

            ESP_LOGI(TAG,
                     "[TIMING] WCET us | Attitude=%llu | Control=%llu | Telemetry=%llu | Health=%llu",
                     (unsigned long long)wcet_a_max_us,
                     (unsigned long long)wcet_b_max_us,
                     (unsigned long long)wcet_c_max_us,
                     (unsigned long long)wcet_d_max_us);

            ESP_LOGI(TAG,
                     "[ISR] Commands=%lu | Semaphore worst=%llu us | Notification worst=%llu us",
                     (unsigned long)presses_observed,
                     (unsigned long long)latency_max_sem_us,
                     (unsigned long long)latency_max_notif_us);
        }

        vTaskDelayUntil(&last, period);
    }
}
/* Fill the load buffers exactly once, off the periodic path. */
static void load_init_buffers(void)
{
    for (int i = 0; i < B_SAMP; i++) b_buf[i]  = (float)((i * 2654435761u) & 0xFFFF) / 65536.0f;
    for (int k = 0; k < B_TAPS; k++) b_coef[k] = 1.0f / (float)B_TAPS;   /* boxcar */
    for (int n = 0; n < C_LEN;  n++) c_pkt[n]  = (uint8_t)(n * 31 + 7);
}

static void start_avionics_tasks(void)
{
    load_init_buffers();
    /* Rate-monotonic ladder, all on Core 1, mirroring App 2. These priorities
     * are FIXED here (this is a load fixture). Note A=15 outranks your
     * bottom-half tasks (12); B/C/D do not. */
   xTaskCreatePinnedToCore(
    attitude_sensor_task,
    "AttitudeSensor",
    2048,
    NULL,
    15,
    NULL,
    APP_CPU_NUM
);

xTaskCreatePinnedToCore(
    flight_control_task,
    "FlightControl",
    4096,
    NULL,
    10,
    NULL,
    APP_CPU_NUM
);

xTaskCreatePinnedToCore(
    telemetry_task,
    "Telemetry",
    4096,
    NULL,
    5,
    NULL,
    APP_CPU_NUM
);

xTaskCreatePinnedToCore(
    health_monitor_task,
    "HealthMonitor",
    8192,
    NULL,
    2,
    NULL,
    APP_CPU_NUM
);
}
#endif /* WITH_LOAD */

/* ============================================================
 *  app_main — wire everything up
 * ============================================================ */
void app_main(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "================================================");
ESP_LOGI(TAG, " AEGIS AVIONICS FLIGHT CONTROL SYSTEM");
ESP_LOGI(TAG, " FreeRTOS scheduling, ISR response, and monitoring");
ESP_LOGI(TAG, "================================================");
#if WITH_LOAD
    ESP_LOGI(TAG, "Mode: NORMAL — avionics task set active on Core 1");
#else
   ESP_LOGI(TAG, "Mode: TEST — emergency-response tasks only");
#endif

    /* Create signaling primitives. */
    btn_sem = xSemaphoreCreateBinary();

    /* Bottom-half tasks. Both pinned to Core 1, both high priority because
     * they're the "real-time response" path. */
    xTaskCreatePinnedToCore(btn_task_sem,  "btn_sem",   8192, NULL, 12, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(btn_task_notif,"btn_notif", 8192, NULL, 12,
                            &task_notif_handle, APP_CPU_NUM);

#if WITH_LOAD
    /* Bring App 2's periodic tasks online as a Core-1 load fixture. */
    start_avionics_tasks();
#endif

    /* Configure GPIOs. */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,    /* button pulls low when pressed */
    };
    gpio_config(&btn_cfg);

    gpio_config_t pulse_cfg = {
        .pin_bit_mask = 1ULL << ISR_PULSE_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0, .pull_down_en = 0, .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pulse_cfg);
    gpio_set_level(ISR_PULSE_GPIO, 0);

    /* Install GPIO ISR service. Flags = 0 means default (low) priority. */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);

    ESP_LOGI(TAG,
         "Cockpit command input ready on GPIO %d | ISR pulse output on GPIO %d",
         BUTTON_GPIO,
         ISR_PULSE_GPIO);
ESP_LOGI(TAG, "System Status:");
ESP_LOGI(TAG, "  • Attitude Sensor ........ ONLINE");
ESP_LOGI(TAG, "  • Flight Controller ...... ONLINE");
ESP_LOGI(TAG, "  • Telemetry .............. ONLINE");
ESP_LOGI(TAG, "  • Health Monitor ......... ONLINE");
ESP_LOGI(TAG, "  • Emergency Response ..... READY");
ESP_LOGI(TAG, "------------------------------------------------");
    /* app_main returns; tasks continue. */
}
