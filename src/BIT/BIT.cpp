#include "BIT.h"
#include <iomanip>
#include <iostream>
//#include <format>
#include "../output/write_log.h"

void BIT::parse(std::vector<uint8_t> data) {

    received = 1;

    EngVer = data[3]; //Grab BIT Engineering Version from data

    unsigned int n = 4;
    nQueued = data[n];
    n += 1;

    // Check if BIT OK [0x5] or BIT FAIL [0x6]; status is only included if BIT FAIL
    if (data[0] == 0xe5) {
		// BIT OK
		status = 0;
    } else {
		// BIT Fail
 		status = ( data[n]*256 + data[n+1] );
		n += 2;
    }

        int pressure_cnt = data[n]*256 + data[n+1]; // raw count = 1/800? SIGNED
        if ( pressure_cnt > 32767 ) {
          pressure_cnt = pressure_cnt - 65536;
        }
        pressure = pressure_cnt *0.00125;
    n += 2;
	cpu_voltage = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 10mV
    n += 2;
	pump_voltage_prior = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 10mV
    n += 2;
	pump_voltage_after = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 10mV
    n += 2;
	pump_current = data[n]*256 + data[n+1]; // raw count = mA
    n += 2;
	pump_time = data[n]*256 + data[n+1];
	n += 2;
	pump_oil_prior = data[n++];
	pump_oil_after = data[n++];
	vacuum_prior = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 0.01 inHg
	n += 2;
	vacuum_after = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 0.01 inHg
	n += 2;
	valve_open = data[n++];
	valve_close = data[n++];
	interrupt_id = data[n]*256 + data[n+1];
	n += 2;
    // 30 byte string from the SBE reply to "PTS"
        for( int c = 0; c < 30; c++ ) {
          if ( data[n] >= 43 && data[n] <= 57  && data[n] != 47 )
	      SBE_response.push_back(data[n]); // only push expected chars
          n++;
	}

//Check for Engineering version
    if ( EngVer >= 6 ) {
	cpu_temp = float(data[n++]) / 8.0 - 2.0; // convert raw cpu_temp counts to degC
	RH = data[n];
    }
}


void BIT_Beacon::parse(std::vector<uint8_t> data) {

  received = 1;
  EngVer = data[3]; //Grab BIT Engineering Version from data

  unsigned int n = 4;
  nQueued = data[n];
  n += 1;
  nTries = data[n]*256 + data[n+1];
  n += 2;
  parXstat = data[n]*256 + data[n+1];
  n += 2;
  SBDIstat= data[n]*256 + data[n+1];
  n += 2;
  SBDsecs = data[n]*256 + data[n+1];
  n += 2;
  cpu_voltage = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 10mV
  n += 2;
  pump_voltage= 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 10mV
  n += 2;
  vacuum_now = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 0.01 inHg
  n += 2;
  vacuum_abort = 0.01 * ( data[n]*256 + data[n+1] ); // raw count = 0.01 inHg
  n += 2;
  ISRID = data[n]*256 + data[n+1];
  n += 2;
  abortFlag = data[n]*256 + data[n+1];
  n += 2;
//Check for Engineering version
  if ( EngVer >= 6) {
      CPUtemp = 0.125 * data[n++] - 2.0;
      RH = data[n++];
  }

}

void Science::Parse(std::vector<uint8_t> data) {
//Not used in S2
  received = 1;

  version = data[7] + (data[9]/10.0);
  files = data[10]*256 + data[11];
  nWritten = data[12]*256 + data[13];
  mbFree = data[14]*256 + data[15];
  ctdParseErr = data[16]*256 + data[17];
  if (data[18] == ';')
    return;
  jumpCtr = data[18]*256 + data[19]; // included starting with firmware v2.4+
}
