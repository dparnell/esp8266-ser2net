/*
  ESP8266 PWM code for Arduino by Daniel Parnell 2nd of May 2015
  
  This code is butchered from the official ESP8266 SDK to make it work in a more Arduino like way.
  It also allows any GPIO ping to be used for PWM, rather than the hard coded 3 pins in the original.  
*/
#ifndef __ESP8266_pwm_h__
#define __ESP8266_pwm_h__

// bring in the ESP8266 hardware stuff
extern "C" {
  #include <ets_sys.h>
  #include <osapi.h>
  #include <os_type.h>
  #include <user_interface.h>
  #include <gpio.h>
}

#define PWM_CHANNELS 3
#define PWM_1S 1000000
#define PWM_MAX_DUTY 255

#define PWM_0_OUT_IO_MUX PERIPHS_IO_MUX_MTDI_U
#define PWM_1_OUT_IO_MUX PERIPHS_IO_MUX_MTDO_U
#define PWM_2_OUT_IO_MUX PERIPHS_IO_MUX_MTCK_U

#define os_timer_arm_us(a, b, c) ets_timer_arm_new(a, b, c, 0)

struct pwm_single_param {
    os_timer_t pwm_timer;
    uint16 h_time;
    uint8 duty;
};

struct pwm_period_param {
    os_timer_t pwm_timer;
    uint16 period;
    uint16 freq;
};

#define PWM_OUTPUT_HIGH(pwm_out_io_num)  \
    gpio_output_set(1<<pwm_out_io_num, 0, 1<<pwm_out_io_num, 0)

#define PWM_OUTPUT_LOW(pwm_out_io_num)  \
    gpio_output_set(0, 1<<pwm_out_io_num, 1<<pwm_out_io_num, 0)

LOCAL struct pwm_single_param pwm_single[PWM_CHANNELS];
LOCAL struct pwm_period_param pwm_period;
LOCAL uint8 pwm_out_io_num[PWM_CHANNELS];

LOCAL uint8_t pwm_count = 0;

/******************************************************************************
 * FunctionName : pwm_output_low
 * Description  : each channel's high level timer function,
 *                after reach the timer, output low level.
 * Parameters   : uint8 channel : channel index
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
pwm_output_low(uint8 channel)
{
    PWM_OUTPUT_LOW(pwm_out_io_num[channel]);
}

/******************************************************************************
 * FunctionName : pwm_period_timer
 * Description  : pwm period timer function, output high level,
 *                start each channel's high level timer
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
pwm_period_timer(void)
{
    uint8 i;

    ETS_INTR_LOCK();

    for (i = 0; i < pwm_count; i++) {
        if (pwm_single[i].h_time != 0) {
            PWM_OUTPUT_HIGH(pwm_out_io_num[i]);

            if (pwm_single[i].h_time != pwm_period.period) {
                os_timer_disarm(&pwm_single[i].pwm_timer);
                os_timer_arm_us(&pwm_single[i].pwm_timer, pwm_single[i].h_time, 0);
            }
        } else {
            PWM_OUTPUT_LOW(pwm_out_io_num[i]);
        }
    }

    ETS_INTR_UNLOCK();
}

class ESP8266_PWM {
  private:
  public:
    void connect(uint8_t index, uint8_t pin) {
      if(index < PWM_CHANNELS) {
        switch(index) {
        case 0:
          PIN_FUNC_SELECT(PWM_0_OUT_IO_MUX, pin);
          break;
        case 1:
          PIN_FUNC_SELECT(PWM_1_OUT_IO_MUX, pin);
          break;
        case 2:
          PIN_FUNC_SELECT(PWM_2_OUT_IO_MUX, pin);
          break;
        }
        pwm_out_io_num[index] = pin;
      }
    }
    
    void begin(uint8_t count, uint16_t freq) {
      int i;
      
      pwm_count = count;
      system_timer_reinit();
      
      if (freq > 500) {
        pwm_period.freq = 500;
      } else if (freq < 1) {
        pwm_period.freq = 1;
      } else {
        pwm_period.freq = freq;
      }

      pwm_period.period = PWM_1S / pwm_period.freq;
      
      /* init each channel's high level timer of pwm. */
      for (i = 0; i < pwm_count; i++) {
          os_timer_disarm(&pwm_single[i].pwm_timer);
          os_timer_setfn(&pwm_single[i].pwm_timer, (os_timer_func_t *)pwm_output_low, (void*)i);
      }
  
      /* init period timer of pwm. */
      os_timer_disarm(&pwm_period.pwm_timer);
      os_timer_setfn(&pwm_period.pwm_timer, (os_timer_func_t *)pwm_period_timer, NULL);
      os_timer_arm_us(&pwm_period.pwm_timer, pwm_period.period, 1);
    }
    
      // Overloaded index operator to allow getting and setting individual PWM channels
    void set(uint8_t index, uint8_t duty) {
      pwm_single[index].duty = duty;
      pwm_single[index].h_time = pwm_period.period * duty / PWM_MAX_DUTY;
    }
  
    uint8 get(uint8_t index) {
      return pwm_single[index].duty;
    }    
};

#endif

