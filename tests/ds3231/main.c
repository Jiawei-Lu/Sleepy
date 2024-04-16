/*
 * Copyright (C) 2020 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for the DS3231 RTC driver
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "board.h"

#include "periph/i2c.h"

/*DS3231*/
#include "shell.h"
#include "ds3231.h"
#include "ds3231_params.h"

/*RTC and RTT*/
#include "periph_conf.h"
// #include "periph/rtt.h"
#include "timex.h"
// #include "rtt_rtc.h"
// #include "periph/rtc.h"


/*Timer*/
#include "timex.h"
#include "ztimer.h"
// #include "xtimer.h"

/*PM layer*/
// #include "periph/pm.h"
// #ifdef MODULE_PERIPH_GPIO
// #include "board.h"
// #include "periph/gpio.h"
// #endif
// #ifdef MODULE_PM_LAYERED
// #ifdef MODULE_PERIPH_RTC
// #include "periph/rtc.h"
// #endif
// #include "pm_layered.h"
// #endif
#include "periph/pm.h"
#include "pm_layered.h"
#include "periph/gpio.h"

/*Radio netif*/
#include "net/gnrc/netif.h"
#include "net/netif.h"
#include "net/gnrc/netapi.h"

/*Radio netif*/
gnrc_netif_t* netif = NULL;

#include "shell.h"
#include "msg.h"
#include "board.h"


#define ISOSTR_LEN      (20U)
#define TEST_DELAY      (10U)

#define TM_YEAR_OFFSET      (1900)

/*DS18*/
#include "ds18.h"
#include "ds18_params.h"
// #define SAMPLING_PERIOD     2

// struct tm alarm_time;
// struct tm current_time;

// static unsigned cnt = 0;
 
int wakeup_gap = 5;
int sleep_gap = 5;

static ds3231_t _dev;
extern ds18_t dev18;

struct tm alarm_time1;
struct tm current_time;

// float ds18_data;



/* 2010-09-22T15:10:42 is the author date of RIOT's initial commit */
static struct tm _riot_bday = {
    .tm_sec = 42,
    .tm_min = 10,
    .tm_hour = 15,
    .tm_wday = 3,
    .tm_mday = 22,
    .tm_mon = 8,
    .tm_year = 123
};

void print_time(const char *label, const struct tm *time)
{
    printf("%s  %04d-%02d-%02d %02d:%02d:%02d\n", label,
            time->tm_year + TM_YEAR_OFFSET,
            time->tm_mon + 1,
            time->tm_mday,
            time->tm_hour,
            time->tm_min,
            time->tm_sec);
}

void radio_off(gnrc_netif_t *netif){
    netopt_state_t state = NETOPT_STATE_SLEEP;
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
}

void radio_on(gnrc_netif_t *netif){
    netopt_state_t state = NETOPT_STATE_IDLE;
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
}
// static void cb_rtc_puts(void *arg)
// {
//     puts(arg);
// }

// static void cb_rtc_puts1(void *arg)
// {
//     puts(arg);
// }

#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void btn_cb(void *ctx)
{
    (void) ctx;
    puts("BTN0 pressed.");
}
#endif /* MODULE_PERIPH_GPIO_IRQ */

static int ds3231_print_time(struct tm testtime)
{
    int re;
    char dstr[ISOSTR_LEN];
    re = ds3231_get_time(&_dev, &testtime);
    if (re != 0) {
        puts("error: unable to read time");
        return 1;
    }

    size_t pos = strftime(dstr, ISOSTR_LEN, "%Y-%m-%dT%H:%M:%S", &testtime);
    dstr[pos] = '\0';
    printf("The current time is: %s\n", dstr);

    return 0;
}


static const shell_command_t shell_commands[] = {
    { NULL, NULL, NULL }
};



