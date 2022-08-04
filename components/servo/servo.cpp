#include "servo.h"
#include "esp_sleep.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

const char* Servo::SERVO_BASE="Servo";

#define NUM_TIME_SLICES 5
#define MS_PER_DEGREE 1.6666666666666667
static const double time_distribution[NUM_TIME_SLICES] = {
    0.25,
    0.20,
    0.10,
    0.20,
    0.25
};

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

bool Servo::isPwmInitialized = false;

Servo::Servo(AppConfig* config)
    :ManagedDevice(SERVO_BASE,config->GetStringProperty("name"),NULL,&ProcessCommand),
    config(config),
    pinNo(config->GetPinNoProperty("pinNo")),
    SERVO_MIN_PULSEWIDTH_US(config->GetIntProperty("SERVO_MIN_PULSEWIDTH_US")),
    SERVO_MAX_PULSEWIDTH_US(config->GetIntProperty("SERVO_MAX_PULSEWIDTH_US")),
    SERVO_MAX_DEGREE(config->GetIntProperty("SERVO_MAX_DEGREE")),
    SERVO_PWM_FREQUENCY(config->GetIntProperty("SERVO_PWM_FREQUENCY")),
    isRunning(false)
{
    AppConfig* appstate = new AppConfig(status,AppConfig::GetAppStatus());
    appstate->SetStringProperty("name",name);
    appstate->SetIntProperty("pinNo",pinNo);
    if ((SERVO_MIN_PULSEWIDTH_US != -1) &&
        (SERVO_MAX_PULSEWIDTH_US != -1) &&
        (SERVO_MAX_DEGREE != -1) &&
        (SERVO_PWM_FREQUENCY != -1) &&
        (pinNo != -1)) {

        cJSON* methods = cJSON_AddArrayToObject(appstate->GetJSONConfig(NULL),"commands");
        cJSON* flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","setTargetAngle");
        cJSON_AddStringToObject(flush,"className","Servo");
        cJSON_AddStringToObject(flush,"name",name);
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"param1_label","angle");
        cJSON_AddNumberToObject(flush,"param1",0);
        cJSON_AddBoolToObject(flush,"param1_editable",true);
        cJSON_AddStringToObject(flush,"param2_label","duration");
        cJSON_AddNumberToObject(flush,"param2",0);
        cJSON_AddBoolToObject(flush,"param2_editable",true);
        cJSON_AddStringToObject(flush,"caption","Set target angle");

        appstate->SetIntProperty("currentAngle",0);    
        appstate->SetIntProperty("targetAngle",0);    
        appstate->SetIntProperty("duration",0);    
        currentAngle = appstate->GetPropertyHolder("currentAngle");
        targetAngle = appstate->GetPropertyHolder("targetAngle");
        duration = appstate->GetPropertyHolder("duration");
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
        this->InitDevice();
    } else {
        appstate->SetIntProperty("currentAngle",-1);    
        ESP_LOGE(__FUNCTION__,"Bad config, noting innited");
    }
    delete appstate;
}

cJSON* Servo::BuildConfigTemplate() {
    cJSON* commandTemplate = ManagedDevice::BuildConfigTemplate();
    cJSON_SetValuestring(cJSON_GetObjectItem(commandTemplate,"class"),"Servo");
    cJSON_AddTrueToObject(commandTemplate, "isArray");
    cJSON_AddStringToObject(commandTemplate,"collectionName","Servos");
    cJSON_AddStringToObject(commandTemplate,"name","New Servo");
    cJSON_AddNumberToObject(commandTemplate,"pinNo",1);
    cJSON_AddNumberToObject(commandTemplate,"SERVO_MIN_PULSEWIDTH_US",1);
    cJSON_AddNumberToObject(commandTemplate,"SERVO_MAX_PULSEWIDTH_US",1);
    cJSON_AddNumberToObject(commandTemplate,"SERVO_MAX_DEGREE",1);
    cJSON_AddNumberToObject(commandTemplate,"SERVO_PWM_FREQUENCY",1);
    return commandTemplate;
}

EventHandlerDescriptor* Servo::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"Pin(%d):%s BuildHandlerDescriptors",pinNo,name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(1,"setTargetAngle",event_data_type_tp::JSON);
  return handler;
}


uint32_t Servo::servo_angle_to_duty_us(int angle)
{
    return (SERVO_MIN_PULSEWIDTH_US + (((SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) * (angle)) / (SERVO_MAX_DEGREE)));
}

void Servo::InitDevice(){
    ESP_LOGI(__FUNCTION__,"Initialising servo at pin %d",this->pinNo);

    ESP_ERROR_CHECK(mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, pinNo));

    if (!isPwmInitialized){
        mcpwm_config_t pwm_config = {
            .frequency = SERVO_PWM_FREQUENCY, // frequency = 50Hz, i.e. for every servo motor time period should be 20ms
            .cmpr_a = 0,     // duty cycle of PWMxA = 0
            .cmpr_b = 0,     // duty cycle of PWMxb = 0
            .duty_mode = MCPWM_DUTY_MODE_0,
            .counter_mode = MCPWM_UP_COUNTER,
        };
        mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
        isPwmInitialized = true;
        ESP_LOGI(__FUNCTION__,"Servo PWM init done");
    }

    ESP_LOGI(__FUNCTION__,"Servo pin %d initialised",this->pinNo);
    CreateBackgroundTask(servoThread,this->name, 4096, this, tskIDLE_PRIORITY, NULL);
}

