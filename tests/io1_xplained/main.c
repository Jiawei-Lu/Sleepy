/*
 * Copyright (C) 2016 Inria
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief       Test application for the Atmel IO1 Xplained extension
 *
 * @author      Alexandre Abadie <alexandre.abadie@inria.fr>
 *
 * @}
 */

#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>

#include "timex.h"
#include "ztimer.h"
#include "board.h"

#include "periph/gpio.h"

#include "at30tse75x.h"
#include "sdcard_spi.h"
#include "io1_xplained.h"
#include "io1_xplained_params.h"

/*vfs default*/
#include "shell.h"
#include "vfs_default.h"
#include "timex.h"
#include "ztimer.h"
#include "board.h"
/*fats_vfs*/
#include "fs/fatfs.h"
#include "vfs.h"
#include "mtd.h"
//#include "board.h"
#if MODULE_MTD_SDMMC
#include "mtd_sdmmc.h"
#elif MODULE_MTD_SDCARD
#include "mtd_sdcard.h"
#include "sdcard_spi.h"
#include "sdcard_spi_params.h"
#endif

#if FATFS_FFCONF_OPT_FS_NORTC == 0
#include "periph/rtc.h"
#endif

#define MNT_PATH  "/test"
#define FNAME1 "TEST.TXT"
#define FNAME2 "NEWFILE.TXT"
#define FNAME "DATA.TXT"
#define FNAME_RNMD  "RENAMED.TXT"
#define FNAME_NXIST "NOFILE.TXT"
#define FULL_FNAME1 (MNT_PATH "/" FNAME1)
#define FULL_FNAME2 (MNT_PATH "/" FNAME2)
#define DATA_FILE1 (FLASH_MOUNT_POINT "/" FNAME)
#define FULL_FNAME_RNMD  (MNT_PATH "/" FNAME_RNMD)
#define FULL_FNAME_NXIST (MNT_PATH "/" FNAME_NXIST)
#define DIR_NAME "SOMEDIR"
static const char test_txt[]  = "the test file content 123 abc";
static const char test_txt2[] = "another text";
// static const char test_txt3[] = "hello world for vfs";
#define FLASH_MOUNT_POINT  "/sd0" /*mount point*/

static fatfs_desc_t fatfs;

static vfs_mount_t _test_vfs_mount = {
    .mount_point = MNT_PATH,
    .fs = &fatfs_file_system,
    .private_data = (void *)&fatfs,
};

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
#endif


///////////////////////////////////////////////////////////////////////////////////

#define DELAY_1S   (1U * MS_PER_SEC) /* 1 seconds delay between each test */

static io1_xplained_t dev;

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
static void print_test_result(const char *test_name, int ok)
{
    printf("%s:[%s]\n", test_name, ok ? "OK" : "FAILED");
}

