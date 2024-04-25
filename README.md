
/test/newriotvfstest
### Test application for the E-IoT System with CoAP, RPL and 6LoWPAN, 
	Hardware platform: Microchip SAM R30 XPRO
	Extension board: IO1 Xplained extension
	RTC: DS3231
	Sensor: DS18
	
	Feature Updates: (12/04/2024)
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
	17.Modified Client.c to test weather the GET /realtime value be used to set DS3231 RTC successfully. (may cause 1s delay caused by program run time)
	18.Data structure has include "negative? +:-"
	
******* RIOT-Sniffer usage hint:
	1. cd applications/sniffer: sudo make BOARD=samr30-xpro PORT=/dev/ttyACM0 flash
	2. cd /tools: in the makefile, define channel page number for 0 of subGHz
	3. sudo ./sniffer.py /dev/ttyACM0 0 | sudo wireshark -k -i -

