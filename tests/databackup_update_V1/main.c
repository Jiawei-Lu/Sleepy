/*
 * Copyright (C) 2018 OTA keys S.A.
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
 * @brief       File system usage example application
 *
 * @author      Vincent Dupont <vincent@otakeys.com>
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


//#include "init_dev.h"
#include "auto_init_priorities.h"
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

#include "msg.h"

/*GCoAP*/
#include "net/gcoap.h"
#include "shell.h"
#include "gcoap_example.h"

/*Kernel*/
#include "kernel_defines.h"

/*RPL*/
#include "net/gnrc/rpl.h"
#include "net/gnrc/rpl/structs.h"
#include "net/gnrc/rpl/dodag.h"

// /*RTC*/
// #include "periph_conf.h"
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

/*DS3231 wakeup and sleep schedule*/
#include "periph_conf.h"
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

// #define GCOAP_MEMO_RETRANSMIT (0)

int data_numbering = 0;
char file_sensing_data[] = "/sd0/data0.txt";

int sensing_rate = 10;
int communication_rate = 60;

int extra_slots;
char data_file_path[40];
char data_file_path2[40];

int count_total_try = 0;
int count_successful = 0;
float successful_rate =0.00;

/*DS3231 Device*/
ds3231_t _dev;

/*DS18 Device*/
extern ds18_t dev18;

/*Radio netif*/
gnrc_netif_t* radio_netif = NULL;
int global_flag =0; 

// static kernel_pid_t _recv_pid;

static io1_xplained_t dev;

/*RTC struct defination*/
// struct tm coaptime;
// struct tm alarm_time;
struct tm current_time;

/*RPL*/
gnrc_ipv6_nib_ft_t entry;      
void *rpl_state = NULL;
uint8_t dst_address[] = {0};
// uint8_t nexthop_address[] = {0};
// int i = 0;
unsigned iface = 0U;

int _coap_result = 0;
int message_ack_flag =0;

extern int _gnrc_netif_config(int argc, char **argv);
extern int gcoap_cli_cmd(int argc, char **argv);
struct  tm  sych_time;
int sych_time_length;
// char* sych_time_payload;
char sych_time_payload[12];
char payload_digit[12];

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


static int _cat(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }
    /* With newlib or picolibc, low-level syscalls are plugged to RIOT vfs
     * on native, open/read/write/close/... are plugged to RIOT vfs */
#if defined(MODULE_NEWLIB) || defined(MODULE_PICOLIBC)
    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        printf("file %s does not exist\n", argv[1]);
        return 1;
    }
    char c;
    while (fread(&c, 1, 1, f) != 0) {
        putchar(c);
    }
    fclose(f);
#else
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        printf("file %s does not exist\n", argv[1]);
        return 1;
    }
    char c;
    while (read(fd, &c, 1) != 0) {
        putchar(c);
    }
    close(fd);
#endif
    fflush(stdout);
    return 0;
}

static int _tee(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: %s <file> <str>\n", argv[0]);
        return 1;
    }

#if defined(MODULE_NEWLIB) || defined(MODULE_PICOLIBC)
    FILE *f = fopen(argv[1], "w+");
    if (f == NULL) {
        printf("error while trying to create %s\n", argv[1]);
        return 1;
    }
    if (fwrite(argv[2], 1, strlen(argv[2]), f) != strlen(argv[2])) {
        puts("Error while writing");
    }
    fclose(f);
#else
    int fd = open(argv[1], O_RDWR | O_CREAT, 00777);
    if (fd < 0) {
        printf("error while trying to create %s\n", argv[1]);
        return 1;
    }
    if (write(fd, argv[2], strlen(argv[2])) != (ssize_t)strlen(argv[2])) {
        puts("Error while writing");
    }
    close(fd);
#endif
    return 0;
}


static int _mount(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if FLASH_AND_FILESYSTEM_PRESENT
    int res = vfs_mount(&flash_mount);
    if (res < 0) {
        printf("Error while mounting %s...try format\n", FLASH_MOUNT_POINT);
        return 1;
    }

    printf("%s successfully mounted\n", FLASH_MOUNT_POINT);
    return 0;
#else
    puts("No external flash file system selected");
    return 1;
#endif
}

static int _format(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if FLASH_AND_FILESYSTEM_PRESENT
    int res = vfs_format(&flash_mount);
    if (res < 0) {
        printf("Error while formatting %s\n", FLASH_MOUNT_POINT);
        return 1;
    }

    printf("%s successfully formatted\n", FLASH_MOUNT_POINT);
    return 0;
#else
    puts("No external flash file system selected");
    return 1;
#endif
}

