/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application for demonstrating the RIOT network stack
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>

#include "net/gcoap.h"
#include "shell.h"

#include "gcoap_example.h"
#include "msg.h"
#include <arpa/inet.h>

/*DS3231 wakeup and sleep schedule*/
#include "periph_conf.h"
#include "periph/i2c.h"
#include "ds3231.h"
#include "ds3231_params.h"
#include "timex.h"
#include "ztimer.h"
#include "xtimer.h"


// #include "auto_init_priorities.h"
#include "net/gnrc/ipv6/hdr.h"
#include "net/gnrc/netif/hdr.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/ethernet.h"
#include "net/gnrc/netif/ieee802154.h"
#include "net/gnrc/netif/internal.h"
#include "net/netdev_test.h"
#include "net/netif.h"
#ifndef NETIF_PRINT_IPV6_NUMOF
#define NETIF_PRINT_IPV6_NUMOF 4
#endif

/*Radio netif*/
#include "net/gnrc/netif.h"
#include "net/netif.h"
#include "net/gnrc/netapi.h"

/*RPL*/
#include "net/gnrc/rpl.h"
#include "net/gnrc/rpl/structs.h"
#include "net/gnrc/rpl/dodag.h"
#include "net/gnrc/netif.h"
#include "shell.h"
#include "trickle.h"
#include "utlist.h"

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
#define ONE_S      (1U * MS_PER_SEC)

/*DS3231 Device*/
ds3231_t _dev;

/*RTC struct defination*/
struct tm current_time;
struct  tm  sych_time;

/*Radio netif*/
gnrc_netif_t* radio_netif = NULL;
int global_flag =0; 

/*RPL*/
gnrc_ipv6_nib_ft_t entry;      
void *rpl_state = NULL;
unsigned iface = 0U;

/*coap message flage define*/
int _coap_result = 0;
int message_ack_flag =0;
char sych_time_payload[12];
char payload_digit[12];
int sych_time_length;
gnrc_rpl_dodag_t *dodag = NULL;
int retries = 0;
int count_total_try = 0;
int count_successful = 0;
extern int _dao_check;

/*external function*/
extern int _gnrc_netif_config(int argc, char **argv);
extern int _gnrc_netif_config(int argc, char **argv);
extern int _gnrc_rpl_send_dis(void);
// static int _gnrc_rpl(int argc, char **argv);
// extern gnrc_pktsnip_t *__netif;
// extern icmpv6_hdr_t *icmpv6_hdr;
//         // kernel_pid_t iface = 7;
// extern msg_t msg;

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

int ds3231_print_time(struct tm testtime)
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

struct tm _riot_bday = {
    .tm_sec = 42,
    .tm_min = 10,
    .tm_hour = 15,
    .tm_wday = 3,
    .tm_mday = 22,
    .tm_mon = 8,
    .tm_year = 123
};

void radio_off(gnrc_netif_t *netif){
    netopt_state_t state = NETOPT_STATE_SLEEP;
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
}
/*
        case NETOPT_STATE_STANDBY:
            at86rf2xx_set_state(dev, AT86RF2XX_STATE_TRX_OFF);
            break;
        case NETOPT_STATE_SLEEP:
            at86rf2xx_set_state(dev, AT86RF2XX_STATE_SLEEP);
            break;
        case NETOPT_STATE_IDLE:
            at86rf2xx_set_state(dev, AT86RF2XX_PHY_STATE_RX);
            break;
        case NETOPT_STATE_TX:
*/

void radio_on(gnrc_netif_t *netif){
    netopt_state_t state = NETOPT_STATE_IDLE;
    while ((netif = gnrc_netif_iter(netif))) {
            /* retry if busy */
            while (gnrc_netapi_set(netif->pid, NETOPT_STATE, 0,
                &state, sizeof(state)) == -EBUSY) {}
    }
}

