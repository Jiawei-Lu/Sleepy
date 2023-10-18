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
// #include "periph/gpio.h"
#include "periph/i2c.h"

/*DS3231*/
#include "shell.h"
#include "ds3231.h"
#include "ds3231_params.h"

/*RTC and RTT*/
#include "periph_conf.h"
// #include "periph/rtt.h"
#include "timex.h"
#include "rtt_rtc.h"
#include "periph/rtc.h"


/*Timer*/
#include "timex.h"
#include "ztimer.h"
// #include "xtimer.h"

/*PM layer*/
#include "periph/pm.h"
#ifdef MODULE_PERIPH_GPIO
#include "board.h"
#include "periph/gpio.h"
#endif
#ifdef MODULE_PM_LAYERED
#ifdef MODULE_PERIPH_RTC
#include "periph/rtc.h"
#endif
#include "pm_layered.h"
#endif

/*Radio netif*/
#include "net/gnrc/netif.h"
#include "net/netif.h"
#include "net/gnrc/netapi.h"

/*Radio netif*/
gnrc_netif_t* netif = NULL;

#include "shell.h"
#include "msg.h"
#include "board.h"

#ifndef BTN0_INT_FLANK
#define BTN0_INT_FLANK  GPIO_RISING
#endif

#define ISOSTR_LEN      (20U)
#define TEST_DELAY      (60U)

#define TM_YEAR_OFFSET      (1900)

// //copy
// #define RTT_SECOND      (RTT_FREQUENCY)

// #define _RTT(n)         ((n) & RTT_MAX_VALUE)

// #define RTT_SECOND_MAX  (RTT_MAX_VALUE/RTT_FREQUENCY)

// #define TICKS(x)        (    (x) * RTT_SECOND)
// #define SECONDS(x)      (_RTT(x) / RTT_SECOND)
// #define SUBSECONDS(x)   (_RTT(x) % RTT_SECOND)
// #define RTT_SECOND      (RTT_FREQUENCY)

// #define _RTT(n)         ((n) & RTT_MAX_VALUE)

// #define RTT_SECOND_MAX  (RTT_MAX_VALUE/RTT_FREQUENCY)

// #define TICKS(x)        (    (x) * RTT_SECOND)
// #define SECONDS(x)      (_RTT(x) / RTT_SECOND)
// #define SUBSECONDS(x)   (_RTT(x) % RTT_SECOND)
// //copy ends



// struct tm alarm_time;
// struct tm current_time;

// static unsigned cnt = 0;
 
int wakeup_gap = 10;
int sleep_gap = 10;

static ds3231_t _dev;

struct tm alarm_time1;
struct tm current_time;


// //copy
// static uint32_t rtc_now;         /**< The RTC timestamp when the last RTT alarm triggered */
// static uint32_t last_alarm;      /**< The RTT timestamp of the last alarm */

// static uint32_t alarm_time;                 /**< The RTC timestamp of the (user) RTC alarm */
// static rtc_alarm_cb_t alarm_cb;             /**< RTC alarm callback */
// static void *alarm_cb_arg;                  /**< RTC alarm callback argument */

// static void _rtt_alarm(void *arg);

// /* convert RTT counter into RTC timestamp */
// static inline uint32_t _rtc_now(uint32_t now)
// {
//     return rtc_now + SECONDS(now - last_alarm);
// }

// static inline void _set_alarm(uint32_t now, uint32_t next_alarm)
// {
//     rtt_set_alarm(now + next_alarm, _rtt_alarm, NULL);
// }

// /* This calculates when the next alarm should happen.
//    Always chooses the longest possible period min(alarm, overflow)
//    to minimize the amount of wake-ups. */
// static void _update_alarm(uint32_t now)
// {
//     uint32_t next_alarm;

//     last_alarm = TICKS(SECONDS(now));

//     /* no alarm or alarm beyond this period */
//     if ((alarm_cb == NULL)     ||
//         (alarm_time < rtc_now) ||
//         (alarm_time - rtc_now > RTT_SECOND_MAX)) {
//         next_alarm = RTT_SECOND_MAX;
//     } else {
//         /* alarm triggers in this period */
//         next_alarm = alarm_time - rtc_now;
//     }

//     /* alarm triggers NOW */
//     if (next_alarm == 0) {
//         next_alarm = RTT_SECOND_MAX;
//         alarm_cb(alarm_cb_arg);
//     }

//     _set_alarm(now, TICKS(next_alarm));
// }

// /* the RTT alarm callback */
// static void _rtt_alarm(void *arg)
// {
//     (void) arg;

//     uint32_t now = rtt_get_counter();
//     rtc_now = _rtc_now(now);

//     _update_alarm(now);
// }
// int rtc3231_set_alarm(struct tm *time, rtc_alarm_cb_t cb, void *arg)
// {
//     /* disable alarm to prevent race condition */
//     rtt_clear_alarm();

