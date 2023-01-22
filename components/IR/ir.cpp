#include "ir.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#define RMT_CLK_DIV 80
#define RMT_TICK_10_US    (80000000/RMT_CLK_DIV/100000)   /*!< RMT counter value for 10 us.(Source clock is APB clock) */
#define RMT_RX_BUF_SIZE 1000
#define RMT_RX_BUF_WAIT 10
#define RMT_ITEM_DURATION(d)  ((d & 0x7fff)*10/RMT_TICK_10_US)  /*!< Parse duration time from memory register value */
#define RMT_FILTER_THRESH 100 // ticks
#define RMT_IDLE_TIMEOUT 8000 // ticks

const char* emptySpaces = "                                       ";
const char* IRDecoder::IRDECODER_BASE="IRDecoder";

static bool isRunning=false;

/*
    typedef struct {
        const char* tag;
        uint16_t carrier_freq_hz;
        uint8_t duty_cycle;
        uint8_t bit_length;
        bool invert;
        uint16_t header_mark_us;
        uint16_t header_space_us;
        uint16_t one_mark_us;
        uint16_t one_space_us;
        uint16_t zero_mark_us;
        uint16_t zero_space_us;
    } rmt_timing_t;

*/
IRDecoder::rmt_timing_t IRDecoder::timing_groups[] = {
    {"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {"HiSense", 38000, 33, 28, 0, 9000, 4200, 650, 1600, 650, 450},
    {"NEC",     38000, 33, 32, 0, 9000, 4250, 560, 1690, 560, 560},
    {"LG",      38000, 33, 28, 0, 9000, 4200, 550, 1500, 550, 550},
    {"samsung", 38000, 33, 32, 0, 4600, 4400, 650, 1500, 553, 453},
    {"LG32",    38000, 33, 32, 0, 4500, 4500, 500, 1750, 500, 560}
};

IRDecoder::~IRDecoder(){
    ldfree((void*)name);
    EventManager::UnRegisterEventHandler(handlerDescriptors);
    ESP_LOGI(__PRETTY_FUNCTION__,"Destructor");
}

IRDecoder::IRDecoder(AppConfig* config)
    :ManagedDevice(IRDECODER_BASE),
    pinNo(config->GetPinNoProperty("pinNo")),
    channelNo((rmt_channel_t)config->GetIntProperty("channelNo")),
    config(config),
    rb(NULL),
    timingGroup(NULL)
{
    isRunning=true;
    AppConfig* apin = new AppConfig(status,AppConfig::GetAppStatus());
    if (!pinNo || (channelNo > RMT_CHANNEL_MAX)) {
        apin->SetStringProperty("error","Invalid IRDecoder Configuration Pin");
        ESP_LOGW(__PRETTY_FUNCTION__,"Invalid IRDecoder Configuration Pin:%d Channel:%d",pinNo,channelNo);
        return;
    }
    EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
    ESP_LOGV(__PRETTY_FUNCTION__,"IRDecoder(%d):%s",pinNo,name);
    apin->SetPinNoProperty("pinNo",pinNo);
    apin->SetIntProperty("numCodes",0);
    apin->SetStringProperty("lastCode",emptySpaces);
    apin->SetStringProperty("lastProvider",emptySpaces);
    lastCode = apin->GetPropertyHolder("lastCode");
    numCodes = apin->GetPropertyHolder("numCodes");
    lastProvider = apin->GetPropertyHolder("lastProvider");
    delete apin;
    CreateBackgroundTask(IRPoller, "IRPoller", 4096, this, tskIDLE_PRIORITY, NULL);
}

bool IRDecoder::IsRunning(){
    return isRunning;
}

EventHandlerDescriptor* IRDecoder::BuildHandlerDescriptors(){
  ESP_LOGV(__PRETTY_FUNCTION__,"Pin(%d):%s BuildHandlerDescriptors",pinNo,name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(eventIds::CODE,"CODE",event_data_type_tp::JSON);
  return handler;
}

void IRDecoder::IRPoller(void *arg){
    IRDecoder* ir = (IRDecoder*)arg;
    uint32_t addr = 0;
    uint32_t cmd = 0;
    size_t length = 0;
    bool repeat = false;
    rmt_item32_t *items = NULL;

    gpio_pullup_en(ir->pinNo);
    rmt_config_t rmt_rx_config = RMT_DEFAULT_CONFIG_RX(ir->pinNo, ir->channelNo);
    ESP_ERROR_CHECK(rmt_config(&rmt_rx_config));
    ESP_ERROR_CHECK(rmt_driver_install(ir->channelNo, 1000, 0));
    ESP_ERROR_CHECK(rmt_get_ringbuf_handle(ir->channelNo, &ir->rb));
    int numReq=0;
    if (ir->rb != NULL) {
        rmt_rx_start(ir->channelNo, true);
        uint32_t code=0;
        while ((items=(rmt_item32_t *) xRingbufferReceive(ir->rb, &length, portMAX_DELAY))) {
            if ((code=ir->read(items,length))){
                length /= 4;
                postedEvent_t pevent;
                pevent.base=ir->IRDECODER_BASE;
                pevent.id=eventIds::CODE;
                pevent.event_data=ir->status;
                pevent.eventDataType=event_data_type_tp::JSON;
                EventManager::ProcessEvent(ir, &pevent);
            }
            vRingbufferReturnItem(ir->rb, (void *) items);
        }
        vRingbufferDelete(ir->rb);
        rmt_driver_uninstall(ir->channelNo);
    } else {
        ESP_LOGW(__PRETTY_FUNCTION__,"Weirdness s afoot.");
    }
    ESP_LOGI(__PRETTY_FUNCTION__, "IR Reader done");
}

int8_t IRDecoder::available()
{
   if (!isRunning) return -1;
   UBaseType_t waiting;
   vRingbufferGetInfo(rb, NULL, NULL, NULL, NULL, &waiting);
   ESP_LOGV(__PRETTY_FUNCTION__,"waiting %d", waiting);
   return waiting;
} 

bool IRDecoder::rx_check_in_range(int duration_ticks, int target_us)
{
    if(( RMT_ITEM_DURATION(duration_ticks) < (target_us + _margin_us))
        && ( RMT_ITEM_DURATION(duration_ticks) > (target_us - _margin_us))) {
        return true;
    }
    return false;
}

bool IRDecoder::rx_header_if(rmt_item32_t* item, uint8_t timing)
{
    ESP_LOGV(__PRETTY_FUNCTION__,"%sif(rx_check_in_range(item->duration0 %d, timing_groups[timing].header_mark_us %d)%d\
        && rx_check_in_range(item->duration1 %d, timing_groups[timing].header_space_us %d)%d)",
            timing_groups[timing].tag,
            item->duration0, timing_groups[timing].header_mark_us,
            rx_check_in_range(item->duration0, timing_groups[timing].header_mark_us),
            item->duration1, timing_groups[timing].header_space_us,
            rx_check_in_range(item->duration1, timing_groups[timing].header_space_us));
    if(rx_check_in_range(item->duration0, timing_groups[timing].header_mark_us)
        && rx_check_in_range(item->duration1, timing_groups[timing].header_space_us)) {
            ESP_LOGV(__PRETTY_FUNCTION__,"The header exists for %s",timing_groups[timing].tag);
        return true;
    }
    return false;
}

bool IRDecoder::rx_bit_one_if(rmt_item32_t* item, uint8_t timing)
{
    ESP_LOGV(__PRETTY_FUNCTION__,"%s if( rx_check_in_range(item->duration0 %d, timing_groups[timing].one_mark_us %d)%d \
        && rx_check_in_range(item->duration1 %d, timing_groups[timing].one_space_us %d) %d)",
            timing_groups[timing].tag,
            item->duration0, timing_groups[timing].one_mark_us,
            rx_check_in_range(item->duration0, timing_groups[timing].one_mark_us),
            item->duration1, timing_groups[timing].one_space_us,
            rx_check_in_range(item->duration1, timing_groups[timing].one_space_us)); 
    if( rx_check_in_range(item->duration0, timing_groups[timing].one_mark_us)
        && rx_check_in_range(item->duration1, timing_groups[timing].one_space_us)) {
        return true;
    }
    return false;
}

bool IRDecoder::rx_bit_zero_if(rmt_item32_t* item, uint8_t timing)
{
    ESP_LOGV(__PRETTY_FUNCTION__,"%s if( rx_check_in_range(item->duration0 %d, timing_groups[timing].zero_mark_us %d)%d \
        && rx_check_in_range(item->duration1 %d, timing_groups[timing].zero_space_us %d)%d)",
            timing_groups[timing].tag,
            item->duration0, timing_groups[timing].zero_mark_us,
            rx_check_in_range(item->duration0, timing_groups[timing].zero_mark_us),
            item->duration1, timing_groups[timing].zero_space_us,
            rx_check_in_range(item->duration1, timing_groups[timing].zero_space_us));
    if( rx_check_in_range(item->duration0, timing_groups[timing].zero_mark_us)
        && rx_check_in_range(item->duration1, timing_groups[timing].zero_space_us)) {
        return true;
    }
    return false;
}

uint32_t IRDecoder::rx_parse_items(rmt_item32_t* item, int item_num, uint8_t timing)
{
    int w_len = item_num;
    if(w_len < timing_groups[timing].bit_length + 2) {
        return 0;
    }
    if(!rx_header_if(item++, timing)) {
        ESP_LOGV(__PRETTY_FUNCTION__,"No rx header");
        return 0;
    }
    ESP_LOGV(__PRETTY_FUNCTION__,"Found a %s header",timing_groups[timing].tag);
    uint32_t data = 0;
    for(uint8_t j = 0; j < timing_groups[timing].bit_length; j++) {
        if(rx_bit_one_if(item, timing)) {
            data <<= 1;
            data += 1;
        } else if(rx_bit_zero_if(item, timing)) {
            data <<= 1;
        } else {
            ESP_LOGV(__PRETTY_FUNCTION__,"No rx_bit_zero_if at idx %d",j);
            return 0;
        }
        item++;
    }
    return data;
}

void dump_item(rmt_item32_t* item, size_t sz)
{
  for (int x=0; x<sz; x++) {
    ESP_LOGW(__PRETTY_FUNCTION__,"Count: %d  duration0: %d  duration1: %d\n", x,item[x].duration0,item[x].duration1);
    if(item[x].duration1==0 || item[x].duration0 == 0 || item[x].duration1 > 0x7f00 || item[x].duration0 > 0x7f00) break;
  }
}
 
uint32_t IRDecoder::read(rmt_item32_t* item, size_t rx_size)
{
    if (!item) return 0;
    uint32_t rx_data;
    uint8_t found_timing = 0;
    for (uint8_t timing : _preferred) {
        rx_data = rx_parse_items(item, rx_size / 4, timing);
        if (rx_data) {
            found_timing = timing;
            ESP_LOGV(__PRETTY_FUNCTION__,"Found timing from prefered %s",(char*) timing_groups[found_timing].tag);
            break;
        }
    }
    if (!rx_data) {
        uint8_t groupCount = sizeof(timing_groups)/sizeof(timing_groups[0]);
        for (uint8_t timing = 0; timing < groupCount; timing++) {
            if (!inPrefVector(timing)) {
                rx_data = rx_parse_items(item, rx_size / 4, timing);
            }
            if (rx_data) {
                found_timing = timing;
                ESP_LOGV(__PRETTY_FUNCTION__,"Found timing %s",(char*) timing_groups[found_timing].tag);
                break;
            }
        }
    }
    if (found_timing) {
        timingGroup = (char*) timing_groups[found_timing].tag;
        sprintf(lastCode->valuestring,"0x%04x", rx_data);
        strcpy(lastProvider->valuestring,timingGroup);
        cJSON_SetIntValue(numCodes,numCodes->valueint+1);

    } else {
        ESP_LOGV(__PRETTY_FUNCTION__,"No timing found");
    }
    return rx_data;
}    

void IRDecoder::setMargin(uint16_t margin_us) {_margin_us = margin_us;}

uint8_t IRDecoder::timingGroupElement(const char* tag)
{
   uint8_t counter = 0;
   for (IRDecoder::rmt_timing_t timing : timing_groups) {
       if(timing.tag == tag) return counter;
       counter++;
   }
   return 0;
}

bool IRDecoder::inPrefVector(uint8_t element)
{
    for (int x : _preferred) if (x == element) return true;
    return false;
}

int IRDecoder::setPreferred(const char* timing_group)
{
    if(timing_group == NULL) {
        _preferred.clear();
        return 0;
    }
    int position = timingGroupElement(timing_group);
    if(position < 0) {
        return -1;
    } else { 
        if (!inPrefVector(position)) _preferred.push_back(position);
        return _preferred.size();
    } 
}
