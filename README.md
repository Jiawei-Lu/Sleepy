
/test/newriotvfstest
### Test application for the E-IoT System with CoAP, RPL and 6LoWPAN, 
	Hardware platform: Microchip SAM R30 XPRO
	Extension board: IO1 Xplained extension
	RTC: DS3231
	Sensor: DS18
	
	Feature Updates: (19/06/2024)
	1. Adapting to the latest RIOT-OS file system and IO1 Xplained extension board sdcard spi & params.
	2. Node communication using CoAP: request PUT/GET
	3. Working sleepy network schdule
	4. CoAP PUT Confirmable with ACK reply
 	5. ACK and message ID reply check scheme 
	6. Node GET real time from BR-libcoap server (/realtime)
		* libcoap server inet6 address [2001:630:d0:1000::d6f9]:5683
		* libcoap server complie: gcc -I /usr/local/include/ coap-server.c -o coap-server -l coap-3-openssl
		* libcaop error: ./coap-server: symbol lookup error: ./coap-server: undefined symbol --> cd liibcoap and follow the 
				 install guidance ofgithub libcoap install --> export LD_LIBRARY_PATH=/usr/local/lib -->gcc as above
		* libcoap server: now = my_clock_base + (t / COAP_TICKS_PER_SECOND) +3600; // +3600 here to apapt to UK Summer Time
	7. Environment ready & Code frame ready for : fixing error in PUT data from sd card file and Payload text.
	8. PUT SD card data to server successful.
	9. Data structure: temp,timestamp,......,temp,timestamp,......
	10.Payload test: ready for further radio conflict test.
	11.PAYLOAD test 128 bytes coap maxsize with 115bytes text.
	12.Fix IO1 bug.
	13.Tried to fix unrecongnized coap realtime reply.
	14.Schedule should not use ONE_S, cause 1000 times bigger gap
	15.CoAP PUT before GET /realtime to sychronize, this can help to avoid long wait to get correct sychronized clock using GCoAP
	16.CFLAGS += -DGCOAP_RESPONSE_TIMEOUT=5000 (default 2000).
	17.Modified Client.c to test weather the GET /realtime value be used to set DS3231 RTC successfully. (may cause 1s delay caused by program run time, fixed message_ack_flag BUG)
	18.Data structure has include "negative? +:-"
	19.Communication slot BUG fixed, sensor reading and store BUG fixed.
	20.Stop CoAP auto-retransmission by adding "CFLAGS += -DCONFIG_COAP_MAX_RETRANSMIT=0" into Makefile.
	21.Set CoAP PUT retry to 3 manually, it can be fixed by identifying the gap between each retry set by RIOT.
	22.Set CFLAGS += -DCONFIG_GCOAP_PDU_BUF_SIZE=1024 to test if multiple and long time test will cause stack overflow by CoAP PUT.
	23.In CoAP Client, buf[CONFIG_GCOAP_PDU_BUF_SIZE] defined in "int gcoap_cli_cmd(int argc, char **argv)" is moved and defined as a globl variable. 
	24.Set CFLAGS += -DTHREAD_STACKSIZE_MAIN=\(3*THREAD_STACKSIZE_DEFAULT\), which is copied from /RIOT/tests/nannocoap_cli to enable more sensible stacksize. 
	25.Modified schedule: 
		* adjust communication retry (within MAX 3*1s reading + 0.1s*3 PUT)
		* if openfile failed, then close() and start alarm
		* set alarm time before communication start
	26.CoAP retry: ztimer_sleep(ZTIMER_MSEC, 0.1* MS_PER_SEC); as the gap between CoAP PUT and ACK rceive is around 110 ms //DO NOT use NS_PER_MS as it curshs program.
	27.open() to create new .txt file has a numberlimitation which is 512, use fopen instead.
	28.Sensing data will be stored line by line, and each CoAP PUT will send last "times" line (times = communication_rate / sensing_rate;).
	29.Still need to change the part of extra line 
	30.Modified the addr obtain and get multicast and global addr okay
	
	
******* RIOT-Sniffer usage hint:
	1. cd applications/sniffer: sudo make BOARD=samr30-xpro PORT=/dev/ttyACM0 flash
	2. cd /tools: in the makefile, define channel page number for 0 of subGHz
	3. sudo ./sniffer.py /dev/ttyACM0 0 | sudo wireshark -k -i -


******* Libcoap server IPV6 address problem:
	1. add ipv6 address to interface :
		a. sudo nano /etc/netplan/01-netcfg.yaml  -->>Netplan (default in Ubuntu 18.04 and later), the file could be in /etc/netplan/01-netcfg.yaml. For older versions, it might 				 
		   be /etc/network/interfaces.
		b. open file, find the interface name.
		c. sudo ifconfig <interface name> inet6 add 2001:630:d0:1000::d6fx [x can be any numeber]
		d. ping 2001:630:d0:1000::d6fx
		e. coap get 2001:630:d0:1000::d6fx to verify
	2. OpenSSL: sudo apt-get install libssl-dev
	3. Libcoap: cd ~/libcoap ---> export LD_LIBRARY_PATH=/usr/local/lib
	   Libcoap_server_JL: cd ~/libcoap_server_JL ---> export LD_LIBRARY_PATH=/usr/local/lib
