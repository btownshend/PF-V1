/*
 * SickIO.cc
 *
 *  Created on: Mar 2, 2014
 *      Author: bst
 */

#include <stdio.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <pthread.h>
#include "sickio.h"

using namespace SickToolbox;
using namespace std;

bool fake=false;

static void *runner(void *t) {
	((SickIO*)t)->run();
	return NULL;
}

SickIO::SickIO(int _id, const char *host, int port) {
	/*
	 * Initialize the Sick LMS 2xx
	 */
	id=_id;
	frame=0;
	valid=false;
	num_measurements=0;
	status=0;

	if (!fake)
		try {
			sick_lms_5xx = new SickLMS5xx(host,port);
			sick_lms_5xx->Initialize();
		} catch(...) {
			fprintf(stderr,"Initialize failed! Are you using the correct IP address?\n");
			exit(1);
		}

	pthread_create(&runThread, NULL, runner, (void *)this);
	setNumEchoes(5);
	setCaptureRSSI(true);
}

SickIO::~SickIO() {
	if (fake)
		return;

	try {
		sick_lms_5xx->Uninitialize();
	} catch(...) {
		fprintf(stderr,"Uninitialize failed!\n");
	}
}

int SickIO::startStop(bool start) {
	fprintf(stderr,"SickIO::startStop not implemented\n");
	return 1;
void SickIO::setNumEchoes(int _nechoes) {
    assert(_nechoes>=1 && _nechoes<=MAXECHOES);
    nechoes=_nechoes;
    if (fake)
	return;
    if (nechoes==1)
	sick_lms_5xx->SetSickEchoFilter(SickLMS5xx::SICK_LMS_5XX_ECHO_FILTER_FIRST);
    else
	sick_lms_5xx->SetSickEchoFilter(SickLMS5xx::SICK_LMS_5XX_ECHO_FILTER_ALL_ECHOES);
}

void SickIO::setCaptureRSSI(bool on) {
    captureRSSI=on;
    if (fake)
	return;

    if (captureRSSI)
	sick_lms_5xx->SetSickScanDataFormat(SickLMS5xx::SICK_LMS_5XX_SCAN_FORMAT_DIST_REFLECT);
    else
	sick_lms_5xx->SetSickScanDataFormat(SickLMS5xx::SICK_LMS_5XX_SCAN_FORMAT_DIST);
}

}

void SickIO::setFPS(int fps) {
	fprintf(stderr,"SickIO::setFPS not implemented\n");
}

void SickIO::setRes(const char *res) {
	fprintf(stderr,"SickIO::setRes not implemented\n");
}

void SickIO::run() {
	printf("SickIO::run()\n");
	while (true)
		get();
}


void SickIO::get() {
	try {
		//unsigned int range_2_vals[SickLMS5xx::SICK_LMS_5XX_MAX_NUM_MEASUREMENTS];
		//sick_lms_5xx.SetSickScanFreqAndRes(SickLMS5xx::SICK_LMS_5XX_SCAN_FREQ_25,
		//SickLMS5xx::SICK_LMS_5XX_SCAN_RES_25);
		//sick_lms_5xx.SetSickScanDataFormat(SickLMS5xx::SICK_LMS_5XX_DIST_DOUBLE_PULSE,
		//				         SickLMS5xx::SICK_LMS_5XX_REFLECT_NONE);
		assert(nechoes>=1 && nechoes<=MAXECHOES);
		if (fake) {
			num_measurements=190*4;
			for (int i=0;i<(int)num_measurements;i++) {
				for (int e=0;e<nechoes;e++) {
					range[e][i]=i+e*100;
					reflect[e][i]=100/(e+1);
				}
			}
			status=1;
			usleep(30000);
		} else
			sick_lms_5xx->GetSickMeasurements(
				range[0], (nechoes>=2)?range[1]:NULL, (nechoes>=3)?range[2]:NULL, (nechoes>=4)?range[3]:NULL, (nechoes>=5)?range[4]:NULL,
				captureRSSI?reflect[0]:NULL, (captureRSSI&&nechoes>=2)?reflect[1]:NULL, (captureRSSI&&nechoes>=3)?reflect[2]:NULL, (captureRSSI&&nechoes>=4)?reflect[3]:NULL, (captureRSSI&&nechoes>=5)?reflect[4]:NULL,
				num_measurements,&status);
	}

	catch(const SickConfigException & sick_exception) {
		printf("%s\n",sick_exception.what());
	}

	catch(const SickIOException & sick_exception) {
		printf("%s\n",sick_exception.what());
	}

	catch(const SickTimeoutException & sick_exception) {
		printf("%s\n",sick_exception.what());
	}

	catch(...) {
		fprintf(stderr,"An Error Occurred!\n");
		throw;
	}

	gettimeofday(&acquired,0);
	frame++;
	valid=true;
	printf("Got %d measurements, status=%d\n",num_measurements,status);
}