void _send_dao(void){
    // icmpv6_hdr_t *icmpv6_hdr;
    // icmpv6_hdr = (icmpv6_hdr_t *)msg.content.ptr->data;
    // while (icmpv6_hdr->code != GNRC_RPL_ICMPV6_CODE_DAO_ACK){
    
    _dao_check = 0;
    while (_dao_check == 0){
        
    // char addr_str[IPV6_ADDR_MAX_STR_LEN];
    // gnrc_rpl_dodag_t *dodag = NULL;
    for (uint8_t i = 0; i < GNRC_RPL_INSTANCES_NUMOF; ++i) {
        if (gnrc_rpl_instances[i].state == 0) {
            continue;
        }

        dodag = &gnrc_rpl_instances[i].dodag;
        
        gnrc_rpl_send_DAO(dodag->instance, NULL, dodag->default_lifetime);
    }

        // // gnrc_pktsnip_t *__netif;
        // // icmpv6_hdr_t *_icmpv6_hdr;
        // kernel_pid_t iface = 7;
        // // msg_t msg

        // // assert(icmpv6 != NULL);

        // // ipv6 = gnrc_pktsnip_search_type(&msg, GNRC_NETTYPE_IPV6);
        // netif = gnrc_pktsnip_search_type(msg, GNRC_NETTYPE_NETIF);

        // if (netif) {
        //     iface = ((gnrc_netif_hdr_t *)etif->data)->if_pid;
        // }
        
    // ztimer_sleep(ZTIMER_MSEC, 2* MS_PER_SEC);
        puts("send DAO\n");
        // if (dodag->dao_ack_received){
        //     _dao_check = 1;
        // }
        int _retries_dao = 0;
        while (_dao_check == 0 && _retries_dao<20){
            ztimer_sleep(ZTIMER_MSEC, 0.1* MS_PER_SEC);
            printf("%d\n", _retries_dao);
            _retries_dao++;
        }
        // // ipv6_hdr = (ipv6_hdr_t *)ipv6->data;

        // _icmpv6_hdr = (icmpv6_hdr_t *)msg->data;
        // _icmpv6_hdr->code == GNRC_RPL_ICMPV6_CODE_DAO_ACK;
    }
}
static int sleepy(int argc, char **argv){
    radio_off(radio_netif);
    for (int _i=1;_i<102;_i++){
    // while(1){
        if (argc < 2) {
            
            return 1;
        }
        struct tm _time;
        int _res = ds3231_clear_alarm_1_flag(&_dev);
        if (_res != 0) {
            puts("error: unable to clear alarm flag");
            return 1;
        }            
        _res = ds3231_get_time(&_dev, &_time);
        if (_res != 0) {
            puts("error: unable to read time");
            return 1;
        }
        time_t _start_time, _test_time;
        _start_time = mktime(&_time);
        int time = atoi(argv[1]);

        //_test_time = (_start_time/60+time*_i)*60; //int time is how many minutes you want the system sleep with radio off 
        _test_time = (_start_time/60+time)*60; 
        // printf("test_time:%lld\n", (long long) _start_time);
        double diff = difftime(_test_time, _start_time);
        
        _time.tm_sec += (int)diff;// * ONE_S;
        mktime(&_time);
        printf("watting %d seconds to start sleepy test\n", (int)diff);
        
        printf("radio off \n");
        _res = ds3231_set_alarm_1(&_dev, &_time, DS3231_AL1_TRIG_H_M_S);
        if (_res != 0) {
            puts("error: unable to program alarm");
            return 1;
        }
        pm_set(SAML21_PM_MODE_STANDBY);

        puts("waking up and ready to do coap \n");
        message_ack_flag =0;
        radio_on(radio_netif);

        /*--------------------------------------------202407 GCoAP (argc=5)------------------------------------------------*/
        int _argc2 = 5;
        char *_argv2[] = {"coap", "put", "-c", "coap://[2001:630:d0:1000::d683]:5683/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};  //mini-linux-pc--remote
        
        message_ack_flag = 0;
        retries = 0;
        // ztimer_sleep(ZTIMER_MSEC, 0.3* MS_PER_SEC);
        // int _argc_rpl =3;
        // char *_argv_rpl[] = {"rpl", "send", "dis"};
        // _gnrc_rpl(_argc_rpl,_argv_rpl);
        
        /*------------------------------Send DAO to rejoin--------------------------------*/
        _send_dao();

        // /*------------------------------Send DAO to rejoin--------------------------------*/
        // _gnrc_rpl_send_dis();
        

        // ztimer_sleep(ZTIMER_MSEC, 10* MS_PER_SEC);
        message_ack_flag = 0;
        while (message_ack_flag != 1 && retries < 3){
            _coap_result = gcoap_cli_cmd(_argc2,_argv2);
            if (_coap_result == 0) {
                printf("Command executed successfully, and Reyries: %d\n", retries);
                int wait = 0;
                while(message_ack_flag == 0 && wait < 10){
                    puts("waitting for the message sent flag\n");
                    ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC); //DO NOT use NS_PER_MS as it curshs program 
                    printf("waitting time is %d\n", wait);
                    wait++;
                }
            }else {
                printf("Command execution failed\n");
                
            }
            retries++;
            count_total_try++;
        }
        count_successful++;
        printf("successful : %d, total : %d\n", count_successful, count_total_try);
        
        radio_off(radio_netif);
    }
    radio_on(radio_netif);
    return 0;
}

