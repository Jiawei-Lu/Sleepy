/*
 * Copyright (c) 2015-2016 Ken Bannister. All rights reserved.
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
 * @brief       gcoap example
 *
 * @author      Ken Bannister <kb2ma@runbox.com>
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

#include "shell.h"
#include "msg.h"
#include <arpa/inet.h>

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

/*gcoap*/
#include "net/gcoap.h"
#include "gcoap_example.h"

// /*Kernel*/
#include "kernel_defines.h"

/*RPL*/
#include "net/gnrc/rpl.h"
#include "net/gnrc/rpl/structs.h"
#include "net/gnrc/rpl/dodag.h"

// /*RTC*/
#include "periph_conf.h"
// #include "periph/rtc.h"
// #include "periph/rtc_mem.h"

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

/*GNRC*/
#include "fmt.h"
#include "net/gnrc.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/netif/hdr.h"
#include "net/ipv6/addr.h"
#include "net/l2util.h"
#include "net/netif.h"

/*DS3231 wakeup and sleep schedule*/
#include "periph/i2c.h"
#include "ds3231.h"
#include "ds3231_params.h"
#include "timex.h"

/*DS18*/
#include "ds18.h"
#include "ds18_params.h"

/*IO1 Xplianed Extension Board*/
#include "at30tse75x.h"
#include "io1_xplained.h"
#include "io1_xplained_params.h"
#include "fmt.h"
#include "periph/gpio.h"
#include "board.h"

/*Timer*/
#include "timex.h"
#include "ztimer.h"
#include "xtimer.h"


/*FAT Filesystem and VFS tool*/
#include "fs/fatfs.h"
#include "vfs.h"
#include "mtd.h"
#include "vfs_default.h"

#ifdef MODULE_MTD_SDCARD
#include "mtd_sdcard.h"
#include "sdcard_spi.h"
#include "sdcard_spi_params.h"
#endif

#if FATFS_FFCONF_OPT_FS_NORTC == 0
#include "periph/rtc.h"
#endif

#define FLASH_MOUNT_POINT  "/sd0" /*mount point*/
#define FNAME "DATA.TXT"
#define DATA_FILE1 (FLASH_MOUNT_POINT "/" FNAME)

/*main queue*/
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

#define PERIOD              (2U * MS_PER_SEC)
#define REPEAT              (4U * MS_PER_SEC)

#define TM_YEAR_OFFSET      (1900)
#define DELAY_1S   (1U * MS_PER_SEC) /* 1 seconds delay between each test */

/*schedule test necessary parameters*/
#define ISOSTR_LEN      (20U)
#define TEST_DELAY      (10U)
#define ONE_S      (1U * MS_PER_SEC)

#define LINE_SIZE 512 //2^21
#define BUFFER_SIZE 128

/*schedule*/
int data_numbering = 0;
char file_sensing_data[] = "/sd0/data0.txt";
int sensing_rate = 600;
int communication_rate = 3600;
int extra_slots;
char data_file_path[40];
char data_file_path2[40];
char com_buffer[CONFIG_GCOAP_PDU_BUF_SIZE];
int count_total_try = 0;
int count_successful = 0;
float successful_rate =0.00;
int sych_time_length;
//char* sych_time_payload;
char sych_time_payload[12];
char payload_digit[12];

/*DS3231 Device*/
ds3231_t _dev;

/*DS18 Device*/
extern ds18_t dev18;

/*Radio netif*/
gnrc_netif_t* radio_netif = NULL;
int global_flag =0; 

/*IO1 xpro board Device*/
static io1_xplained_t dev;

/*RTC struct defination*/
struct tm current_time;
struct tm sych_time;
// struct tm start_time;
/*RPL*/
gnrc_ipv6_nib_ft_t entry;      
void *rpl_state = NULL;
unsigned iface = 0U;
// uint8_t dst_address[] = {0};

/*coap message flage define*/
int _coap_result = 0;
int message_ack_flag =0;

/*external function*/
extern int _gnrc_netif_config(int argc, char **argv);
extern int gcoap_cli_cmd(int argc, char **argv);


/*-----------------FAT File System config Start-----------------*/
static fatfs_desc_t fatfs;

static vfs_mount_t flash_mount = {
    .mount_point = FLASH_MOUNT_POINT,
    .fs = &fatfs_file_system,
    .private_data = (void *)&fatfs,
};