//     uint32_t now = rtt_get_counter();

//     rtc_tm_normalize(time);

//     alarm_time   = rtc_mktime(time);
//     alarm_cb_arg = arg;
//     alarm_cb     = cb;

//     /* RTT interrupt is disabled here */
//     rtc_now      = _rtc_now(now);
//     _update_alarm(now);

//     return 0;
// }
// //copy ends

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

/* parse ISO date string (YYYY-MM-DDTHH:mm:ss) to struct tm */
static int _tm_from_str(const char *str, struct tm *time)
{
    char tmp[5];

    if (strlen(str) != ISOSTR_LEN - 1) {
        return -1;
    }
    if ((str[4] != '-') || (str[7] != '-') || (str[10] != 'T') ||
        (str[13] != ':') || (str[16] != ':')) {
        return -1;
    }

    memset(time, 0, sizeof(struct tm));

    memcpy(tmp, str, 4);
    tmp[4] = '\0';
    str += 5;
    time->tm_year = atoi(tmp) - 1900;

    memcpy(tmp, str, 2);
    tmp[2] = '\0';
    str += 3;
    time->tm_mon = atoi(tmp) - 1;

    memcpy(tmp, str, 2);
    str += 3;
    time->tm_mday = atoi(tmp);

    memcpy(tmp, str, 2);
    str += 3;
    time->tm_hour = atoi(tmp);

    memcpy(tmp, str, 2);
    str += 3;
    time->tm_min = atoi(tmp);

    memcpy(tmp, str, 2);
    time->tm_sec = atoi(tmp);

    return 0;
}

static int _cmd_get(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char dstr[ISOSTR_LEN];

    struct tm time;
    int res;
    res = ds3231_get_time(&_dev, &time);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }

    size_t pos = strftime(dstr, ISOSTR_LEN, "%Y-%m-%dT%H:%M:%S", &time);
    dstr[pos] = '\0';
    printf("The current time is: %s\n", dstr);

    return 0;
}

static int _cmd_set(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: %s <iso-date-str YYYY-MM-DDTHH:mm:ss>\n", argv[0]);
        return 1;
    }

    if (strlen(argv[1]) != (ISOSTR_LEN - 1)) {
        puts("error: input date string has invalid length");
        return 1;
    }

    struct tm target_time;
    int res = _tm_from_str(argv[1], &target_time);
    if (res != 0) {
        puts("error: unable do parse input date string");
        return 1;
    }

    res = ds3231_set_time(&_dev, &target_time);
    if (res != 0) {
        puts("error: unable to set time");
        return 1;
    }

    printf("success: time set to %s\n", argv[1]);
    return 0;
}

static int _cmd_temp(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int16_t temp;

    int res = ds3231_get_temp(&_dev, &temp);
    if (res != 0) {
        puts("error: unable to read temperature");
        return 1;
    }

    int t1 = temp / 100;
    int t2 = temp - (t1 * 100);
    printf("Current temperature: %i.%02i°C\n", t1, t2);

    return 0;
}

static int _cmd_aging(int argc, char **argv)
{
    int8_t val;
    int res;

    if (argc == 1) {
        res = ds3231_get_aging_offset(&_dev, &val);
        if (res != 0) {
            puts("error: unable to obtain aging offset");
            return 1;
        }
        printf("Aging offset: %i\n", (int)val);
    }
    else {
        val = atoi(argv[1]);
        res = ds3231_set_aging_offset(&_dev, val);
        if (res != 0) {
            puts("error: unable to set againg offset");
            return 1;
        }
        printf("Success: set aging offset to %i\n", (int)val);
    }

    return 0;
}

static int _cmd_bat(int argc, char **argv)
{
    int res;

    if (argc != 2) {
        printf("usage: %s <'0' or '1'>\n", argv[0]);
        return 1;
    }

    if (argv[1][0] == '1') {
        res = ds3231_enable_bat(&_dev);
        if (res == 0) {
            puts("success: backup battery enabled");
        }
        else {
            puts("error: unable to enable backup battery");
        }
    }
    else if (argv[1][0] == '0') {
        res = ds3231_disable_bat(&_dev);
        if (res == 0) {
            puts("success: backup battery disabled");
        }
        else {
            puts("error: unable to disable backup battery");
        }
    }
    else {
        puts("error: unable to parse command");
        return 1;
    }

    return 0;
}

