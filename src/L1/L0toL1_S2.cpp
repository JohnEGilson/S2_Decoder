#include <algorithm>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <string>
#include <filesystem>
#include <boost/date_time.hpp>
using namespace boost::posix_time;
#include "../json/json.hpp"  // Include the Nlohmann JSON library

//global variables
using json = nlohmann::ordered_json;
json config_template;
json mission_template;
json diagnostic_template;
json engineering_template;
json SBDjson;
json RunMission;
json GPS;
std::vector<nlohmann::json> GPSobj;
int float_type = 0; //default to S2 for starters (cycle -1,0 do NOT have ARGO_Mission)
float float_version = 2.6; //default to latest S2  for starters (cycle -1,0 do NOT have ARGO_Mission)
std::string L1_path;
std::string L0_path;
std::string SBD_path;
int rest=3;  //default temperature resolution
int resp=2;  //default pressure resolution
int ress=3;  //default salinity resolution

extern json config;
extern std::string S2_PATH;

std::string decimal( float val, int cwidth, int cpres ) {
    std::stringstream ss;
        // Test for NaN; JSON does not support NaN. Send "null" instead
        if (std::isnan(val))
                ss << "null";
        else if (val == -999)
                ss << std::setw(cwidth) << -999; // if -999, no need to pad out extra decimals; -999.0000
        else
        ss << std::fixed << std::setprecision(cpres) << std::setw(cwidth) << val;
    return ss.str();
}

boost::posix_time::ptime utcnow() {
  return boost::posix_time::second_clock::universal_time();
}