bool Servo::ProcessCommand(ManagedDevice* servo, cJSON * parms) {
    if (strcmp(servo->GetName(), cJSON_GetObjectItem(parms,"name")->valuestring) == 0) {
        Servo* s = (Servo*) servo;
        int angle = 0;
        int dur = -1;
        cJSON* jangle = cJSON_GetObjectItem(parms,"param1");
        if (jangle->valuestring != NULL) {
            angle = std::stoi(jangle->valuestring);
        } else {
            angle = jangle->valueint;
        }
        ESP_LOGI(__FUNCTION__,"Processing angle request for %s angle:%d",servo->GetName(), angle);
        if (angle < 0) {
            angle = 0;
        }
        if (angle > s->SERVO_MAX_DEGREE) {
            angle = s->SERVO_MAX_DEGREE;
        }
        cJSON_SetIntValue(s->targetAngle,angle);

        cJSON* jdur= cJSON_GetObjectItem(parms,"param2");
        if (jdur) {
            if (jdur->valuestring != NULL) {
                cJSON_SetNumberValue(s->duration, std::stoi(jdur->valuestring));
            } else {
                cJSON_SetNumberValue(s->duration, jdur->valueint);
            }
        }
        AppConfig::SignalStateChange(state_change_t::MAIN);
        return true;
    }
    return false;
}

void Servo::servoThread(void* instance) {
    Servo* servo = (Servo*)instance;
    servo->isRunning = true;
    cJSON_SetIntValue(servo->currentAngle, 0);
    cJSON_SetIntValue(servo->targetAngle, 0);
    ESP_LOGI(__FUNCTION__, "Angle of rotation: %d/%d", servo->currentAngle->valueint,servo->servo_angle_to_duty_us(servo->currentAngle->valueint));
    ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo->servo_angle_to_duty_us(servo->currentAngle->valueint)));
    EventGroupHandle_t stateEg = AppConfig::GetStateGroupHandle();
    EventBits_t bits = 0;

    while (servo->isRunning) {
        bits = xEventGroupWaitBits(stateEg,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
        if (bits & state_change_t::MAIN) {
            if (servo->targetAngle->valueint != servo->currentAngle->valueint) {
                ESP_LOGI(__FUNCTION__, "Angle of rotation: %d/%d in %dms", servo->targetAngle->valueint,servo->servo_angle_to_duty_us(servo->targetAngle->valueint),servo->duration->valueint);

                if (servo->duration->valueint == 0) {
                    ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo->servo_angle_to_duty_us(servo->targetAngle->valueint)));
                    vTaskDelay(pdMS_TO_TICKS((abs(servo->targetAngle->valueint - servo->currentAngle->valueint) / 60) * 100 )); //Add delay, since it takes time for servo to rotate, generally 100ms/60degree rotation under 5V power supply
                    cJSON_SetIntValue(servo->currentAngle, servo->targetAngle->valueint);
                } else {
                    uint32_t timeLeft = servo->duration->valueint;
                    uint32_t totalAngle = abs(servo->targetAngle->valueint - servo->currentAngle->valueint);
                    uint32_t sliceSize = totalAngle / NUM_TIME_SLICES;
                    int direction = (servo->targetAngle->valueint > servo->currentAngle->valueint) ? 1 : -1;
                    TickType_t xLastWakeTime = 0;
                    ESP_LOGV(__FUNCTION__,"timeleft:%d, totalAngle:%d, sliceSize:%d",timeLeft,totalAngle,sliceSize);
                    for (int i = 0; i < NUM_TIME_SLICES; i++) {
                        uint32_t sliceTime = max(1,servo->duration->valueint * time_distribution[i]);
                        uint32_t stepAngle = max(1, sliceSize / (sliceTime/10));
                        TickType_t sliceTicks = pdMS_TO_TICKS(sliceTime);
                        ESP_LOGV(__FUNCTION__,"slicetime:%d,stepAngle:%d,sliceTicks:%d",sliceTime,stepAngle,sliceTicks);
                        TickType_t stepStartTime = xTaskGetTickCount();
                        for (int x = 0; x < sliceSize; x+=stepAngle) {
                            xLastWakeTime = xTaskGetTickCount();
                            TickType_t expEndTime = stepStartTime +  (sliceTicks * ((x+stepAngle) / (sliceSize*1.0)));
                            ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo->servo_angle_to_duty_us(servo->currentAngle->valueint + (x * direction))));
                            if (expEndTime > xLastWakeTime) {
                                vTaskDelayUntil(&xLastWakeTime, expEndTime - xLastWakeTime);
                            }
                        }
                        cJSON_SetIntValue(servo->currentAngle, servo->currentAngle->valueint + (sliceSize * direction));
                    }
                    ESP_ERROR_CHECK(mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, servo->servo_angle_to_duty_us(servo->targetAngle->valueint)));
                    cJSON_SetIntValue(servo->currentAngle, servo->targetAngle->valueint);
                }
                AppConfig::SignalStateChange(state_change_t::MAIN);
            }
        }
    }
}
