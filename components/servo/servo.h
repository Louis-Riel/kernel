#ifndef __servo_h
#define __servo_h

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/mcpwm.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "cJSON.h"
#include "eventmgr.h"

class Servo:ManagedDevice {
public:
    Servo(AppConfig* config);
    ~Servo();

protected:
    static const char* SERVO_BASE;
    AppConfig* config;
    char* name;
    gpio_num_t pinNo;
    uint32_t SERVO_MIN_PULSEWIDTH_US;   // Minimum pulse width in microsecond
    uint32_t SERVO_MAX_PULSEWIDTH_US;   // Maximum pulse width in microsecond
    uint32_t SERVO_MAX_DEGREE;          // Maximum angle in degree upto which servo can rotate
    uint32_t SERVO_PWM_FREQUENCY;       // Frequency pulse width in microsecond; ex: 50Hz, i.e. for every servo motor time period should be 20ms
    cJSON* currentAngle;
    cJSON* targetAngle;
    cJSON* duration;
    bool isRunning;

    void InitDevice();
    static char* getName(AppConfig* config);
    static bool isPwmInitialized;
    static void servoThread(void* instance);
    uint32_t servo_angle_to_duty_us(int angle);
    EventHandlerDescriptor* BuildHandlerDescriptors();

    static bool ProcessCommand(ManagedDevice*, cJSON *);
};

#endif