int main(void)
{
    float temperature;

    puts("IO1 Xplained extension test application\n");
    puts("+-------------Initializing------------+\n");

    if (io1_xplained_init(&dev, &io1_xplained_params[0]) != IO1_XPLAINED_OK) {
        puts("[Error] Cannot initialize the IO1 Xplained extension\n");
        return 1;
    }
    /* Card detect pin is inverted */

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


    /*fats_vfs*/
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
    /*test _test_vfs_mount*/
    puts("testing _test_vfs_mount\n");
    puts("+-------------Testing------------+\n");
    int fd;
    // print_test_result("test_open__mount", vfs_mount(&_test_vfs_mount) == 0);

    /* try to open file that doesn't exist */
    fd = vfs_open(FULL_FNAME_NXIST, O_RDONLY, 0);
    print_test_result("test_open__open", fd == -ENOENT);

    /* open file with RO, WO and RW access */
    fd = vfs_open(FULL_FNAME1, O_RDONLY, 0);
    print_test_result("test_open__open_ro", fd >= 0);
    print_test_result("test_open__close_ro", vfs_close(fd) == 0);
    fd = vfs_open(FULL_FNAME1, O_WRONLY, 0);
    print_test_result("test_open__open_wo", fd >= 0);
    print_test_result("test_open__close_wo", vfs_close(fd) == 0);
    fd = vfs_open(FULL_FNAME1, O_RDWR, 0);
    print_test_result("test_open__open_rw", fd >= 0);
    print_test_result("test_open__close_rw", vfs_close(fd) == 0);

    print_test_result("test_open__umount", vfs_umount(&_test_vfs_mount, false) == 0);

    puts("testing flash_mount\n");
    puts("+-------------Testing------------+\n");
    int fdFLASH;
    print_test_result("test_open__mount", vfs_mount(&flash_mount) == 0);

    /* try to open file that doesn't exist */
    fdFLASH = vfs_open(FULL_FNAME_NXIST, O_RDONLY, 0);
    print_test_result("test_open__open", fdFLASH == -ENOENT);

    /* open file with RO, WO and RW access */
    fdFLASH = vfs_open(DATA_FILE1, O_RDONLY, 0);
    print_test_result("test_open__open_ro", fdFLASH >= 0);
    print_test_result("test_open__close_ro", vfs_close(fdFLASH) == 0);
    fdFLASH = vfs_open(DATA_FILE1, O_WRONLY, 0);
    print_test_result("test_open__open_wo", fdFLASH >= 0);
    print_test_result("test_open__close_wo", vfs_close(fdFLASH) == 0);
    fdFLASH = vfs_open(DATA_FILE1, O_RDWR, 0);
    print_test_result("test_open__open_rw", fdFLASH >= 0);
    print_test_result("test_open__close_rw", vfs_close(fdFLASH) == 0);
    char path[] = "/sd0/DATA.TXT";
    int fo = open(path, O_RDWR | O_CREAT, 00777);
    if (fo < 0) {
        printf("error while trying to create %s\n", path);
        return 1;
    }
    else{
        puts("creating file success");
    }
    char test_data[] = "111111111111111111111111111";


    if (write(fo, test_data, strlen(test_data)) != (ssize_t)strlen(test_data)) {
        puts("Error while writing");
    }
    close(fo);
    int fr = open(path, O_RDONLY | O_CREAT, 00777);  //before open with O_RDWR which 
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

    print_test_result("test_open__umount", vfs_umount(&flash_mount, false) == 0);

    print_test_result("test_rw__mount", vfs_mount(&_test_vfs_mount) == 0);

    char buf[sizeof(test_txt) + sizeof(test_txt2)];
    ssize_t nr, nw;
    fdFLASH = vfs_open(DATA_FILE1, O_RDONLY, 0);
    print_test_result("test_rw__open_ro", fdFLASH >= 0);

    /* compare file content with expected value */
    memset(buf, 0, sizeof(buf));
    nr = vfs_read(fdFLASH, buf, sizeof(test_txt));
    print_test_result("test_rw__read_ro", (nr == sizeof(test_txt)) &&
                      (strncmp(buf, test_txt, sizeof(test_txt)) == 0));

    /* try to write to RO file (success if no bytes are actually written) */
    nw = vfs_write(fdFLASH, test_txt2, sizeof(test_txt2));
    print_test_result("test_rw__write_ro", nw <= 0);
    print_test_result("test_rw__close_ro", vfs_close(fdFLASH) == 0);

    fdFLASH = vfs_open("/sd0/data.txt", O_WRONLY, 0);
    print_test_result("test_rw__open_wo", fdFLASH >= 0);

    /* try to read from WO file (success if no bytes are actually read) */
    nr = vfs_read(fdFLASH, buf, sizeof(test_txt));
    print_test_result("test_rw__read_wo", nr <= 0);

    print_test_result("test_rw__close_wo", vfs_close(fdFLASH) == 0);



    puts("\n+--------Starting tests --------+");
    while (1) {
        /* Get temperature in degrees celsius */
        at30tse75x_get_temperature(&dev.temp, &temperature);
        printf("Temperature [°C]: %i.%03u\n"
               "+-------------------------------------+\n",
               (int)temperature,
               (unsigned)((temperature - (int)temperature) * 1000));
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        
        if (!gpio_read(IO1_SDCARD_SPI_PARAM_DETECT)) {
            _sd_card_cid();
            ztimer_sleep(ZTIMER_MSEC, DELAY_1S);
        }

        uint16_t light;
        io1_xplained_read_light_level(&light);
        printf("Light level: %i\n"
               "+-------------------------------------+\n",
               light);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* set led */
        gpio_set(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* clear led */
        gpio_clear(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* toggle led */
        gpio_toggle(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);

        /* toggle led again */
        gpio_toggle(IO1_LED_PIN);
        ztimer_sleep(ZTIMER_MSEC, DELAY_1S);
    }

    return 0;
}