std::string date_format( const ptime &t, std::string format ) {
  int yr,mo,dy,hr,mn,sc;
  const std::string alphabet = "ymdHMS"; // define characters to be considered symbols
  std::string tmp = ""; // temporary variables used to parse format string

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

// Cross-platform wrapper to convert a GMT std::tm structure back to Unix seconds cleanly (Gemini)
std::time_t portable_timegm(std::tm* gmt_time) {
#if defined(_WIN32) || defined(_WIN64)
    return _mkgmtime(gmt_time); // Native Windows standard library function
#else
    return timegm(gmt_time);    // Native Linux / macOS POSIX standard library function
#endif
}

// Function : Converts any GMT/UTC std::tm structure into seconds since Jan 1, 1950 (Gemini)
long long get_seconds_since_1950(const std::tm& gmt_time) {
    // Create a modifiable copy since portable_timegm requires a non-const pointer
    std::tm mutable_time = gmt_time;

    // Get the exact Unix time (seconds since Jan 1, 1970) using standard OS layers
    std::time_t unix_seconds = portable_timegm(&mutable_time);

    // Fixed constant offset: Jan 1, 1950 to Jan 1, 1970 is exactly 631,152,000 seconds
    const long long seconds_between_1950_and_1970 = 631152000LL;

    return static_cast<long long>(unix_seconds) + seconds_between_1950_and_1970;
}
std::tm string_to_tm(const std::string& time_str) {
    std::tm tm_struct = {};
    std::istringstream ss(time_str);

    // %Y = Year (4 digits)
    // %m = Month (01-12)
    // %d = Day of the month (01-31)
    // %H = Hour (00-23)
    // %M = Minute (00-59)
    // %S = Second (00-60)
    // 'T' and 'Z' are literal characters parsed out of the stream
    ss >> std::get_time(&tm_struct, "%Y-%m-%dT%H:%M:%S");

    if (ss.fail()) {
        std::cerr << "Error: Failed to parse time string format.\n";
    }

    // Set standard defaults for clean parsing downstream
    tm_struct.tm_isdst = 0; // The 'Z' indicates UTC/GMT, so daylight savings is 0

    return tm_struct;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Function Declaration 
void write_BIST (std::ofstream& fout, std::string senname, std::string varname, const json &Doc_L0c, int wid, int res, int last);

// Function Declaration 
void write_CTD (std::ofstream& fout, std::string varname, const json &Doc_L0c, int chan);

// Function Declaration 
void write_Config (std::ofstream& fout, std::vector<int> Snd, std::string varname);

// Function Declaration 
void copyLine ( std::ofstream& fout, std::string item, std::string endc, const json &Doc_L0c);

// Function Declaration 
int determine_resolution (int digits);

// Function Declaration 
void rewrite_json (std::string filename, const json &Doc_L0);

// Function Declaration
void IdentEchoConfig(int SoloType, std::string command);

// Function Declaration
void ModOneConfig(std::string ConfigName, int ConfigValue, std::string unit, std::string desc);

/////////////////////////////////////////////////////////////////////////////////////////////


int main( int argc, char* argv[] ) {

  const char *s2_PATH = getenv("S2_PATH");
  if (s2_PATH == NULL) {
        // If S2_PATH env variable is not defined, exit. Write to stdout becuase log() requires S2_PATH
    std::cout << "Unable to load S2_PATH environment variable; exiting." << std::endl;
    return 0;
  }

  std::cout << "Program Name: " << argv[0] << std::endl;

  int tarcynum;
  if (argc > 1) {
    std::cout << "Number of arguments: " << argc - 1 << std::endl;
    std::cout << "Arguments:" << std::endl;
    for (int i = 1; i < argc; ++i) {
      std::cout << "  Argument " << i << ": " << argv[i] << std::endl;
    }
    if ( argc - 1 == 1 ) {
      std::cout << "  No target cycle number specified -> setting to -1  " << std::endl;
      tarcynum = -1;
    } else {
      tarcynum = std::stoi(argv[2]); 
    }
  } else {
    std::cout << "argv[1] (mandatory): float SN; argv[2] (optional): CONFIG target cycle number to process" << std::endl;
    return 0;
  }

  std::string s2_path = s2_PATH;

  int value = std::stoi(argv[1]);
  std::string missname;
  std::string Engname;
  std::string Dianame;
  std::string configname;
  std::string floatconfig;
  std::ostringstream padossn;
  int padwidth=4;
  if ( value >= 10000 ) { padwidth=5; }
  padossn << std::setw(padwidth) << std::setfill('0') << value;
  std::string padsn=padossn.str();
  missname = s2_path + "/data/" + padsn + "/" + padsn + "_mission.json"; //path of specific mission json
  configname = s2_path + "/data/" + padsn + "/modified_" + padsn + "_config.json"; //path of specific config json
  floatconfig = s2_path + "/data/" + padsn + "/" + padsn + "_config.json"; //path of specific config json which lists ECO/OCR channel names ::named the same as S2
  L0_path = s2_path + "/data/" + padsn + "/json/L0"; //path of original L0 json
  L1_path = s2_path + "/data/" + padsn + "/json/L1"; //path of new L1 json
  SBD_path = s2_path + "/data/" + padsn + "/json/SBD"; //path of possible SBD json
  // LOAD mission.json template for units/description
  if (!std::filesystem::exists(missname)) {
     missname = s2_path + "/config/mission.json"; //path of default mission json
  }
  //std::cout << missname << std::endl;
  std::ifstream tempmiss(missname, std::ifstream::binary);
  tempmiss >> mission_template; //json of mission template (global)
  // LOAD config.json template for units/description
  if (!std::filesystem::exists(configname)) {  //check to see if float specific "modified_" config exists
    configname = s2_path + "/config/config.json"; //path of default original L0 json
  }
  //std::cout << configname << std::endl;
  std::ifstream tempconfig(configname, std::ifstream::binary);
  tempconfig >> config_template; //json of config template (global)
  

  // Read float specific config file and write firmware version to json file
  if (std::filesystem::exists(floatconfig)) {  //check to see if float specific "floatconfig" channel specification
    std::ifstream f2(floatconfig);
    auto float_config = nlohmann::ordered_json::parse(f2); // use ordered_json to preserve order
    // assign sensor specific column names to ECO template profiles
    for (auto p : {"ECO_Discrete","ECO_Drift"}) {
      for (auto ch : {"ch1","ch2","ch3"}) {
        std::string colname = float_config["ECO"][ch];
        config_template["prof"][p][colname] = config_template["prof"][p][ch];
        config_template["prof"][p].erase(ch);
      }
    }
    // assign sensor specific column names to OCR_Discrete template  profile
    for ( auto ch : {"ch1","ch2","ch3","ch4"}) {
      std::string colname = float_config["OCR"][ch];
      config_template["prof"]["OCR_Discrete"][colname] = config_template["prof"]["OCR_Discrete"][ch];
      config_template["prof"]["OCR_Discrete"].erase(ch);
    }
  } 

  std::cout << "L0_path " << L0_path << std::endl;
  std::cout << "L1_path " << L1_path << std::endl;
  std::cout << "SBD_path " << SBD_path << std::endl;

  // Sort L0 JSON files: Done early to allow L0 file to be Mission Target file
  std::vector<std::filesystem::path> files_in_directory;
  try {
         for (const auto& entry : std::filesystem::directory_iterator(L0_path)) {
            if (entry.is_regular_file()) {
                if (entry.path().extension() == ".json") {
                        files_in_directory.push_back(entry.path());
                }
            }
         }
  } catch (const std::filesystem::filesystem_error& ex) {
          std::cerr << "Filesystem error: " << ex.what() << std::endl;
  }
 
 //exit if there are no files in L0 directory (Can this happen?)
  if ( files_in_directory.size() == 0 ) {
	  std::cout << "No json files in L0 directory " << std::endl;
	  return 0;
  }

  std::sort(files_in_directory.begin(), files_in_directory.end()); //sort the L0 file names

  std::string first_L0_file;  // Identify the first file in L0 directory for targetfile if no L1 exist
  for (const std::string& filename : files_in_directory) {  
	if (first_L0_file.empty()) {
  	  first_L0_file = filename;
	  break;
	}
  }

  std::string tarfilename; //Sort L1 JSON files, choose Target...file to start with
  int start_iter=9999;
  try {  //create directory for new json if necessary + read in L1 directory for reference
    if (std::filesystem::create_directory(L1_path)) {
      std::cout << "  Directory '" << L1_path << "' created successfully." << std::endl;
      tarcynum = -1; //there was no directory so target defaults to -1 from L0
      tarfilename = first_L0_file;
      start_iter=-1;
    } else { 
      std::cout << "  Directory '" << L1_path << "' already exists." << std::endl;
  
      // Sort L1 JSON files within directory: Pull out starting "Mission" 
      std::vector<std::filesystem::path> files_in_L1_directory;
      try {
    	  for (const auto& entry : std::filesystem::directory_iterator(L1_path)) {
      	      if (entry.is_regular_file()) {
       		   if (entry.path().extension() == ".json") {
               		 files_in_L1_directory.push_back(entry.path());
               	   }
       	      }
       	  }
      } catch (const std::filesystem::filesystem_error& ex) {
       	  std::cerr << "Filesystem error: " << ex.what() << std::endl;
      }

      if ( files_in_L1_directory.size() == 0 | tarcynum < 0 ) {
        tarcynum = -1; //there were no files in the L1 directory: target defaults to -1 from L0
        tarfilename = first_L0_file;
        start_iter=-1;

      } else { // L1 files already exists
        std::sort(files_in_L1_directory.begin(), files_in_L1_directory.end());
// find the latest L1 cycle to load prior or equal to tarcynum
        std::string delimiters("_."); //two delimiters in typical json filename
        std::vector<std::string> token;
        auto it = files_in_L1_directory.end()-1; //search in reverse order 
        while (it >= files_in_L1_directory.begin() & start_iter > tarcynum) {
  	  boost::split(token, it->filename().string(), boost::is_any_of(delimiters));
//	  std::cout << token[token.size()-2] <, std::endl;  
	  start_iter = stoi(token[token.size()-2]);  //assume that cycle number is 2nd to last token
	  --it;
        }
        it++; //increase iterator by 1
        tarfilename = L1_path + "/" + it->filename().string();
      }
    }
  

    std::cout << "  Reference Mission is from  " << tarfilename << std::endl;
    std::ifstream json_file(tarfilename, std::ifstream::binary);
    json Doc_L1;
    json_file >> Doc_L1;
    if ( Doc_L1.contains(std::string{ "Mission" }) ) {
      RunMission = Doc_L1.at("Mission"); //this is the starting point for Mission
    } else {
      std::cout << "There is no MISSION in target file: exit " << tarfilename << std::endl;
      //  return 0;
    }
  } catch (const std::filesystem::filesystem_error& ex) {
      std::cerr << "Filesystem error: " << ex.what() << std::endl;
  }

  std::cout << "  Writing of L1 will start from cycle " << start_iter << std::endl;


//Do first loop to pull in all GPS records for rearranging
// Store in vector array GPS, of json GPSobj with cycle addon
  int initial_EngVer;
  for (const std::string& filename : files_in_directory) {

    //std::cout << filename << std::endl;
    std::string delimiters("_."); //two delimiters in typical json filename
    std::vector<std::string> token;
    boost::split(token, filename, boost::is_any_of(delimiters));
    int check_iter = stoi(token[token.size()-2]);  //assume that cycle number is 2nd to last token
   
    json Doc_L0; //create uninitialized json object
    std::ifstream json_file(filename, std::ifstream::binary);
    json_file >> Doc_L0; // initialize json object with input from oringal file

    if ( Doc_L0.contains(std::string{ "GPS" }) ) {
      GPS["cycle"]=check_iter;
      GPS["data"]=Doc_L0.at("GPS");
      GPSobj.push_back(GPS);
    }
//    for (const auto& obj : GPSobj) {
//        std::cout << obj.dump(4) << std::endl; // Pretty print the JSON object
//    }
    if ( Doc_L0.contains(std::string{ "Engineering_Data" }) ) {
      initial_EngVer=Doc_L0.at("Engineering_Data")["Eng_ver"]["value"];
    }
  }
  std::cout << "  Collected all GPS " << start_iter << std::endl;

// Have now collectioned EngVer...open Engineering_template
  Dianame = s2_path + "/config/diagnostic_" + std::to_string(initial_EngVer) + ".json";
  Engname = s2_path + "/config/Engineering_Data_" + std::to_string(initial_EngVer) + ".json";
  std::ifstream Diamiss(Dianame, std::ifstream::binary);
  Diamiss >> diagnostic_template; //json of diagnosticEng template (global)
  std::ifstream Engmiss(Engname, std::ifstream::binary);
  Engmiss >> engineering_template; //json of Eng template (global)
  
  for (const std::string& filename : files_in_directory) {
//   	std::cout << "\n" << std::endl; 
      	std::cout << filename << std::endl; // printed in alphabetical order
 

        std::string delimiters("_."); //two delimiters in typical json filename
        std::vector<std::string> token;
        boost::split(token, filename, boost::is_any_of(delimiters));
	int check_iter = stoi(token[token.size()-2]);  //assume that cycle number is 2nd to last token

// process if cycnum of filename is > tarcynum
	if ( check_iter >= start_iter ) {
    	  //std::cout << std::endl; 
    	  //std::cout << "Reading " << filename << std::endl; // printed in alphabetical order

  	  json Doc_L0; //create uninitialized json object
	  std::ifstream json_file(filename, std::ifstream::binary);
	  json_file >> Doc_L0; // initialize json object with input from oringal file

  	  if ( Doc_L0.contains(std::string{ "ARGO_Mission" }) ) {
	    std::string unit="";
	    std::string desc="";
            float_type = Doc_L0.at("ARGO_Mission")["float_model"]; // save the float type for use of this code with non-BGC
            float_version = Doc_L0.at("ARGO_Mission")["float_telemetry_format"]; // save the float version for use of this code with non-BGC
            resp=determine_resolution(Doc_L0.at("ARGO_Mission")["ctd_pres"]["gain"]);
            rest=determine_resolution(Doc_L0.at("ARGO_Mission")["ctd_temp"]["gain"]);
            ress=determine_resolution(Doc_L0.at("ARGO_Mission")["ctd_psal"]["gain"]);
            ModOneConfig("MinRis",Doc_L0.at("ARGO_Mission")["min_ascent_rate_cmpersec"],unit,desc);
            if ( Doc_L0[ "Engineering_Data" ].contains("SubCycle") ) {  //in order to apply must get subcycle from engineering
       	      int subcyc = Doc_L0.at("Engineering_Data")["SubCycle"]["value"];
      	      ModOneConfig("Zpro" + std::to_string(subcyc),Doc_L0.at("ARGO_Mission")["profile_target_dbar"],unit,desc);
              ModOneConfig("Ztar" + std::to_string(subcyc),Doc_L0.at("ARGO_Mission")["drift_target_dbar"],unit,desc);
              ModOneConfig("Rise" + std::to_string(subcyc),Doc_L0.at("ARGO_Mission")["max_rise_minute"],unit,desc);
              ModOneConfig("Fall" + std::to_string(subcyc),Doc_L0.at("ARGO_Mission")["max_fall_to_park_minute"],unit,desc);
              ModOneConfig("Pwait" + std::to_string(subcyc),Doc_L0.at("ARGO_Mission")["max_fall_to_profile_minute"],unit,desc);
	    }
            ModOneConfig("Nseek",Doc_L0.at("ARGO_Mission")["seek_periods"],unit,desc);
            ModOneConfig("STLmin",Doc_L0.at("ARGO_Mission")["seek_minute"],unit,desc);
            ModOneConfig("Sgain",Doc_L0.at("ARGO_Mission")["ctd_psal"]["gain"],unit,desc);
            ModOneConfig("Soff",Doc_L0.at("ARGO_Mission")["ctd_psal"]["offset"],unit,desc);
            ModOneConfig("Tgain",Doc_L0.at("ARGO_Mission")["ctd_temp"]["gain"],unit,desc);
            ModOneConfig("Toff",Doc_L0.at("ARGO_Mission")["ctd_temp"]["offset"],unit,desc);
            ModOneConfig("Pgain",Doc_L0.at("ARGO_Mission")["ctd_pres"]["gain"],unit,desc);
            ModOneConfig("Poff",Doc_L0.at("ARGO_Mission")["ctd_pres"]["offset"],unit,desc);
	  }

	  // write file after ARGO_Mission but before others
          rewrite_json(filename, Doc_L0);   //write json file here prior to looking at CONFIG dumps or 2-way

	  if ( Doc_L0.contains(std::string{ "Mission" }) ) { //Ordering "Mission" prior to "Upload Command" as mission could be requested before all 2-way applied (example 0001, cycle 332)
                json NewMission = Doc_L0.at("Mission"); //this is a new Mission to compare against
// loop through Mission Configs individually as Iridium transmission might be partial;
		for (const auto& loop : NewMission.items())  {
// 	           std::cout << loop.key() << " = " << loop.value()["config_value"] << "\n";
                   ModOneConfig(loop.key(),loop.value()["config_value"],loop.value()["unit"],loop.value()["description"]);
		}
	  }

	  if ( Doc_L0.contains(std::string{ "Upload_Command" }) ) { //As Upload_Command is last, I'm trusting my parsing of the message : Dangerous
	        std::string UpC = Doc_L0.at( "Upload_Command" );
		//std::cout << "Upload_Command = " << UpC  << std::endl;

		std::stringstream ss(UpC);
                std::string segment;	
	        std::vector<std::string> commandVector;  // Declare a vector to hold substrings (commands)
                while (std::getline(ss,segment)) {
                  std::size_t prev=0, pos;
		  while ((pos = segment.find_first_of(";:",prev)) != std::string::npos) { //identify segments separated by ";" AND ":"
	  	    if (pos > prev && segment.substr(pos,1) != ":")  //Only save commands that end in ";"
			  commandVector.push_back(segment.substr(prev,pos-prev));
		    prev = pos+1;
		  }
		}
                for (const std::string& command : commandVector) {
			IdentEchoConfig(std::min(float_type,2), command); // Call routine to identify CONFIG and values in command
		}

	  }


 	} // end of loop over files > tarcynum
  }
  return 0;
}  // end of Main
	
/////////////////////////////////////////////////////////////////////////////////////////////
void IdentEchoConfig(int SoloType, std::string command) {


// return if command is length 1 : No CONFIG mods
	if (command.length() < 2) return;

// outputs the command sent to this subroutine
//	std::cout << "In IdentEchoConfig \"" << command << "\"" << std::endl;

        char Cp = command.at(0);  // The first character is the primary command
	std::string Cs = std::string(1, command.at(1)); // Secondary command (save as string to be able to 'add' to Config vectors

	std::vector<std::string> config_strings{};
	std::vector<std::string> sci_strings{};
        std::string Cb = "  ";
	switch (Cp) { 
          case '4': {
		if ( Cs == "S" ) {    // BGC Science Board CONFIGS
		    Cb = command.substr(3,2); // BGC Board command
		    if ( Cb == "2E" ) {
	              std::stringstream ss(command.substr(5,command.length()));
		      std::string token;
		      ss >> token;
		      config_strings.push_back(token);
                      command.erase(1,token.length()+5);  // remove CONFIG name & "S 2E" from command, parse values below as a standard SOLOII CONFIG eg "4 Zpro2 XX"
		    } else if ( Cb == "S1" ) {
		      std::cout << "Science Board change single sensor parameter for 1 sensor =" << Cb << std::endl;
                      command.erase(0,4);  // remove CONFIG name & "4S S" from command, parse values below
		    } else if ( Cb == "S2" ) {
		      std::cout << "Science Board change regional parameter for 5 regions for 1 sensor =" << Cb << std::endl;
                      command.erase(0,4);  // remove CONFIG name & "4S S" from command, parse values below
		    } else if ( Cb == "S3" ) {
		      std::cout << "Science Board change regional parameter for 1 region for 1 sensor =" << Cb << std::endl;
                      command.erase(0,4);  // remove CONFIG name & "4S S" from command, parse values below
		    } else if ( Cb == "S4" ) {
		      std::cout << "Science Board change sensor variable parameter for 1 sensor =" << Cb << std::endl;
                      command.erase(0,4);  // remove CONFIG name & "4S S" from command, parse values below
		    } else {
		      std::cout << "Science Board PSM Odd: Ignore Command? = " << Cb << std::endl;
		      return;
		    }
		} else {   // Float CONFIGS
	            std::stringstream ss(command.substr(2,command.length()));
		    std::string token;
		    ss >> token;
		    config_strings.push_back(token);
                    command.erase(2,token.length());  // remove CONFIG name from command, parse values below
		}
		break;
		    }
          case 'B': {
                config_strings.push_back("BinMod"); 
		break;
		    }
          case 'C': {
                config_strings.push_back("PROup"); 
                config_strings.push_back("BLOK"); 
                config_strings.push_back("PB1"); 
                config_strings.push_back("PB2"); 
                config_strings.push_back("AV1"); 
                config_strings.push_back("AV2"); 
                config_strings.push_back("CTDofZ"); 
		break;
		    }
          case 'D': {
                config_strings.push_back("SAMmn"+Cs); //must append sub-cycle
                config_strings.push_back("Nsam"+Cs); 
		break;
		    }
          case 'F': {
                config_strings.push_back("SkSLsc"); 
                config_strings.push_back("AsSLsc"); 
		break;
		    }
          case 'G': { 
                config_strings.push_back("Password");  //the G command must have initial password
                config_strings.push_back(Cs+"gain"); //must prepend P/S/T
                config_strings.push_back(Cs+"off"); 
		break;
		    }
          case 'I': {
                config_strings.push_back("ABcymn"); 
		break;
		    }
          case 'J': {
                config_strings.push_back("DrfDat"); 
		break;
		    }
          case 'N': {
                config_strings.push_back("Ndives");
		break;
		    }
          case 'S': {
                config_strings.push_back("GPSsec"); 
                config_strings.push_back("IRIsec"); 
                config_strings.push_back("MxSfP"); 
		break;
		    }
          case 'T': {
                config_strings.push_back("Fall"+Cs); //must append sub-cycle
                config_strings.push_back("Rise"+Cs);
                config_strings.push_back("Pwait"+Cs);
		break;
		    }
          case 'U': {
                config_strings.push_back("UNBINd"); 
                config_strings.push_back("UBZmax"); 
                config_strings.push_back("UBn"); 
		break;
		    }
          case 'V': {
                config_strings.push_back("Ventop"); 
                config_strings.push_back("Ventsc"); 
		break;
		    }
          case 'W': {
                config_strings.push_back("WD_EN");
                config_strings.push_back("WDhour");
		break;
		    }
          case 'X': {
                config_strings.push_back("XP0"); 
                config_strings.push_back("XP1"); 
                config_strings.push_back("XP2"); 
                config_strings.push_back("XPdly"); 
                config_strings.push_back("XP0dZ"); 
		break;
		    }
          default: 

// for commands that differ by float type 
//            std::cout << SoloType << " SoloType " << std::endl;
	    switch (SoloType) { // Pass the float type code (SOLO=0, Deep=1, BGC=2)
	      case 0: {  //SOLOII
//    	        std::cout << "examining SOLO2 specific commands " << std::endl;
	        switch (Cp) { // The first character is the primary command
                  case 'E': {
                    config_strings.push_back("Ice_Tc"); 
                    config_strings.push_back("Ice_Ps"); 
                    config_strings.push_back("Ice_Pd"); 
                    config_strings.push_back("Ice_Mn"); 
                    config_strings.push_back("Ice_Sc"); 
		    break;
			    }
                  case 'H': {
                    config_strings.push_back("MxHiP");
                    config_strings.push_back("PmpBtm");
                    config_strings.push_back("Pmpslo");
                    config_strings.push_back("MinRis");
                    config_strings.push_back("OILvac");
                    config_strings.push_back("MnSfP");
                    config_strings.push_back("VlvDly");
		    break;
		            }
                  case 'L': {
                    config_strings.push_back("SrfDft"); 
                    config_strings.push_back("SrfLon"); 
                    config_strings.push_back("SrfLat"); 
                    config_strings.push_back("SrfMxN"); 
                    config_strings.push_back("SrfInt"); 
	            break;
			    }
                  case 'Z': {
                    config_strings.push_back("Cyc"+Cs); //must append sub-cycle
                    config_strings.push_back("Ztar"+Cs); 
                    config_strings.push_back("Zpro"+Cs); 
                    config_strings.push_back("Tlast"+Cs); 
		    break;
		            }
                  case '#': {
                    config_strings.push_back("Nseek");
                    config_strings.push_back("STLmin");
                    config_strings.push_back("dTadZ");
                    config_strings.push_back("dTsdZ");
                    config_strings.push_back("TlastCF");
	            break;
		            }
                  default: 
	    	    break;
                            } //end of Cp switch
              }  // end of case 0 (SOLO2)
	      case 1: { //Deep SOLO 
//    	        std::cout << "examining DEEP SOLO specfic commands " << std::endl;
	        switch (Cp) { // The first character is the primary command
//                case 'Z': { // firware V 0.5 and prior
//                  config_strings.push_back("Cyc"+Cs); //must append sub-cycle
//                  config_strings.push_back("Zpark"+Cs); 
//                  config_strings.push_back("Zpro"+Cs); 
//                  config_strings.push_back("Clast"+Cs); 
//		    break;
//		            }
                  case 'A': { 
                    config_strings.push_back("Z1_RIS");
                    config_strings.push_back("Z2_RIS"); 
                    config_strings.push_back("W0_RIS"); 
                    config_strings.push_back("W1_RIS"); 
                    config_strings.push_back("W2_RIS"); 
	      	    break;
		            }
                  case 'E': { 
                    config_strings.push_back("dT_CP");
                    config_strings.push_back("dT_DP"); 
                    config_strings.push_back("T_WAIT"); 
                    config_strings.push_back("FP_CHK"); 
                    config_strings.push_back("DT_PMP"); 
                    config_strings.push_back("Z_dPMP"); 
	    	    break;
		            }
                  case 'H': {
                    config_strings.push_back("MxHiP");
                    config_strings.push_back("PmpBtm");
                    config_strings.push_back("Pmpslo");
                    config_strings.push_back("OILvac");
                    config_strings.push_back("MnSfP");
                    config_strings.push_back("VlvDly");
		    break;
		            }
                  case 'K': {  //"K" has different application on ascent/descent
                    Cb = command.substr(2,1); // If KA versus K
                    if ( Cb == "A" ) {
                      config_strings.push_back("AT1");
                      config_strings.push_back("AT2");
                      config_strings.push_back("AT3");
                    } else {
                      config_strings.push_back("PTS_T0");
                      config_strings.push_back("PTS_T1");
                      config_strings.push_back("PTS_T2");
                      config_strings.push_back("PTS_T3");
                      config_strings.push_back("PTS_T4");
                    }
                    break;
                        }
//                case 'L': { // V0.5 and before
//                  config_strings.push_back("CCzero");
//                  config_strings.push_back("CC_km");
//                  config_strings.push_back("ZN_CF");
//                  config_strings.push_back("Z_Neu");
//	            break;
//		            }
                  case 'L': { // V0.6 and after
                    config_strings.push_back("CCzero");
                    config_strings.push_back("CC_km");
                    config_strings.push_back("ZN_CF");
                    config_strings.push_back("ccCor");
	    	    break;
		            }
                  case 'M': { 
                    config_strings.push_back("dZ_0");
                    config_strings.push_back("dZ_1");
                    config_strings.push_back("dZ_2");
                    config_strings.push_back("dZ_3");
                    config_strings.push_back("dZ_4");
		    break;
		            }
                  case 'Y': { //for V1.0 and later, use "4" for V0.8
                    config_strings.push_back("Ice_Tc"); 
                    config_strings.push_back("Ice_Ps"); 
                    config_strings.push_back("Ice_Pd"); 
                    config_strings.push_back("Ice_Mn"); 
                    config_strings.push_back("Ice_Sc"); 
                    config_strings.push_back("IceIn"); 
                    config_strings.push_back("IceOut"); 
	    	    break;
			    }
                  case 'Z': { // firware V 0.6 and later
                    config_strings.push_back("Cyc"+Cs); //must append sub-cycle
                    config_strings.push_back("Zpark"+Cs); 
                    config_strings.push_back("Zpro"+Cs); 
                    config_strings.push_back("ccPro"+Cs); 
                    config_strings.push_back("ccPrk"+Cs); 
	        	break;
		            }
                  case '7': {
                    config_strings.push_back("Z_C2D");
                    config_strings.push_back("Z_DP_1");
                    config_strings.push_back("Z_DP_2");
                    config_strings.push_back("Z_DP_3");
                    config_strings.push_back("Z_DP_4");
		    break;
		            }
                  case '#': {
                    config_strings.push_back("Nseek");
                    config_strings.push_back("STLmin");
                    config_strings.push_back("dDdZ");
                    config_strings.push_back("dDdV2");
		    break;
		            }
                  case '@': {
                    config_strings.push_back("MAX_XL");
		    break;
		            }
                  default: 
		    break;
                            } //end of Cp switch
	      } // end of case 1 (DEEP)
	      case 2: { //BGC SOLO
//	        std::cout << "examining BGC SOLO specific commands " << std::endl;
	        switch (Cp) { // The first character is the primary command
                  case 'E': {
                    config_strings.push_back("Ice_Tc"); 
                    config_strings.push_back("Ice_Ps"); 
                    config_strings.push_back("Ice_Pd"); 
                    config_strings.push_back("Ice_Mn"); 
                    config_strings.push_back("Ice_Sc"); 
                    config_strings.push_back("IceIn"); 
                    config_strings.push_back("IceOut"); 
	    	    break;
			    }
                  case 'H': {
                    config_strings.push_back("MxHiP");
                    config_strings.push_back("PmpBtm");
                    config_strings.push_back("Pmpslo");
                    config_strings.push_back("MinRis");
                    config_strings.push_back("OILvac");
                    config_strings.push_back("MnSfP");
                    config_strings.push_back("VlvDly");
		    break;
		            }
                  case 'L': {
                    config_strings.push_back("SrfDft"); 
                    config_strings.push_back("SrfLon"); 
                    config_strings.push_back("SrfLat"); 
                    config_strings.push_back("SrfMxN"); 
                    config_strings.push_back("SrfInt"); 
		    break;
			    }
                  case 'Z': {
                    config_strings.push_back("Cyc"+Cs); //must append sub-cycle
                    config_strings.push_back("Ztar"+Cs); 
                    config_strings.push_back("Zpro"+Cs); 
                    config_strings.push_back("Tlast"+Cs); 
		    break;
		           }
                  case '#': {
                    config_strings.push_back("Nseek");
                    config_strings.push_back("STLmin");
                    config_strings.push_back("dTadZ");
                    config_strings.push_back("dTsdZ");
                    config_strings.push_back("TlastCF");
		    break;
		           }
                  default: 
                    break;
                           } //end of Cp switch
	      default:
                break;
            } //end SOLOtype switch
   	  } //end of dafault from inital casecase 
  	} // end of Solotype block

	std::string good_chars = "1234";
	if ( Cs == "S" && good_chars.find(Cb[1]) != std::string::npos) {
	  //int ans = ModSCI(command.substr(0,command.length()));
 	  return;

	} else {

// PROCESS CONFIGS

// Outputs the CONFIG names that are in this command
//       for (size_t i = 0; i < config_strings.size(); i++) {
//         std::cout << config_strings[i] << std::endl;
//       }

// Outputs the command
//	 std::cout << command.substr(2,command.length()) << std::endl;

	
         std::vector<int> config_values;
	 std::stringstream ss(command.substr(2,command.length()));
	 int num;
         while ( ss >> num ) {  //Extracts integers from command separated by whitespace
		config_values.push_back(num);
 	 }

//Calls routine to search for Config String/ Modify to Config Value/Outputs the changed parameters
         for (size_t i= 0; i < std::min(config_values.size(),config_strings.size()); i++) {
	   std::string unit="";
	   std::string desc="";
//	   std::cout << "Identified Upload_Command CONFIG " << config_strings[i] << "=" << config_values[i] << std::endl;
           if ( config_strings[i] != "Password" ) { ModOneConfig(config_strings[i],config_values[i],unit,desc); }
	   //Special for CTD gain
	   if ( config_strings[i] == "Sgain" ) { ress=determine_resolution(config_values[i]); }
	   if ( config_strings[i] == "Tgain" ) { rest=determine_resolution(config_values[i]); }
	   if ( config_strings[i] == "Pgain" ) { resp=determine_resolution(config_values[i]); }
         }
 	return;

        }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ModOneConfig(std::string ConfigName, int ConfigValue, std::string unit, std::string desc) {

//	std::cout << "In ModOneConfig " << ConfigName << "  " << ConfigValue << std::endl;	
        int indx = 0; //value passed back by function to communicate index of CONFIG within RunMission
	if ( RunMission.contains(std::string{ConfigName}) ) {
          if ( RunMission[ConfigName]["config_value"] != ConfigValue ) {
             std::cout << "\"" << ConfigName << "\" MODIFIED from " << RunMission[ConfigName]["config_value"] << " to NEW VALUE =>  " << ConfigValue <<std::endl; 
	     RunMission[ConfigName]["config_value"] = ConfigValue;
	     if ( mission_template[ConfigName].contains("scale") ) {
	       RunMission[ConfigName]["value"] = static_cast<double>(ConfigValue) / int(mission_template[ConfigName]["scale"]);
	     } else {
	       RunMission[ConfigName]["value"] = ConfigValue;
             } 
          } 
	// Always see if need to update "unit" and "description"
          if ( !unit.empty() ) {
  		RunMission[ConfigName]["unit"] = unit;
//                std::cout << "Unit Mission  = " << RunMission[ConfigName]["unit"] <<std::endl; 
	  }
          if ( !desc.empty() ) {
		RunMission[ConfigName]["description"] = desc;
 //               std::cout << "Desc Mission  = " << RunMission[ConfigName]["description"] <<std::endl; 
	  }
	} else {
	  //If made it here then there needs to be a CONFIG added to RunMission
          RunMission[ConfigName]["config_value"] = ConfigValue;	
	  if ( mission_template[ConfigName].contains("scale") ) {
	    RunMission[ConfigName]["value"] = static_cast<double>(ConfigValue) / int(mission_template[ConfigName]["scale"]);
	  } else {
            RunMission[ConfigName]["value"] = ConfigValue;
	  }
	  if ( mission_template.contains(std::string{ConfigName}) ) {
            RunMission[ConfigName]["unit"] = mission_template[ConfigName]["units"];  //Oddly declared differently (+s) 
            RunMission[ConfigName]["description"] = mission_template[ConfigName]["description"];	
	  } else {
            RunMission[ConfigName]["unit"] = unit;
            RunMission[ConfigName]["description"] = desc;	
	  }
	}
        return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
void rewrite_json(std::string filename, const json &Doc_L0) { 

        //read in some info from L0 json
        std::ostringstream ss;
        std::ostringstream is; //Check if Iridium position json is available
	if ( Doc_L0.contains("DAC_ID_NUMBER") ) {
  	  int iid=Doc_L0.at("DAC_ID_NUMBER");
          ss <<  L1_path << "/" << std::setw(4) << std::setfill('0') << iid << "_";
          is <<  SBD_path << "/" << std::setw(4) << std::setfill('0') << iid << "_";
	} else {
          ss <<  L1_path << "/99999_";
          is <<  SBD_path << "/99999_";
	}
	if ( Doc_L0.contains("TRANSMISSION_ID_NUMBER") ) {
	  int tid=Doc_L0.at("TRANSMISSION_ID_NUMBER");
          ss << std::setw(6) << std::setfill('0') << tid << "_L1_"; 
          is << std::setw(6) << std::setfill('0') << tid << "_SBD_"; 
	} else {
          ss <<  999999 << "_L1_";
          is <<  999999 << "_SBD_";
	}
        int cyn = Doc_L0.at("CYCLE_NUMBER");
	if ( cyn > -1 ) {
          ss << std::setw(4) << std::setfill('0') << cyn << ".json";
          is << std::setw(4) << std::setfill('0') << cyn << ".json";
	} else {
          ss << "-001" << ".json";
          is << "-001" << ".json";
	}
	std::string outFile = ss.str();
	std::string sbdFile = is.str();

    //std::cout << "DAC: " << Doc_L0["DAC_ID_NUMBER"] << std::endl; // ======================BG
    //std::cout << "TN#: " << Doc_L0["TRANSMISSION_ID_NUMBER"] << std::endl;
    //std::cout << "CYC: " << Doc_L0["CYCLE_NUMBER"] << std::endl;
    //std::cout << outFile << std::endl; //==================================================BG

        std::vector<nlohmann::json> GPSnew;
// choose GPS positions that were from THIS cycles surface interval...save to a new json GPSnew
        for (const auto& obj : GPSobj) {
          int gpscy = obj["cycle"].get<int>();
  	  const auto& matrix = obj.at("data"); 
	  for (const auto& row : matrix) {
	    const auto& gpstype = row.at("description"); 
      	    if ( ( gpscy == ( cyn + 1 ) & gpstype == "GPS_START" ) | ( gpscy == cyn & gpstype != "GPS_START" ) ) {
//     	          std::cout << gpscy << " " << gpstype << std::endl; 
//     	          std::cout << row << std::endl; 
      	      GPSnew.push_back(row); 
            }
          }
        }
//     	std::cout << GPSnew.size() << std::endl; 
//     	std::cout << GPSnew << std::endl; 



// determine if there are any CONFIG that relate to whether data is transmitted or not thus "Missing"
	std::vector<int> SBck(9,1); //presume that all CTD data is transmitted (SndBak=511)
	bool writeBinCTD = true; //presume that all BinnedCTD data is transmitted
        bool writeRawCTD = true; //presume that all Raw CTD is collected 
        bool writeDriftCTD = true; //presume that all drift CTD is collected 
        bool isBEACON = false; //presume that not in beacon
        bool isDPLY = false; //presume that not in cycle 0 BIST
        bool isBIST = false; //presume that not in cycle -1 BIST
	if ( RunMission.contains("SndBak") ) { // parses the CTD SndBak CONFIG
	  int val = RunMission["SndBak"]["config_value"].get<int>();
          int i=SBck.size()-1;
	  for ( int& num : SBck ) {
	    int p2 = pow(2,i);
	    if ( val < p2 ) {
	      SBck[i]=0;
	    } else {
	      val=val-p2;
	    }
 	    i--;
	  }
//	  std::cout << SBck[8] << " Send Binned PSAL" << std::endl;
//	  std::cout << SBck[7] << " Send Binned TEMP" << std::endl;
//	  std::cout << SBck[6] << " Send Binned PRES" << std::endl;
//	  std::cout << SBck[5] << " Send CTD Drift" << std::endl;
//	  std::cout << SBck[4] << " Send Pump" << std::endl;
//	  std::cout << SBck[3] << " Send Fall" << std::endl;
//	  std::cout << SBck[2] << " Send Rise" << std::endl;
//	  std::cout << SBck[1] << " Send Raw" << std::endl;
//	  std::cout << SBck[0] << " Send GPS/Eng/Argo" << std::endl;
	  auto it = std::find(SBck.end()-3, SBck.end(), 1);  //check last 3 "Binned" flags to see if any are set to return data
	  if ( it != SBck.end() ) {
	    writeBinCTD = true;
	  } else {
	    writeBinCTD = false; 
	  }
	}
	if ( RunMission.contains("UNBINd") ) {
	  int val = RunMission["UNBINd"]["config_value"].get<int>();
	  if ( val == 0 ) {
	    writeRawCTD = false; //there is no raw CTD collected
	  }
	}
  	if ( RunMission.contains("DrfDat") ) {
	  int val = RunMission["DrfDat"]["config_value"].get<int>();
	  if ( val == 0 ) {
	    writeDriftCTD = false; //there is no Drift CTD collected 
	  }
	}
// Determine the number of drift measurements (CTD) that are expected in cycle
	int Nsam = -9;
	if ( Doc_L0.contains(std::string{ "Engineering_Data" }) ) {  //in order to apply must get subcycle from engineering
	  if ( Doc_L0[ "Engineering_Data" ].contains("SubCycle") ) {  
            int subcyc = Doc_L0.at("Engineering_Data")["SubCycle"]["value"];
	    if ( RunMission.contains("Nsam" + std::to_string(subcyc)) ) {
	      Nsam = RunMission["Nsam" + std::to_string(subcyc)]["config_value"].get<int>();
	      if ( Nsam == 0 ) {
	        writeDriftCTD = false; //there is no Drift CTD collected 
	      }  
//	      std::cout << "Nsam " << Nsam << std::endl;
	    }
	  }
	}

// Check for special cycles through "packet_info" that return non-CONFIG data
	if ( Doc_L0.contains(std::string{ "packet_info" }) ) {
	  const auto& matrix = Doc_L0.at("packet_info")["packet_type"];
	  for (const auto& row : matrix) {
	     std::string id_string = row["id"].get<std::string>();
	     if ( id_string == "00" | id_string == "E0" ) {
	       isDPLY = true;
	       std::cout << "Identified Deployment cycle " << cyn << " packet_info id_string => " << id_string << std::endl;
	     }
	     if ( id_string == "05" | id_string == "06" | id_string == "E5" ) {
	       isBIST = true; 
	       std::cout << "Identified BIST cycle " << cyn << " packet_info id_string => " << id_string << std::endl;
	     }
	     if ( id_string == "03" | id_string == "E3" ) {
	       isBEACON = true;
	       std::cout << "Identified BEACON cycle " << cyn << " packet_info id_string => " << id_string << std::endl;
	     }
	  }
	}
// FINISHED gathering of CONFIG
//
// write the updated json file
    	//std::cout << "Output: " << outFile << std::endl; 
        std::ofstream fout(outFile);

        fout << "{" << std::endl;
	copyLine ( fout, "FILE_CREATION_DATE", "", Doc_L0 );  // as last field dont send in ","
        fout << "," << std::endl << "  \"FILE_UPDATE_DATE\": \"" << date_format(utcnow(),"yyyy-mm-ddTHH:MM:SSZ") << "\"";
	copyLine ( fout, "DECODER_VERSION", ",", Doc_L0 );
	copyLine ( fout, "SCHEMA_VERSION", ",", Doc_L0 );
	copyLine ( fout, "INTERNAL_ID_NUMBER", ",", Doc_L0 );
	copyLine ( fout, "DAC_ID_NUMBER", ",", Doc_L0 );
	copyLine ( fout, "WMO_ID_NUMBER", ",", Doc_L0 );
	copyLine ( fout, "TRANSMISSION_ID_NUMBER", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "INSTRUMENT_TYPE" } ) )
	  copyLine ( fout, "INSTRUMENT_TYPE", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "WMO_INSTRUMENT_TYPE" } ) )
	  copyLine ( fout, "WMO_INSTRUMENT_TYPE", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "WMO_RECORDER_TYPE" } ) )
	  copyLine ( fout, "WMO_RECORDER_TYPE", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "PI" } ) )
	  copyLine ( fout, "PI", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "OPERATING_INSTITUTION" } ) )
	  copyLine ( fout, "OPERATING_INSTITUTION", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "PROJECT_NAME" } ) )
	  copyLine ( fout, "PROJECT_NAME", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "PROGRAM_NAME" } ) )
	  copyLine ( fout, "PROGRAM_NAME", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "FIRMWARE_VERSION" } ) )
	  copyLine ( fout, "FIRMWARE_VERSION", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "POSITIONING_SYSTEM" } ) )
	  copyLine ( fout, "POSITIONING_SYSTEM", ",", Doc_L0 );
        if ( Doc_L0.contains(std::string{ "CYCLE_NUMBER" } ) )
	  copyLine ( fout, "CYCLE_NUMBER", ",", Doc_L0 ); 

	if ( Doc_L0.contains(std::string{ "telemetry_summary" }) ) {
  	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"telemetry_summary\": [" << std::endl;
	  const auto& matrix = Doc_L0.at("telemetry_summary");
          size_t j = 0;
	  for (const auto& row : matrix) {
            j++; 
            fout << "    { \"PID\": " << std::setw(2) << row["PID"].get<int>() << ", ";
            fout << "\"source\": " << row["source"] << ", ";
            fout << "\"TIME\": " << row["TIME"] << ", ";
	    std::tm result = string_to_tm(row["TIME"]);
            long long seconds_result = get_seconds_since_1950(result);
            fout << "\"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << seconds_result/86400. << ", ";
	    fout << "\"momsn\": " << row["momsn"].get<int>() << ", ";
            fout << "\"size\": " << std::setw(4) << row["size"].get<int>() << ", ";
            fout << "\"sensor_ids\": "; 
            size_t i = 0;
      	    for (const auto& element : row) {
              i++; 
     	      if (i == row.size() & j == matrix.size() ) {
	  	fout << element << " }" << std::endl;
  	      } else if ( i == row.size() ) {
		fout << element << " }," << std::endl;
	      }
  	    }
	  }
	  fout << "  ]";
	}

	if ( Doc_L0.contains(std::string{ "packet_info" }) ) {
  	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"packet_info\": {" << std::endl;
          fout << "    \"packet_count\": " << Doc_L0.at("packet_info")["packet_count"] << "," << std::endl;
          fout << "    \"packet_bytes\": " << Doc_L0.at("packet_info")["packet_bytes"] << "," << std::endl;
          fout << "    \"packet_type\": [" << std::endl;
	  const auto& matrix = Doc_L0.at("packet_info")["packet_type"];
          size_t i = 0;
	  for (const auto& row : matrix) {
             i++; 
             fout << "      { \"id\": " << row["id"] << ", ";
             fout << "\"bytes\": " << std::setw(4) << row["bytes"].get<int>() << ", ";
             fout << "\"packets\": " << std::setw(2) << row["packets"].get<int>() << ", ";
             fout << "\"description\": " << row["description"];
     	     if ( i == matrix.size() ) {
               fout <<  " }" << std::endl;
	     } else {
               fout <<  " }," << std::endl;
	     }
	  } 
	  fout << "    ]" << std::endl;
	  fout << "  }";
	} 

	// Check to see if SBD Iridium position json exists...if so read it in and write it out to new L1
        if (std::filesystem::exists(sbdFile)) {
          std::ifstream tempsbd(sbdFile, std::ifstream::binary);
          tempsbd >> SBDjson; //json of Iridium SBD data (global)

  	  if ( SBDjson.contains("sbd_iridium_position_summary") && SBDjson["sbd_iridium_position_summary"].is_array()) {
  	    fout << "," << std::endl; //finish the previous block
  	    fout << "  \"sbd_iridium_position_summary\": [ " << std::endl;
            size_t j = 0;
            for (const auto& row : SBDjson["sbd_iridium_position_summary"]) {
	        j++;
                fout << "    { \"source\": \"" << row.value("source","") << "\", ";
                fout << "\"TIME\": \"" << row.value("TIME","") << "\", ";
                fout << "\"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << row.value("JULD",999999.) << ", ";
                fout << "\"momsn\": " << row.value("momsn",-9) << ", ";
                fout << "\"LATITUDE_iridium\": " << std::fixed << std::setprecision(5) << std::setw(7) << row.value("LATITUDE_iridium",99999.) << ", ";
                fout << "\"LONGITUDE_iridium\": " << std::fixed << std::setprecision(5) << std::setw(8) << row.value("LONGITUDE_iridium",99999.) << ", ";
                fout << "\"error_iridium\": " << row.value("error_iridium",-9) << ", ";
                fout << "\"sbd_contains_cycle\": " << row.value("sbd_contains_cycle",-9);
     	        if ( j == SBDjson["sbd_iridium_position_summary"].size() ) {
                  fout <<  " }" << std::endl;
	        } else {
                  fout <<  " }," << std::endl;
	        }
	    }
            fout <<  "  ]";
	  } else {
  	    fout << "," << std::endl; //finish the previous block
  	    fout << "  \"sbd_iridium_position_summary\": [ " << std::endl;
            fout <<  "  ]";
	  } 
  	  if ( SBDjson.contains("sbd_weighted_iridium_position") && SBDjson["sbd_weighted_iridium_position"].is_array()) {
  	    fout << "," << std::endl; //finish the previous block
  	    fout << "  \"sbd_weighted_iridium_position\": [ " << std::endl;
            size_t j = 0;
	    for (const auto& row : SBDjson["sbd_weighted_iridium_position"]) {;
              j++;
              fout << "    { \"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << row.value("JULD",999999.) << ", ";
              //fout << "\"weighted_subset_count\": " << std::fixed << std::setw(2) << row.value("weighted_subset_count",-1) << ", ";
              fout << "\"LATITUDE\": " << std::fixed << std::setprecision(3) << std::setw(7) << row.value("LATITUDE",99999.) << ", ";
              fout << "\"LONGITUDE\": " << std::fixed << std::setprecision(3) << std::setw(8) << row.value("LONGITUDE",99999.) << ", ";
              fout << "\"POSITION_ERROR_ESTIMATED\": " << row["POSITION_ERROR_ESTIMATED"].get<int>() << ", ";
              fout << "\n      \"POSITION_ERROR_COMMENT\": \"" << row["POSITION_ERROR_COMMENT"].get<std::string>() << "\"";
     	      if ( j == SBDjson["sbd_weighted_iridium_position"].size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
            }
            fout <<  "  ]";
          }
        }

// Modified to shift GPS from what is in Doc_L0
//	if ( Doc_L0.contains(std::string{ "GPS" }) ) {
	if ( GPSnew.size() > 0 ) {
  	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"GPS\": [" << std::endl;
//	  const auto& matrix = Doc_L0.at("GPS");
	  const auto& matrix = GPSnew;
          size_t i = 0;
	  for (const auto& row : matrix) {
            i++; 
	    std::string s=row["description"].get<std::string>();
            fout << "    { \"description\": " << std::setw(9-s.length()) << "" << row["description"] << ",";
            fout << " \"TIME\": " << row["TIME"] << ", ";
	    std::tm result = string_to_tm(row["TIME"]);
            long long seconds_result = get_seconds_since_1950(result);
            fout << "\"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << seconds_result/86400. << ", ";
            fout << "\"LATITUDE\": " << decimal(row["LATITUDE"],9,5) << ",";
            fout << " \"LONGITUDE\": " << decimal(row["LONGITUDE"],10,5) << ",";
            fout << " \"HDOP\": " << decimal(row["HDOP"],5,1) << ", ";
            fout << "\"sat_cnt\": " << std::setw(2) << row["sat_cnt"].get<int>() << ",";
            fout << " \"snr_min\": " << std::setw(2) << row["snr_min"].get<int>() << ",";
            fout << " \"snr_mean\": " << std::setw(2) << row["snr_mean"].get<int>() << ",";
            fout << " \"snr_max\": " << std::setw(2) <<  row["snr_max"].get<int>() << ",";
            fout << " \"time_to_fix\": " << std::setw(3) << row["time_to_fix"].get<int>() << ",";
            fout << " \"valid\": " << std::setw(2) << row["valid"].get<int>();
     	    if ( i == matrix.size() ) {
              fout <<  " }" << std::endl;
	    } else {
              fout <<  " }," << std::endl;
	    }
	  } 
	  fout << "  ]";
	} 

	if ( Doc_L0.contains(std::string{ "Upload_Command" }) ) {
          fout << "," << std::endl; //finish the previous block
          fout << "  \"Upload_Command\": " << Doc_L0.at("Upload_Command");
        }

	if ( Doc_L0.contains(std::string{ "ARGO_Mission" }) ) {
  	  fout << "," << std::endl; //finish the previous block
//          resp=determine_resolution(Doc_L0.at("ARGO_Mission")["ctd_pres"]["gain"]);
//          rest=determine_resolution(Doc_L0.at("ARGO_Mission")["ctd_temp"]["gain"]);
//          ress=determine_resolution(Doc_L0.at("ARGO_Mission")["ctd_psal"]["gain"]);
  	  fout << "  \"ARGO_Mission\": {" << std::endl;
          fout << "    \"float_model\": " << Doc_L0.at("ARGO_Mission")["float_model"] << "," << std::endl;
          fout << "    \"float_telemetry_format\": " << decimal(Doc_L0.at("ARGO_Mission")["float_telemetry_format"],3,1) << "," << std::endl;
          fout << "    \"min_ascent_rate_cmpersec\": " << Doc_L0.at("ARGO_Mission")["min_ascent_rate_cmpersec"] << "," << std::endl;
          fout << "    \"profile_target_dbar\": " << Doc_L0.at("ARGO_Mission")["profile_target_dbar"] << "," << std::endl;
          fout << "    \"drift_target_dbar\": " << Doc_L0.at("ARGO_Mission")["drift_target_dbar"] << "," << std::endl;
          fout << "    \"max_rise_minute\": " << Doc_L0.at("ARGO_Mission")["max_rise_minute"] << "," << std::endl;
          fout << "    \"max_fall_to_park_minute\": " << Doc_L0.at("ARGO_Mission")["max_fall_to_park_minute"] << "," << std::endl;
          fout << "    \"max_fall_to_profile_minute\": " << Doc_L0.at("ARGO_Mission")["max_fall_to_profile_minute"] << "," << std::endl;
          fout << "    \"target_drift_5minute\": " << Doc_L0.at("ARGO_Mission")["target_drift_5minute"] << "," << std::endl;
          fout << "    \"target_surface_second\": " << Doc_L0.at("ARGO_Mission")["target_surface_second"] << "," << std::endl;
          fout << "    \"seek_periods\": " << Doc_L0.at("ARGO_Mission")["seek_periods"] << "," << std::endl;
          fout << "    \"seek_minute\": " << Doc_L0.at("ARGO_Mission")["seek_minute"] << "," << std::endl;
          fout << "    \"ctd_pres\": { \"gain\":" << std::setw(5) << Doc_L0.at("ARGO_Mission")["ctd_pres"]["gain"].get<int>() << ", \"offset\":" << std::setw(4) << Doc_L0.at("ARGO_Mission")["ctd_pres"]["offset"].get<int>() << "}," << std::endl;
          fout << "    \"ctd_temp\": { \"gain\":" << std::setw(5) << Doc_L0.at("ARGO_Mission")["ctd_temp"]["gain"].get<int>() << ", \"offset\":" << std::setw(4) << Doc_L0.at("ARGO_Mission")["ctd_temp"]["offset"].get<int>() << "}," << std::endl;
          fout << "    \"ctd_psal\": { \"gain\":" << std::setw(5) << Doc_L0.at("ARGO_Mission")["ctd_psal"]["gain"].get<int>() << ", \"offset\":" << std::setw(4) << Doc_L0.at("ARGO_Mission")["ctd_psal"]["offset"].get<int>() << "}" << std::endl;
	  fout << "  }";
	}

        if ( Doc_L0.contains(std::string{ "BIT" }) ) {
          fout << "," << std::endl; //finish the previous block
          const auto& matrix = Doc_L0.at("BIT");
          fout << "  \"BIT\": {" << std::endl;
//          std::string status = Doc_L0["BIT"]["status"];
          float pump_voltage;
	  if (Doc_L0["BIT"]["status"] == "Beacon") {
             fout << "    \"status\": " << Doc_L0["BIT"]["status"] << "," << std::endl;
	     write_BIST( fout, "BIT", "Eng_ver" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "nQueued" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "nTries" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "parXstat" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "SBDIstat" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "cpu_voltage" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "pump_voltage" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "vacuum_transmit_inHg" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "vacuum_abort_inHg" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "last_interrupt" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "abortFlag" , Doc_L0 ,2,0,0);
	     write_BIST( fout, "BIT", "CPUtemp" , Doc_L0 ,4,2,0);
	     write_BIST( fout, "BIT", "RH" , Doc_L0 ,2,0,1);
          } else {
             fout << "    \"status\": " << Doc_L0["BIT"]["status"] << "," << std::endl;
	     write_BIST( fout, "BIT", "Eng_ver" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "blocks_queued" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "pressure" , Doc_L0 ,5,resp,0);
	     write_BIST( fout, "BIT", "cpu_voltage" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "pump_voltage_prior" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "pump_voltage_after" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "pump_current_mA" , Doc_L0 ,2,0,0);
	     write_BIST( fout, "BIT", "pump_time_s" , Doc_L0 ,2,0,0);
	     write_BIST( fout, "BIT", "pump_oil_prior" , Doc_L0 ,2,0,0);
	     write_BIST( fout, "BIT", "pump_oil_after" , Doc_L0 ,2,0,0);
	     write_BIST( fout, "BIT", "vacuum_prior_inHg" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "vacuum_after_inHg" , Doc_L0 ,5,2,0);
	     write_BIST( fout, "BIT", "valve_open" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "valve_close" , Doc_L0 ,1,0,0);
	     write_BIST( fout, "BIT", "interrupt_id" , Doc_L0 ,1,0,0);
	     if (Doc_L0["BIT"]["Eng_ver"] >= 6) {
               fout << "    \"SBE_response\": " << Doc_L0["BIT"]["SBE_response"] << "," << std::endl;
	       write_BIST( fout, "BIT", "cpu_temp_degC" , Doc_L0 ,4,2,0);
	       write_BIST( fout, "BIT", "RH" , Doc_L0 ,2,0,1);
	     } else {
               fout << "    \"SBE_response\": " << Doc_L0["BIT"]["SBE_response"] << std::endl;
             }
          }
          fout << "  }";
        }

	if ( Doc_L0.contains(std::string{ "Fall" }) || ( SBck[3] == 1 && !isBEACON && !isBIST && !isDPLY ) ) {
  	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"Fall\": [" << std::endl;
	  if ( Doc_L0.contains(std::string{ "Fall" }) ) {
	  const auto& matrix = Doc_L0.at("Fall");
          size_t i = 0;
	  for (const auto& row : matrix) {
             i++; 
             fout << "    { \"TIME\": " << row["TIME"] << ", ";
	     std::tm result = string_to_tm(row["TIME"]);
             long long seconds_result = get_seconds_since_1950(result);
             fout << "\"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << seconds_result/86400. << ", ";
             fout << "\"PRES\": " << decimal(row["PRES"],7,resp) << ", ";
             fout << "\"phase\": " << decimal(row["phase"],2,0) << ", ";
             fout << "\"description\": " << row["description"];
     	     if ( i == matrix.size() ) {
               fout <<  " }" << std::endl;
	     } else {
               fout <<  " }," << std::endl;
	     }
          } 
          } 
	  fout << "  ]";
        }

	if ( Doc_L0.contains(std::string{ "Rise" }) || ( SBck[2] == 1 && !isBEACON && !isBIST ) ) {
 	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"Rise\": [" << std::endl;
	  if ( Doc_L0.contains(std::string{ "Rise" }) ) {
	    const auto& matrix = Doc_L0.at("Rise");
            size_t i = 0;
	    for (const auto& row : matrix) {
             i++; 
             fout << "    { \"TIME\": " << row["TIME"] << ", ";
	     std::tm result = string_to_tm(row["TIME"]);
             long long seconds_result = get_seconds_since_1950(result);
             fout << "\"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << seconds_result/86400. << ", ";
             fout << "\"PRES\": " << decimal(row["PRES"],7,resp) << ", ";
             fout << "\"phase\": " << decimal(row["phase"],2,0) << ", ";
             fout << "\"description\": " << row["description"];
     	     if ( i == matrix.size() ) {
               fout <<  " }" << std::endl;
	     } else {
               fout <<  " }," << std::endl;
	     }
	    } 
	  } 
	  fout << "  ]";
        } 

	if ( Doc_L0.contains(std::string{ "bist_ctd" }) ) {
  	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("bist_ctd");
  	  fout << "  \"bist_ctd\": {" << std::endl;
	  write_BIST( fout, "bist_ctd", "status" , Doc_L0 ,2,0,0);
          if ( Doc_L0["bist_ctd"].contains("errors") ) {
	    fout << "    \"errors\": [" << std::endl;
	    const auto& matrix = Doc_L0["bist_ctd"].at("errors");
            size_t i = 0;
	    for (const auto& row : matrix) {
              i++; 
	      fout << "      { \"";
              for (auto const& [keyr,valr] : row.items()) {
		   fout  << keyr << "\": "; 
		   fout << valr;
              }
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
	    }
            fout <<  "    ]," << std::endl;
	  }
	  write_BIST( fout, "bist_ctd", "numErr" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ctd", "voltage_V" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_ctd", "pressure_dbar" , Doc_L0 ,7,resp,0);
	  write_BIST( fout, "bist_ctd", "current_idle_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ctd", "current_ctd_high_mA", Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ctd", "current_ctd_low_mA", Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ctd", "current_ctd_sleep_mA", Doc_L0 ,2,0,1);
  	  fout << "  }";
	}

	if ( Doc_L0.contains(std::string{ "bist_DO" }) ) {
  	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("bist_DO");
  	  fout << "  \"bist_DO\": {" << std::endl;
	  write_BIST( fout, "bist_DO", "status" , Doc_L0 ,2,0,0);
          if ( Doc_L0["bist_DO"].contains("errors") ) {
	    fout << "    \"errors\": [" << std::endl;
	    const auto& matrix = Doc_L0["bist_DO"].at("errors");
            size_t i = 0;
	    for (const auto& row : matrix) {
              i++; 
	      fout << "      { \"";
              for (auto const& [keyr,valr] : row.items()) {
		   fout  << keyr << "\": "; 
		   fout << valr;
              }
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
	    }
            fout <<  "    ]," << std::endl;
	  }
	  write_BIST( fout, "bist_DO", "numErr" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_DO", "voltage_V" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_DO", "max_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_DO", "avg_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_DO", "phase_delay_us" , Doc_L0 ,6,3,0);
	  write_BIST( fout, "bist_DO", "thermistor_voltage_V" , Doc_L0 ,5,3,0);
	  write_BIST( fout, "bist_DO", "DO" , Doc_L0 ,5,3,0);
	  write_BIST( fout, "bist_DO", "thermistor_degC" , Doc_L0 ,5,3,1);
          fout << "  }";
        }

	if ( Doc_L0.contains(std::string{ "bist_pH" }) ) {
  	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("bist_pH");
  	  fout << "  \"bist_pH\": {" << std::endl;
	  write_BIST( fout, "bist_pH", "status" , Doc_L0 ,2,0,0);
          if ( Doc_L0["bist_pH"].contains("errors") ) {
	    fout << "    \"errors\": [" << std::endl;
	    const auto& matrix = Doc_L0["bist_pH"].at("errors");
            size_t i = 0;
	    for (const auto& row : matrix) {
              i++; 
	      fout << "      { \"";
              for (auto const& [keyr,valr] : row.items()) {
		   fout  << keyr << "\": "; 
		   fout << valr;
              }
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
	    }
            fout <<  "    ]," << std::endl;
	  }
	  write_BIST( fout, "bist_pH", "numErr" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_pH", "voltage_V" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_pH", "max_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_pH", "avg_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_pH", "vRef" , Doc_L0 ,6,3,0);
	  write_BIST( fout, "bist_pH", "Vk" , Doc_L0 ,6,3,0);
	  write_BIST( fout, "bist_pH", "Ik" , Doc_L0 ,7,3,0);
	  write_BIST( fout, "bist_pH", "Ib" , Doc_L0 ,7,3,1);
          fout << "  }";
        }

	if ( Doc_L0.contains(std::string{ "bist_ECO" }) ) {
  	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("bist_ECO");
  	  fout << "  \"bist_ECO\": {" << std::endl;
	  write_BIST( fout, "bist_ECO", "status" , Doc_L0 ,2,0,0);
          if ( Doc_L0["bist_ECO"].contains("errors") ) {
	    fout << "    \"errors\": [" << std::endl;
	    const auto& matrix = Doc_L0["bist_ECO"].at("errors");
            size_t i = 0;
	    for (const auto& row : matrix) {
              i++; 
	      fout << "      { \"";
              for (auto const& [keyr,valr] : row.items()) {
		   fout  << keyr << "\": "; 
		   fout << valr;
              }
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
	    }
            fout <<  "    ]," << std::endl;
	  }
	  write_BIST( fout, "bist_ECO", "numErr" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ECO", "voltage_V" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_ECO", "max_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ECO", "avg_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_ECO", "ch01" , Doc_L0 ,1,0,0);
	  write_BIST( fout, "bist_ECO", "ch02" , Doc_L0 ,1,0,0);
	  write_BIST( fout, "bist_ECO", "ch03" , Doc_L0 ,1,0,1);
  	  fout << "  }";
        }

	if ( Doc_L0.contains(std::string{ "bist_OCR" }) ) {
 	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("bist_OCR");
  	  fout << "  \"bist_OCR\": {" << std::endl;
	  write_BIST( fout, "bist_OCR", "status" , Doc_L0 ,2,0,0);
          if ( Doc_L0["bist_OCR"].contains("errors") ) {
	    fout << "    \"errors\": [" << std::endl;
	    const auto& matrix = Doc_L0["bist_OCR"].at("errors");
            size_t i = 0;
	    for (const auto& row : matrix) {
              i++; 
	      fout << "      { \"";
              for (auto const& [keyr,valr] : row.items()) {
		   fout  << keyr << "\": "; 
		   fout << valr;
              }
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
	    }
            fout <<  "    ]," << std::endl;
	  }
	  write_BIST( fout, "bist_OCR", "numErr" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_OCR", "voltage_V" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_OCR", "max_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_OCR", "avg_current_mA" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_OCR", "ch01" , Doc_L0 ,1,0,0);
	  write_BIST( fout, "bist_OCR", "ch02" , Doc_L0 ,1,0,0);
	  write_BIST( fout, "bist_OCR", "ch03" , Doc_L0 ,1,0,0);
	  write_BIST( fout, "bist_OCR", "ch04" , Doc_L0 ,1,0,1);
          fout << "  }";
        }

	if ( Doc_L0.contains(std::string{ "bist_Nitrate" }) ) {
  	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("bist_Nitrate");
  	  fout << "  \"bist_Nitrate\": {" << std::endl;
	  write_BIST( fout, "bist_Nitrate", "status" , Doc_L0 ,2,0,0);
          if ( Doc_L0["bist_Nitrate"].contains("errors") ) {
	    fout << "    \"errors\": [" << std::endl;
	    const auto& matrix = Doc_L0["bist_Nitrate"].at("errors");
            size_t i = 0;
	    for (const auto& row : matrix) {
              i++; 
	      fout << "      { \"";
              for (auto const& [keyr,valr] : row.items()) {
		   fout  << keyr << "\": "; 
		   fout << valr;
              }
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " }," << std::endl;
	      }
	    }
            fout <<  "    ]," << std::endl;
	  }
	  write_BIST( fout, "bist_Nitrate", "numErr" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_Nitrate", "JErr" , Doc_L0 ,3,0,0);
	  write_BIST( fout, "bist_Nitrate", "J_rh" , Doc_L0 ,6,2,0);
	  write_BIST( fout, "bist_Nitrate", "J_volt" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_Nitrate", "J_amps" , Doc_L0 ,5,3,0);
	  write_BIST( fout, "bist_Nitrate", "J_darkM" , Doc_L0 ,6,1,0);
	  write_BIST( fout, "bist_Nitrate", "J_darkS" , Doc_L0 ,6,1,0);
	  write_BIST( fout, "bist_Nitrate", "J_Nitrate" , Doc_L0 ,6,3,0);
	  write_BIST( fout, "bist_Nitrate", "J_res" , Doc_L0 ,5,3,0);
	  write_BIST( fout, "bist_Nitrate", "J_fit1" , Doc_L0 ,5,1,0);
	  write_BIST( fout, "bist_Nitrate", "J_fit2" , Doc_L0 ,5,1,0);
	  write_BIST( fout, "bist_Nitrate", "J_Spectra" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_Nitrate", "J_SeaDark" , Doc_L0 ,7,2,0);
  	  if ( !Doc_L0["bist_Nitrate"]["sval"].is_null() ) {
            fout << "    \"sval\": " << Doc_L0.at("bist_Nitrate")["sval"] << "," << std::endl;
	  } else {
            fout << "    \"sval\": " << "\"\"" << "," << std::endl;
	  }
  	  if ( !Doc_L0["bist_Nitrate"]["SUNA_str"].is_null() ) {
            fout << "    \"SUNA_str\": " << Doc_L0.at("bist_Nitrate")["SUNA_str"] << "," << std::endl;
	  } else {
            fout << "    \"SUNA_str\": " << "\"\"" << "," << std::endl;
	  }
  	  if ( !Doc_L0["bist_Nitrate"]["ascii_timestamp"].is_null() ) {
            fout << "    \"ascii_timestamp\": " << Doc_L0.at("bist_Nitrate")["ascii_timestamp"] << "," << std::endl;
	  } else {
            fout << "    \"ascii_timestamp\": " << "\"\"" << "," << std::endl;
	  }
	  write_BIST( fout, "bist_Nitrate", "ascii_pressure" , Doc_L0 ,7,resp,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_temperature" , Doc_L0 ,6,rest,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_salinity" , Doc_L0 ,6,ress,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_sample_count" , Doc_L0 ,3,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_cycle_count" , Doc_L0 ,3,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_error_count" , Doc_L0 ,3,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_internal_temp" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_spectrometer" , Doc_L0 ,6,2,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_RH" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_voltage" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_current" , Doc_L0 ,6,3,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_ref_detector_mean" , Doc_L0 ,3,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_ref_detector_std" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_dark_spectrum_mean" , Doc_L0 ,3,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_dark_spectrum_std" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_sensor_salinity" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_sensor_nitrate" , Doc_L0 ,5,2,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_residual_rms" , Doc_L0 ,8,7,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_FIT_pixel_begin" , Doc_L0 ,2,0,0);
	  write_BIST( fout, "bist_Nitrate", "ascii_FIT_pixel_end" , Doc_L0 ,2,0,1);
  	  fout << "  }";
	}

	if ( Doc_L0.contains(std::string{ "Science" }) ) {
 	  fout << "," << std::endl; //finish the previous block
	  const auto& matrix = Doc_L0.at("Science");
  	  fout << "  \"Science\": {" << std::endl;
          fout << "    \"Version\": " << Doc_L0.at("Science")["Version"] << "," << std::endl;
          fout << "    \"Files\": " << Doc_L0.at("Science")["Files"] << "," << std::endl;
          fout << "    \"Files_Written\": " << Doc_L0.at("Science")["Files_Written"] << "," << std::endl;
          fout << "    \"Space_available_MB\": " << Doc_L0.at("Science")["Space_available_MB"] << "," << std::endl;
          fout << "    \"CTD_Errors\": " << Doc_L0.at("Science")["CTD_Errors"] << "," << std::endl;
          fout << "    \"Pressure_Jumps\": " << Doc_L0.at("Science")["Pressure_Jumps"] << std::endl;
  	  fout << "  }";
	}
	
// Note: in BEACON MODE, Engineering data is saved to "BIT" within the json
	if ( Doc_L0.contains(std::string{ "Engineering_Data" }) || ( SBck[0] == 1 && !isBEACON && !isBIST) ) {
  	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"Engineering_Data\": {" << std::endl;
	  if ( Doc_L0.contains(std::string{ "Engineering_Data" }) ) {

     	    const auto& usejson = engineering_template;
	    if ( Doc_L0["Engineering_Data"].contains("vacBIT") || Doc_L0["Engineering_Data"].contains("VcpuBIT") ) {
                    const auto& usejson = diagnostic_template; 
            } 
	
	    const auto& matrix = Doc_L0.at("Engineering_Data");
            int i = 0;
	    for (const auto& loop : matrix.items()) {
  	      i++;
	      std::string name = "\"" + loop.key() + "\"";
 	      fout << "    " << std::setw(12) << name << ": {";

	      int prec = 0;
              if ( usejson.contains(loop.key()) ) {
                if ( usejson[loop.key()].contains("scale") ) {
                  if ( usejson[loop.key()]["scale"] == "pres") {
	            prec = resp;
	          } else if ( usejson[loop.key()]["scale"] == "temp") {
	            prec = rest;
	          } else if ( usejson[loop.key()]["scale"] == "psal") {
	            prec = ress;
                  } else {
                    prec = int( usejson[loop.key()]["prec"]);
                  }
	        }  
	      }  

	      std::stringstream ss;
              if ( !writeDriftCTD & ( loop.key() == "DPavg0" || loop.key() == "DPavg1" || 
				      loop.key() == "DTavg0" || loop.key() == "DTavg1" || 
				      loop.key() == "DSavg0" || loop.key() == "DSavg1")) {
                prec=0;
	        ss << std::fixed << std::setprecision(prec) << -999;
              } else {
	        ss << std::fixed << std::setprecision(prec) << loop.value()["value"].get<double>();
	      }
 	      fout << " \"value\": " << std::setw(8) << ss.str() << ",";
	      ss.str("");
	      ss << loop.value()["unit"];
  	      fout << " \"unit\": " << std::setw(6) << ss.str() << ","; 
  	      fout << " \"description\": " << matrix[loop.key()]["description"];
     	      if ( i == matrix.size() ) {
                fout <<  " }" << std::endl;
	      } else {
                fout <<  " },"<< std::endl;
	      }
	    }
	  }
          fout << "  }";
        }
      
// output "Mission" no matter what
 	fout << "," << std::endl; //finish the previous block
  	fout << "  \"Mission\": {" << std::endl;
        int i = 0;
	for (const auto& loop : RunMission.items()) {
	  i++;
	  std::string name = "\"" + loop.key() + "\"";
 	  fout << "    " << std::setw(12) << name << ": {";
	  std::stringstream ss,sc;
          int prec;
	  if ( mission_template[loop.key()].contains("prec") ) {
	    prec = int( mission_template[loop.key()]["prec"] );
	  } else {
	    prec = 0;
	  }
	  // for some CONFIG that are disabled within firmware (e.g. Ice with version >= 1.5 except V0.7 and V0.8) mark -1
	  if ( name == "\"Ice_Mn" && std::round(10*float_version) <= 14 && std::round(10*float_version) != 7 && std::round(10*float_version) != 8 ) {
	    ss << -1;
	    sc << -1;
          } else {
	    ss << std::fixed << std::setprecision(prec) << loop.value()["value"].get<double>();
	    sc << loop.value()["config_value"];
	  }
       	  fout << " \"config_value\": " << std::setw(6) << sc.str() << ",";
       	  fout << " \"value\": " << std::setw(7) << ss.str() << ",";
	  ss.str("");
	  ss << loop.value()["unit"];
  	  fout << " \"unit\": " << std::setw(6) << ss.str() << ","; 
  	  fout << " \"description\": " << RunMission[loop.key()]["description"];
     	  if ( i == RunMission.size() ) {
            fout <<  " }" << std::endl;
	  } else {
            fout <<  " },"<< std::endl;
	  }
	}
        fout << "  }";

	if ( Doc_L0.contains(std::string{ "Pump" }) || ( SBck[4] == 1 && !isBEACON && !isBIST) ) {
  	  fout << "," << std::endl; //finish the previous block
  	  fout << "  \"Pump\": [" << std::endl;
	  if ( Doc_L0.contains(std::string{ "Pump" }) ) {
	    const auto& matrix = Doc_L0.at("Pump");
            size_t i = 0;
	    for (const auto& row : matrix) {
             i++; 
	     int iTime = row.contains(std::string{"TIME"});
             if ( iTime ) {
               fout << "    { \"TIME\": " << row["TIME"] << ", ";
	       std::tm result = string_to_tm(row["TIME"]);
               long long seconds_result = get_seconds_since_1950(result);
               fout << "\"JULD\": " << std::fixed << std::setprecision(5) << std::setw(10) << seconds_result/86400. << ", ";
               fout << "\"PRES\": " << decimal(row["PRES"],7,resp) << ", ";
	     } else {
               fout << "    { \"PRES\": " << decimal(row["PRES"],7,resp) << ", ";
	     }
             fout << "\"current\": " <<  decimal(row["current"],5,0) << ", ";
             fout << "\"voltage\": " << decimal(row["voltage"],5,2) << ", ";
             fout << "\"pump_time\": " << decimal(row["pump_time"],4,0) << ", ";
             fout << "\"vac_start\": " << decimal(row["vac_start"],3,0) << ", ";
             fout << "\"vac_end\": " << decimal(row["vac_end"],3,0) << ", ";
             fout << "\"phase\": " << decimal(row["phase"],2,0) << ", ";
             fout << "\"description\": " << row["description"];
     	     if ( i == matrix.size() ) {
               fout <<  " }" << std::endl;
	     } else {
               fout <<  " }," << std::endl;
	     }
	    } 
	  } 
	  fout << "  ]";
	}

	if ( Doc_L0.contains(std::string{ "CTD_Raw" }) || ( SBck[1]==1 && writeRawCTD && !isBEACON && !isBIST ) ) {
  	  fout << "," << std::endl; //finish the previous block
      	  fout << "  \"" << "CTD_Raw" << "\": [" << std::endl;
	  if ( Doc_L0.contains(std::string{ "CTD_Raw" }) ) {
	    write_CTD( fout, "CTD_Raw" , Doc_L0, 1 );
	  } else {
	    std::vector<int> SndCTD(3,1); //Variables sent by CTD sensor; P,T,S
	    write_Config( fout, SndCTD, "CTD_Raw" );
	  }
	  fout << "  ]";
	}

	if ( Doc_L0.contains(std::string{ "CTD_Binned" }) || ( writeBinCTD && !isBEACON && !isBIST ) ) {
  	  fout << "," << std::endl; //finish the previous block
      	  fout << "  \"" << "CTD_Binned" << "\": [" << std::endl;
	  if ( Doc_L0.contains(std::string{ "CTD_Binned" }) ) {
	    write_CTD( fout, "CTD_Binned" , Doc_L0, 0 );
	  } else {
	    std::vector<int> SndCTD(3,1); //Variables sent by CTD sensor; P,T,S
	    write_Config( fout, SndCTD, "CTD_Binned" );
	  }
	  fout << "  ]";
	}

	if ( Doc_L0.contains(std::string{ "CTD_Drift" }) || ( SBck[5]==1 && writeDriftCTD && !isBEACON && !isBIST && !isDPLY ) ) {
  	  fout << "," << std::endl; //finish the previous block
      	  fout << "  \"" << "CTD_Drift" << "\": [" << std::endl;
	  if ( Doc_L0.contains(std::string{ "CTD_Drift" }) ) {
	    write_CTD( fout, "CTD_Drift" , Doc_L0 , 2 );
	  } else {
	    std::vector<int> SndCTD(3,1); //Variables sent by CTD sensor; P,T,S
	    write_Config( fout, SndCTD, "CTD_Drift" );
	  }
	  fout << "  ]";
	}


	fout << std::endl;
        fout << "}" << std::endl;
        return;
}  // end of rewrite_json

/////////////////////////////////////////////////////////////////////////////////////////////
// 
int determine_resolution (int digits) {
    int res;
    if (digits > 1000) {
	  res=4;
    } else if ( digits > 100 ) {
	  res=3;
    } else if ( digits > 10 ) {
	  res=2;
    } else {
	  res=1;
    }
    return res;
}
/////////////////////////////////////////////////////////////////////////////////////////////
void write_BIST (std::ofstream& fout, std::string senname, std::string varname, const json &Doc_L0c, int wid, int res, int last) {
  	  if ( !Doc_L0c[senname][varname].is_null() ) {
            fout << "    \"" << varname << "\": " << decimal(Doc_L0c.at(senname)[varname],wid,res); 
          } else {
	    fout << "    \"" << varname <<  "\": -999";
	  }
	  if (!last) { fout << ","; }
	  fout << std::endl;
	return; 
}
/////////////////////////////////////////////////////////////////////////////////////////////
void write_CTD (std::ofstream& fout, std::string varname, const json &Doc_L0c, int chan) {
        float ShalCutoff = -5.0; //define non reachable default for other channels
	if ( chan == 0 ) { //identify shallow cutoff of Binned CTD data
	  if ( RunMission.contains("BLOK") && RunMission.contains("CTDofZ") ) {
            float cB = RunMission["BLOK"]["config_value"];
            float cZ = RunMission["CTDofZ"]["config_value"];
            ShalCutoff = cZ - ( cB / 2. );
	  }
//          std::cout << "ShalCutoff " << ShalCutoff << std::endl;
	}
        std::vector<int> ChPack(3,0); //flag to determine if Gain/Offset changed for CTD, if yes than correct
	std::vector<int> oO(3,0),oG(3,0),nO(3,0),nG(3,0);
	if ( RunMission.contains("Pgain") ) {  // Silly check to see if the runMission is contains the Gain info
            ChPack[0]=1;
            if ( Doc_L0c.contains("ARGO_Mission") ) {  // use ARGO_Mission as utilized S2_Decoder gain/offset
              oO[0] = Doc_L0c["ARGO_Mission"]["ctd_pres"]["offset"];
	      oG[0] = Doc_L0c["ARGO_Mission"]["ctd_pres"]["gain"];
	    } else {
	      std::cout << "Found NO Argo_Mission in L0:  Use config.json" << std::endl;
              oO[0] = config_template["prof"]["CTD_Binned"]["PRES"]["offset"];
	      oG[0] = config_template["prof"]["CTD_Binned"]["PRES"]["gain"];
	    }
	    nG[0] = RunMission["Pgain"]["config_value"];
	    nO[0] = RunMission["Poff"]["config_value"];
        }
	if ( RunMission.contains("Tgain") ) {  // Silly check to see if the runMission is contains the Gain info
            ChPack[1]=1;
            if ( Doc_L0c.contains("ARGO_Mission") ) {  // use ARGO_Mission as utilized S2_Decoder gain/offset
              oO[1] = Doc_L0c["ARGO_Mission"]["ctd_temp"]["offset"];
	      oG[1] = Doc_L0c["ARGO_Mission"]["ctd_temp"]["gain"];
	    } else {
              oO[1] = config_template["prof"]["CTD_Binned"]["TEMP"]["offset"];
	      oG[1] = config_template["prof"]["CTD_Binned"]["TEMP"]["gain"];
	    }
	    nG[1] = RunMission["Tgain"]["config_value"];
	    nO[1] = RunMission["Toff"]["config_value"];
        }
	if ( RunMission.contains("Sgain") ) {  // Silly check to see if the runMission is contains the Gain info
            ChPack[2]=1;
            if ( Doc_L0c.contains("ARGO_Mission") ) {  // use ARGO_Mission as utilized S2_Decoder gain/offset
              oO[2] = Doc_L0c["ARGO_Mission"]["ctd_psal"]["offset"];
	      oG[2] = Doc_L0c["ARGO_Mission"]["ctd_psal"]["gain"];
	    } else {
              oO[2] = config_template["prof"]["CTD_Binned"]["PSAL"]["offset"];
	      oG[2] = config_template["prof"]["CTD_Binned"]["PSAL"]["gain"];
	    }
	    nG[2] = RunMission["Sgain"]["config_value"];
	    nO[2] = RunMission["Soff"]["config_value"];
        }
	if ( Doc_L0c.contains(varname) ) {
	  const auto& matrix = Doc_L0c.at(varname);
          size_t skip = 0;
          if ( varname == "CTD_Binned" ) {
            for (const auto& row : matrix) {
              if ( row.contains("PRES") ) { 
                if (row["PRES"] <= ShalCutoff && row["PRES"] > 0 ) { //escape if this data is shallower than ShalCutoff but > 0 which would indicates complete bin
                         std::cout << "Not reporting Binned CTD data at PRES = " << row["PRES"] << std::endl;
                         skip++;
                }
              }
            }
          }

	  size_t i = 0;
          std::vector<int> PTS(3,0); //can be outside loop as L0 should have fillvalue in partial missing
	  for (const auto& row : matrix) {
             if ( i == 0 ) { //first iteration see if missing variables
               for (auto const& [keyr,valr] : row.items()) {
      		     if ( keyr == "PRES" ) {
			     PTS[0]=1;
		     } else if ( keyr == "TEMP" ) {
			     PTS[1]=1;
		     } else if ( keyr == "PSAL" ) {
			     PTS[2]=1;
		     }
	       }
	     }
             std::vector<int> tPTS = PTS;
	     if ( varname == "CTD_Binned" && row.contains("PRES") ) { //escape if this data is shallower than ShalCutoff
	 	if (row["PRES"] <= ShalCutoff && row["PRES"] > 0 ) { //escape if this data is shallower than ShalCutoff but not negative (indictes full bin)
			 //std::cout << "Not transferring Binned CTD data at PRES = " << row["PRES"] << std::endl;
			 continue;
	        }
	     }
             i++; 
             fout << "    { ";
             for (auto const& [keyr,valr] : row.items()) {
      	       if ( keyr == "PRES" | tPTS[0] == 0 ) {
                 if ( keyr == "PRES" ) {
                   if ( ( ChPack[0]==1 ) && ( row.at("PRES") > -99 ) ) { // PRES Gain/Offset has been changed via 2-way
		     float present = row.at("PRES");
                     float fixed = (((( present + oO[0] ) * oG[0] ) / nG[0] ) - nO[0] ); 
                     fout << "\"PRES\": " << decimal(fixed,7,resp) << ", ";
		   } else {
                     fout << "\"PRES\": " << decimal(row["PRES"],7,resp) << ", ";
		   }
		 } else {
                   fout << "\"PRES\": " << decimal(-999,7,resp) << ", ";
		 }
	         tPTS[0]=2;
	       } 
      	       if ( keyr == "TEMP" | ( tPTS[1] == 0 && tPTS[0] == 2 )) { //only enters if PRES has been addressed (not really necessary)
                 if ( keyr == "TEMP" ) {
                   if ( ( ChPack[1]==1 ) && ( row.at("TEMP") > -99 ) ) { // TEMP Gain/Offset has been changed via 2-way
		     float present = row.at("TEMP");
                     float fixed = (((( present + oO[1] ) * oG[1] ) / nG[1] ) - nO[1] ); 
                     fout << "\"TEMP\": " << decimal(fixed,6,rest) << ", ";
		   } else {
                     fout << "\"TEMP\": " <<  decimal(row["TEMP"],6,rest) << ", ";
		   }
		 } else {
                   fout << "\"TEMP\": " << decimal(-999,6,rest) << ", ";
		 }
	         tPTS[1]=2;
	       }
      	       if ( keyr == "PSAL" | ( tPTS[2] == 0 && tPTS[1] == 2 )) { //only enters if TEMP has been addressed
                 if ( keyr == "PSAL" ) {
                   if ( ( ChPack[2]==1 ) && ( row.at("PSAL") > -99 ) ) { // PSAL Gain/Offset has been changed via 2-way
		     float present = row.at("PSAL");
                     float fixed = (((( present + oO[2] ) * oG[2] ) / nG[2] ) - nO[2] );
                     fout << "\"PSAL\": " << decimal(fixed,7,ress);
		   } else {
                     fout << "\"PSAL\": " <<  decimal(row["PSAL"],7,ress);
		   }
		 } else {
                   fout << "\"PSAL\": " << decimal(-999,7,ress);
		 }
	         tPTS[2]=2;
	       }
	     }
//	     std::cout << i << " " << matrix.size() << " " << skip << std::endl;
     	     if ( i == matrix.size() - skip ) {
               fout <<  " }" << std::endl;
	     } else {
               fout <<  " }," << std::endl;
	     }
          }
        }
	return; 
}
/////////////////////////////////////////////////////////////////////////////////////////////
void write_Config(std::ofstream& fout, std::vector<int> Snd, std::string varname) {
#include <algorithm>
#include <iterator>
	int count = std::count(std::begin(Snd),std::end(Snd),1);
	//std::cout << "# in Config " << count << std::endl;

	fout << "    { ";
	size_t j = 0;
	size_t k = 0;
        for (const auto& loop : config_template["prof"][varname].items()) { // loop over config.json
	       j++;
	       if ( Snd[j-1] >= 1 ) { //should have been sent
                 k++;
                 fout << "\"" << loop.key() << "\": " << decimal(-999,loop.value()["col_width"],loop.value()["col_precision"]);
                 //std::cout << "check " << varname << config_template["prof"][varname].size() << " " << count << std::endl;
		 if ( ( j != config_template["prof"][varname].size() ) && ( k != count ) ) {
                   fout << ", ";
	         }
	       }
	}
        fout <<  " }" << std::endl;
}
/////////////////////////////////////////////////////////////////////////////////////////////
void copyLine ( std::ofstream& fout, std::string item, std::string endc, const json &Doc_L0c ) {
	if ( Doc_L0c.contains(item) ) {
          if ( endc != "" ) {
		  fout << endc << std::endl;
	  }
          fout << "  \"" << item << "\": " << Doc_L0c.at(item);
	}
	return;
}

