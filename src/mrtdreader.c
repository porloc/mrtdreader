/*
 *   mrtdreader.c - Main program 
 *
 *   Copyright (C) 2014 Ruben Undheim <ruben.undheim@gmail.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <nfc/nfc.h>
#include "crypto.h"
#include "bac.h"
#include "bachelper.h"
#include "fileread.h"

#define MAX_DEVICE_COUNT 5

void printhex(char *description, uint8_t *input, int length)
{
	int i;
	printf("%s: ",description);
	for(i=0;i<length;i++){
		printf("%02x",input[i]);
	}
	printf("\n");
}

int main(int argc, char **argv)
{
	int i,ret,res;
	if(argc != 2){
		fprintf(stderr,"Usage: %s <kmrz>\n",argv[0]);
		exit(-1);
	}
	uint8_t *kmrz = argv[1];

	nfc_context *context = NULL;
	nfc_init(&context);
	nfc_device *pnd = NULL;
	nfc_target ant;

	if(context == NULL){
		fprintf(stderr,"Unable to init libnfc (malloc)\n");
		goto failed;
	}
	nfc_connstring connstrings[MAX_DEVICE_COUNT];
	size_t szDeviceFound = nfc_list_devices(context,connstrings,MAX_DEVICE_COUNT);

	if(szDeviceFound == 0){
		fprintf(stderr,"No NFC device found.\n");
		goto failed;
	}

	pnd = nfc_open(context, connstrings[0]);
	if(pnd == NULL){
		fprintf(stderr,"Unable to open NFC device: %s\n",connstrings[0]);
		goto failed;
	}
	if(nfc_initiator_init(pnd) < 0){
		nfc_perror(pnd,"nfc_initiator_init");
		goto failed;
	}
	printf("NFC device: %s opened\n",nfc_device_get_name(pnd));

	nfc_modulation nm;
	nm.nmt = NMT_ISO14443B;
	nm.nbr = NBR_106;

	while(nfc_initiator_select_passive_target(pnd,nm,NULL,0,&ant) <= 0);
	printf("Target found!\n");

	uint8_t txbuffer[300];
	int txlen;
	uint8_t rxbuffer[300];
	int rxlen;


	uint8_t rnd_icc[8];
	uint8_t kicc[16];
	uint64_t ssc_long;
	uint8_t ksenc[16];
	uint8_t ksmac[16];

	ret = mrtd_bac_keyhandshake(pnd,kmrz,ksenc,ksmac,&ssc_long);

	if(ret == RET_CHALLENGE_FAILED){
		fprintf(stderr,"======================\nChallenge failed...\n======================\n");
		goto failed;
	}
	else if(ret < 0){
		goto failed;
	}
	printf("======================\nChallenge successful!\n======================\n");

	//printhex("ksenc",ksenc,16);
	//printhex("ksmac",ksmac,16);

	//printf("ssc: %lx\n",ssc_long);
	printf("\n");
	uint8_t filecontent[17000];
	int filecontentlength;

	printf("Getting EF.COM...");
	fflush(stdout);
	mrtd_fileread_read(pnd,"\x01\x1e",filecontent,&filecontentlength,ksenc,ksmac,&ssc_long);
	printf(" done\n");

	printhex("File content",filecontent,filecontentlength);
	printf("File size: %d\n",filecontentlength);

	printf("\n");

	printf("Getting EF.DG1...");
	fflush(stdout);
	mrtd_fileread_read(pnd,"\x01\x01",filecontent,&filecontentlength,ksenc,ksmac,&ssc_long);
	printf(" done\n");

	filecontent[filecontentlength] = 0;
	printf("%s\n\n",filecontent);
	printf("File size: %d\n",filecontentlength);

	printf("\n");
	printf("Getting EF.DG2 which contains the image...");
	fflush(stdout);
	mrtd_fileread_read(pnd,"\x01\x02",filecontent,&filecontentlength,ksenc,ksmac,&ssc_long);
	printf(" done\n");


	mrtd_fileread_write_image_to_file(filecontent, filecontentlength, "image.jpg");
	printf("Image saved to image.jpg\n");

	nfc_close(pnd);
	nfc_exit(context);
	return 0;

	failed:
		if(pnd != NULL)
			nfc_close(pnd);
		if(context != NULL)
			nfc_exit(context);
		return -1;
}

