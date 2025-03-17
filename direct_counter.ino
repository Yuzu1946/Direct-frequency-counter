// 23 Dec 2024
// Sourcecode for ESP32 dev module 
// 24 Dec 2024
// Add comments
// 4 Feb 2024
// Remove accum_count
// Add PCNT unit_stop

// Hardware APIs
#include "driver/pulse_cnt.h"
#include "driver/ledc.h"
#include "esp_timer.h"

// PCNT
#define PCNT_LOW_LIMIT  -32768 // -2^16 : min limit of PCNT (16-bit counter)
#define PCNT_HIGH_LIMIT  32767 // 2^16 - 1 : max limit

#define CHAN_GPIO_A   34  // Edge triggering : PCNT counts up/down when rising/falling edge is detected from GPIO 34
#define CHAN_GPIO_B   -1  // Level triggering : not used in this project

int        pulse_count  =  0; // to be used with pcnt_unit_get_count()
// int        accum_count  =  0; // for storing value that PCNT reads beyond 32767 
double     frequency    =  0; // the calculated frequency (double: up to 15 decimal places)

pcnt_unit_handle_t pcnt_unit = NULL;

// LEDC
#define LEDC_HS_CH0_GPIO      33          // PWM signal output at GPIO 33

uint32_t   osc_freq     =  13000000;      // internal oscillator frequency: 1 Hz - 40 MHz
uint32_t   mDuty        =  0;             // duty cycle
uint32_t   resolution   =  0;             

// ESP timer (High Resolution Timer)
uint64_t period = 2000000; // gate time of 2 second

// Status
bool ready = false; // status for calculating frequency 

// Callbacks
void timer_callback(void *arg) {
  ready = true;
}

// static bool pcnt_event_callback(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
//   accum_count += PCNT_HIGH_LIMIT;
//   return true; // to indicate that the event has been handled
// }

// Set up all modules
void init_osc_freq() {                                                    // Initialize Oscillator to test Freq Meter
  resolution = (log (80000000 / osc_freq)  / log(2)) / 2 ;                // Calc of resolution of Oscillator
  if (resolution < 1) resolution = 1;                                     // set min resolution 
  mDuty = (pow(2, resolution)) / 2;                                       // Calc of Duty Cycle 50% of the pulse
 
  ledc_timer_config_t ledc_timer = {                                      // LEDC timer config instance
    .speed_mode = LEDC_HIGH_SPEED_MODE,                                   // ** ESP32-S3 cannot use high speed mode
    .duty_resolution =  ledc_timer_bit_t(resolution),                     // Set resolution
    .timer_num = LEDC_TIMER_0,                                            // Set LEDC timer index - 0
    .freq_hz    = osc_freq,                                               // Set Oscillator frequency
  };                                                              
  ledc_timer_config(&ledc_timer);                                         // Set LEDC Timer config

  ledc_channel_config_t ledc_channel = {                                  // LEDC Channel config instance
    .gpio_num   = LEDC_HS_CH0_GPIO,                                       // LEDC Oscillator output GPIO 33
    .speed_mode = LEDC_HIGH_SPEED_MODE,                                   // Set LEDC high speed mode
    .channel    = LEDC_CHANNEL_0,                                         // Set HS Channel - 0
    .intr_type  = LEDC_INTR_DISABLE,                                      // LEDC Fade interrupt disable
    .timer_sel  = LEDC_TIMER_0,                                           // Set timer source of channel - 0
    .duty       = mDuty,                                                  // Set Duty Cycle 50%
  };                                
  ledc_channel_config(&ledc_channel);                                     // Config LEDC channel
}

void init_pcnt(){
  // pcnt_unit_config_t unit_config = {
  //   .low_limit = PCNT_LOW_LIMIT,
  //   .high_limit = PCNT_HIGH_LIMIT,
  // };

  pcnt_unit_config_t unit_config = {
    .low_limit = PCNT_LOW_LIMIT,
    .high_limit = PCNT_HIGH_LIMIT,
    .flags = {
      .accum_count = true
    }
  };
  pcnt_new_unit(&unit_config, &pcnt_unit); // PCNT unit allocation and initialization

  pcnt_chan_config_t chan_config = {
    .edge_gpio_num = CHAN_GPIO_A,
    .level_gpio_num = CHAN_GPIO_B,
  };
  pcnt_channel_handle_t pcnt_chan = NULL;
  pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan); // attach edge & level input signal from GPIO to PCNT channel

  // increase the counter on rising edge, hold the counter on falling edge
  pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);

  pcnt_unit_add_watch_point(pcnt_unit, PCNT_HIGH_LIMIT); // when count reaches PCNT_HIGH_LIMIT, pcnt_event_callback is called

  // pcnt_event_callbacks_t callbacks = {
  //   .on_reach = pcnt_event_callback, // function to be called when pulse_count reaches the watch point value
  // };
  // pcnt_unit_register_event_callbacks(pcnt_unit, &callbacks, NULL); // register PCNT callback

  pcnt_unit_enable(pcnt_unit);
  pcnt_unit_clear_count(pcnt_unit); 
  pcnt_unit_start(pcnt_unit); // *** tell PCNT to start counting pulses
}

void init_esp_timer(){
  esp_timer_create_args_t create_args = {
    .callback = timer_callback       // function to be called when the timer expires
  };
  esp_timer_handle_t timer;  
  esp_timer_create(&create_args, &timer);

  esp_timer_start_periodic(timer, period); // make esp timer works in periodic mode
}

void setup() {

  Serial.begin(9600);

  init_osc_freq();
  init_pcnt();
  init_esp_timer();
  
}

void loop() {
  if (ready) {
    pcnt_unit_stop(pcnt_unit);
    pcnt_unit_get_count(pcnt_unit, &pulse_count);
    frequency = pulse_count/(period*1e-6) ; // calculate freq for 1 second gate time
    // frequency = (pulse_count + accum_count)/(period*1e-6) ; // calculate freq for 1 second gate time
    Serial.println(frequency, 2);

    // reset
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
    // accum_count = 0;
    ready = false;
  }
}
