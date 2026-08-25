#include <iostream> // BG DEBUG
#include <fstream>
#include <regex>
#include <sstream>
#include <cmath>
#include <filesystem>
#include <algorithm> //JG addition
#include <boost/date_time.hpp>
using namespace boost::posix_time;
#include "../hexfile/hexfile.h"
#include "../json/json.hpp"

#include "../output/write_log.h"
//#include <format>

using json = nlohmann::ordered_json;
extern json config;
extern string S2_PATH;
extern int MAJOR_VERSION,MINOR_VERSION;

auto DACid=0;
auto TRid=0;

std::string decimal( float val, int cwidth, int cpres ) {
    std::stringstream ss;
	// Test for NaN; JSON does not support NaN. Send "null" instead
	if (std::isnan(val))
		ss << "null";
	else if (val == -999)
		ss << std::setw(cwidth) << -999; // if -999, no need to pad out extra decimals; -999.0000
	else
    	ss << std::fixed << std::setw(cwidth) << std::setprecision(cpres) << val;
    return ss.str();
}

boost::posix_time::ptime utcnow() {
  return boost::posix_time::second_clock::universal_time();
}

string date_format( const ptime &t, string format ) {
  int yr,mo,dy,hr,mn,sc;
  const string alphabet = "ymdHMS"; // define characters to be considered symbols
  string tmp = ""; // temporary variables used to parse format string

  if ( t.is_not_a_date_time() ) {
    yr=9999; mo=99; dy=99; hr=99; mn=99; sc=99; // set time to fill if not_a_date_time
  }
  else {
    try {
      tm Date = to_tm(t);
      yr = Date.tm_year + 1900;
      mo = Date.tm_mon + 1;
      dy = Date.tm_mday;
      hr = Date.tm_hour;
      mn = Date.tm_min;
      sc = Date.tm_sec;
    }
    catch (const std::out_of_range &e) {
      yr=9999; mo=99; dy=99; hr=99; mn=99; sc=99; // set time to fill if any value is out of range
    }
  }
  // parse format string
  std::stringstream out;
  out << std::fixed << std::setfill('0');
  while ( format.size() ) {
    if (format.substr(0,4) == "yyyy") {
      out << std::setw(4) << yr;
      format.erase(0,4);
    }
    else if (format.substr(0,2) == "yy") {
      out << std::setw(2) << yr % 100;
      format.erase(0,2);
    }
    else if (format.substr(0,2) == "mm") {
      out << std::setw(2) << mo;
      format.erase(0,2);
    }
    else if (format.substr(0,2) == "dd") {
      out << std::setw(2) << dy;
      format.erase(0,2);
    }
    else if (format.substr(0,2) == "HH") {
      out << std::setw(2) << hr;
      format.erase(0,2);
    }
    else if (format.substr(0,2) == "MM") {
      out << std::setw(2) << mn;
      format.erase(0,2);
    }
    else if (format.substr(0,2) == "SS") {
      out << std::setw(2) << sc;
      format.erase(0,2);
    }
    else {
      out << format[0];
      format.erase(0,1);
    }
  }
  return out.str();
}

