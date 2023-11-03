/*
 * fan.c
 *
 * Created: 21.08.2020 12:45:12
 *  Author: E1130513
 */ 

#include <asf.h>

#include "fan.h" 
#include "config.h"
#include "uart.h"
#include "sys_timer.h"
#include "modbus.h"
#include "env.h"

#ifndef BOOTLOADER

static uint32_t cnt_tacho_1;
static uint32_t last_pwm_adjust;
static uint32_t last_tacho_measure;
static uint32_t last_sync;
static uint8_t current_pwm=0;
static uint32_t fantacho1 = 0;
static uint8_t pwm_frequency;
static uint8_t new_pwm_frequency;

static struct tc_module tc_instance_pwm;
static struct tc_module tc_instance_tacho;

static void delete_extint_callbacks(void);
static void set_pwm(void);
static void tc_callback_timer1(struct tc_module *const module_inst);
static void enable_extint_callbacks(void);
static void extint_detection_callback_int_0(void);
static void get_fan_speed(void);
static void fan_tacho_init(void);
static void fan_sync_to_modbus(void);
static void fan_pwm_init(uint8_t pwm_frequency_init);


/*
 * Calculate PWM value for the fans
 * Set PWM for the fans
 */
static void set_pwm(void)
{
	static volatile uint8_t pwm_to_fan = CFG_PWM_INITIAL_VALUE;
	uint8_t pwm_to_fan_invert;
	uint8_t pwm;
	
	if(modbus_watchdog()==1)
	{
		pwm = modbus_get_holding_reg(HOLD_REG__PRECONFIG_FAN_REQUEST);
	}
	else
	{
		pwm = modbus_get_holding_reg(HOLD_REG__FAN_REQUEST);
	}
	
	if (pwm < modbus_get_holding_reg(HOLD_REG__FAN_REUEST_MIN))
	{
		pwm = modbus_get_holding_reg(HOLD_REG__FAN_REUEST_MIN);
	}
	
	if (pwm > modbus_get_holding_reg(HOLD_REG__FAN_REUEST_MAX))
	{
		pwm = modbus_get_holding_reg(HOLD_REG__FAN_REUEST_MAX);
	}
	
	if(modbus_get_holding_reg(HOLD_REG__UNIT_OFF_ON) == 0)
	{
		pwm = 0;
	}
	
	if(pwm_to_fan < pwm) 
	{
		pwm_to_fan++;
	}
		
	if(pwm_to_fan > pwm) 
	{
		pwm_to_fan--;
	}
	
	if(pwm_to_fan > 100)
	{
		pwm_to_fan = 100;
	}
	
	current_pwm = pwm_to_fan;
	pwm_to_fan_invert = abs(pwm_to_fan - 100); //invert PWM	
	tc_set_compare_value(&tc_instance_pwm, TC_COMPARE_CAPTURE_CHANNEL_0, pwm_to_fan_invert);
}


/*
 * Initialize the PWM
 */
void fan_pwm_init(uint8_t pwm_frequency_init)
{	
	struct tc_config config_tc_fan_pwm;
	tc_get_config_defaults(&config_tc_fan_pwm);
	config_tc_fan_pwm.counter_size = TC_COUNTER_SIZE_8BIT;
	
	switch(pwm_frequency_init)
	{   
		case 0:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV256; 	break;	//300Hz
		case 1:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV64; 	break;  //1250Hz
		case 2:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV16; 	break;  //5000Hz
		case 3:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV8; 	break;  //10000Hz
		case 4:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV4; 	break;	//20000Hz
		case 5:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV2; 	break;	//40000Hz
		default:	config_tc_fan_pwm.clock_prescaler = TC_CLOCK_PRESCALER_DIV16;	break; //5000Hz
	}
	
	config_tc_fan_pwm.clock_source = GCLK_GENERATOR_0;
	config_tc_fan_pwm.wave_generation = TC_WAVE_GENERATION_NORMAL_PWM;
	config_tc_fan_pwm.counter_8_bit.value = 0;
	config_tc_fan_pwm.counter_8_bit.compare_capture_channel[0] = 100-CFG_PWM_INITIAL_VALUE;
	config_tc_fan_pwm.counter_8_bit.period = 100;
	config_tc_fan_pwm.pwm_channel[0].enabled = true;
	config_tc_fan_pwm.pwm_channel[0].pin_out = CFG_PWM1_PIN;
	config_tc_fan_pwm.pwm_channel[0].pin_mux = CFG_PWM1_MUX;
	tc_init(&tc_instance_pwm, CFG_PWM_MODULE, &config_tc_fan_pwm);
	tc_enable(&tc_instance_pwm);
}


/*
 * Initialize Tacho Pulse counter
 */