#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void btn_cb(void *ctx)
{
    (void) ctx;
    puts("BTN0 pressed.");
}
#endif /* MODULE_PERIPH_GPIO_IRQ */


static const shell_command_t shell_commands[] = {
    { "coap", "CoAP example", gcoap_cli_cmd },
    { "sleepy", "make system sleepy and wake up n minutes after", sleepy },
    { NULL, NULL, NULL }
};


int main(void)
{
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    // ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    puts("RIOT network stack example application");
    // int shell_on = 1;
    int shell_on = 2;
    // int shell_on = 0;
    if (shell_on == 1){
    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    }
    puts("ztimer sleep for few seconds wait rpl configuration\n");
    // ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);

    _gnrc_netif_config(0, NULL);

    ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    kernel_pid_t iface_pid = 7;
    if (gnrc_netif_get_by_pid(iface_pid) == NULL) {
        printf("unknown interface specified\n");
        pm_reboot();
        return 1;
    }
    gnrc_rpl_init(iface_pid);
    printf("successfully initialized RPL on interface %d\n", iface_pid);

    puts("ztimer sleep for few seconds wait rpl configuration\n");
    ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    puts("{\"IPv6 addresses\": [\"");
    //netifs_print_ipv6("\", \"");
    while (global_flag == 0){
        // netif_t *netif = 0;
        // bool first = true;
        // while ((netif = netif_iter(netif)) != NULL) {
            ipv6_addr_t addrs[NETIF_PRINT_IPV6_NUMOF];
        //     ssize_t num = netif_get_ipv6(netif, addrs, ARRAY_SIZE(addrs));
        //     if (num > 0) {
        //         if (first) {
        //             first = false;
        //         }
        //         else {
        //             printf("%s", "\", \"");
        //         }
        //         ipv6_addrs_print(addrs, num, "\", \"");
        //     }
        char addr_str[IPV6_ADDR_MAX_STR_LEN];

        printf("inet6 addr: ");
        ipv6_addr_to_str(addr_str, addrs, sizeof(addr_str));
        printf("%s  scope: ", addr_str);
        if(ipv6_addr_is_global(addrs)){
            puts("global address received\n");
            global_flag = 1;
        }
        else{
            _gnrc_netif_config(0, NULL);
            //ztimer_sleep(ZTIMER_MSEC, 10* MS_PER_SEC);
            ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
        }
    //   }	
    }
    puts("\"]}");


    /*----------------------------------DS3231 INIT-------------------------------------*/
    int res;

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

    #if defined(MODULE_PERIPH_GPIO_IRQ)
        puts("using DS3231 Alarm Flag as wake-up source");
        gpio_init_int(GPIO_PIN(PA , 15), GPIO_IN, GPIO_FALLING, btn_cb, NULL);
    #endif

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
    puts("Clock value is now :");
    ds3231_print_time(current_time);

    /*------------------------RPL config and print------------------------*/
    puts("Configured rpl:");
    // gpio_set(GPIO_PIN(PA, 13));
    // // //gpio_set(DS18_PARAM_PIN);
    iface_pid = 7;
    if (gnrc_netif_get_by_pid(iface_pid) == NULL) {
        printf("unknown interface specified\n");
        return 1;
    }
    gnrc_rpl_init(iface_pid);
    printf("successfully initialized RPL on interface %d\n", iface_pid);



    while (message_ack_flag == 0){
    
        /*---------------------------------------202107 GCoAP-------------------------------------------*/
        // int argc1 = 4;
        // char *argv1[] = {"coap", "get", "[2001:630:d0:1000::d6f8]:5683", "/realtime"}; //glacsweb-pi
        // char *argv1[] = {"coap", "get", "[2001:630:d0:1000::d6f9]:5683", "/realtime"};//glacsweb-jiawei
        // char *argv1[] = {"coap", "get", "[2001:db8::58a4:8450:8511:6445]:5683", "/riot/value"};


        /*---------------------------------------202407 GCoAP (argc=3)-------------------------------------------*/
        int argc1 = 3;
        char *argv1[] = {"coap", "get", "coap://[2001:630:d0:1000::d683]:5683/realtime"};  //mini-linux-pc--remote



        _coap_result = gcoap_cli_cmd(argc1,argv1);

        // ztimer_sleep(ZTIMER_MSEC, 3* MS_PER_SEC);
        // xtimer_sleep(10);
        int _wait = 0;
        while(message_ack_flag == 0 && _wait < 4){
            puts("waitting for the message sent flag\n");
            ztimer_sleep(ZTIMER_MSEC, 1.2* MS_PER_SEC); //DO NOT use NS_PER_MS as it curshs program 
            printf("waitting time is %d\n", _wait);
            _wait++;
        }
        if (_coap_result == 0) {
            printf("Command executed successfully\n");
            
            ds3231_get_time(&_dev, &current_time);
            ds3231_print_time(current_time);
            
        } 
        else {
            printf("Command execution failed\n");
        }
    }
    message_ack_flag =0;

    _send_dao();
    if (shell_on == 2){
    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    }
    
    /*------------------------Test: Running test in 2min after------------------------*/
    struct tm testtime;
    struct tm targettime;
    
    res = ds3231_clear_alarm_1_flag(&_dev);
    if (res != 0) {
        puts("error: unable to clear alarm flag");
        return 1;
    }            
    res = ds3231_get_time(&_dev, &targettime);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }
    time_t start_time, test_time;
    start_time = mktime(&targettime);
    test_time = (start_time/60+3)*60;
    printf("test_time:%lld\n", (long long) start_time);
    double diff = difftime(test_time, start_time);
    
    targettime.tm_sec += (int)diff;// * ONE_S;
    mktime(&targettime);
    printf("watting %d seconds to start sleepy test\n", (int)diff);
    radio_off(radio_netif);
    printf("radio off \n");
    res = ds3231_set_alarm_1(&_dev, &targettime, DS3231_AL1_TRIG_H_M_S);
    if (res != 0) {
        puts("error: unable to program alarm");
        return 1;
    }
    pm_set(SAML21_PM_MODE_STANDBY);

    puts("Running the Sleepy Test \n");
    message_ack_flag =0;
    
    /*------------------------Test: How sleepy system can be ->> increment 100s------------------------*/
    int count_sleepgap = 1;
    retries = 0;
    count_total_try = 0;
    count_successful = 0;
    // float successful_rate =0.00;
    while (1){
        radio_on(radio_netif);
        res = ds3231_clear_alarm_1_flag(&_dev);
        if (res != 0) {
            puts("error: unable to clear alarm flag");
            return 1;
        }            
        res = ds3231_get_time(&_dev, &testtime);
        if (res != 0) {
            puts("error: unable to read time");
            return 1;
        }
        testtime.tm_sec += 60 * count_sleepgap+300;// * ONE_S;
        mktime(&testtime);
        
        /*---------------------------------------202107 GCoAP-------------------------------------------*/
        // int argc2 = 6;
        // char *argv2[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f8]:5683", "/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};   //glacsweb-pi
        //char *argv2[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};  //glacsweb-jiawei
        // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};

        /*--------------------------------------------202407 GCoAP (argc=5)------------------------------------------------*/
        int argc2 = 5;
        char *argv2[] = {"coap", "put", "-c", "coap://[2001:630:d0:1000::d683]:5683/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};  //mini-linux-pc--remote
        
        message_ack_flag = 0;
        retries = 0;
        while (message_ack_flag != 1 && retries < 3){
            _coap_result = gcoap_cli_cmd(argc2,argv2);
            if (_coap_result == 0) {
                printf("Command executed successfully, and Reyries: %d\n", retries);
                int wait = 0;
                while(message_ack_flag == 0 && wait < 3){
                    puts("waitting for the message sent flag\n");
                    ztimer_sleep(ZTIMER_MSEC, 0.4* MS_PER_SEC); //DO NOT use NS_PER_MS as it curshs program 
                    printf("waitting time is %d\n", wait);
                    wait++;
                }
            }else {
                printf("Command execution failed\n");
                
            }
            retries++;
            count_total_try++;
        }
        count_successful++;
        printf("successful : %d, total : %d\n", count_successful, count_total_try);
        // if (count_total_try > 0) {
        // successful_rate = (float)(count_successful*100/count_total_try)/100;
        // // printf("packet deliver rate : %f\n", successful_rate);
        // // printf("packet deliver rate : %f\n", (float)(count_successful/count_total_try));
        // }
        fflush(stdout);
        message_ack_flag = 0;

        radio_off(radio_netif);
        count_sleepgap++;
        puts("start alarm1");
        // res = ds3231_enable_bat(&_dev);
        res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
        if (res != 0) {
            puts("error: unable to program alarm");
            return 1;
        }

        pm_set(SAML21_PM_MODE_STANDBY);

        puts(" WAKED UP SUCCESSFULLY \n");

    }
    if (shell_on == 0){
    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    }
    /* should be never reached */
    return 0;
}