void hexfile::write_JSON() {
  json Doc;
  bool first1,first2,first3,first4;
  uint16_t flag;

  std::stringstream gpstr;

  // check if float subdirectories exist. If not, create them
  std::ostringstream ssjg;
  ssjg.str("");
  ssjg.clear();
  ssjg << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn);

  std::string floatdir = ssjg.str();
  if (!std::filesystem::exists(floatdir)) {
    std::filesystem::create_directory(floatdir);
//    log(std::format("+ Creating float {} subdirectory",sn));
  }
  if (!std::filesystem::exists(floatdir+"/json")) {
    std::filesystem::create_directory(floatdir + "/json");
//    log(std::format("+ Creating float {} json subdirectory",sn));
  }
  if (!std::filesystem::exists(floatdir+"/json/L0")) {
    std::filesystem::create_directory(floatdir + "/json/L0");
//    log(fmt::format("+ Creating float {} json/L0 subdirectory",sn));
  }
  if (!std::filesystem::exists(floatdir+"/json/L1")) {
    std::filesystem::create_directory(floatdir + "/json/L1");
//    log(fmt::format("+ Creating float {} json/L1 subdirectory",sn));
  }
  // Float specific meta-data. moved here from below
  //string metapath = format("{:s}/{:d}/{:d}_meta.json",std::string(config["directories"]["output"]),sn,sn);
  ssjg.str("");
  ssjg.clear();
  ssjg << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "_meta.json";
  string metapath=ssjg.str();
  ssjg.str("");
  ssjg.clear();
  // Look for SN_meta.json file in float subdirectory, if it doesn't exist, create one using default values [see config/default_meta.json]
  if (!std::filesystem::exists(metapath)) {
    string metadefault = S2_PATH + "/config/default_meta.json";
    std::filesystem::copy(metadefault,metapath);
//    log(std::format("+ Create Float {}_meta.json with default values",sn));
  }
  std::ifstream f(metapath);
  auto float_meta = nlohmann::ordered_json::parse(f); // user ordered_json to preserver order
    if (float_meta.contains("DAC_ID_NUMBER")) {
    DACid = float_meta.at("DAC_ID_NUMBER"); //save DAC_ID for json file name
  }
  if (float_meta.contains("TRANSMISSION_ID_NUMBER")) {
    TRid = float_meta.at("TRANSMISSION_ID_NUMBER"); //save DAC_ID for json file name
  }
  // end of moved 

  //Float specific config info
  ssjg.str("");
  ssjg.clear();
  ssjg << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "_config.json";
  string floatconfig=ssjg.str();
  ssjg.str("");
  ssjg.clear();
  // Look for SN_config.json file in float subdirectory. If it doesn't exist, create one using default value [see config/default_config.json]
  if (!std::filesystem::exists(floatconfig)) {
    string configdefault = S2_PATH + "/config/default_float_config.json";
    if (std::filesystem::exists(configdefault)) {
      std::filesystem::copy(configdefault,floatconfig);
    } 
  }

  //  jsonpath = format("{:s}/{:d}/json/{:d}_{:-03d}.json",std::string(config["directories"]["output"]),sn,sn,cycle);
  ssjg.str("");
  ssjg.clear();
  if (cycle<0) {
    ssjg << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/json/L0/" << std::setw(4) << std::setfill('0') << DACid << "_" << std::setw(6) << std::setfill('0') << TRid << "_L0_-01.json";
  } else if (cycle>999) {
    ssjg << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/json/L0/" << std::setw(4) << std::setfill('0') << DACid << "_" << std::setw(6) << std::setfill('0') << TRid << "_L0_" << std::setw(4) << std::setfill('0') << std::to_string(cycle) << ".json";
  } else {
      ssjg << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/json/L0/" << std::setw(4) << std::setfill('0') << DACid << "_" << std::setw(6) << std::setfill('0') << TRid << "_L0_" << std::setw(3) << std::setfill('0') << std::to_string(cycle) << ".json";
  }
  jsonpath = ssjg.str();

  //std::cout << "json path " << jsonpath << std::endl;
  std::ofstream fout(jsonpath);
  if (fout.good()) {
//    log( std::format("Writing {:s}",jsonpath) );
    }
  else {
//    log( std::format("Unable to open {:s}",jsonpath) );
	return;
  }



  fout << "{" << std::endl;

  // ====== Hexfile summary =====
  int packet_count = 0;
  int packet_bytes = 0;
  int total_bytes = 0;
  char profile_key[7];
  string desc;

  std::map<std::string,int> packets;
  std::map<std::string,int> pcnt;

  fout << "  \"FILE_CREATION_DATE\": \"" << date_format(utcnow(),config["DATE_FORMAT"]) << "\"," << std::endl;
  fout << "  \"DECODER_VERSION\": " << "\"" << MAJOR_VERSION << "." << MINOR_VERSION << "\"," << std::endl;
  fout << "  \"SCHEMA_VERSION\": " << config["SCHEMA_VERSION"] << "," << std::endl;

  // Float specific meta-data. moved above
  //string metapath = format("{:s}/{:d}/{:d}_meta.json",std::string(config["directories"]["output"]),sn,sn);
  // Look for SN_meta.json file in float subdirectory, if it doesn't exist, create one using default values [see config/default_meta.json]
  //if (!std::filesystem::exists(metapath)) {
  //  string metadefault = S2_PATH + "/config/default_meta.json";
  //  std::filesystem::copy(metadefault,metapath);