static int _cmd_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int res;
    struct tm time;

    puts("testing device now");

    /* set time to RIOT birthdate */
    res = ds3231_set_time(&_dev, &_riot_bday);
    if (res != 0) {
        puts("error: unable to set time");
        return 1;
    }

    /* read time and compare to initial value */
    res = ds3231_get_time(&_dev, &time);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }

    if ((mktime(&time) - mktime(&_riot_bday)) > 1) {
        puts("error: device time has unexpected value");
        return 1;
    }

    /* wait a short while and check if time has progressed */
    // ztimer_sleep(TEST_DELAY);
    ztimer_sleep(ZTIMER_USEC, TEST_DELAY * US_PER_SEC);
    res = ds3231_get_time(&_dev, &time);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }

    if (!(mktime(&time) > mktime(&_riot_bday))) {
        puts("error: time did not progress");
        return 1;
    }

    /* clear all existing alarm flag */
    res = ds3231_clear_alarm_1_flag(&_dev);
    if (res != 0) {
        puts("error: unable to clear alarm flag");
        return 1;
    }

    /* get time to set up next alarm*/
    res = ds3231_get_time(&_dev, &time);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }



    time.tm_sec += 60;
    mktime(&time);

    /* set alarm */
    res = ds3231_set_alarm_1(&_dev, &time, DS3231_AL1_TRIG_H_M_S);
    if (res != 0) {
        puts("error: unable to program alarm");
        return 1;
    }

#ifdef MODULE_DS3231_INT

    /* wait for an alarm with GPIO interrupt */
    res = ds3231_await_alarm(&_dev);
    if (res < 0){
        puts("error: unable to program GPIO interrupt or to clear alarm flag");
    }

    if (!(res & DS3231_FLAG_ALARM_1)){
        puts("error: alarm was not triggered");
    }

    puts("OK1");
    return 0;

#else

    /* wait for the alarm to trigger */
    ztimer_sleep(ZTIMER_USEC, TEST_DELAY * US_PER_SEC);

    bool alarm;

    /* check if alarm flag is on */
    res = ds3231_get_alarm_1_flag(&_dev, &alarm);
    if (res != 0) {
        puts("error: unable to get alarm flag");
        return 1;
    }

    if (alarm != true){
        puts("error: alarm was not triggered");
    }

    /* clear alarm flag */
    res = ds3231_clear_alarm_1_flag(&_dev);
    if (res != 0) {
        puts("error: unable to clear alarm flag");
        return 1;
    }

    puts("OK");
    return 0;

#endif
}

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

static void cb_rtc_puts(void *arg)
{
    puts(arg);
}

static void cb_rtc_puts1(void *arg)
{
    puts(arg);
}

#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void btn_cb(void *ctx)
{
    (void) ctx;
    puts("BTN0 pressed.");
}
#endif /* MODULE_PERIPH_GPIO_IRQ */

static const shell_command_t shell_commands[] = {
    { "time_get", "init as output (push-pull mode)", _cmd_get },
    { "time_set", "init as input w/o pull resistor", _cmd_set },
    { "temp", "get temperature", _cmd_temp },
    { "aging", "get or set the aging offset", _cmd_aging },
    { "bat", "en/disable backup battery", _cmd_bat },
    { "test", "test if the device is working properly", _cmd_test},
    { NULL, NULL, NULL }
};

