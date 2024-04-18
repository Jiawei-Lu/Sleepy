
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
	7.