//    log(std::format("+ Create Float {}_meta.json with default values",sn));
 // }

  //std::ifstream f(metapath);
  //auto float_meta = nlohmann::ordered_json::parse(f); // user ordered_json to preserver order
  // end of moved above
   
  // iterate over meta attributes defined in floatxxx.json file
  for (auto & [key,val] : float_meta.items())
    fout << "  \"" << key << "\": " << val << "," << std::endl;
  fout << "  \"CYCLE_NUMBER\": " << cycle << "," << std::endl;


  // ====== HEX SUMMARY ==========
  fout << "  \"telemetry_summary\": [" << std::fixed << std::endl;
  first1 = true;
  for (auto & [PID,m] : messages) {
    if (!first1)
      fout << "," << std::endl;
    first1 = false;
    fout << "    { \"PID\": " << std::setw(2) << PID << ", ";
    fout << "\"source\": \"" << m.type << "\", ";
    fout << "\"TIME\": \"" << date_format(m.time,config["DATE_FORMAT"]) << "\", ";
    if ( m.momsn > 32767 ) { //convert to signed
      fout << "\"momsn\": " << m.momsn - 65536 << ", ";
    } else {
      fout << "\"momsn\": " << m.momsn << ", ";
    }
	fout << std::setfill(' ');
    fout << "\"size\": " << std::setw(4) << m.size << ", ";
    fout << "\"sensor_ids\": [";
    total_bytes += m.size;
    first2 = true;
    // compute packet statistics
    for (auto p : m.packets) {
      if (!first2)
        fout << ",";
      first2 = false;
      packet_count++;
      packet_bytes += p.size;
      int sensor_id = 0; 
      int segment = ( p.data[0] & 0x0f ) / 8; //in S2 the profile can be up to 8 IDS apart (0x90 and 0x98), Deep will be 4
      int data_id;
      if ( p.data[0] > 9 & p.data[0] < 218 ) { //these span the key that have multiple messages
        data_id = ( p.data[0] & 0xf0 ) + 8 * segment; 
      } else {
        data_id = p.data[0]; 
      }
      //std::cout << " data_id in write_json " << data_id << std::endl;
      fout << (unsigned int)data_id;
      int pro = 0; //int pro = p.data[4];

      // Pump v1 packet format does not have a fixed pro byte; hardcode it to be zero
      if (sensor_id == 4 && data_id == 4)
        pro = 0;

      sprintf(profile_key,"%02X",data_id);
      // Initialize if missing
      if (packets.count(std::string(profile_key)) == 0)
        packets[std::string(profile_key)] = 0;
      if (pcnt.count(std::string(profile_key)) == 0)
        pcnt[std::string(profile_key)] = 0;
      // Increment
      packets[std::string(profile_key)] += p.size;
      pcnt[std::string(profile_key)]++;
    }
    fout << "] }";
  }
  fout << std::endl;
  fout << "  ]," << std::endl;

  // ====== PACKETS =============
  fout << "  \"packet_info\": {" << std::fixed << std::endl;
  fout << "    \"packet_count\": " << packet_count << "," << std::endl;
  fout << "    \"packet_bytes\": " << packet_bytes << "," << std::endl;
  fout << "    \"packet_type\": [" << std::endl;

  first1 = true;
  for( auto & [key,val] : packets ) {
    if (!first1)
      fout << "," << std::endl;
    first1 = false;
    if (config["packets"].count(key))
      desc = std::string(config["packets"][key]["profile"]) + " " + std::string(config["packets"][key]["name"]);
    else
      desc = "unknown";
    fout << "      { \"id\": \"" << key << "\", \"bytes\": " << std::setw(4) << val << ", \"packets\": " << std::setw(2) << pcnt[key] << ", \"description\": \"" << desc << "\" }";
  }
  fout << std::endl << "    ]" << std::endl;
  fout << "  }";


  // ====== GPS =================
  std::map<int,std::string> gps_phase = {{0,"GPS_BIST"},{1,"GPS_START"},{2,"GPS_END"},{3,"GPS_ABORT"},{5,"GPS_BITPASS"},{6,"GPS_BITFAIL"}};
  if (gps.size()) {
    fout << "," << std::endl;
    fout << "  \"GPS\": [" << std::fixed << std::endl;
    first1 = true;
    for ( auto g : gps ) {
      if (!first1)
        fout << "," << std::endl;
      first1 = false;
      gpstr.str("");
	  gpstr << "\"" << gps_phase[g.phase] << "\"";
      fout << "    { \"description\": " << std::setw(11) << gpstr.str() << ", ";
      fout << "\"TIME\": \"" << date_format(g.gps_time,config["DATE_FORMAT"]) << "\", ";
      fout << "\"LATITUDE\": " << decimal(g.flat,9,5)  << ", ";
      fout << "\"LONGITUDE\": " << decimal(g.flon,10,5) << ", ";
      fout << "\"HDOP\": " << decimal(g.hdop,5,1) << ", ";
      fout << "\"sat_cnt\": " << std::setw(2) << g.num_sat << ", ";
      fout << "\"snr_min\": " << std::setw(2) << g.snr_min << ", ";
      fout << "\"snr_mean\": " << std::setw(2) << g.snr_mean << ", ";
      fout << "\"snr_max\": " << std::setw(2) << g.snr_max << ", ";
      fout << "\"time_to_fix\": " << std::setw(2) << g.time2fix << ", ";
      fout << "\"valid\": " << std::setw(2) << g.valid << " }";
    }
    fout << std::endl;
    fout << "  ]";
  }

  // ==== Upload command ========
  if ( upload_command.size() ) {
    fout << "," << std::endl;
    fout << "  \"Upload_Command\": \"" << upload_command << "\"";
  }

  // ==== ARGO Mission ==========
  if (argo.received) {
    fout << "," << std::endl;
    fout << "  \"ARGO_Mission\": {" << std::fixed << std::endl;
    fout << "    \"float_model\": " << argo.float_version << "," << std::endl;
    fout << "    \"float_telemetry_format\": " << decimal(argo.firmware_version,3,1) << "," << std::endl;
    fout << "    \"min_ascent_rate_cmpersec\": " << argo.ascent_speed_target << "," << std::endl;
    fout << "    \"profile_target_dbar\": " << argo.prof_depth_target << "," << std::endl;
    fout << "    \"drift_target_dbar\": " << argo.park_depth_target << "," << std::endl;
    fout << "    \"max_rise_minute\": " << decimal(argo.rise_time_max,4,0) << "," << std::endl;
    fout << "    \"max_fall_to_park_minute\": " << decimal(argo.fall2park_time_max,4,0) << "," << std::endl;
    fout << "    \"max_fall_to_profile_minute\": " << decimal(argo.park2prof_time_max,4,0) << "," << std::endl;
    fout << "    \"target_drift_5minute\": " << decimal(argo.drift_time_target,3,0) << "," << std::endl;
    fout << "    \"target_surface_second\": " << decimal(argo.surf_time_max,3,0) << "," << std::endl;
    fout << "    \"seek_periods\": " << argo.num_seeks << "," << std::endl;
    fout << "    \"seek_minute\": " << decimal(argo.seek_time_max,3,0) << "," << std::endl;
    fout << "    \"ctd_pres\": { \"gain\": " << decimal(argo.pres_gain,4,0) << ", \"offset\": " << decimal(argo.pres_offset,3,0) << "}," << std::endl;
    fout << "    \"ctd_temp\": { \"gain\": " << decimal(argo.temp_gain,4,0) << ", \"offset\": " << decimal(argo.temp_offset,3,0) << "}," << std::endl;
    fout << "    \"ctd_psal\": { \"gain\": " << decimal(argo.psal_gain,4,0) << ", \"offset\": " << decimal(argo.psal_offset,3,0) << "}" << std::endl;
    //fout << "    \"cycle_time_max\": " << decimal(argo.cycle_time_max,6,3) << std::endl; // remove from json (derived, not telemetered)
    fout << "  }";
  }

  // ==== BIT ===================
  std::map<int,std::string> BIT_status = {
    {0,"RAM BAD"},{1,"ROM BAD"},{2,"EEPROM test failed"},{3,"Air Vent failed"},{4,"CPU Voltage low"},{5,"Pump Voltage low"},{6,"Low Vacuum"},
    {7,"Slow Oil Pump"},{8,"Pump current too high or low"},{9,"SBE comm fail"},{10,"Vale open or close failure"},{11,"Air vacuum high (>1100)"},
    {12,"Comms failed to iridium modem"},{13,"Comms failed to gps device"},{14,"HP pump didn't run long enough"},{15,"pH Bias Battery too low"} };

  // check BIT.status for errors, and add to BIT_errors map
  std::map<int,std::string> BIT_errors;

  if (bit.received) {
    int prec = int(config["prof"]["CTD_Binned"]["PRES"]["col_precision"]);

    for (int p = 0; p < 16; p++) {
      flag = std::pow(2,p);
      if (bit.status & flag) {
        BIT_errors[flag] = BIT_status[p];
	  }
    }

    fout << "," << std::endl;
    fout << "  \"BIT\": {" << std::endl;
    if (bit.status == 0)
      fout << "    \"status\": \"OK\"," << std::endl;
    else {
      fout << "    \"status\": \"FAIL\"," << std::endl;
      fout << "    \"errors\": [" << std::hex << std::setfill('0') << std::endl;
      first1 = true;
      for (auto & [key,error] : BIT_errors) {
        if (!first1)
          fout << "," << std::endl;
        first1 = false;
        fout << "      { \"0x" << std::setw(4) << key << "\": \"" << error << "\" }";
      }
      fout << std::endl <<  "    ]," << std::endl;
      fout << std::dec << std::setfill(' '); // set standard output back to decimal
    }
    fout << "    \"Eng_ver\": " << bit.EngVer << "," << std::endl;
    fout << "    \"blocks_queued\": " << bit.nQueued << "," << std::endl;
    fout << "    \"pressure\": " << decimal(bit.pressure,5,prec) << "," << std::endl;
    fout << "    \"cpu_voltage\": " << decimal(bit.cpu_voltage,5,2) << "," << std::endl;
    fout << "    \"pump_voltage_prior\": " << decimal(bit.pump_voltage_prior,5,2) << "," << std::endl;
    fout << "    \"pump_voltage_after\": " << decimal(bit.pump_voltage_after,5,2) << "," << std::endl;
    fout << "    \"pump_current_mA\": " << bit.pump_current << "," << std::endl;
    fout << "    \"pump_time_s\": " << bit.pump_time << "," << std::endl;
    fout << "    \"pump_oil_prior\": " << bit.pump_oil_prior << "," << std::endl;
    fout << "    \"pump_oil_after\": " << bit.pump_oil_after << "," << std::endl;
    fout << "    \"vacuum_prior_inHg\": " << decimal(bit.vacuum_prior,5,2) << "," << std::endl;
    fout << "    \"vacuum_after_inHg\": " << decimal(bit.vacuum_after,5,2) << "," << std::endl;
    fout << "    \"valve_open\": " << bit.valve_open << "," << std::endl;
    fout << "    \"valve_close\": " << bit.valve_close << "," << std::endl;
    fout << "    \"interrupt_id\": " << bit.interrupt_id << "," << std::endl;
    if (bit.EngVer >= 6) {
      fout << "    \"SBE_response\": \"" << bit.SBE_response << "\"," << std::endl;
      fout << "    \"cpu_temp_degC\": " << decimal(bit.cpu_temp,6,3) << "," << std::endl;
      fout << "    \"RH\": " << bit.RH << std::endl;
    } else {
      fout << "    \"SBE_response\": \"" << bit.SBE_response << "\"" << std::endl;
    }
    fout << "  }";	
  }

  // ==== BEACON ================
  if (beacon.received) {

    fout << "," << std::endl;
    fout << "  \"BIT\": {" << std::endl;
    fout << "    \"status\": \"Beacon\"," << std::endl;
    fout << "    \"Eng_ver\": " << beacon.EngVer << "," << std::endl;
    fout << "    \"nQueued\": " << beacon.nQueued << "," << std::endl;
    fout << "    \"nTries\": " << beacon.nTries << "," << std::endl;
    fout << "    \"parXstat\": " << beacon.parXstat << "," << std::endl;
    fout << "    \"SBDIstat\": " << beacon.SBDIstat << "," << std::endl;
    fout << "    \"cpu_voltage\": " << decimal(beacon.cpu_voltage,5,2) << "," << std::endl;
    fout << "    \"pump_voltage\": " << decimal(beacon.pump_voltage,5,2) << "," << std::endl;
    fout << "    \"vacuum_transmit_inHg\": " << decimal(beacon.vacuum_now,5,2) << "," << std::endl;
    fout << "    \"vacuum_abort_inHg\": " << decimal(beacon.vacuum_abort,5,2) << "," << std::endl;
    fout << "    \"last_interrupt\": " << beacon.ISRID << "," << std::endl;
    if (beacon.EngVer > 5.5) {
      fout << "    \"abortFlag\": " << beacon.abortFlag << "," << std::endl;
      fout << "    \"CPUtemp\": " << decimal(beacon.CPUtemp,6,3) << "," << std::endl;
      fout << "    \"RH\": " << beacon.RH << std::endl;
    } else {
      fout << "    \"abortFlag\": " << beacon.abortFlag << std::endl;
    }
    fout << "  }";
  }


  // ==== FALL TIME-SERIES ======
  //std::map<int,std::string> dive_phase_v0 = {
  //  {0,"Dive start"},{1,"Start of sink"},{2,"Pump 2 target"},{3,"Seek"},{4,"Drift begin"},{5,"Drift seek"},{6,"Fall to profile begin"},
  //  {7,"pre-ascend"},{8,"Profile start"},{9,"Profile end"},{10,"Ice turnaround"},{11,"Sinking"},{12,"Drifting"},{13,"Descending"},
  //  {14,"Ascending"},{15,"Reached surface"} };

  std::map<int,std::string> dive_phase_v1 = { // firmware v10.2+; "pre-ascend" moved from 7 to 15 for uniformity with other SOLO floats
    {0,"Dive start"},{1,"Start of sink"},{2,"Pump 2 target"},{3,"Seek"},{4,"Drift begin"},{5,"Drift seek"},{6,"Fall to profile begin"},
    {7,"Profile start"},{8,"Profile end"},{9,"Ice turnaround"},{10,"Sinking"},{11,"Drifting"},{12,"Descending"},{13,"Ascending"},
    {14,"Reached surface"},{15,"pre-ascend"} };


  if (fall.Scan.size()) {
    int prec = int(config["prof"]["CTD_Binned"]["PRES"]["col_precision"]);
    fout << "," << std::endl;
    fout << "  \"Fall\": [" << std::endl;
    first1 = true;
    for ( auto s : fall.Scan ) {
      if (!first1)
        fout << "," << std::endl;
      first1 = false;
      fout << "    { ";
      fout << "\"TIME\": \"" << date_format(s.time,config["DATE_FORMAT"]) << "\", ";
      fout << "\"PRES\": " << decimal(s.pres,7,prec) << ", ";
      fout << "\"phase\": " << decimal(s.phase,2,0) << ", ";
      //switch(fall.version) {
      //  case 1:  fout << "\"description\": \"" << dive_phase_v1[s.phase] << "\" }"; break;// firmware v10.2+
      //  default: fout << "\"description\": \"" << dive_phase_v0[s.phase] << "\" }"; break;
      //}
      fout << "\"description\": \"" << dive_phase_v1[s.phase] << "\" }";
    }
    fout << std::endl;
    fout << "  ]";
  }

  // ==== RISE TIME-SERIES ======
  if (rise.Scan.size()) {
    int prec = int(config["prof"]["CTD_Binned"]["PRES"]["col_precision"]);
    fout << "," << std::endl;
    fout << "  \"Rise\": [" << std::endl;
    first1 = true;
    for ( auto s : rise.Scan ) {
      if (!first1)
        fout << "," << std::endl;
      first1 = false;
      fout << "    { ";
      fout << "\"TIME\": \"" << date_format(s.time,config["DATE_FORMAT"]) << "\", ";
      fout << "\"PRES\": " << decimal(s.pres,7,prec) << ", ";
      fout << "\"phase\": " << decimal(s.phase,2,0) << ", ";
      //switch(rise.version) {
      //  case 1:  fout << "\"description\": \"" << dive_phase_v1[s.phase] << "\" }"; break; // firmware v10.2+
      //  default: fout << "\"description\": \"" << dive_phase_v0[s.phase] << "\" }"; break;
      //}
      fout << "\"description\": \"" << dive_phase_v1[s.phase] << "\" }";
    }
    fout << std::endl;
    fout << "  ]";
  }

  // ====== ENGINEERING PARAMETERS ==========
  if (eng_data.list.size()) {
    fout << "," << std::endl;
    fout << "  \"Engineering_Data\": {" << std::endl;
    first1 = true;
    for( auto p : eng_data.list ) {
      if (!first1)
        fout << "," << std::endl;
      first1 = false;
      fout << "    " << std::setw(12) << p.name << ": { ";
      fout << "\"value\": " << std::setw(8) << p.val << ", ";
      fout << "\"unit\": " << std::setw(6) << p.unit << ", ";
      fout << "\"description\": \"" << p.desc << "\" }";
    }
    fout << std::endl;
    fout << "  }";
  }

  // ====== MISSION PARAMETERS ==========
  string pname;
  if (miss.list.size()) {
    fout << "," << std::endl;
    fout << "  \"Mission\": {" << std::endl;
    first1 = true;
    std::stringstream funit;
    for( auto p : miss.list ) {
      if (!first1)
        fout << "," << std::endl;
      first1 = false;
      pname = "\"" + p.name + "\"";
      fout << "    " << std::setw(12) << std::setfill(' ') << pname << ": { ";
      fout << "\"config_value\": " << std::setw(5) << p.tval << ", ";
      fout << "\"value\": " << std::setw(6) << p.val << ", ";
      funit.str("");
      funit << "\"" << p.unit << "\"";
      fout << "\"unit\": " << std::setw(6) << funit.str() << ", ";
      //fout << "\"unit\": \"" << std::setw(4) << p.unit << "\", ";
      fout << "\"description\": \"" << p.desc << "\" }";
    }
    fout << std::endl;
    fout << "  }";
  }

  // ======== WRITE PUMP ========
  if (pump.Scan.size()) {
    int prec = int(config["prof"]["CTD_Binned"]["PRES"]["col_precision"]);
    fout << "," << std::endl;
    fout << "  \"Pump\": [" << std::endl;
    first1 = true;
    for( auto s : pump.Scan ) {
      if (!first1)
        fout << "," << std::endl;
      first1 = false;
      fout << "    { ";
      //if (pump.version == 1) // firmware v10.2+
      //  fout << "\"TIME\": \"" << date_format(s.time,config["DATE_FORMAT"]) << "\", ";
      fout << "\"PRES\": " << decimal(s.pres,7,prec) << ", ";
      fout << "\"current\": " << decimal(s.curr,4,0) << ", ";
      fout << "\"voltage\": " << decimal(s.volt,5,2) << ", ";
      fout << "\"pump_time\": " << decimal(s.pump_time,4,0) << ", ";
      fout << "\"vac_start\": " << decimal(s.vac_strt,3,0) << ", ";
      fout << "\"vac_end\": " << decimal(s.vac_end,3,0) << ", ";
      fout << "\"phase\": " << decimal(s.phase,2,0) << ", ";
      //switch(pump.version) {
      //  case 1:  fout << "\"description\": \"" << dive_phase_v1[s.phase] << "\" }"; break; // firmware v10.2+
      //  default: fout << "\"description\": \"" << dive_phase_v0[s.phase] << "\" }"; break;
      // }
      fout << "\"description\": \"" << dive_phase_v1[s.phase] << "\" }";
    }
    fout << std::endl;
    fout << "  ]";
  }
  //std::cout << " writing profiles in write_json " << std::endl;

  // ======== WRITE PROFILES ========

  first3 = true;
  for (auto &[pname,vdict] : config["prof"].items()) {
    if (!prof[pname].size)
      continue; // skip json output for empty profiles
    fout << "," << std::endl;
    first3 = false;
    fout << "  \"" << pname << "\": [";
	int cpres;    // column precision
	int cwidth;   // column width defined in config.json
    // columns
    first1 = true;
    int loopStart=prof[pname].size - 1; //default reverse order
    int loopEnd= -1;
    int loopDir= -1;
    if ( pname == "CTD_Drift" ) { // But do not flip Drift
      loopStart=0;
      loopEnd= prof[pname].size;
      loopDir= 1;
    }
    //for (int i = 0; i < prof[pname].size; i++) {
    for (int i = loopStart; i != loopEnd; i=i+loopDir ) { 
      if (!first1)
        fout << ",";
      fout << std::endl;
      first1 = false;
      fout << "    { ";
      first2 = true;
      for (auto & [var,vatts] : vdict.items()) {
        if ( !prof[pname][var].size() ) // skip empty columns
          continue;
        cwidth = vatts["col_width"];
        cpres  = vatts["col_precision"];

        if (!first2)
          fout << ", ";
        first2 = false;

	fout << "\"" << var << "\": " << decimal(prof[pname][var][i],cwidth,cpres);
      }
      fout << " }";
    }
    fout << std::endl;
    fout << "  ]";
  }

  fout << std::endl << "}" << std::endl;
  //std::cout << " exiting in write_json " << std::endl;
  fout.close();
}