#if defined(MODULE_MTD_NATIVE) || defined(MODULE_MTD_MCI)
/* mtd devices are provided in the board's board_init.c*/
#elif defined(MODULE_MTD_SDMMC)
extern mtd_sdmmc_t mtd_sdmmc_dev0;
#elif defined(MODULE_MTD_SDCARD)
#define SDCARD_SPI_NUM ARRAY_SIZE(sdcard_spi_params)
extern sdcard_spi_t sdcard_spi_devs[SDCARD_SPI_NUM];
mtd_sdcard_t mtd_sdcard_devs[SDCARD_SPI_NUM];
/* always default to first sdcard*/
static mtd_dev_t *mtd_sdcard = (mtd_dev_t*)&mtd_sdcard_devs[0];
#define FLASH_AND_FILESYSTEM_PRESENT    1
#endif

/*-----------------FAT File System End-----------------------*/
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

#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void btn_cb(void *ctx)
{
    (void) ctx;
    puts("BTN0 pressed.");
}
#endif /* MODULE_PERIPH_GPIO_IRQ */

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

int throughput_test(int argc, char **argv) {
    (void)argc;
    (void)argv;

    int argc1 = 4;
    char *argv1[] = {"coap","put","coap://[2001:630:d0:1000::d6f9]:5683/data","01234567890123456789012345678901234567890123456"};
    // gpio_set(GPIO_PIN(PA , 14));cess, code 2.04,
    for (int i = 0; i < 10; i++) {
            gpio_set(GPIO_PIN(PA , 14));
            printf("sending NO.%d CoAP PUT request...\n", i + 1);
            gcoap_cli_cmd(argc1, argv1);
            // gpio_clear(GPIO_PIN(PA , 14));
            ztimer_sleep(ZTIMER_MSEC, 2* MS_PER_SEC);
        
    }
    // gpio_clear(GPIO_PIN(PA , 14));
    printf("10 CoAP Request has beend sent\n");
    return 0;
}

static void _sd_card_cid(void)
{
    puts("SD Card CID info:");
    printf("MID: %d\n", dev.sdcard.cid.MID);
    printf("OID: %c%c\n", dev.sdcard.cid.OID[0], dev.sdcard.cid.OID[1]);
    printf("PNM: %c%c%c%c%c\n",
           dev.sdcard.cid.PNM[0], dev.sdcard.cid.PNM[1], dev.sdcard.cid.PNM[2],
           dev.sdcard.cid.PNM[3], dev.sdcard.cid.PNM[4]);
    printf("PRV: %u\n", dev.sdcard.cid.PRV);
    printf("PSN: %" PRIu32 "\n", dev.sdcard.cid.PSN);
    printf("MDT: %u\n", dev.sdcard.cid.MDT);
    printf("CRC: %u\n", dev.sdcard.cid.CID_CRC);
    puts("+----------------------------------------+\n");
}


static const shell_command_t shell_commands[] = {
    { "coap", "CoAP example", gcoap_cli_cmd },
    { "tpt", "Throughput test", throughput_test},
    { NULL, NULL, NULL }
};

