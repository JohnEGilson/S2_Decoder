#include "argo.h"
#include <iostream>
//#include <iomanip>
//#include <format>
#include <list>
#include "../output/write_log.h"

#include "../json/json.hpp"

using json = nlohmann::ordered_json;
extern json config;

Argo_Mission::Argo_Mission() {
  // Argo_Mission Constructor used to set default mission values
  received = 0;
  //packet_version = -1;
  firmware_version = -1;
  float_version = 99;
  mission_number = 1;
  prof_depth_target = 150;
  park_depth_target = 150;
  rise_time_max = 180;
  fall2park_time_max = 360;
  park2prof_time_max = 0;
  drift_time_target = 0;
  ascent_speed_target = 14;
  num_seeks = 1;
  surf_time_max = 0;
  seek_time_max = 0;
  // default CTD gain and offsets
  pres_gain = 25;
  pres_offset = 10;
  temp_gain = 1000;
  temp_offset = 5;
  psal_gain = 1000;
  psal_offset = 1;
  cycle_time_max = 0;
}

void Argo_Mission::Decode(std::vector<uint8_t> &data) {
  received = 1;
  //packet_version = (data[0] & 0xf0) >> 4;
  //int pnum = data[0] & 0x0f;
  //log( std::format("Packet[{:2X}] Argo Mission ",pnum) );
  int firmware_version_major = ( data[3] & 0x0F );
  int firmware_version_minor = ( ( data[3] & 0xF0 ) >> 4);
  firmware_version = firmware_version_major + firmware_version_minor/10.;
  prof_depth_target = (data[4]<<8) + data[5];
  park_depth_target = (data[6]<<8) + data[7];
  rise_time_max = (int)((data[8]<<8) + data[9]);
  fall2park_time_max = (int)((data[10]<<8) + data[11]);
  park2prof_time_max = (int)((data[12]<<8) + data[13]);
  drift_time_target = (int)((data[14]<<8) + data[15]);
  float_version = data[16];
  ascent_speed_target = data[17]; 
  num_seeks = (data[18]<<8) + data[19]; // number of seek periods
  surf_time_max = (int)((data[20]<<8) + data[21]);
  seek_time_max = (int)((data[22]<<8) + data[23]);
  pres_gain = (data[24]<<8) + data[25];
  pres_offset = (data[26]<<8) + data[27];
  temp_gain = (data[28]<<8) + data[29];
  temp_offset = (data[30]<<8) + data[31];
  psal_gain = (data[32]<<8) + data[33];
  psal_offset = (data[34]<<8) + data[35];

  // CTD gains and offsets used for CTD Discrete,CTD Binned, CTD Drift profiles as well as engineering drift averages
  for(const auto &profile : std::list<std::string>({"CTD_Raw","CTD_Binned","CTD_Drift"})) {
    config["prof"][profile]["PRES"]["gain"] = pres_gain;
    if ( pres_gain <= 10 ) {
      config["prof"][profile]["PRES"]["col_width"] = 6;
      config["prof"][profile]["PRES"]["col_precision"] = 1;
    } else if ( pres_gain <= 100 ) { 
      config["prof"][profile]["PRES"]["col_width"] = 7;
      config["prof"][profile]["PRES"]["col_precision"] = 2;
    } else if ( pres_gain <= 1000 ) { 
      config["prof"][profile]["PRES"]["col_width"] = 8;
      config["prof"][profile]["PRES"]["col_precision"] = 3;
    } else {
      std::cout << " PRESSURE resolution is finer than code " << std::endl;
    }
    config["prof"][profile]["PRES"]["offset"] = pres_offset;
    config["prof"][profile]["TEMP"]["gain"] = temp_gain;
    if ( temp_gain <= 10 ) {
      config["prof"][profile]["TEMP"]["col_width"] = 4;
      config["prof"][profile]["TEMP"]["col_precision"] = 1;
    } else if ( temp_gain <= 100 ) { 
      config["prof"][profile]["TEMP"]["col_width"] = 5;
      config["prof"][profile]["TEMP"]["col_precision"] = 2;
    } else if ( temp_gain <= 1000 ) { 
      config["prof"][profile]["TEMP"]["col_width"] = 6;
      config["prof"][profile]["TEMP"]["col_precision"] = 3;
    } else {
      std::cout << " TEMPERATURE resolution is finer than code " << std::endl;
    }
    config["prof"][profile]["TEMP"]["offset"] = temp_offset;
    config["prof"][profile]["PSAL"]["gain"] = psal_gain;
    if ( psal_gain <= 10 ) {
      config["prof"][profile]["PSAL"]["col_width"] = 4;
      config["prof"][profile]["PSAL"]["col_precision"] = 1;
    } else if ( psal_gain <= 100 ) { 
      config["prof"][profile]["PSAL"]["col_width"] = 5;
      config["prof"][profile]["PSAL"]["col_precision"] = 2;
    } else if ( psal_gain <= 1000 ) { 
      config["prof"][profile]["PSAL"]["col_width"] = 6;
      config["prof"][profile]["PSAL"]["col_precision"] = 3;
    } else if ( psal_gain <= 10000 ) { 
      config["prof"][profile]["PSAL"]["col_width"] = 7;
      config["prof"][profile]["PSAL"]["col_precision"] = 4;
    } else {
      std::cout << " SALINITY resolution is finer than code " << std::endl;
    }
    config["prof"][profile]["PSAL"]["offset"] = psal_offset;
  }

  compute_cycle_time_max();
  //log( std::format("Packet[{:2X}] Argo Mission {:d}m park, {:d}m profile, {:.2f} days",pnum,park_depth_target,prof_depth_target,cycle_time_max) );
}

void Argo_Mission::compute_cycle_time_max() {
  // profile depth [m] / ascent speed [cm/s] x 100 [cm/m] / 3600 [s/hr] = rise time [hr]
  //std::cout << "rise time: min(" << rise_time_max << "," << 100./3600.*prof_depth_target/ascent_speed_target << ")" << std::endl;
  //std::cout << "seek: " << seek_time_max << " x " << num_seeks << " = " << seek_time_max*num_seeks << std::endl;
  //std::cout << "hours = " << std::min(rise_time_max,100./3600.*prof_depth_target/ascent_speed_target) + fall2park_time_max
  //  + drift_time_target + park2prof_time_max + (seek_time_max*num_seeks) + surf_time_max << std::endl;

  cycle_time_max = (std::min(rise_time_max,100./3600.*prof_depth_target/ascent_speed_target) + fall2park_time_max
    + drift_time_target + park2prof_time_max + (seek_time_max*num_seeks) + surf_time_max) / 24; // convert hours to days
}

