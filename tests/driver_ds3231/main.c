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
#include "xtimer.h"
#include "ds3231.h"
#include "ds3231_params.h"

/*RTC and RTT*/
#include "periph_conf.h"
#include "periph/rtt.h"
#include "timex.h"
#include "rtt_rtc.h"
#include "periph/rtc.h"


/*Timer*/
#include "timex.h"
#include "ztimer.h"
#include "xtimer.h"

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


#define ISOSTR_LEN      (20U)
#define TEST_DELAY      (60U)

#define TM_YEAR_OFFSET      (1900)
#define DELAY_1S   (1U) /* 1 seconds delay between each test */

static unsigned cnt = 0;

int wakeup_gap =60;

static ds3231_t _dev;

struct tm alarm_time;
struct tm current_time;



/* 2010-09-22T15:10:42 is the author date of RIOT's initial commit */
static struct tm _riot_bday = {
    .tm_sec = 42,
    .tm_min = 10,
    .tm_hour = 15,
    .tm_wday = 3,
    .tm_mday = 22,
    .tm_mon = 8,
    .tm_year = 110
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
    xtimer_sleep(TEST_DELAY);
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

    time.tm_sec += TEST_DELAY;
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
    xtimer_sleep(TEST_DELAY);

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

    puts("DS3231 RTC test\n");

    ds3231_params_t params= ds3231_params[0];
    params.opt     = DS3231_OPT_BAT_ENABLE;
    params.opt    |= DS3231_OPT_INTER_ENABLE;

    res = ds3231_init(&_dev, &params);

    if (res != 0) {
        puts("error: unable to initialize DS3231 [I2C initialization error]");
        return 1;
    }

    res = ds3231_set_time(&_dev, &_riot_bday);    
    print_time("default time: \n",&_riot_bday);

    while (1) {
        ++cnt;
        rtc_get_time(&current_time);
        print_time("currenttime:\n", &current_time);
        int current_timestamp= mktime(&current_time);
        printf("current time stamp: %d\n", current_timestamp);
        int alarm_timestamp = 0;
        if ((int)(current_timestamp % 120) < (wakeup_gap*1)){
            puts("111");
            pm_set(1);
            // radio_on(netif);
            int chance = ( wakeup_gap ) - ( current_timestamp % 120 );
            alarm_timestamp = (current_timestamp / 120) *120+ (wakeup_gap * 1);
            alarm_timestamp = alarm_timestamp- 1577836800;
            rtc_localtime(alarm_timestamp, &alarm_time);
            /*RTC SET ALARM*/
            rtc_set_alarm(&alarm_time, cb_rtc_puts, "Time to sleep");
            print_time("alarm time:\n", &alarm_time);
            printf("---------%ds\n",chance);
            puts("xtimer sleep");
                        
            xtimer_sleep(chance);

            // rtc_set_alarm(&alarm_time, cb_rtc, "111");
            

            /*源代码*/
            // rtc_get_alarm(&time);
            // inc_secs(&time, PERIOD);
            // rtc_set_alarm(&time, cb, &rtc_mtx);
        }
        else{
            //printf("fflush");
            puts("222");
            fflush(stdout);
            
            // radio_off(netif);
            alarm_timestamp =  current_timestamp + (120- (current_timestamp % 120));
            // alarm_timestamp = (current_timestamp / 360) *360+ (wakeup_gap * 1);
            alarm_timestamp = alarm_timestamp - 1577836800;
            rtc_localtime(alarm_timestamp, &alarm_time);
            print_time("alarm time:\n", &alarm_time);
            
            /*RTC SET ALARM*/
            rtc_set_alarm(&alarm_time, cb_rtc_puts, "TIme to wake up");//(void *)modetest);
            // rtc_set_alarm(&alarm_time, cb_rtc, (void *)modetest);
            pm_set(0);
            // pm_set(0);
        }
    }

    // while (1) {
    //     int alarm_timestamp = 0;
    //     pm_set(1);
    //     puts("testing device now");

    //     /* set time to RIOT birthdate */
    //     struct tm time;
    //     // res = ds3231_set_time(&_dev, &_riot_bday);


    //     /* read time and compare to initial value */
    //     res = ds3231_get_time(&_dev, &time);
    //     print_time("currenttime 111:\n", &time);

    //     /* wait a short while and check if time has progressed */
    //     // xtimer_sleep(TEST_DELAY);
    //     res = ds3231_get_time(&_dev, &time);
    //     print_time("currenttime 222:\n", &time);

    //     /* clear all existing alarm flag */
    //     res = ds3231_clear_alarm_1_flag(&_dev);

    //     /* get time to set up next alarm*/
    //     res = ds3231_get_time(&_dev, &time);
    //     print_time("currenttime 333:\n", &time);

        
    //     int current_timestamp = mktime(&time);
    //     alarm_timestamp = current_timestamp + 60;
    //     rtc_localtime(alarm_timestamp, &alarm_time);
    //     /* set alarm */
    //     ds3231_toggle_alarm_1(&_dev,true);
    //     res = ds3231_set_alarm_1(&_dev, &time, DS3231_AL1_TRIG_H_M_S);
    
    //     puts("alarm");
    //     xtimer_sleep(3);

    //     rtc_set_alarm(&alarm_time, cb_rtc_puts, "Time to wake up");
    //     puts("sleep");
    //     pm_set(0);
    // }    

    /* start the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
