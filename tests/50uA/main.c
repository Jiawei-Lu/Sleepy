/*
 * Copyright (C) 2016-2018 Bas Stottelaar <basstottelaar@gmail.com>
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
 * @brief       Power management peripheral test.
 *
 * @author      Bas Stottelaar <basstottelaar@gmail.com>
 * @author      Vincent Dupont <vincent@otakeys.com>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/*ztimer usecond*/
#include "ztimer.h"
#include "timex.h"



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
#include "rtt_rtc.h"
#include "periph/rtc.h"


/*Radio netif*/
#include "net/gnrc/netif.h"
#include "net/netif.h"
#include "net/gnrc/netapi.h"

/*Radio netif*/
gnrc_netif_t* netif = NULL;

#include "shell.h"
#include "msg.h"
#include "board.h"

/*copy*/
#include <time.h>

// #include "periph/gpio.h"
#include "periph/i2c.h"

#include "xtimer.h"
#include "ds3231.h"
#include "ds3231_params.h"

#define TEST_DELAY      (10U)


static ds3231_t _dev;

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
/*copy ends*/

#ifndef BTN0_INT_FLANK
#define BTN0_INT_FLANK  GPIO_RISING
#endif

#ifdef MODULE_PM_LAYERED

#ifdef MODULE_PERIPH_RTC
static int check_mode_duration(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <power mode> <duration (s)>\n", argv[0]);
        return -1;
    }

    return 0;
}

static int parse_mode(char *argv)
{
    uint8_t mode = atoi(argv);

    if (mode >= PM_NUM_MODES) {
        printf("Error: power mode not in range 0 - %d.\n", PM_NUM_MODES - 1);
        return -1;
    }

    return mode;
}

static int parse_duration(char *argv)
{
    int duration = atoi(argv);

    if (duration < 0) {
        puts("Error: duration must be a positive number.");
        return -1;
    }

    return duration;
}

static void cb_rtc(void *arg)
{
    int level = (int)arg;

    pm_block(level);
}

static void cb_rtc_puts(void *arg)
{
    puts(arg);
}

static int cmd_unblock_rtc(int argc, char **argv)
{
    if (check_mode_duration(argc, argv) != 0) {
        return 1;
    }

    int mode = parse_mode(argv[1]);
    int duration = parse_duration(argv[2]);

    if (mode < 0 || duration < 0) {
        return 1;
    }

    pm_blocker_t pm_blocker = pm_get_blocker();
    if (pm_blocker.blockers[mode] == 0) {
        printf("Mode %d is already unblocked.\n", mode);
        return 1;
    }

    printf("Unblocking power mode %d for %d seconds.\n", mode, duration);
    fflush(stdout);

    struct tm time;

    rtc_get_time(&time);
    time.tm_sec += duration;
    rtc_set_alarm(&time, cb_rtc, (void *)mode);

    pm_unblock(mode);

    return 0;
}

static int cmd_set_rtc(int argc, char **argv)
{
    if (check_mode_duration(argc, argv) != 0) {
        return 1;
    }

    int mode = parse_mode(argv[1]);
    int duration = parse_duration(argv[2]);

    if (mode < 0 || duration < 0) {
        return 1;
    }

    printf("Setting power mode %d for %d seconds.\n", mode, duration);
    fflush(stdout);

    struct tm time;

    rtc_get_time(&time);
    time.tm_sec += duration;
    rtc_set_alarm(&time, cb_rtc_puts, "The alarm rang");

    pm_set(mode);

    return 0;
}
#endif /* MODULE_PERIPH_RTC */
#endif /* MODULE_PM_LAYERED */

#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void btn_cb(void *ctx)
{
    (void) ctx;
    puts("BTN0 pressed.");
}
#endif /* MODULE_PERIPH_GPIO_IRQ */

/**
 * @brief   List of shell commands for this example.
 */
static const shell_command_t shell_commands[] = {
#if defined MODULE_PM_LAYERED && defined MODULE_PERIPH_RTC
    { "set_rtc", "temporary set power mode", cmd_set_rtc },
    { "unblock_rtc", "temporarily unblock power mode", cmd_unblock_rtc },
#endif
    { NULL, NULL, NULL }
};

#if IS_USED(MODULE_PM_LAYERED)
static void _show_blockers(void)
{
    uint8_t lowest_allowed_mode = 0;

    pm_blocker_t pm_blocker = pm_get_blocker();
    for (unsigned i = 0; i < PM_NUM_MODES; i++) {
        printf("mode %u blockers: %u \n", i, pm_blocker.blockers[i]);
        if (pm_blocker.blockers[i]) {
            lowest_allowed_mode = i + 1;
        }
    }

    printf("Lowest allowed mode: %u\n", lowest_allowed_mode);
}
#endif /* MODULE_PM_LAYERED */