int main(void)
{

    int res;

    ds18_t dev18;
    int result;
    gpio_init(DS18_PARAM_PIN, GPIO_OUT); 
    gpio_set(DS18_PARAM_PIN);

    char line_buf[SHELL_DEFAULT_BUFSIZE];

    ds3231_params_t params= ds3231_params[0];
    params.opt     = DS3231_OPT_BAT_ENABLE;
    params.opt    |= DS3231_OPT_INTER_ENABLE;

    res = ds3231_init(&_dev, &params);

    if (res != 0) {
        puts("error: unable to initialize DS3231 [I2C initialization error]");
        return 1;
    }

    /*Enable bakc battery of DS3231*/
    res = ds3231_enable_bat(&_dev);
    if (res == 0) {
        puts("success: backup battery enabled");
    }
    else {
        puts("error: unable to enable backup battery");
    }

    /* print test application information */
    #ifdef MODULE_PM_LAYERED
        printf("This application allows you to test the CPU power management.\n"
           "The available power modes are 0 - %d. Lower-numbered power modes\n"
           "save more power, but may require an event/interrupt to wake up\n"
           "the CPU. Reset the CPU if needed.\n",
        PM_NUM_MODES - 1);

    /* In case the system boots into an unresponsive shell, at least display
     * the state of PM blockers so that the user will know which power mode has
     * been entered and is presumably responsible for the unresponsive shell.
     */

    #else
        puts("This application allows you to test the CPU power management.\n"
             "Layered support is not unavailable for this CPU. Reset the CPU if\n"
             "needed.");
    #endif


    gpio_init_int(GPIO_PIN(PA , 14), BTN0_MODE, GPIO_FALLING, btn_cb, NULL);

    // #if defined(MODULE_PERIPH_GPIO_IRQ)
    //     puts("using DS3231 Alarm Flag as wake-up source");
    //     gpio_init_int(GPIO_PIN(PA , 15), GPIO_IN, GPIO_BOTH, btn_cb, NULL);
    // #endif
    // /* 1. Run a callback after 3s */
    // static ztimer_t cb_timer = {.callback = callback, .arg = "Hello World"};
    // ztimer_set(ZTIMER_USEC, &cb_timer, 3 * US_PER_SEC);
    // /* 2. Sleep the current thread for 60s */
    // ztimer_sleep(ZTIMER_MSEC, 5 * MS_PER_SEC);


    res = ds3231_set_time(&_dev, &_riot_bday);
    if (res != 0) {
        puts("error: unable to set time");
        return 1;
    }
    ds3231_get_time(&_dev, &current_time);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }
    if ((mktime(&current_time) - mktime(&_riot_bday)) > 1) {
        puts("error: device time has unexpected value");
        return 1;
    }
    // rtc_set_time(&current_time);
    // rtc_init(); 
    res = ds3231_clear_alarm_1_flag(&_dev);
    if (res != 0) {
        puts("error: unable to clear alarm flag");
        return 1;
    }

    while(1){   
        struct tm testtime;
        gpio_init(GPIO_PIN(PA, 13), GPIO_OUT);
        gpio_init(GPIO_PIN(PA, 23), GPIO_IN);
        // gpio_set(GPIO_PIN(PA, 13));
        gpio_set(GPIO_PIN(PA, 23));
        /* read time and compare to initial value */
        res = ds3231_get_time(&_dev, &testtime);
        if (res != 0) {
            puts("error: unable to read time");
            return 1;
        }
        // print_time("current time is:", &current_time);

        // size_t pos = strftime(dstr, ISOSTR_LEN, "%Y-%m-%dT%H:%M:%S", &testtime);
        // dstr[pos] = '\0';
        // printf("The current time is: %s\n", dstr);    
        ds3231_print_time(testtime);

        /* clear all existing alarm flag */
        res = ds3231_clear_alarm_1_flag(&_dev);
        if (res != 0) {
            puts("error: unable to clear alarm flag");
            return 1;
        }

        /* get time to set up next alarm*/
        res = ds3231_get_time(&_dev, &testtime);
        if (res != 0) {
            puts("error: unable to read time");
            return 1;
        }
        // ds3231_print_time(testtime);
        // puts("setting up wakeup dalay for 10s");
        testtime.tm_sec += TEST_DELAY;
        mktime(&testtime);

        /*radio off*/
        radio_off(netif);
        // res = ds3231_disable_bat(&_dev);
        /* set alarm */
        puts("start alarm1");
        // res = ds3231_enable_bat(&_dev);
        res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
        if (res != 0) {
            puts("error: unable to program alarm");
            return 1;
        }


        // gpio_clear(GPIO_PIN(PA,16));
        // gpio_clear(GPIO_PIN(PA,17));
        pm_set(SAML21_PM_MODE_STANDBY);

        puts(" WAKED UP SUCCESSFULLY ");
        // gpio_clear(GPIO_PIN(PA, 18));
        // res = ds3231_disable_bat(&_dev);
        /*radio on*/
        gpio_set(GPIO_PIN(PA, 13));
        radio_on(netif);
        gpio_clear(GPIO_PIN(PA, 13));

        /*DS18 INIT*/
        // gpio_set(GPIO_PIN(PA, 13));
        result = ds18_init(&dev18, &ds18_params[0]);
        if (result == DS18_ERROR) {
            puts("[Error] The sensor pin could not be initialized");
            return 1;
        }
        
        // /*DS18 sensing*/
        // int16_t temperature;
        // // gpio_set(GPIO_PIN(PA, 13));
        // /* Get temperature in centidegrees celsius */
        // if (ds18_get_temperature(&dev, &temperature) == DS18_OK) {
        //     bool negative = (temperature < 0);
        //     if (negative) {
        //         temperature = -temperature;
        //     }
        //     printf("Temperature [ºC]: %c%d.%02d"
        //            "\n+-------------------------------------+\n",
        //            negative ? '-': ' ',
        //            temperature / 100,
        //            temperature % 100);
        // }
        // else{
        //     puts("error");
        // }

        int16_t temperature;
        
        
        // gpio_set(GPIO_PIN(PA, 13));
        /* Get temperature in centidegrees celsius */
        if (ds18_get_temperature(&dev18, &temperature) == DS18_OK) {
            bool negative = (temperature < 0);
            if (negative) {
                temperature = -temperature;
            }
            printf("Temperature [ºC]: %c%d"
                   "\n+-------------------------------------+\n",
                   negative ? '-': ' ',
                   temperaturn);
        }
        else{
            puts("error");
        }
        char test_data[100000];
        sprintf(test_data, "%d", temperature);
        // gpio_clear(GPIO_PIN(PA, 13));
        /*DS18 sampling rate*/
        // ztimer_sleep(ZTIMER_USEC, SAMPLING_PERIOD * US_PER_SEC);


        /*CLEAR ALARM FLAG*/
        res = ds3231_clear_alarm_1_flag(&_dev);
        if (res != 0) {
            puts("error: unable to clear alarm flag");
            return 1;
        }

        /*wait for the pin gpio goes high*/
        res = ds3231_get_time(&_dev, &testtime);
        if (res != 0) {
            puts("error: unable to read time");
            return 1;
        }

        ds3231_print_time(testtime);

        testtime.tm_sec += (2*TEST_DELAY);
        mktime(&testtime);
        
        puts("start alarm2");
        // ztimer_sleep(ZTIMER_USEC, TEST_DELAY * US_PER_SEC);
        res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
        if (res != 0) {
        puts("error: unable to program alarm");
        return 1;
        }


        res = ds3231_await_alarm(&_dev);
        if (res < 0){
        puts("error: unable to program GPIO interrupt or to clear alarm flag");
        }

        if (!(res & DS3231_FLAG_ALARM_1)){
        puts("error: alarm was not triggered");
        }

        puts("OK1");
        /* clear all existing alarm flag */

    }

    // //set a 3231 alarm to make all node wake up at the same day xx clock and do the loop
    // //set a rtc alarm to wake up till  DS3231_AL1_TRIG_D_H_M_S matches

    // while (1) {
    //     // one flag for schedule update by coap put -- maybe the first duration of the next start of zhengdian (xx:00m:00s) xx is integer
    //     /* get time to set up next alarm*/
        
    //     // res = ds3231_get_time(&_dev, &current_time);
    //     int duration = wakeup_gap + sleep_gap;

    //     // print_time("current time is:", &current_time);
    //     // puts("11");
    //     int current_timestamp= mktime(&current_time);  //curent timestamp
    //     // int alarm_timestamp = 0;
        
    //     printf("---------%ds\n",current_timestamp);
        
    //     // if((current_timestamp % duration) < sleep_gap){  //use gpio test to see what level s/ms/us should be considered here be more precessly
    //     int sleep_chance = wakeup_gap - (current_timestamp % duration);
    //     int wake_up_alarm_timestamp = current_timestamp + sleep_chance;
    //     // puts("start sleep");
    //     rtc_localtime(wake_up_alarm_timestamp -1577836800, &current_time);
    //     mktime(&current_time);
    //     /*set wake up alarm*/
    //     ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_H_M_S);
    //     if (res != 0) {
    //         puts("error: unable to program alarm");
    //         return 1;
    //     }
    //     puts("flag cleared");
    //     // puts("going to sleep");
    //      puts("satndby");
    //     pm_set(SAML21_PM_MODE_STANDBY);
    //     puts("satndby");
    //     // puts("going to wake up");
    //     res = ds3231_clear_alarm_1_flag(&_dev);
    //     if (res != 0) {
    //         puts("error: unable to clear alarm flag");
    //         return 1;
    //     }
    //     // puts("1");
    //     // pm_set(SAML21_PM_MODE_IDLE);
    //     // }
    //     // else{
    //      /* clear all existing alarm flag */
    //     // pm_set(SAML21_PM_MODE_IDLE);
    //     /* set sleep alarm */
    //     res = ds3231_get_time(&_dev, &current_time);
    //     // print_time("WAKE UP time is:", &current_time);
    //     // puts("2");
    //     int wakeup_chance = wakeup_gap - (current_timestamp % duration);
    //     int sleep_alarm_timestamp = current_timestamp + wakeup_chance;
    //     rtc_localtime(sleep_alarm_timestamp -1577836800, &current_time);
    //     mktime(&current_time);
    //     res = ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_D_H_M_S);
    //     if (res != 0) {
    //         puts("error: unable to program alarm");
    //         return 1;
    //     }
    //      puts("1");
    //     ztimer_sleep(ZTIMER_USEC, wakeup_chance * US_PER_SEC);
    //              puts("1");
    //      puts("1");
    //      puts("1");
    //     res = ds3231_clear_alarm_1_flag(&_dev);
    //     if (res != 0) {
    //         puts("error: unable to clear alarm flag");
    //         return 1;
    //     }
    //     // res = ds3231_await_alarm(&_dev);
    //     // if (res < 0){
    //     //     puts("error: unable to program GPIO interrupt or to clear alarm flag");
    //     // }

    //     // if (!(res & DS3231_FLAG_ALARM_2)){
    //     //     puts("error: alarm was not triggered");
    //     // }
    //     puts("going to sleep");
    // }
    
    // rtc3231_set_alarm(&alarm_time1, cb_rtc_puts, "Time to wake up");
    puts("noooooooooo");
    /* start the shell */

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
