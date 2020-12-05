/*******************************************************
 Windows HID simplification

 Alan Ott
 Signal 11 Software

 8/22/2009

 Copyright 2009

 This contents of this file may be used by anyone
 for any reason without any conditions and may be
 used as a starting point for your own applications
 which use HIDAPI.
********************************************************/

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include "hidapi.h"

// Headers needed for sleeping.
#ifdef _WIN32
	#include <windows.h>
#else
	#include <unistd.h>
#endif

float acc_cal(unsigned char l, unsigned char h){
	return (float) ((int16_t) ((h<<8) + l))/32768 * 16 * 9.8; 
}

float angv_cal(unsigned char l, unsigned char h){
	return (float) ((int16_t) ((h<<8) + l))/32768 * 2000; 
}

float ang_cal(unsigned char l, unsigned char h){
	return (float) ((int16_t) ((h<<8) + l))/32768 * 180; 
}

int16_t mag_cal(unsigned char l, unsigned char h){
	return (int16_t) ((h<<8) + l); 
}

void packet_parsing(unsigned char* packet){
	unsigned char sensor_date[8];
	unsigned char sensor_data[24];
	unsigned char i;
	memcpy(sensor_date, &packet[2], 8);
	printf("20%02d-%02d-%02d %02d:%02d:%02d.%03d \n",
			sensor_date[0], sensor_date[1], sensor_date[2], sensor_date[3],
			sensor_date[4], sensor_date[5], (int)(sensor_date[7]<<8) + sensor_date[6]);
	for(i = 0; i < 6; i++){
		if(packet[i * 25 + 10] == i + 0x50){
			printf("    sensor%d \n", i);
			memcpy(sensor_data, &packet[i * 25 + 10 + 1], 24);
			printf("        acc  x:%f, y:%f, z:%f \n",  acc_cal(sensor_data[0],sensor_data[1]),
														acc_cal(sensor_data[2],sensor_data[3]),
														acc_cal(sensor_data[4],sensor_data[5]));
			printf("        agv  x:%f, y:%f, z:%f \n",  angv_cal(sensor_data[0+6],sensor_data[1+6]),
														angv_cal(sensor_data[2+6],sensor_data[3+6]),
														angv_cal(sensor_data[4+6],sensor_data[5+6]));
			printf("        ang  x:%f, y:%f, z:%f \n",  ang_cal(sensor_data[0+12],sensor_data[1+12]),
														ang_cal(sensor_data[2+12],sensor_data[3+12]),
														ang_cal(sensor_data[4+12],sensor_data[5+12]));
			printf("        mag  x:%d, y:%d, z:%d \n",  mag_cal(sensor_data[0+18],sensor_data[1+18]),
														mag_cal(sensor_data[2+18],sensor_data[3+18]),
														mag_cal(sensor_data[4+18],sensor_data[5+18]));
		}
	}

}

int main(int argc, char* argv[])
{
	(void)argc;
	(void)argv;

	int res;
	unsigned char buf[64];
	unsigned char data[1984];
	unsigned char packet[161];
	unsigned char sum;
	int offset;
	#define MAX_STR 64
	wchar_t wstr[MAX_STR];
	hid_device *handle;
	int i;
	int j;

	int count;
	int err_cnt;
	int err;

	// struct hid_device_info *devs, *cur_dev;

	printf("hidapi test/example tool. Compiled with hidapi version %s, runtime version %s.\n", HID_API_VERSION_STR, hid_version_str());
	if (hid_version()->major == HID_API_VERSION_MAJOR && hid_version()->minor == HID_API_VERSION_MINOR && hid_version()->patch == HID_API_VERSION_PATCH) {
		printf("Compile-time version matches runtime version of hidapi.\n\n");
	}
	else {
		printf("Compile-time version is different than runtime version of hidapi.\n]n");
	}

	if (hid_init())
		return -1;

	// Set up the command buffer.
	memset(buf,0x00,sizeof(buf));
	buf[0] = 0x01;
	buf[1] = 0x81;


	// Open the device using the VID, PID,
	// and optionally the Serial number.
	////handle = hid_open(0x4d8, 0x3f, L"12345");
	handle = hid_open(0x1920, 0x0100, NULL);
	if (!handle) {
		printf("unable to open device\n");
 		return 1;
	}

	// Read the Manufacturer String
	wstr[0] = 0x0000;
	res = hid_get_manufacturer_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read manufacturer string\n");
	printf("Manufacturer String: %ls\n", wstr);

	// Read the Product String
	wstr[0] = 0x0000;
	res = hid_get_product_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read product string\n");
	printf("Product String: %ls\n", wstr);

	// Read the Serial Number String
	wstr[0] = 0x0000;
	res = hid_get_serial_number_string(handle, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read serial number string\n");
	printf("Serial Number String: (%d) %ls", wstr[0], wstr);
	printf("\n");

	// Read Indexed String 1
	wstr[0] = 0x0000;
	res = hid_get_indexed_string(handle, 1, wstr, MAX_STR);
	if (res < 0)
		printf("Unable to read indexed string 1\n");
	printf("Indexed String 1: %ls\n", wstr);

	// Set the hid_read() function to be non-blocking.
	hid_set_nonblocking(handle, 1);

	// Try to read from the device. There should be no
	// data here, but execution should not block.
	res = hid_read(handle, buf, 17);


	memset(buf,0,sizeof(buf));

	// Read requested state. hid_read() has been set to be
	// non-blocking by the call to hid_set_nonblocking() above.
	// This loop demonstrates the non-blocking nature of hid_read().
	count = 0;
	err_cnt = 0;
	while(1){
		count++;
		err =1;
		res = 0;
		offset = 0;
		memset(data,0,sizeof(data));
		memset(packet,0,sizeof(packet));
		for (i = 0; i < 32; i++)
		{
			res = hid_read(handle, buf, 64);
			if (res == 64){
				memcpy(&data[offset*62], &buf[2], 62);
				offset ++;
			}
			if(res == 0){
				continue;
			}
			if(res < 0 ){
				break;
			}
		}
		for (i = 0; i < sizeof(data); i++){
			if(data[i] == 0x55 && data[i+1] == 0x80 && i < sizeof(data) - 160){
				sum = 0;
				for(j = 0; j< 160; j++)
					sum += data[j+i];
				if(sum == data[i+160]){
					memcpy(packet,&data[i],161);
					err = 0;
					// printf("Data read: (recv:%d err:%d ratio:%2.1f%%)\n   ",count, err_cnt, (float)err_cnt/count*100);
					// packet_parsing(packet);
					// printf("\n");
					break;
				}
			}
		}
		if(err == 0){
			printf("Data read: (recv:%d err:%d ratio:%2.1f%%)\n   ",count, err_cnt, (float)err_cnt/count*100);
			//Print out the returned buffer.
			// for (i = 0; i < sizeof(packet); i++)
			// 	printf("%02hhx ", packet[i]);
			
			packet_parsing(packet);
			printf("\n");
		}else{
			err_cnt ++;
			// for (i = 0; i < sizeof(data); i++)
			// 	printf("%02hhx ", data[i]);
			usleep(10*1000);
		}
		
		usleep(100*1000);
	}
	hid_close(handle);

	/* Free static HIDAPI objects. */
	hid_exit();

#ifdef WIN32
	system("pause");
#endif

	return 0;
}