int main(void)
{
    int res;
    // struct tm time;

    char line_buf[SHELL_DEFAULT_BUFSIZE];

    ds3231_params_t params= ds3231_params[0];
    params.opt     = DS3231_OPT_BAT_ENABLE;
    params.opt    |= DS3231_OPT_INTER_ENABLE;

    res = ds3231_init(&_dev, &params);

    if (res != 0) {
        puts("error: unable to initialize DS3231 [I2C initialization error]");
        return 1;
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

    #if defined(MODULE_PERIPH_GPIO_IRQ)
        puts("using DS3231 Alarm Flag as wake-up source");
        gpio_init_int(GPIO_PIN(PA , 15), GPIO_IN, GPIO_FALLING, btn_cb, NULL);
    #endif
    // /* 1. Run a callback after 3s */
    // static ztimer_t cb_timer = {.callback = callback, .arg = "Hello World"};
    // ztimer_set(ZTIMER_USEC, &cb_timer, 3 * US_PER_SEC);
    // /* 2. Sleep the current thread for 60s */
    // ztimer_sleep(ZTIMER_MSEC, 5 * MS_PER_SEC);

    puts("radio off for test lowest current consumption");
    // pm_set(SAML21_PM_MODE_IDLE);
    netopt_state_t state = NETOPT_STATE_SLEEP;//NETOPT_STATE_OFF - will increse power consumption
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }

    /* run the shell and wait for the user to enter a mode */

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

    //set a 3231 alarm to make all node wake up at the same day xx clock and do the loop

    while (1) {
        // one flag for schedule update by coap put -- maybe the first duration of the next start of zhengdian (xx:00m:00s) xx is integer
        /* get time to set up next alarm*/
        res = ds3231_get_time(&_dev, &current_time);
        int current_timestamp= mktime(&current_time);  //curent timestamp
        // int alarm_timestamp = 0;
        int duration = wakeup_gap + sleep_gap;
        printf("---------%ds\n",current_timestamp);
        if((current_timestamp % duration) < sleep_gap){  //use gpio test to see what level s/ms/us should be considered here be more precessly
            int wake_up_alarm_timestamp = current_timestamp + sleep_gap - (current_timestamp % duration);
            puts("start sleep");
            rtc_localtime(wake_up_alarm_timestamp -1577836800, &current_time);
            mktime(&current_time);
            /*set wake up alarm*/
            ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_H_M_S);
            if (res != 0) {
                puts("error: unable to program alarm");
                return 1;
            }
            puts("going to sleep");
            pm_set(SAML21_PM_MODE_STANDBY);
            puts("going to wake up");
            pm_set(SAML21_PM_MODE_IDLE);

            /* set sleep alarm */
            res = ds3231_get_time(&_dev, &current_time);
            int sleep_alarm_timestamp = current_timestamp + wakeup_gap;
            rtc_localtime(sleep_alarm_timestamp -1577836800, &current_time);
            mktime(&current_time);
            res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
            if (res != 0) {
                puts("error: unable to program alarm");
                return 1;
            }
            ztimer_sleep(ZTIMER_USEC, wakeup_gap * US_PER_SEC);
            puts("going to sleep");
        }
        else{
            pm_set(SAML21_PM_MODE_IDLE);
            int sleep_alarm_timestamp = current_timestamp + wakeup_gap;
            rtc_localtime(sleep_alarm_timestamp -1577836800, &current_time);
            mktime(&current_time);
            /*set sleep alarm*/
            ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_H_M_S);
            if (res != 0) {
                puts("error: unable to program alarm");
                return 1;
            }
            
            puts("going to sleep");
            
        }
        
        /*set wake up alarm*/
        ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_H_M_S);
        if (res != 0) {
            puts("error: unable to program alarm");
            return 1;
        }
        puts("going to sleep");
        pm_set(SAML21_PM_MODE_STANDBY);
        puts("going to wake up");
        

        pm_set(SAML21_PM_MODE_IDLE);

        
         
    }
    /*OBSOLETED wakeup/sleep schedule version*/
    
    // while (1){
    //     ds3231_get_time(&_dev, &current_time);
    //     print_time("currenttime:\n", &current_time);
    //     int current_timestamp= mktime(&current_time);  //curent timestamp
    //     int alarm_timestamp = 0;
    //     int duration = wakeup_gap + sleep_gap;
    //     printf("---------%ds\n",current_timestamp);
    //     puts("111");
    //     if ((int)(current_timestamp % duration) < (wakeup_gap)){
    //         puts("111");

    //         // pm_set(SAML21_PM_MODE_STANDBY);
    //         pm_set(SAML21_PM_MODE_IDLE);

    //         // radio_on(netif);
    //         int chance = ( wakeup_gap ) - ( current_timestamp % duration );

    //         puts("111");

    //         /*Setting up the sleep alarm after wakeup*/
    //         alarm_timestamp = (current_timestamp / duration) * duration;
    //         alarm_timestamp = alarm_timestamp + wakeup_gap;
    //         rtc_localtime(alarm_timestamp-1577836800, &alarm_time1);
    //         puts("111");
    //         /*RTC SET ALARM*/
    //         rtc_set_alarm(&alarm_time1, cb_rtc_puts, "Time to sleep");

    //         puts("commnication...");            
    //         // ztimer_sleep(chance);
    //         ztimer_sleep(ZTIMER_USEC, chance * US_PER_SEC);


    //         // rtc_set_alarm(&alarm_time, cb_rtc, "111");
            

    //         /*源代码*/
    //         // rtc_get_alarm(&time);
    //         // inc_secs(&time, PERIOD);
    //         // rtc_set_alarm(&time, cb, &rtc_mtx);
    //     }
    //     else{
    //         // printf("fflush");
    //         fflush(stdout);
    //         // radio_off(netif);

    //         alarm_timestamp =  current_timestamp + (duration- (current_timestamp % duration));

    //         alarm_timestamp = alarm_timestamp - 1577836800;
    //         rtc_localtime(alarm_timestamp, &alarm_time1);

            
    //         /*RTC SET ALARM*/
    //         rtc_set_alarm(&alarm_time1, cb_rtc_puts1, "Time to wake up");
    //         puts("go sleep");
    //         // pm_set(SAML21_PM_MODE_IDLE);
    //         pm_set(SAML21_PM_MODE_STANDBY);
    //     }
    // }
    
    /*ends of the OBSOLETED version*/
    // rtc3231_set_alarm(&alarm_time1, cb_rtc_puts, "Time to wake up");
    puts("noooooooooo");
    /* start the shell */

    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