static int _umount(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if FLASH_AND_FILESYSTEM_PRESENT
    //int res = vfs_umount_jl(&flash_mount);//

    int res = vfs_umount(&flash_mount, false);
    if (res < 0) {
        printf("Error while unmounting %s\n", FLASH_MOUNT_POINT);
        return 1;
    }

    printf("%s successfully unmounted\n", FLASH_MOUNT_POINT);
    return 0;
#else
    puts("No external flash file system selected");
    return 1;
#endif
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

// int get_total_lines(const char *file_path) {
//     FILE *file = fopen(file_path, "r");
//     if (!file) {
//         perror("Error opening file");
//         return -1;
//     }

//     int total_lines = 0;
//     char _line[LINE_SIZE];
//     while (fgets(_line, sizeof(_line), file)) {
//         total_lines++;
//     }

//     fclose(file);
//     return total_lines;
// }



#if defined(MODULE_PERIPH_GPIO_IRQ) && defined(BTN0_PIN)
static void btn_cb(void *ctx)
{
    (void) ctx;
    puts("BTN0 pressed.");
}
#endif /* MODULE_PERIPH_GPIO_IRQ */


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


static const shell_command_t shell_commands[] = {
    { "cat", "print the content of a file", _cat },
    { "tee", "write a string in a file", _tee },
    { "mount", "mount flash filesystem", _mount },
    { "format", "format flash file system", _format },
    { "umount", "unmount flash filesystem", _umount },
    { "coap", "CoAP example", gcoap_cli_cmd },
    // { "set_rtc", "temporary set power mode", cmd_set_rtc },
    // { "unblock_rtc", "temporarily unblock power mode", cmd_unblock_rtc },
    { NULL, NULL, NULL }
};

int main(void){   

    /* start the shell */
    puts("Initialization successful - starting the shell now");
    board_antenna_config(RFCTL_ANTENNA_EXT);
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    server_init();

    puts("gcoap example app");   
    
    puts("Waiting for address autoconfiguration...");
    // xtimer_sleep(5);
    

    /* print network addresses */
    
    _gnrc_netif_config(0, NULL);
    //ztimer_sleep(ZTIMER_MSEC, 10* MS_PER_SEC);
    while(1){
    _gnrc_netif_config(0, NULL);
    xtimer_sleep(1);
    }
    puts("{\"IPv6 addresses\": [\"");
    //netifs_print_ipv6("\", \"");
    while (global_flag == 0){
        netif_t *netif = 0;
        bool first = true;
        while ((netif = netif_iter(netif)) != NULL) {
            ipv6_addr_t addrs[NETIF_PRINT_IPV6_NUMOF];
            ssize_t num = netif_get_ipv6(netif, addrs, ARRAY_SIZE(addrs));
            if (num > 0) {
                if (first) {
                    first = false;
                }
                else {
                    printf("%s", "\", \"");
                }
                ipv6_addrs_print(addrs, num, "\", \"");
            }
      
        if(ipv6_addr_is_global(addrs)){
            global_flag = 1;
        }
        else{
            _gnrc_netif_config(0, NULL);
            //ztimer_sleep(ZTIMER_MSEC, 10* MS_PER_SEC);
            xtimer_sleep(1);
        }
      }	
    }
    puts("\"]}");
    puts("gcoap example app");
    puts("insert SD-card and use 'init' command to set card to spi mode");
    puts("WARNING: using 'write' or 'copy' commands WILL overwrite data on your sd-card and");
    puts("almost for sure corrupt existing filesystems, partitions and contained data!");

    float at30tse75x_temperature;

    puts("IO1 Xplained extension test application\n");
    puts("+-------------Initializing------------+\n");

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
    
    
    /*------------Typical file create and write test end----------------*/

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
    gpio_init(DS18_PARAM_PIN, GPIO_OUT);
    gpio_init(GPIO_PIN(PA , 8), GPIO_OUT); 
    gpio_init(GPIO_PIN(PA , 13), GPIO_OUT); 
    // gpio_init(GPIO_PIN(PA , 15), GPIO_OUT);
    gpio_set(GPIO_PIN(PA , 13));
    // gpio_set(DS18_PARAM_PIN);

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
        gpio_init_int(GPIO_PIN(PA , 14), GPIO_IN, GPIO_FALLING, btn_cb, NULL);
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
    
    

    /*RPL config and print*/
    puts("Configured rpl:");
    // gpio_set(GPIO_PIN(PA, 13));
    // // //gpio_set(DS18_PARAM_PIN);

    // gnrc_rpl_init(7);
    puts("ztimer sleep for 60s wait rpl configuration\n");
    ztimer_sleep(ZTIMER_MSEC, 5* MS_PER_SEC);
    
    // gpio_set(GPIO_PIN(PA, 13));
    // ztimer_sleep(ZTIMER_MSEC, 3 * MS_PER_SEC);
    // gpio_clear(GPIO_PIN(PA, 13));
    // gpio_set(DS18_PARAM_PIN);
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

    // _gnrc_netif_config(0, NULL);

    // gpio_set(GPIO_PIN(PA, 13));
    // //gpio_set(DS18_PARAM_PIN);

    /*FS part*/
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
    // gpio_set(GPIO_PIN(PA, 13));
    // gpio_set(DS18_PARAM_PIN);
    /*11111111111111111*/
    vfs_mount(&flash_mount);

    char path1[] = "/sd0/DATAtest.TXT";
    // int fo = open(path1, O_RDWR | O_CREAT, 00777);
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

    // if (write(fo, test_data, strlen(test_data)) != (ssize_t)strlen(test_data)) {
    //     puts("Error while writing");
    // }
    fclose(fo);
    int fr = open(path1, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
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
       
    close(fr);
    puts("closing file");

    /*11111111111111111*/
    //vfs_umount_jl(&flash_mount);
    vfs_umount(&flash_mount, false);   
     // gpio_set(DS18_PARAM_PIN);
    puts("flash point umount");
    // gpio_clear(GPIO_PIN(PA, 13));
    
    /*sychronization*/
    //



    
    ds3231_get_time(&_dev, &current_time);
    puts("This is the current system time");
    ds3231_print_time(current_time);
    /*DS18 INIT*/
    // gpio_set(GPIO_PIN(PA, 13));


    // while (message_ack_flag == 0){
    // // xtimer_sleep(3);
    // int argc2 = 6;
    // char *argv2[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};//15-4 maximum 128 bytes
    // // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};

    // _coap_result = gcoap_cli_cmd(argc2,argv2);
    // xtimer_sleep(5);

    // }
    // message_ack_flag = 0;

    while (message_ack_flag == 0){
        int argc1 = 4;
        char *argv1[] = {"coap", "get", "[2001:630:d0:1000::d6f8]:5683", "/realtime"}; //glacsweb-pi
        // char *argv1[] = {"coap", "get", "[2001:630:d0:1000::d6f9]:5683", "/realtime"};//glacsweb-jiawei
        // char *argv1[] = {"coap", "get", "[2001:db8::58a4:8450:8511:6445]:5683", "/riot/value"};
        _coap_result = gcoap_cli_cmd(argc1,argv1);

        ztimer_sleep(ZTIMER_MSEC, 2* MS_PER_SEC);
        // xtimer_sleep(10);

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

    // gpio_set(GPIO_PIN(PA,14));


    
    // xtimer_sleep(10);
    int sens = 0;
    char buffer[128];  // Adjust the buffer size according to your expectations of the data size
    ssize_t bytes_read;
    int16_t temperature_test;
    float ds18_data_test = 0.00;  
    int nn =0;
    // while(1){
        while (sens!=6){
    
            // gpio_set(DS18_PARAM_PIN);
            // gpio_set(GPIO_PIN(PA, 13));
            // vfs_DIR mount = {0};
            puts("******************\n");

            /* list mounted file systems */

            // puts("mount points:");
            // while (vfs_iterate_mount_dirs(&mount)) {
            //     printf("\t%s\n", mount.mp->mount_point);
            // }
            vfs_mount(&flash_mount);
            char path2[] = "/sd0/DATA";
            sprintf(data_file_path2, "%s%d.txt", path2, nn);
            int fo = open(data_file_path2, O_RDWR | O_CREAT | O_APPEND, 00777);
            while (fo < 0) {
                printf("error while trying to create %s\n", data_file_path2);
                fo = open(data_file_path2, O_RDWR | O_CREAT | O_APPEND, 00777);
            }
            
            puts("creating file success");
        
            /*DS18 INIT*/
            // gpio_set(GPIO_PIN(PA, 13));
            result = ds18_init(&dev18, &ds18_params[0]);
            if (result == DS18_ERROR) {
                puts("[Error] The sensor pin could not be initialized");
                return 1;
            }
            // gpio_set(GPIO_PIN(PA, 13));
            /* Get temperature in centidegrees celsius */
            ds18_get_temperature(&dev18, &temperature_test);
            bool negative = (temperature_test < 0);
            ds18_data_test = (float) temperature_test/100;
            if (negative) {
                ds18_data_test = -ds18_data_test;
            }
            
            
            printf("Temperature [ºC]: %c%.2f"
                    "\n+-------------------------------------+\n",
                    negative ? '-': '+',
                    ds18_data_test);
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
            printf("%s\n",payloadtest);
            // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
            


            if (write(fo, payloadtest, strlen(payloadtest)) != (ssize_t)strlen(payloadtest)) {
                puts("Error while writing");
            }
            close(fo);

            int fr = open(data_file_path2, O_RDONLY, 00777);  //before open with O_RDWR which 
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
            
            close(fr);
            puts("closing file");
            
            vfs_umount(&flash_mount, false);   
            // gpio_set(DS18_PARAM_PIN);
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
        // gpio_set(DS18_PARAM_PIN);
        puts("flash point umount");
    
        // while (message_ack_flag == 0){
        // xtimer_sleep(3);
        int argc = 6;
        
        char *argv[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f8]:5683", "/data", buffer};  //glacsweb-pi
        // char *argv[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", buffer};  //glacsweb-jiawei

        // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};

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
        // while (message_ack_flag != 1 && retry < 3){
        //     _coap_result = gcoap_cli_cmd(argc,argv);

        //     ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC);
        //     // xtimer_sleep(10);

        //     if (_coap_result == 0) {
        //         printf("Command executed successfully\n");
                
        //     } 
        //     else {
        //         printf("Command execution failed\n");
                
        //     }
        //     retry++;
        // }        
        // res = ds3231_await_alarm(&_dev);
        // if (res < 0){
        // puts("error: unable to program GPIO interrupt or to clear alarm flag");
        // }
        // if (!(res & DS3231_FLAG_ALARM_1)){
        // puts("error: alarm was not triggered");
        // }

        // puts("awaiting\n");
        // gpio_toggle(DS3231_PARAM_INT_PIN);

        sens =0;
        /*check start*/
        
        // ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC);

        if (message_ack_flag == 1){
            // count_successful++;
        }
        // successful_rate=count_successful/ (float)count_total_try;
        // printf("packet deliver rate : %f\n", successful_rate);
        message_ack_flag = 0;
        
        /*check finish*/
        
        // res = ds3231_clear_alarm_1_flag(&_dev);
        // if (res != 0) {
        //     puts("error: unable to clear alarm flag");
        //     return 1;
        // }
        
    // }
    // Check the result
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


    /*tiny test to measure how long an ACK can be received after sent PUT request*/
    /*identified ACK receiving gap is around 110ms*/
    // while (1){
    // retry = 0;
    // // while (message_ack_flag != 1 && retry < 3){
    //     _coap_result = gcoap_cli_cmd(argc,argv);
    //     if (_coap_result == 0) {
    //         printf("Command executed successfully\n");
    //         gpio_clear(GPIO_PIN(PA , 13));    
    //     } 
    //     else {
    //         printf("Command execution failed\n");
            
    //     }
    //     while(message_ack_flag != 1){
    //         puts("waitting for the message sent flag\n");
    //         ztimer_sleep(ZTIMER_MSEC, 110* NS_PER_MS);
    //     }
    //     gpio_set(GPIO_PIN(PA , 13));   
    //     count_total_try++;
    // // }
    // xtimer_sleep(3);
    // }
    /*test finish*/


    //
    radio_off(radio_netif);

   

    int retries = 0;
    // /*test*/
    //     while (1){
    //     struct tm testtime;
    //     int slots = 86400 / sensing_rate; 
    //     int times = communication_rate / sensing_rate;
    //     char data_buffer[128];  
    //     if (86400 % sensing_rate != 0 ){
    //         extra_slots = 1 ;
    //     }
    //     else{
    //         extra_slots = 0 ;
    //     }
    //     char file_path[] = "/sd0/data";
    //     /*sensing state start*/
    //     for (int counter_slots = 0; counter_slots != slots; counter_slots++){
    //         res = ds3231_clear_alarm_1_flag(&_dev);
    //         if (res != 0) {
    //             puts("error: unable to clear alarm flag");
    //             return 1;
    //         }            
    //         res = ds3231_get_time(&_dev, &testtime);
    //         if (res != 0) {
    //             puts("error: unable to read time");
    //             return 1;
    //         }
    //         // ds3231_print_time(testtime);
    //         // int mktime0=mktime(&testtime);
    //         // printf("%d\n",mktime0);
    //         testtime.tm_sec += sensing_rate;// * ONE_S;
    //         // int mktime1=mktime(&testtime);
    //         mktime(&testtime);
    //         // printf("%d\n",mktime1);
    //         /*DS18 INIT*/
    //         // gpio_set(GPIO_PIN(PA, 13));
    //         if (counter_slots % times == 0){
    //             //commnunication start
                
    //             radio_on(netif);
    //             vfs_mount(&flash_mount);
    //             sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
    //             int fd_com = open(data_file_path, O_RDONLY | O_CREAT, 00777);
    //             while ((fd_com < 0) && (retries < 3)){
    //                 printf("error while trying to create %s\n", data_file_path);
    //                 perror("Failed to open file for sending");
    //                 fd_com = open(data_file_path, O_RDONLY | O_CREAT, 00777);
    //                 ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
    //                 retries++;
    //             }
    //             if (fd_com >= 0){
    //                 bytes_read = read(fd_com, data_buffer, sizeof(data_buffer) - 1);  // Leave space for null terminator
    //                 if (bytes_read < 0) {
    //                     perror("Failed to read from file");
    //                     close(fd_com);
    //                     return 1;
    //                 }
    //                 data_buffer[bytes_read] = '\0'; 
    //                 printf("Read data: %s\n", data_buffer);
    //                 close(fd_com);
    //                 vfs_umount(&flash_mount, false);   
    //                 // gpio_set(DS18_PARAM_PIN);
    //                 puts("flash point umount");
    //                 // int counter_retry = 0;
    //                 // while (message_ack_flag == 0 && counter_retry < 3){
    //                     // xtimer_sleep(3);
    //                 // int argc = 6;
    //                 // char *argv[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", data_buffer};
    //                 //     // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};
    //                 // message_ack_flag = 0;
    //                 // // while (message_ack_flag != 1 && retry < 3){
    //                 // // _coap_result = gcoap_cli_cmd(argc,argv);

    //                 // // ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC);
    //                 // // // xtimer_sleep(10);

    //                 // // if (_coap_result == 0) {
    //                 // //     printf("Command executed successfully\n");
                        
    //                 // // } 
    //                 // // else {
    //                 // //     printf("Command execution failed\n");
    //                 // //     xtimer_sleep(1);
    //                 // // }
    //                 // // retry++;
    //                 // // } 
    //                 // retries = 0;
    //                 // while (message_ack_flag != 1 && retries < 3){
    //                 //     _coap_result = gcoap_cli_cmd(argc,argv);
    //                 //     if (_coap_result == 0) {
    //                 //         printf("Command executed successfully\n");
                            
    //                 //         while(message_ack_flag != 1){
    //                 //         puts("waitting for the message sent flag\n");
    //                 //         ztimer_sleep(ZTIMER_MSEC, 0.1* MS_PER_SEC); //DO NOT use NS_PER_MS as it curshs program 
    //                 //     }
    //                 //     } 
    //                 //     else {
    //                 //         printf("Command execution failed\n");
                            
    //                 //     }
    //                 //     retries++;
    //                 //     count_total_try++;
    //                 // }
    //                 // // ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC);
    //                 // // xtimer_sleep(1);
    //                 //     // counter_retry ++;
    //                 // // }
                    
    //                 // // ztimer_sleep(ZTIMER_MSEC, 1U * US_PER_MS);

                    
    //                 // if (message_ack_flag == 1){
    //                 //     count_successful++;
    //                 // }
                    
    //                 // successful_rate=count_successful/ (float)count_total_try;
    //                 // printf("packet deliver rate : %f out of %d tries.\n", successful_rate, count_total_try);
                    
    //                 // Check the result
    //                 // if (_coap_result == 0) {
    //                 //     printf("Command executed successfully\n");
    //                 // } else {
    //                 //     printf("Command execution failed\n");
    //                 // }
    //                 data_numbering = data_numbering +1;    
    //             } else{
    //                 close(fd_com);
    //                 data_numbering = data_numbering +1;
    //             }
    //             radio_off(netif);
    //         }
    //         printf("%d", data_numbering);
            
    //         result = ds18_init(&dev18, &ds18_params[0]);
    //         if (result == DS18_ERROR) {
    //             puts("[Error] The sensor pin could not be initialized");
    //             return 1;
    //         }
            
    //         /*DS18 sensing*/
        
            
    //         // int16_t temperature;
    //         // float ds18_data = 0.00;   
    //         retries = 0;
    //         // gpio_set(DS18_PARAM_PIN);
    //         // gpio_set(GPIO_PIN(PA, 13));
    //         vfs_mount(&flash_mount);

    //         sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
    //         int ff = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);
    //         while ((ff < 0) && (retries < 3)){
    //             close(ff);
    //             vfs_umount(&flash_mount, false);
    //             printf("error while trying to create %s\n", data_file_path);
    //             perror("Failed to open file for reading");
    //             ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
    //             retries++;
    //             vfs_mount(&flash_mount);
    //             ff = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);
    //         }
    //         if (ff >= 0){

    //             puts("creating file success");
            
    //             // gpio_set(GPIO_PIN(PA, 13));
    //             // /* Get temperature in centidegrees celsius */
    //             // ds18_get_temperature(&dev18, &temperature);
    //             // bool negative = (temperature < 0);
    //             // ds18_data = (float) temperature/100;
    //             // if (negative) {
    //             //     ds18_data = -ds18_data;
    //             // }
                
                
    //             // printf("Temperature [ºC]: %c%.2f"
    //             //         "\n+-------------------------------------+\n",
    //             //         negative ? '-': '+',
    //             //         ds18_data);
    //             // char test[40];
    //             // int len = fmt_float(test,ds18_data,2);
    //             // ds3231_get_time(&_dev, &current_time);
    //             // int current_sensing_time = mktime(&current_time);
    //             // if (negative) {
    //             //     test[0] = '-';
    //             //     len = 1 + fmt_float(test + 1, -ds18_data_test, 2); // Ensure the float is positive for correct formatting.
    //             // } else {
    //             //     test[0] = '+';
    //             //     len = 1 + fmt_float(test + 1, ds18_data_test, 2);
    //             // }
    //             // len += snprintf(test + len, sizeof(test) - len, ",%d,", current_sensing_time);
    //             // if (len >= (int)sizeof(test) - 2) {  // Ensure there's space for two more characters and a null terminator
    //             //     test[sizeof(test) - 1] = '\0';
    //             // } else {
    //             //     puts("Not enough space to append characters");
    //             // }
    //             // printf("%s\n",test);
    //             // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
                


    //             // if (write(ff, test, strlen(test)) != (ssize_t)strlen(test)) {
    //             //     puts("Error while writing");
    //             // }

    //             // char test_data[] = "1710953364/+23.23/1710953364/-22.22/1710953364/+22.22/1710953364/+58.56/1710953364/+23.23/1710953364/";
                
    //             char test_data[50];
    //             sprintf(test_data, "%d", sens);

    //             if (write(ff, test_data, strlen(test_data)) != (ssize_t)strlen(test_data)) {
    //             puts("Error while writing");
    //             }
    //             close(ff);
    //             sens++;
    //             int fr = open(data_file_path, O_RDONLY, 00777);  //before open with O_RDWR which 
    //                                                                     //will conflict with open(file)
    //                                                                     //open(file)will equal 0, have to beb a O_RDPNLY for read
    //             // char data_buf[sizeof(test_data)];
    //             // printf("data:[],length=");
    //             // vfs_read(fo,data_buf,sizeof(test_data));    
    //             // printf("data:[],length=");
    //             char c;

    //             while (read(fr, &c, 1) != 0){
    //             putchar(c);  //printf won't work here
    //             }
    //             puts("\n");
                
    //             close(fr);
    //             puts("closing file");

    //             /*11111111111111111*/
    //             //vfs_umount_jl(&flash_mount);
    //             // }
    //             vfs_umount(&flash_mount, false);
    //             // gpio_set(DS18_PARAM_PIN);
    //             // gpio_set(GPIO_PIN(PA, 13));
    //             puts("flash point umount");
    //         }
    //         else{
    //             close(ff);
    //         }
    //         puts("start alarm1");
    //         // res = ds3231_enable_bat(&_dev);
    //         res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
    //         if (res != 0) {
    //             puts("error: unable to program alarm");
    //             return 1;
    //         }

    //         pm_set(SAML21_PM_MODE_STANDBY);

    //         puts(" WAKED UP SUCCESSFULLY ");
    //     }
    //     if (extra_slots == 1){
    //             res = ds3231_clear_alarm_1_flag(&_dev);
    //             if (res != 0) {
    //                 puts("error: unable to clear alarm flag");
    //                 return 1;
    //             }
    //             res = ds3231_get_time(&_dev, &testtime);
    //             if (res != 0) {
    //                 puts("error: unable to read time");
    //                 return 1;
    //             }
    //             testtime.tm_sec += (86400 -sensing_rate * slots) ;
    //             mktime(&testtime);
    //             /*DS18 INIT*/
    //             // gpio_set(GPIO_PIN(PA, 13));
    //             result = ds18_init(&dev18, &ds18_params[0]);
    //             if (result == DS18_ERROR) {
    //                 puts("[Error] The sensor pin could not be initialized");
    //                 return 1;
    //             }
                
    //             /*DS18 sensing*/
    //             int16_t temperature;
    //             float ds18_data = 0.00;


    //             gpio_set(DS18_PARAM_PIN);
    //             // gpio_set(GPIO_PIN(PA, 13));
    //             vfs_mount(&flash_mount);
    //             retries = 0;
    //             char file_path[] = "/sd0/data";
    //             sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
    //             int fo = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);

    //             while ((fo < 0) && (retries < 3)){
    //                 close(fo);
    //                 vfs_umount(&flash_mount, false);
    //                 printf("error while trying to create %s\n", data_file_path);
    //                 perror("Failed to open file for reading");
    //                 ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
    //                 retries++;
    //                 vfs_mount(&flash_mount);
    //                 fo = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);
    //                 // return 1;
    //             }
    //             // else{
    //             puts("creating file success");
    //             // }
    //             // gpio_set(GPIO_PIN(PA, 13));
    //             /* Get temperature in centidegrees celsius */
    //             ds18_get_temperature(&dev18, &temperature);
    //             bool negative = (temperature < 0);
    //             ds18_data = (float) temperature/100;
    //             if (negative) {
    //                 ds18_data = -ds18_data;
    //             }
                
                
    //             printf("Temperature [ºC]: %c%.2f"
    //                     "\n+-------------------------------------+\n",
    //                     negative ? '-': '+',
    //                     ds18_data);
    //             char test[100];
    //             // fmt_float(test,ds18_data,2);
    //             int len = fmt_float(test,ds18_data,2);
    //             ds3231_get_time(&_dev, &current_time);
    //             int current_sensing_time = mktime(&current_time);
    //             if (negative) {
    //                 test[0] = '-';
    //                 len = 1 + fmt_float(test + 1, -ds18_data_test, 2); // Ensure the float is positive for correct formatting.
    //             } else {
    //                 test[0] = '+';
    //                 len = 1 + fmt_float(test + 1, ds18_data_test, 2);
    //             }
    //             len += snprintf(test + len, sizeof(test) - len, ",%d,", current_sensing_time);
    //             if (len >= (int)sizeof(test) - 2) {  // Ensure there's space for two more characters and a null terminator
    //                 test[sizeof(test) - 1] = '\0';
    //             } else {
    //                 puts("Not enough space to append characters");
    //             }
    //             printf("%s\n",test);
    //             // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
    //             // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
                

    //             if (write(fo, test, strlen(test)) != (ssize_t)strlen(test)) {
    //                 puts("Error while writing");
    //             }
    //             close(fo);
    //             int fr = open(data_file_path, O_RDONLY, 00777);  //before open with O_RDWR which 
    //                                                                     //will conflict with open(file)
    //                                                                     //open(file)will equal 0, have to beb a O_RDPNLY for read
    //             // char data_buf[sizeof(test_data)];
    //             // printf("data:[],length=");
    //             // vfs_read(fo,data_buf,sizeof(test_data));    
    //             // printf("data:[],length=");
    //             char c;

    //             while (read(fr, &c, 1) != 0){
    //             putchar(c);  //printf won't work here
    //             }
    //             puts("\n");
                
    //             close(fr);
    //             puts("closing file");

    //             /*11111111111111111*/
    //             //vfs_umount_jl(&flash_mount);
    //             vfs_umount(&flash_mount, false);
    //             // gpio_set(DS18_PARAM_PIN);
    //             // gpio_set(GPIO_PIN(PA, 13));
    //             puts("flash point umount");
    //             puts("start alarm2");
    //             // res = ds3231_enable_bat(&_dev);
    //             res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
    //             if (res != 0) {
    //                 puts("error: unable to program alarm");
    //                 return 1;
    //             }

    //             pm_set(SAML21_PM_MODE_STANDBY);

    //             puts(" WAKED UP SUCCESSFULLY ");
                        
    //         }
    //         //else {
    //     /*sensing state end*/
    //     /*
    //     if (extra_slots){
    //         int sensing_count = slots ++;
    //     }
    //     else{
    //         int sensing_count = slots;
    //     }
    //     */
    //         //}
    // }

    // /*stop*/
    int count_sleepgap = 1;
    struct tm testtime;
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
        testtime.tm_sec += 100 * count_sleepgap;// * ONE_S;
        mktime(&testtime);
        
        int argc2 = 6;
        char *argv2[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f8]:5683", "/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};   //glacsweb-pi
        //char *argv2[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", "+11.11,1111111111,+22.22,2222222222,+33.33,3333333333,+44.44,4444444444,+55.55,5555555555,+66.66,6666666666,"};  //glacsweb-jiawei
        // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};

        
        message_ack_flag = 0;
        retries = 0;
        while (message_ack_flag != 1 && retries < 3){
            _coap_result = gcoap_cli_cmd(argc2,argv2);
            if (_coap_result == 0) {
                printf("Command executed successfully\n");
                
                while(message_ack_flag != 1){
                puts("waitting for the message sent flag\n");
                ztimer_sleep(ZTIMER_MSEC, 0.1* MS_PER_SEC); //DO NOT use NS_PER_MS as it curshs program 
            }
            }else {
                printf("Command execution failed\n");
                
            }
            retries++;
            count_total_try++;
        }
        
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

        puts(" WAKED UP SUCCESSFULLY ");

    }

    while (1){
        struct tm testtime;
        int slots = 86400 / sensing_rate; 
        int times = communication_rate / sensing_rate;
        int line_number = 0;
        char line[LINE_SIZE];
        char data_buffer[BUFFER_SIZE];  
        char file_path[] = "/sd0/data";
        if (86400 % sensing_rate != 0 ){
            extra_slots = 1 ;
        }
        else{
            extra_slots = 0 ;
        }
        
        /*sensing state start*/
        for (int counter_slots = 0; counter_slots != slots; counter_slots++){
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
            
            // ds3231_print_time(testtime);
            // int mktime0=mktime(&testtime);
            // printf("%d\n",mktime0);
            testtime.tm_sec += sensing_rate;// * ONE_S;
            // int mktime1=mktime(&testtime);
            mktime(&testtime);
            // printf("%d\n",mktime1);
            /*DS18 INIT*/
            // gpio_set(GPIO_PIN(PA, 13));

            //bug start
            if (counter_slots % times == 0){
                //commnunication start
                
                radio_on(radio_netif);

                vfs_mount(&flash_mount);

            
                FILE *file = fopen(file_sensing_data, "r");
                if (!file) {
                    perror("Error opening file");
                    return -1;
                }

                int total_lines = 0;
                
                while (fgets(line, sizeof(line), file)) {
                    total_lines++;
                }

                fclose(file);
                // int total_lines = get_total_lines(file_sensing_data);
                if (total_lines < 0) {
                    return 1;
                }
                printf("%d\n",total_lines);
                // total_lines=0;
                // if (extra_slots ==1){
                //     int lines_reading = times;
                // } else {
                //     int lines_reading = times +1;
                // }
                
                int start_line = total_lines - times;
                if (start_line < 0) {
                    start_line = 0;  // read from line 0 if line number less than times
                }

                FILE *fd_com = fopen(file_sensing_data, "r");
                while ((!fd_com) && (retries < 3)){
                    printf("error while trying to OPEN %s\n", file_sensing_data);
                    perror("Failed to open file for sending");
                    fd_com = fopen(file_sensing_data, "r");
                    ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
                    retries++;
                }
                if (fd_com){
                    // puts("aaa\n");
                    /*read the sensing data since last communication window*/  
                    int current_line = 0;
                    data_buffer[0] = '\0';  // init data_buffer from the begining of every communication window
                    while (fgets(line, sizeof(line), fd_com)) {
                        if (current_line >= start_line) {
                            strncat(data_buffer, line, BUFFER_SIZE - strlen(data_buffer) - 1);
                        }
                        current_line++;
                        if (current_line >= start_line + times) {
                            break;
                        }
                    }
                    // /*old version read*/
                    // bytes_read = read(fd_com, data_buffer, sizeof(data_buffer) - 1);  // Leave space for null terminator
                    // if (bytes_read < 0) {
                    //     perror("Failed to read from file");
                    //     close(fd_com);
                    //     return 1;
                    // }
                    // data_buffer[bytes_read] = '\0'; 
                    printf("Read data: %s\n", data_buffer);
                    fclose(fd_com);
                    vfs_umount(&flash_mount, false);   
                    // gpio_set(DS18_PARAM_PIN);
                    puts("flash point umount");
                    // int counter_retry = 0;
                    // while (message_ack_flag == 0 && counter_retry < 3){
                        // xtimer_sleep(3);
                    int argc = 6;
                    char *argv[] = {"coap", "put", "-c", "[2001:630:d0:1000::d6f9]:5683", "/data", data_buffer};  //glacsweb-jiawei
                        // char *argv[] = {"coap", "put", "[2001:630:d0:1000::d6f9]:5683", "/riot/value", "1710939181/+24.23/"};
                    message_ack_flag = 0;
                    // while (message_ack_flag != 1 && retry < 3){
                    // _coap_result = gcoap_cli_cmd(argc,argv);

                    // ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC);
                    // // xtimer_sleep(10);

                    // if (_coap_result == 0) {
                    //     printf("Command executed successfully\n");
                        
                    // } 
                    // else {
                    //     printf("Command execution failed\n");
                    //     xtimer_sleep(1);
                    // }
                    // retry++;
                    // } 
                    retries = 0;
                    while (message_ack_flag != 1 && retries < 3){
                        _coap_result = gcoap_cli_cmd(argc,argv);
                        if (_coap_result == 0) {
                            printf("Command executed successfully\n");
                            
                            while(message_ack_flag != 1){
                            puts("waitting for the message sent flag\n");
                            ztimer_sleep(ZTIMER_MSEC, 0.1* MS_PER_SEC); //DO NOT use NS_PER_MS as it curshs program 
                        }
                        }else {
                            printf("Command execution failed\n");
                            
                        }
                        retries++;
                        count_total_try++;
                    }
                    // ztimer_sleep(ZTIMER_MSEC, 0.2* MS_PER_SEC);
                    // xtimer_sleep(1);
                        // counter_retry ++;
                    // }
                    
                    // ztimer_sleep(ZTIMER_MSEC, 1U * US_PER_MS);

                    
                    if (message_ack_flag == 1){
                        count_successful++;
                    }
                    else{
                        vfs_mount(&flash_mount);
                        sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
                        int f_databackup = open(data_file_path, O_RDWR | O_CREAT | O_APPEND, 00777);
                        if (f_databackup >= 0){
                            strncat(data_buffer, "\n", BUFFER_SIZE - strlen(data_buffer) - 1);
                            if (write(f_databackup, data_buffer, strlen(data_buffer)) != (ssize_t)strlen(data_buffer)) {
                                puts("Error while writing");
                            }
                            close(f_databackup);
                        }
                        vfs_umount(&flash_mount, false);  
                    }
                    
                    successful_rate=count_successful/ (float)count_total_try;
                    printf("packet deliver rate : %f out of %d tries.\n", successful_rate, count_total_try);
                    
                    message_ack_flag = 0;
                    // Check the result
                    // if (_coap_result == 0) {
                    //     printf("Command executed successfully\n");
                    // } else {
                    //     printf("Command execution failed\n");
                    // }
                    data_numbering = data_numbering +1;    
                } else{
                    fclose(fd_com);
                    vfs_umount(&flash_mount, false); 
                    data_numbering = data_numbering +1;
                }
                radio_off(radio_netif);
            }
            
            printf("%d", data_numbering);
            
            result = ds18_init(&dev18, &ds18_params[0]);
            if (result == DS18_ERROR) {
                puts("[Error] The sensor pin could not be initialized");
                return 1;
            }
            
            /*DS18 sensing*/
        
            
            int16_t temperature;
            float ds18_data = 0.00;   
            retries = 0;
            // gpio_set(DS18_PARAM_PIN);
            // gpio_set(GPIO_PIN(PA, 13));
            vfs_mount(&flash_mount);

            // sprintf(data_file_path, "%s%d.txt", file_path, data_numbering);
            int ff = open(file_sensing_data, O_RDWR | O_CREAT | O_APPEND, 00777);
            while ((ff < 0) && (retries < 3)){
                close(ff);
                vfs_umount(&flash_mount, false);
                printf("error while trying to create %s\n", file_sensing_data);
                perror("Failed to open file for reading");
                ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
                retries++;
                vfs_mount(&flash_mount);
                ff = open(file_sensing_data, O_RDWR | O_CREAT | O_APPEND, 00777);
            }
            if (ff >= 0){

                puts("creating file success");
            
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
                char test[40];
                int len = fmt_float(test,ds18_data,2);
                ds3231_get_time(&_dev, &current_time);
                int current_sensing_time = mktime(&current_time);
                if (negative) {
                    test[0] = '-';
                    len = 1 + fmt_float(test + 1, -ds18_data_test, 2); // Ensure the float is positive for correct formatting.
                } else {
                    test[0] = '+';
                    len = 1 + fmt_float(test + 1, ds18_data_test, 2);
                }
                len += snprintf(test + len, sizeof(test) - len, ",%d,\n", current_sensing_time);
                if (len >= (int)sizeof(test) - 2) {  // Ensure there's space for two more characters and a null terminator
                    test[sizeof(test) - 1] = '\0';
                } else {
                    puts("Not enough space to append characters");
                }
                printf("%s\n",test);
                // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
                


                if (write(ff, test, strlen(test)) != (ssize_t)strlen(test)) {
                    puts("Error while writing");
                }
                line_number++;
                close(ff);
                // int fr = open(data_file_path, O_RDONLY, 00777);  //before open with O_RDWR which 
                                                                        //will conflict with open(file)
                                                                        //open(file)will equal 0, have to beb a O_RDPNLY for read
                // char data_buf[sizeof(test_data)];
                // printf("data:[],length=");
                // vfs_read(fo,data_buf,sizeof(test_data));    
                // printf("data:[],length=");
                // char c;

                // while (read(fr, &c, 1) != 0){
                // putchar(c);  //printf won't work here
                // }
                // puts("\n");
                
                // close(fr);
                puts("closing file");

                /*11111111111111111*/
                //vfs_umount_jl(&flash_mount);
                // }
                vfs_umount(&flash_mount, false);
                // gpio_set(DS18_PARAM_PIN);
                // gpio_set(GPIO_PIN(PA, 13));
                puts("flash point umount");
            }
            else{
                close(ff);
                vfs_umount(&flash_mount, false);
            }
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
                testtime.tm_sec += (86400 -sensing_rate * slots) ;
                mktime(&testtime);
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
                retries = 0;

                // gpio_set(DS18_PARAM_PIN);
                // gpio_set(GPIO_PIN(PA, 13));
                vfs_mount(&flash_mount);

                int fe = open(file_sensing_data, O_RDWR | O_CREAT | O_APPEND, 00777);

                while ((fe < 0) && (retries < 3)){
                    close(fe);
                    vfs_umount(&flash_mount, false);
                    printf("error while trying to create %s\n", file_sensing_data);
                    perror("Failed to open file for reading");
                    ztimer_sleep(ZTIMER_MSEC, 1* MS_PER_SEC);
                    retries++;
                    vfs_mount(&flash_mount);
                    fe = open(file_sensing_data, O_RDWR | O_CREAT | O_APPEND, 00777);
                    // return 1;
                }
                if (fe >= 0){
                    puts("creating file success");
                    // }
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
                    // fmt_float(test,ds18_data,2);
                    int len = fmt_float(test,ds18_data,2);
                    ds3231_get_time(&_dev, &current_time);
                    int current_sensing_time = mktime(&current_time);
                    if (negative) {
                        test[0] = '-';
                        len = 1 + fmt_float(test + 1, -ds18_data_test, 2); // Ensure the float is positive for correct formatting.
                    } else {
                        test[0] = '+';
                        len = 1 + fmt_float(test + 1, ds18_data_test, 2);
                    }
                    len += snprintf(test + len, sizeof(test) - len, ",%d,\n", current_sensing_time);
                    if (len >= (int)sizeof(test) - 2) {  // Ensure there's space for two more characters and a null terminator
                        test[sizeof(test) - 1] = '\0';
                    } else {
                        puts("Not enough space to append characters");
                    }
                    printf("%s\n",test);
                    // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
                    // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
                    

                    if (write(fe, test, strlen(test)) != (ssize_t)strlen(test)) {
                        puts("Error while writing");
                    }
                    line_number++;
                    close(fe);
                    // int fr = open(data_file_path, O_RDONLY, 00777);  //before open with O_RDWR which 
                    //                                                         //will conflict with open(file)
                    //                                                         //open(file)will equal 0, have to beb a O_RDPNLY for read
                    // // char data_buf[sizeof(test_data)];
                    // // printf("data:[],length=");
                    // // vfs_read(fo,data_buf,sizeof(test_data));    
                    // // printf("data:[],length=");
                    // char c;

                    // while (read(fe, &c, 1) != 0){
                    // putchar(c);  //printf won't work here
                    // }
                    // puts("\n");
                    
                    // close(fe);
                    puts("closing file");

                    /*11111111111111111*/
                    //vfs_umount_jl(&flash_mount);
                    vfs_umount(&flash_mount, false);
                    // gpio_set(DS18_PARAM_PIN);
                    // gpio_set(GPIO_PIN(PA, 13));
                    puts("flash point umount");
                } else {
                    close(fe);
                    vfs_umount(&flash_mount, false);
                }
                puts("start alarm2");
                // res = ds3231_enable_bat(&_dev);
                res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
                if (res != 0) {
                    puts("error: unable to program alarm");
                    return 1;
                }

                pm_set(SAML21_PM_MODE_STANDBY);

                puts(" WAKED UP SUCCESSFULLY ");
                        
            }
            //else {
        /*sensing state end*/
        /*
        if (extra_slots){
            int sensing_count = slots ++;
        }
        else{
            int sensing_count = slots;
        }
        */
            //}
    }


    // while (1) {
    //     struct tm testtime;
        
    //     /* read time and compare to initial value */
    //     res = ds3231_get_time(&_dev, &testtime);
    //     if (res != 0) {
    //         puts("error: unable to read time");
    //         return 1;
    //     }
    //     // print_time("current time is:", &current_time);

    //     // size_t pos = strftime(dstr, ISOSTR_LEN, "%Y-%m-%dT%H:%M:%S", &testtime);
    //     // dstr[pos] = '\0';
    //     // printf("The current time is: %s\n", dstr);    
    //     ds3231_print_time(testtime);

    //     /* clear all existing alarm flag */
    //     res = ds3231_clear_alarm_1_flag(&_dev);
    //     if (res != 0) {
    //         puts("error: unable to clear alarm flag");
    //         return 1;
    //     }

    //     /* get time to set up next alarm*/
    //     res = ds3231_get_time(&_dev, &testtime);
    //     if (res != 0) {
    //         puts("error: unable to read time");
    //         return 1;
    //     }
    //     // ds3231_print_time(testtime);
    //     // puts("setting up wakeup dalay for 10s");
    //     testtime.tm_sec += TEST_DELAY;
    //     mktime(&testtime);

    //     /*radio off*/
    //     radio_off(netif);
    //     // res = ds3231_disable_bat(&_dev);
    //     /* set alarm */
    //     puts("start alarm1");
    //     // res = ds3231_enable_bat(&_dev);
    //     res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
    //     if (res != 0) {
    //         puts("error: unable to program alarm");
    //         return 1;
    //     }


    //     // gpio_clear(GPIO_PIN(PA,16));
    //     // gpio_clear(GPIO_PIN(PA,17));
    //     pm_set(SAML21_PM_MODE_STANDBY);

    //     puts(" WAKED UP SUCCESSFULLY ");
    //     // res = ds3231_disable_bat(&_dev);
    //     res = ds3231_clear_alarm_1_flag(&_dev);
    //     if (res != 0) {
    //         puts("error: unable to clear alarm flag");
    //         return 1;
    //     }

    //     /*wait for the pin gpio goes high*/
    //     res = ds3231_get_time(&_dev, &testtime);
    //     if (res != 0) {
    //         puts("error: unable to read time");
    //         return 1;
    //     }

    //     ds3231_print_time(testtime);

    //     testtime.tm_sec += (2*TEST_DELAY);
    //     mktime(&testtime);
    //     /*radio on*/
    //     radio_on(netif);

    //     /*DS18 INIT*/
    //     // gpio_set(GPIO_PIN(PA, 13));
    //     result = ds18_init(&dev18, &ds18_params[0]);
    //     if (result == DS18_ERROR) {
    //         puts("[Error] The sensor pin could not be initialized");
    //         return 1;
    //     }
        
    //     /*DS18 sensing*/
    
        
    //     int16_t temperature;
    //     float ds18_data = 0.00;
        
        
        
    //     gpio_set(DS18_PARAM_PIN);
    //     // gpio_set(GPIO_PIN(PA, 13));
    //     vfs_mount(&flash_mount);

    //     char data_file_path1[] = "/sd0/DATA.TXT";
    //     int fo = open(data_file_path1, O_RDWR | O_CREAT, 00777);
    //     if (fo < 0) {
    //         printf("error while trying to create %s\n", data_file_path1);
    //         return 1;
    //     }
    //     else{
    //         puts("creating file success");
    //     }
    //     // gpio_set(GPIO_PIN(PA, 13));
    //     /* Get temperature in centidegrees celsius */
    //     ds18_get_temperature(&dev18, &temperature);
    //     bool negative = (temperature < 0);
    //     ds18_data = (float) temperature/100;
    //     if (negative) {
    //         ds18_data = -ds18_data;
    //     }
        
        
    //     printf("Temperature [ºC]: %c%.2f"
    //             "\n+-------------------------------------+\n",
    //             negative ? '-': '+',
    //             ds18_data);
    //     char test[100];
    //     fmt_float(test,ds18_data,2);
    //     printf("%s\n",test);
    //     // sprintf(test, "%c%f", negative ? '-': '+', ds18_data);
        


    //     if (write(fo, test, strlen(test)) != (ssize_t)strlen(test)) {
    //         puts("Error while writing");
    //     }
    //     close(fo);
    //     int fr = open(data_file_path1, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
    //                                                             //will conflict with open(file)
    //                                                             //open(file)will equal 0, have to beb a O_RDPNLY for read
    //     // char data_buf[sizeof(test_data)];
    //     // printf("data:[],length=");
    //     // vfs_read(fo,data_buf,sizeof(test_data));    
    //     // printf("data:[],length=");
    //     char c;

    //     while (read(fr, &c, 1) != 0){
    //     putchar(c);  //printf won't work here
    //     }
    //     puts("\n");
        
    //     close(fo);
    //     puts("closing file");

    //     /*11111111111111111*/
    //     //vfs_umount_jl(&flash_mount);
    //     vfs_umount(&flash_mount, false);
    //     // gpio_set(DS18_PARAM_PIN);
    //     // gpio_set(GPIO_PIN(PA, 13));
    //     puts("flash point umount");
    //     // gpio_clear(GPIO_PIN(PA, 13));
        
    //     // gpio_clear(GPIO_PIN(PA, 13));
    //     /*DS18 sampling rate*/
    //     // ztimer_sleep(ZTIMER_USEC, SAMPLING_PERIOD * US_PER_SEC);

    //     /*CLEAR ALARM FLAG*/

        
    //     puts("start alarm2");
    //     // ztimer_sleep(ZTIMER_USEC, TEST_DELAY * US_PER_SEC);
    //     res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
    //     if (res != 0) {
    //     puts("error: unable to program alarm");
    //     return 1;
    //     }


    //     res = ds3231_await_alarm(&_dev);
    //     if (res < 0){
    //     puts("error: unable to program GPIO interrupt or to clear alarm flag");
    //     }

    //     if (!(res & DS3231_FLAG_ALARM_1)){
    //     puts("error: alarm was not triggered");
    //     }

    //     puts("OK1");
    //     /* clear all existing alarm flag */
    //     // struct tm testtime;

    //     // /* read time and compare to initial value */
    //     // res = ds3231_get_time(&_dev, &testtime);
    //     // if (res != 0) {
    //     //     puts("error: unable to read time");
    //     //     return 1;
    //     // }
    //     // // print_time("current time is:", &current_time);

    //     // // size_t pos = strftime(dstr, ISOSTR_LEN, "%Y-%m-%dT%H:%M:%S", &testtime);
    //     // // dstr[pos] = '\0';
    //     // // printf("The current time is: %s\n", dstr);    
    //     // ds3231_print_time(testtime);

    //     // /* clear all existing alarm flag */
    //     // res = ds3231_clear_alarm_1_flag(&_dev);
    //     // if (res != 0) {
    //     //     puts("error: unable to clear alarm flag");
    //     //     return 1;
    //     // }

    //     // /* get time to set up next alarm*/
    //     // res = ds3231_get_time(&_dev, &testtime);
    //     // if (res != 0) {
    //     //     puts("error: unable to read time");
    //     //     return 1;
    //     // }

    //     // ds3231_print_time(testtime);
        
    //     // puts("setting up wakeup delay for 10s");
    //     // testtime.tm_sec += TEST_DELAY;
    //     // mktime(&testtime);

    //     // /*radio off*/
    //     // radio_off(netif);
    //     // gpio_clear(GPIO_PIN(PB, 23));
    //     // gpio_set(GPIO_PIN(PA , 14));
    //     // /* set alarm */
    //     // res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_H_M_S);
    //     // if (res != 0) {
    //     //     puts("error: unable to program alarm");
    //     //     return 1;
    //     // }
        
    //     // pm_set(SAML21_PM_MODE_STANDBY);
        
    //     // puts(" WAKED UP SUCCESSFULLY ");
    //     // gpio_set(GPIO_PIN(PB , 23));
    //     // gpio_clear(GPIO_PIN(PA , 14));
    //     // /*radio on*/
    //     // radio_on(netif);

    //     // /*CLEAR ALARM FLAG*/
    //     // res = ds3231_clear_alarm_1_flag(&_dev);
    //     // if (res != 0) {
    //     //     puts("error: unable to clear alarm flag");
    //     //     return 1;
    //     // }
        
    //     // /*wait for the pin gpio goes high*/
    //     // res = ds3231_get_time(&_dev, &testtime);

    //     // ds3231_print_time(testtime);

    //     // testtime.tm_sec += TEST_DELAY;
    //     // mktime(&testtime);
    //     // // bool alarm;
    //     // // ds3231_toggle_alarm_2(&_dev, alarm);
    //     // res = ds3231_set_alarm_1(&_dev, &testtime, DS3231_AL1_TRIG_M_S);
    //     // if (res != 0) {
    //     //     puts("error: unable to program alarm");
    //     //     return 1;
    //     // }
        
    //     // pm_set(SAML21_PM_MODE_IDLE);
    //     // // ztimer_sleep(ZTIMER_USEC, TEST_DELAY * US_PER_SEC);

    //     // res = ds3231_clear_alarm_1_flag(&_dev);
    //     // if (res != 0) {
    //     //     puts("error: unable to clear alarm flag");
    //     //     return 1;
    //     // }
    //     // puts("GOING TO SLEEP NOW");
    //     // // gpio_clear(GPIO_PIN(PB, 23));
    // }


    /* start shell */
    puts("All up, running the shell now");

    puts("RIOT network stack example application");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