/*ztimer callback*/
static void callback (void * arg) {
puts((char*) arg);
}

/**
 * @brief   Application entry point.
 */
int main(void)
{
    char line_buf[SHELL_DEFAULT_BUFSIZE];

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
        _show_blockers();
    #else
        puts("This application allows you to test the CPU power management.\n"
             "Layered support is not unavailable for this CPU. Reset the CPU if\n"
             "needed.");
    #endif

    #if defined(MODULE_PERIPH_GPIO_IRQ) //&& defined(BTN0_PIN)
        puts("using BTN0 as wake-up source");
        gpio_init_int(GPIO_PIN(PA , 15), GPIO_IN, GPIO_BOTH, btn_cb, NULL);//GPIO_FALLING,
    #endif

    int res;
    struct tm testtime; 
    // sturct tm alarmtime;

    puts("DS3231 RTC test\n");

    ds3231_params_t params= ds3231_params[0];
    params.opt     = DS3231_OPT_BAT_ENABLE;
    params.opt    |= DS3231_OPT_INTER_ENABLE;

    res = ds3231_init(&_dev, &params);

    if (res != 0) {
        puts("error: unable to initialize DS3231 [I2C initialization error]");
        return 1;
    }
    /* 1. Run a callback after 3s */
    static ztimer_t cb_timer = {.callback = callback, .arg = "Hello World"};
    ztimer_set(ZTIMER_USEC, &cb_timer, 3 * US_PER_SEC);
    /* 2. Sleep the current thread for 60s */
    // ztimer_sleep(ZTIMER_MSEC, 5 * MS_PER_SEC);
    ztimer_sleep(ZTIMER_USEC, 5 * US_PER_SEC);

    puts("set idle");
    // pm_set(SAML21_PM_MODE_IDLE);
    netopt_state_t state = NETOPT_STATE_SLEEP;//NETOPT_STATE_OFF - will increse power consumption
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
    
    // pm_set(SAML21_PM_MODE_STANDBY);
    _show_blockers();
 while(1){   
    /*test if the pin can wake up node from standby*/
    res = ds3231_set_time(&_dev, &_riot_bday);
    if (res != 0) {
        puts("error: unable to set time");
        return 1;
    }

    /* read time and compare to initial value */
    res = ds3231_get_time(&_dev, &testtime);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }

    if ((mktime(&testtime) - mktime(&_riot_bday)) > 1) {
        puts("error: device time has unexpected value");
        return 1;
    }
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
    
    puts("setting up wakeup dalay for 10s");
    testtime.tm_sec += TEST_DELAY;
    mktime(&testtime);
    /* set alarm */
    res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
    if (res != 0) {
        puts("error: unable to program alarm");
        return 1;
    }
    puts("1");
    pm_set(SAML21_PM_MODE_STANDBY);
    puts(" WAKED UP SUCCESSFULLY ");
    // pm_set(SAML21_PM_MODE_IDLE);
    // puts(" WAKED UP SUCCESSFULLY ");
    // puts("setting SLEEP dalay for 10s");
    /*CLEAR ALARM FLAG*/
    res = ds3231_clear_alarm_1_flag(&_dev);
    if (res != 0) {
        puts("error: unable to clear alarm flag");
        return 1;
    }
    /*wait for the pin gpio goes high*/
    res = ds3231_get_time(&_dev, &testtime);
    testtime.tm_sec += TEST_DELAY;
    mktime(&testtime);
    // testtime_stamp = mktime(&alarmtime);
    // rtc_localtime(testtime_stamp - 1577836800 + 5, &alarmtime);
    ztimer_sleep(ZTIMER_USEC, TEST_DELAY * US_PER_SEC);
    // mktime(&testtime);

    // /* set alarm */
    // res = ds3231_set_alarm_2(&_dev, &alarmtime, DS3231_AL2_TRIG_D_H_M_S);
    // if (res != 0) {
    //     puts("error: unable to program alarm");
    //     return 1;
    // }
    // res = ds3231_await_alarm(&_dev);
    //     if (res < 0){
    //         puts("error: unable to program GPIO interrupt or to clear alarm flag");
    //     }
    // res = ds3231_clear_alarm_2_flag(&_dev);
    // if (res != 0) {
    //     puts("error: unable to clear alarm flag");
    //     return 1;
    // }
    // pm_set(SAML21_PM_MODE_STANDBY);
    // pm_set(SAML21_PM_MODE_IDLE);
    // puts(" WAKED UP SUCCESSFULLY ");
    }
    /* run the shell and wait for the user to enter a mode */
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