static void tc_callback_timer1(struct tc_module *const module_inst)
{
	uint8_t pulses_per_rotation;
	tc_stop_counter(&tc_instance_tacho);
	delete_extint_callbacks();
	pulses_per_rotation = modbus_get_holding_reg(HOLD_REG__PULSES_PER_REVOLUTION);
	fantacho1 = cnt_tacho_1 * (60/pulses_per_rotation);
	modbus_set_input_reg(INPUT_REG__RPM_DEVIATION_1_0, (uint16_t)(100 * (float)fantacho1 / (float)(modbus_get_holding_reg(HOLD_REG__FAN_CURVE_PWM_0+current_pwm))));
}

/*---configure_extint_callbacks---
- configure the interrupt callback
*/
static void delete_extint_callbacks(void)
{
	extint_unregister_callback(extint_detection_callback_int_0 ,0,EXTINT_CALLBACK_TYPE_DETECT);
}

static void enable_extint_callbacks(void)
{
	extint_chan_enable_callback(0 ,EXTINT_CALLBACK_TYPE_DETECT);
	extint_register_callback(extint_detection_callback_int_0, 0 ,EXTINT_CALLBACK_TYPE_DETECT);
}

static void extint_detection_callback_int_0(void)
{
	cnt_tacho_1++;
}


/* Initialize 1sec timer for fan measurement. */
static void fan_tacho_init(void)
{
	struct tc_config config_tc_tacho;
	tc_get_config_defaults(&config_tc_tacho);
	config_tc_tacho.counter_size = TC_COUNTER_SIZE_16BIT;
	config_tc_tacho.clock_source = GCLK_GENERATOR_0;
	config_tc_tacho.clock_prescaler = TC_CLOCK_PRESCALER_DIV256;
	config_tc_tacho.counter_16_bit.value = 0;
	config_tc_tacho.counter_16_bit.compare_capture_channel[TC_COMPARE_CAPTURE_CHANNEL_0] = 31250; //1 sec timer ==> 8000000Hz/256/31250 = 1Hz
	tc_init(&tc_instance_tacho, CFG_TACHO_MODULE, &config_tc_tacho);
	tc_enable(&tc_instance_tacho);
	tc_register_callback(&tc_instance_tacho, tc_callback_timer1, TC_CALLBACK_CC_CHANNEL0);
	tc_enable_callback(&tc_instance_tacho, TC_CALLBACK_CC_CHANNEL0);
	tc_stop_counter(&tc_instance_tacho);
	
	struct extint_chan_conf config_extint_0;
	extint_chan_get_config_defaults(&config_extint_0);
	config_extint_0.gpio_pin           = CFG_INT0_PIN_FAN1;
	config_extint_0.gpio_pin_mux       = CFG_INT0_MUX_FAN1;
	config_extint_0.gpio_pin_pull      = EXTINT_PULL_UP;
	config_extint_0.detection_criteria = EXTINT_DETECT_RISING;
	extint_chan_set_config(0, &config_extint_0);
}

static void get_fan_speed(void)
{
	cnt_tacho_1 = 0;
	tc_stop_counter(&tc_instance_tacho);
	tc_start_counter(&tc_instance_tacho);
	enable_extint_callbacks();
}


static void fan_sync_to_modbus(void)
{	
	modbus_set_input_reg(INPUT_REG__FAN_CURRENT_PWM, current_pwm);
	modbus_set_input_reg(INPUT_REG__FAN_CURRENT_SPEED, fantacho1);
}

void fan_init(void)
{	
	ioport_set_pin_dir(CFG_CONVERTER_OFF, IOPORT_DIR_OUTPUT);
	ioport_set_pin_level(CFG_CONVERTER_OFF, IOPORT_PIN_LEVEL_LOW);
	
	pwm_frequency = modbus_get_holding_reg(HOLD_REG__PWM_FREQUENCY);
	
	fan_pwm_init(pwm_frequency);
	fan_tacho_init();
	
}

void do_fan(void)
{	
	if (get_jiffies() - last_pwm_adjust >= (100 * (1+ modbus_get_holding_reg(HOLD_REG__PWM_DELAY))))
	{
		last_pwm_adjust = get_jiffies();
		
		set_pwm();
		
		if(modbus_get_holding_reg(HOLD_REG__UNIT_OFF_ON) == 0)
		{
			ioport_set_pin_level(CFG_CONVERTER_OFF, IOPORT_PIN_LEVEL_HIGH);
		}
		else
		{
			ioport_set_pin_level(CFG_CONVERTER_OFF, IOPORT_PIN_LEVEL_LOW);
		}	
	}
	
	if (get_jiffies() - last_tacho_measure >= 2000) {
		last_tacho_measure = get_jiffies();
		get_fan_speed();
	}
	
	if (get_jiffies() - last_sync >= 1000) {
		last_sync = get_jiffies();
		fan_sync_to_modbus();
	}
	

	new_pwm_frequency = modbus_get_holding_reg(HOLD_REG__PWM_FREQUENCY);
	if (new_pwm_frequency != pwm_frequency) {
		PRINTF("PWM: changing pwm frequency to %d\r\n", (int)new_pwm_frequency);
		pwm_frequency = new_pwm_frequency;
		tc_reset(&tc_instance_pwm);
		fan_pwm_init(pwm_frequency);
	}
}

#endif /* BOOTLOADER */