int main(void)
{
    /* for the thread running the shell */
    /* start the shell */
    puts("Initialization successful - starting the shell now");
    //board_antenna_config(RFCTL_ANTENNA_EXT);
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    server_init();
    puts("gcoap example app");
    
    xtimer_sleep(2);
    puts("Waiting for address autoconfiguration...");
    /* print network addresses */
    _gnrc_netif_config(0, NULL);
    ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    kernel_pid_t iface_pid = 7;
    if (gnrc_netif_get_by_pid(iface_pid) == NULL) {
        printf("unknown interface specified\n");
        return 1;
    }
    
    printf("successfully initialized RPL on interface %d\n", iface_pid);

    ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    puts("{\"IPv6 addresses\": [\"");
    //netifs_print_ipv6("\", \"");
    while (global_flag == 0){

        ipv6_addr_t addrs[NETIF_PRINT_IPV6_NUMOF];

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
    }
    puts("\"]}");

    puts("IO1 Xplained extension test application\n");
    puts("+-------------Initializing------------+\n");
    float at30tse75x_temperature;
    if (io1_xplained_init(&dev, &io1_xplained_params[0]) != IO1_XPLAINED_OK) {
        puts("[Error] Cannot initialize the IO1 Xplained extension\n");
        // return 1;
    }
    /* Initialize the SD Card */
    sdcard_spi_params_t sdcard_params = {
        .spi_dev        = IO1_SDCARD_SPI_PARAM_SPI,
        .cs             = IO1_SDCARD_SPI_PARAM_CS,
        .clk            = IO1_SDCARD_SPI_PARAM_CLK,
        .mosi           = IO1_SDCARD_SPI_PARAM_MOSI,
        .miso           = IO1_SDCARD_SPI_PARAM_MISO,
        .power          = IO1_SDCARD_SPI_PARAM_POWER,
        .power_act_high = IO1_SDCARD_SPI_PARAM_POWER_AH
    };
    if (sdcard_spi_init(&dev.sdcard, &sdcard_params) != 0 ){
        puts("sdcard init failed");
    }

    puts("Initialization successful");
    puts("\n+--------Starting tests --------+");
    /* Card detect pin is inverted */
    if (!gpio_read(IO1_SDCARD_SPI_PARAM_DETECT)) {
        _sd_card_cid();
        // ztimer_sleep(ZTIMER_MSEC, DELAY_1S);
    }
    /* Get temperature in degrees celsius */
    at30tse75x_get_temperature(&dev.temp, &at30tse75x_temperature);
    printf("Temperature [°C]: %i.%03u\n"
               "+-------------------------------------+\n",
            (int)at30tse75x_temperature,
            (unsigned)((at30tse75x_temperature - (int)at30tse75x_temperature) * 1000));
    

    

    /*-------------CoAP CLient with RTC config and init Start------------*/
    /* for the thread running the shell */
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

    /*DS18 Param_Pin init*/
    ds18_t dev18;
    int result;

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

    /*------------------------FS part------------------------*/
    if (gnrc_netif_get_by_pid(iface_pid) == NULL) {
        printf("unknown interface specified\n");
        return 1;
    }
    gnrc_rpl_init(iface_pid);
    puts("ztimer sleep for few seconds wait rpl configuration\n");
    ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    puts("printing route:");
    while (gnrc_ipv6_nib_ft_iter(NULL, iface, &rpl_state, &entry)) {
        char addr_str[IPV6_ADDR_MAX_STR_LEN];
        if ((entry.dst_len == 0) || ipv6_addr_is_unspecified(&entry.dst)) {
            printf("default%s ", (entry.primary ? "*" : ""));
            puts("printing route:");   

        }
        else {
            printf("%s/%u ", ipv6_addr_to_str(addr_str, &entry.dst, sizeof(addr_str)),entry.dst_len);
            puts("printing route:");
        }
        if (!ipv6_addr_is_unspecified(&entry.next_hop)) {
           printf("via %s ", ipv6_addr_to_str(addr_str, &entry.next_hop, sizeof(addr_str)));
           puts("printing route:");
        }
        // printf("dev #%u\n", fte->iface);
        // char a[i][4] = entry->iface;
       // i++;
    }
    /*------------------------FS part------------------------*/
    #if defined(MODULE_MTD_NATIVE) || defined(MODULE_MTD_MCI)
        fatfs.dev = mtd_dev_get(0);
    #elif defined(MODULE_MTD_SDMMC)
        fatfs.dev = &mtd_sdmmc_dev0.base;
    #elif defined(MODULE_MTD_SDCARD)
        for(unsigned int i = 0; i < SDCARD_SPI_NUM; i++){
            mtd_sdcard_devs[i].base.driver = &mtd_sdcard_driver;
            mtd_sdcard_devs[i].sd_card = &dev.sdcard;
            mtd_sdcard_devs[i].params = &sdcard_params;
            mtd_init(&mtd_sdcard_devs[i].base);
        }

        //     for(unsigned int i = 0; i < SDCARD_SPI_NUM; i++){
        // mtd_sdcard_devs[i].base.driver = &mtd_sdcard_driver;
        // mtd_sdcard_devs[i].sd_card = &dev.sdcard;
        // mtd_sdcard_devs[i].params = &sdcard_spi_params[i];
        // mtd_init(&mtd_sdcard_devs[i].base);
        fatfs.dev = mtd_sdcard;
    #endif
    
    vfs_format(&flash_mount);
    puts("******************\n");
    vfs_DIR mount = {0};
    puts("******************\n");

    /* list mounted file systems */

    puts("mount points:");
    while (vfs_iterate_mount_dirs(&mount)) {
        printf("\t%s\n", mount.mp->mount_point);
    }
    
    //printf("\ndata dir: %s\n", VFS_DEFAULT_DATA);
    
    /*------------Typical file create and write test start--------------*/

    vfs_mount(&flash_mount);

    char path1[] = "/sd0/DATAtest.TXT";
    FILE *fo = fopen(path1, "w+");
    // if (fo < 0) {
    if(!fo){
        printf("error while trying to create %s\n", path1);
        return 1;
    }
    else{
        puts("creating file success");
    }

    char test_data[] = "1710953364/+23.23/1710953364/-22.22/1710953364/+22.22/1710953364/+58.56/1710953364/+23.23/1710953364/";
    fprintf(fo,"%s\n",test_data);
    fclose(fo);
    int fr = open(path1, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
                                                               //will conflict with open(file)
                                                               //open(file)will equal 0, have to beb a O_RDPNLY for read
 
    char c;

    while (read(fr, &c, 1) != 0){
    putchar(c);  //printf won't work here
    }
    puts("\n");
       
    close(fr);
    puts("closing file");

    vfs_umount(&flash_mount, false);   

    puts("flash point umount");

    
    /*-----------------------------------------------sychronization-----------------------------------------------*/
    //
    
    ds3231_get_time(&_dev, &current_time);
    puts("This is the current system time");
    ds3231_print_time(current_time);
  
    while (message_ack_flag == 0){
        
        /*---------------------------------------202107 GCoAP-------------------------------------------*/
        // int argc1 = 4;
        // char *argv1[] = {"coap", "get", "[2001:630:d0:1000::d6f8]:5683/realtime"}; //glacsweb-pi
        // char *argv1[] = {"coap", "get", "[2001:630:d0:1000::d6f9]:5683/realtime"};//glacsweb-jiawei
        // char *argv1[] = {"coap", "get", "[2001:db8::58a4:8450:8511:6445]:5683/riot/value"};
        //  1-sudo ifconfig [IP interface name] add 2001:630:d0:1000::d6f9
        //  2-libcaop coap-server -A 2001:630:d0:1000::d6f9

        /*---------------------------------------202407 GCoAP (argc=3)-------------------------------------------*/
        int argc1 = 3;
        char *argv1[] = {"coap", "get", "coap://[2001:630:d0:1000::d6f9]:5683/realtime"};  
        _coap_result = gcoap_cli_cmd(argc1,argv1);
        ztimer_sleep(ZTIMER_MSEC, 2* MS_PER_SEC);

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
    gpio_init(GPIO_PIN(PA , 14), GPIO_OUT);
    gpio_set(GPIO_PIN(PA , 14));
    xtimer_sleep(1);
    gpio_clear(GPIO_PIN(PA , 14));

    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);


    int sens = 0;
    char buffer[128];  // Adjust the buffer size according to your expectations of the data size
    ssize_t bytes_read;
    int16_t temperature_test;
    int16_t temperature;
    float ds18_data_test = 0.00;  
    int nn =0;

    while (sens!=6){

        puts("******************\n");

   
        vfs_mount(&flash_mount);
        char path2[] = "/sd0/DATA";
        sprintf(data_file_path, "%s%d.txt", path2, nn);
        int fo = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);
        while (fo < 0) {
            printf("error while trying to create %s\n", data_file_path);
            fo = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);
        }
        
        puts("creating file success");
        result = ds18_init(&dev18, &ds18_params[0]);
        if (result == DS18_ERROR) {
            puts("[Error] The sensor pin could not be initialized");
            return 1;
        }
        /* Get temperature in centidegrees celsius */
        ds18_get_temperature(&dev18, &temperature_test);
        bool negative = (temperature_test < 0);
        ds18_data_test = (float) temperature_test/100;
        if (negative) {
            ds18_data_test = -ds18_data_test;
        }
        
        char payloadtest[40];
        int len = fmt_float(payloadtest,ds18_data_test,2);
        ds3231_get_time(&_dev, &current_time);
        int current_sensing_time = mktime(&current_time);
        if (negative) {
            payloadtest[0] = '-';
            len = 1 + fmt_float(payloadtest + 1, -ds18_data_test, 2); // Ensure the float is positive for correct formatting.
        } else {
            payloadtest[0] = '+';
            len = 1 + fmt_float(payloadtest + 1, ds18_data_test, 2);
        }
        len += snprintf(payloadtest + len, sizeof(payloadtest) - len, ",%d,\n", current_sensing_time);

        if (len >= (int)sizeof(payloadtest) - 2) {  // Ensure there's space for two more characters and a null terminator
            payloadtest[sizeof(payloadtest) - 1] = '\0';
        } else {
            puts("Not enough space to append characters");
        }
        
        if (write(fo, payloadtest, strlen(payloadtest)) != (ssize_t)strlen(payloadtest)) {
            puts("Error while writing");
        }
        close(fo);

        int fr = open(data_file_path, O_RDONLY, 00777);  //before open with O_RDWR which 
                                                                //will conflict with open(file)
                                                                //open(file)will equal 0, have to beb a O_RDPNLY for read
        char c;

        while (read(fr, &c, 1) != 0){
        putchar(c);  //printf won't work here
        }
        puts("\n");
        
        close(fr);
        puts("closing file");
        
        vfs_umount(&flash_mount, false);   
        puts("flash point umount");
        sens = sens + 1;
    }


    /* -----------------------------------start shell -----------------------------------------*/
    // radio_off(radio_netif);
    // pm_set(SAML21_PM_MODE_STANDBY);
    // puts("All up, running the shell now");
    // char line_buf[SHELL_DEFAULT_BUFSIZE];
    // shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    // xtimer_sleep(10);

    data_numbering = data_numbering +1;
    while (1){
        // ds3231_get_time(&_dev, &current_time);
        // puts("This is the current system time");
        // ds3231_print_time(current_time);
        radio_off(radio_netif);
        if (ds3231_get_time(&_dev, &current_time) != 0) {
            puts("Error: get time failed, check DS3231");
            return 1;
        }
        puts("This is the current system time");
        ds3231_print_time(current_time);
        time_t _start_time, _test_time;
        _start_time = mktime(&current_time);
        
        _test_time = (_start_time/60+2)*60; 
        double diff = difftime(_test_time, _start_time);
        current_time.tm_sec += (int)diff;// * ONE_S;
        
        mktime(&current_time);
        
        // if (ds3231_get_time(&_dev, &start_time) != 0) {
        //     puts("Error: get time failed, check DS3231");
        //     return 1;
        // }
        // puts("This is the current system time");
        // ds3231_print_time(current_time);

        // start_time.tm_sec = 0;
        // start_time.tm_min += 1;
        
        // mktime(&start_time);
        gpio_init(GPIO_PIN(PA , 14), GPIO_OUT);
        gpio_set(GPIO_PIN(PA , 14));
        xtimer_sleep(1);
        gpio_clear(GPIO_PIN(PA , 14));

        printf("on hold: system will enter schdule loop at %02d:%02d:00 ......\n", 
                current_time.tm_hour, current_time.tm_min);
        radio_off(radio_netif);
        res = ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_H_M_S);
        if (res != 0) {
            puts("error: unable to program alarm");
            return 1;
        }        
        pm_set(SAML21_PM_MODE_STANDBY);
        // radio_off(radio_netif);
        puts(" WAKED UP SUCCESSFULLY ");
        
        while (1){
            res = ds3231_clear_alarm_1_flag(&_dev);
            if (res != 0) {
                puts("error: unable to clear alarm flag");
                return 1;
            }
            puts("Clock value is now :");
            struct tm testtime;
            int slots = 86400 / sensing_rate; 
            int times = communication_rate / sensing_rate; 
            if (86400 % sensing_rate != 0 ){
                extra_slots = 1 ;
            }
            else{
                extra_slots = 0 ;
            }
            /*sensing state start*/
            for (int counter_slots = 0; counter_slots != slots; counter_slots++){
                if (counter_slots % times == 0){
                    //commnunication start
                    radio_on(radio_netif);
                    xtimer_sleep(20);
                    // //send /sd0/DATA_data_numbering.txt
                    
                    // ztimer_acquire(ZTIMER_MSEC);
                    // uint32_t start_time = ztimer_now(ZTIMER_MSEC);
                    
                    // char com_buffer[256];
                    // int bytes_read = 0; 

                    vfs_mount(&flash_mount);
                    int fd1 = open(data_file_path, O_RDONLY, 00777);
                    if (fd1 < 0) {
                        perror("Failed to open file for reading");
                        return 1;
                    }
                    memset(com_buffer, 0, sizeof(com_buffer));
                    bytes_read = read(fd1, com_buffer, sizeof(com_buffer) - 1);  // Leave space for null terminator
                    if (bytes_read < 0) {
                        perror("Failed to read from file");
                        close(fd1);
                        return 1;
                    }
                
                    com_buffer[bytes_read] = '\0'; 
                    printf("Read data: %s\n", com_buffer);
                    close(fd1);
                    vfs_umount(&flash_mount, false);   
                    
                    // puts("flash point umount");

                    // uint32_t compensation = ztimer_now(ZTIMER_MSEC) - start_time;
                    // if (compensation < 18000) {
                    //     uint32_t remaining = 18000 - compensation;
                    //     ztimer_sleep(ZTIMER_MSEC, remaining);
                    // }

                    // int argc_tx = 5;
                    // char *argv_tx[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/data", com_buffer};

                    // ztimer_sleep(ZTIMER_MSEC, 0.3*MS_PER_SEC);

                    // if (_coap_result == 0) {
                    //     printf("CoAP transmission successful\n");
                    //     count_successful += 1;
                    //     count_total_try += 1;
                    // } else {
                    //     printf("CoAP transmission failed\n");
                    //     count_total_try += 1;
                    // }
                    radio_off(radio_netif);
                    data_numbering = data_numbering +1;
                }
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
                    ds3231_print_time(testtime);
                    time_t _start_time, _test_time;
                    _start_time = mktime(&testtime);
                    
                    _test_time = (_start_time/60+sensing_rate/60)*60; 
                    double diff = difftime(_test_time, _start_time);
                    testtime.tm_sec += (int)diff;// * ONE_S;
                    
                    mktime(&testtime);
                    printf("sensing... and system will enter schdule loop at %02d:%02d:00 ......\n", 
                testtime.tm_hour, testtime.tm_min);
                    
                    /*DS18 INIT*/
                    // gpio_set(GPIO_PIN(PA, 13));
                    result = ds18_init(&dev18, &ds18_params[0]);
                    if (result == DS18_ERROR) {
                        puts("[Error] The sensor pin could not be initialized");
                        return 1;
                    }
                    
                    /*DS18 sensing*/
                    int16_t temperature;
                    float ds18_data = 0.00;   
                    vfs_mount(&flash_mount);
                    char file_path[] = "/sd0/data";
                    sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
                    int fo = open(data_file_path, O_RDWR | O_CREAT, 00777);
                    if (fo < 0) {
                        printf("error while trying to create %s\n", data_file_path);
                        return 1;
                    }
                    else{
                        puts("creating file success");
                    }
                    /* Get temperature in centidegrees celsius */
                    ds18_get_temperature(&dev18, &temperature);
                    bool negative = (temperature < 0);
                    ds18_data = (float) temperature/100;
                    if (negative) {
                        ds18_data = -ds18_data;
                    }
                    
                    char test[200];
                    int len = fmt_float(test,ds18_data,2);
                    ds3231_get_time(&_dev, &current_time);
                    int current_sensing_time = mktime(&current_time);
            
                    if (negative) {
                        test[0] = '-';
                        len = 1 + fmt_float(test + 1, -ds18_data, 2); 
                    } else {
                        test[0] = '+';
                        len = 1 + fmt_float(test + 1, ds18_data, 2);
                    }
                    len += snprintf(test + len, sizeof(test) - len, ",%d,\n", current_sensing_time);

                    if (len >= (int)sizeof(test) - 2) { 
                        test[sizeof(test) - 1] = '\0';
                    } else {
                        puts("Not enough space to append characters");
                    }

                    if (write(fo, test, strlen(test)) != (ssize_t)strlen(test)) {
                        puts("Error while writing");
                    }
                    close(fo);
                    int fr = open(data_file_path, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
                                                                            //will conflict with open(file)
                                                                            //open(file)will equal 0, have to beb a O_RDPNLY for read

                    char c;
                    while (read(fr, &c, 1) != 0){
                    putchar(c);  //printf won't work here
                    }
                    puts("\n");
                    
                    close(fo);
                    puts("closing file");

                    /*11111111111111111*/
                    //vfs_umount_jl(&flash_mount);
                    vfs_umount(&flash_mount, false);
                    // gpio_set(DS18_PARAM_PIN);
                    // gpio_set(GPIO_PIN(PA, 13));
                    puts("flash point umount");
                    puts("start alarm1");
                    // res = ds3231_enable_bat(&_dev);
                    res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
                    if (res != 0) {
                        puts("error: unable to program alarm");
                        return 1;
                    }
                    pm_set(SAML21_PM_MODE_STANDBY);

                    puts(" WAKED UP SUCCESSFULLY ");

            }
            if (extra_slots == 1){
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
                testtime.tm_sec += (86400 -sensing_rate * slots) * ONE_S;
                mktime(&testtime);
                /*DS18 INIT*/
                // gpio_set(GPIO_PIN(PA, 13));
                result = ds18_init(&dev18, &ds18_params[0]);
                if (result == DS18_ERROR) {
                    puts("[Error] The sensor pin could not be initialized");
                    return 1;
                }
                
                /*DS18 sensing*/
                
                float ds18_data = 0.00;


                // gpio_set(GPIO_PIN(PA, 13));
                vfs_mount(&flash_mount);

                char file_path[] = "/sd0/data";
                sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
                int fo = open(data_file_path, O_RDWR | O_CREAT, 00777);
                if (fo < 0) {
                    printf("error while trying to create %s\n", data_file_path);
                    return 1;
                }
                else{
                    puts("creating file success");
                }
                // gpio_set(GPIO_PIN(PA, 13));
                /* Get temperature in centidegrees celsius */
                ds18_get_temperature(&dev18, &temperature);
                bool negative = (temperature < 0);
                ds18_data = (float) temperature/100;
                if (negative) {
                    ds18_data = -ds18_data;
                }
                
                
                printf("Temperature [ºC]: %c%.2f"
                        "\n+-------------------------------------+\n",
                        negative ? '-': '+',
                        ds18_data);
                char test[100];
                int len = fmt_float(test,ds18_data,2);
                ds3231_get_time(&_dev, &current_time);
                int current_sensing_time = mktime(&current_time);
                printf("%s\n",test);
                if (negative) {
                    test[0] = '-';
                    len = 1 + fmt_float(test + 1, -ds18_data, 2); // Ensure the float is positive for correct formatting.
                } else {
                    test[0] = '+';
                    len = 1 + fmt_float(test + 1, ds18_data, 2);
                }
                len += snprintf(test + len, sizeof(test) - len, ",%d,\n", current_sensing_time);

                if (len >= (int)sizeof(test) - 2) {  // Ensure there's space for two more characters and a null terminator
                    test[sizeof(test) - 1] = '\0';
                } else {
                    puts("Not enough space to append characters");
                }
                


                if (write(fo, test, strlen(test)) != (ssize_t)strlen(test)) {
                    puts("Error while writing");
                }
                close(fo);
                int fr = open(data_file_path, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
                                                                        //will conflict with open(file)
                                                                        //open(file)will equal 0, have to beb a O_RDPNLY for read
                // char data_buf[sizeof(test_data)];
                // printf("data:[],length=");
                // vfs_read(fo,data_buf,sizeof(test_data));    
                // printf("data:[],length=");
                char c;

                while (read(fr, &c, 1) != 0){
                putchar(c);  //printf won't work here
                }
                puts("\n");
                
                close(fo);
                puts("closing file");

                /*11111111111111111*/
                //vfs_umount_jl(&flash_mount);
                vfs_umount(&flash_mount, false);
                // gpio_set(DS18_PARAM_PIN);
                // gpio_set(GPIO_PIN(PA, 13));
                puts("flash point umount");
                puts("start alarm1");
                // res = ds3231_enable_bat(&_dev);
                res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
                if (res != 0) {
                    puts("error: unable to program alarm");
                    return 1;
                }

                pm_set(SAML21_PM_MODE_STANDBY);

                puts(" WAKED UP SUCCESSFULLY ");
                            
            }
            else {
                /*sensing state end*/
                /*
                if (extra_slots){
                    int sensing_count = slots ++;
                }
                else{
                    int sensing_count = slots;
                }
                */
            }
        }
    }
    while (sens!=6){

        puts("******************\n");

   
        vfs_mount(&flash_mount);
        char path2[] = "/sd0/DATA";
        sprintf(data_file_path2, "%s%d.txt", path2, nn);
        int fo = open(data_file_path2, O_RDWR | O_CREAT | O_APPEND, 00777);
        while (fo < 0) {
            printf("error while trying to create %s\n", data_file_path2);
            fo = open(data_file_path2, O_RDWR | O_CREAT | O_APPEND, 00777);
        }
        
        puts("creating file success");
        result = ds18_init(&dev18, &ds18_params[0]);
        if (result == DS18_ERROR) {
            puts("[Error] The sensor pin could not be initialized");
            return 1;
        }
        /* Get temperature in centidegrees celsius */
        ds18_get_temperature(&dev18, &temperature_test);
        bool negative = (temperature_test < 0);
        ds18_data_test = (float) temperature_test/100;
        if (negative) {
            ds18_data_test = -ds18_data_test;
        }
        
        char payloadtest[40];
        int len = fmt_float(payloadtest,ds18_data_test,2);
        ds3231_get_time(&_dev, &current_time);
        int current_sensing_time = mktime(&current_time);
        if (negative) {
            payloadtest[0] = '-';
            len = 1 + fmt_float(payloadtest + 1, -ds18_data_test, 2); // Ensure the float is positive for correct formatting.
        } else {
            payloadtest[0] = '+';
            len = 1 + fmt_float(payloadtest + 1, ds18_data_test, 2);
        }
        len += snprintf(payloadtest + len, sizeof(payloadtest) - len, ",%d,\n", current_sensing_time);

        if (len >= (int)sizeof(payloadtest) - 2) {  // Ensure there's space for two more characters and a null terminator
            payloadtest[sizeof(payloadtest) - 1] = '\0';
        } else {
            puts("Not enough space to append characters");
        }
        
        if (write(fo, payloadtest, strlen(payloadtest)) != (ssize_t)strlen(payloadtest)) {
            puts("Error while writing");
        }
        close(fo);

        int fr = open(data_file_path2, O_RDONLY, 00777);  //before open with O_RDWR which 
                                                                //will conflict with open(file)
                                                                //open(file)will equal 0, have to beb a O_RDPNLY for read
        char c;

        while (read(fr, &c, 1) != 0){
        putchar(c);  //printf won't work here
        }
        puts("\n");
        
        close(fr);
        puts("closing file");
        
        vfs_umount(&flash_mount, false);   
        puts("flash point umount");
        sens = sens + 1;
    }
    vfs_mount(&flash_mount);
    int fd1 = open(data_file_path2, O_RDONLY, 00777);
    if (fd1 < 0) {
        perror("Failed to open file for reading");
        return 1;
    }
    bytes_read = read(fd1, buffer, sizeof(buffer) - 1);  // Leave space for null terminator
    if (bytes_read < 0) {
        perror("Failed to read from file");
        close(fd1);
        return 1;
    }
    buffer[bytes_read] = '\0'; 
    printf("Read data: %s\n", buffer);
    close(fd1);
    vfs_umount(&flash_mount, false);   
    puts("flash point umount");

    /*---------------------------------------202107 GCoAP-------------------------------------------*/
    // int argc = 6;
    // char *argv[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f8]:5683", "/data", buffer};  //glacsweb-pi
    // char *argv[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", buffer};  //glacsweb-jiawei

    // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};

    /*--------------------------------------------202407 GCoAP (argc=5)------------------------------------------------*/
    int argc = 5;
    char *argv[] = {"coap", "put", "-c", "coap://[2001:630:d0:1000::d6f9]:5683/data", buffer};  //mini-linux-pc--remote


    // res = ds3231_get_time(&_dev, &current_time);
    // if (res != 0) {
    //     puts("error: unable to read time");
    //     return 1;
    // }
    // current_time.tm_sec += TEST_DELAY;
    // res = ds3231_set_alarm_1(&_dev, &current_time, DS3231_AL1_TRIG_H_M_S);
    // if (res != 0) {
    //     puts("error: unable to program alarm");
    //     return 1;
    // }
    int retry = 0;
    while (message_ack_flag != 1 && retry < 3){
        _coap_result = gcoap_cli_cmd(argc,argv);
        if (_coap_result == 0) {
            printf("Command executed successfully\n");
            
        } 
        else {
            printf("Command execution failed\n");
            
        }
        while(message_ack_flag != 1){
            puts("waitting for the message sent flag\n");
            ztimer_sleep(ZTIMER_MSEC, 0.1* MS_PER_SEC);
        }
        // count_total_try++;
    }
    sens =0;
    if (message_ack_flag == 1){
        // count_successful++;
    }
    // successful_rate=count_successful/ (float)count_total_try;
    // printf("packet deliver rate : %f\n", successful_rate);
    message_ack_flag = 0;

    if (_coap_result == 0) {
        printf("Command executed successfully\n");
    } else {
        printf("Command execution failed\n");
    }
    xtimer_sleep(3);

    
    ds3231_get_time(&_dev, &current_time);
    if (res != 0) {
        puts("error: unable to read time");
        return 1;
    }

    puts("Clock value is now :");
    ds3231_print_time(current_time);

    radio_off(radio_netif);


    /* start shell */
    // puts("All up, running the shell now");
    // char line_buf[SHELL_DEFAULT_BUFSIZE];
    // shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should never be reached */
    return 0;
}
