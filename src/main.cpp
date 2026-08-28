#include "hexfile/hexfile.h"
#include "output/write_log.h"
#include <cstdlib>
#include <string>
#include <fstream>
//#include <format>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <chrono>
#include <thread>

using namespace boost::filesystem;

#include "json/json.hpp"
using json = nlohmann::ordered_json;
json config;

std::string S2_PATH;
int MAJOR_VERSION = 1;
int MINOR_VERSION = 0;

int main( int argc, char **argv) {

  // Read decoder config file
  const char *s2_path = getenv("S2_PATH");
  if (s2_path == NULL) {
	// If S2_PATH env variable is not defined, exit. Write to stdout becuase log() requires S2_PATH
    std::cout << "Unable to load S2_PATH environment variable; exiting." << std::endl;
    return 0;
  }
  S2_PATH = s2_path;
  std::ifstream f(S2_PATH + "/config/config.json");
  if (!f) {
	// If config.json is not found, exit. Write to stdout because log() requires S2_PATH
    std::cout << "Unable to open config.json; exiting." << std::endl;
	return 0;
  }
  config = json::parse(f);

  //std::cout << argc << std::endl;

  //JG addition use config in directory if present
  if (argc >= 2) {
    int sn = std::stoi(argv[1]);
    //std::cout << sn << std::endl;
    std::ostringstream ssjg;
    ssjg.str("");
    ssjg.clear();
    ssjg << S2_PATH << "/data/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/modified_" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "_config.json";
    std::string config_filename = ssjg.str();
    std::ifstream f2(config_filename);
    if (f2) {
      std::cout << "Float specific config.json " << config_filename << std::endl;
      config = json::parse(f2);
    }
    ssjg.str("");
    ssjg.clear();
  }

  //log("Starting S2-SOLO");

  std::vector<std::filesystem::path> hexfiles;
  std::string incoming_dir = S2_PATH + "/incoming";

  //std::cout << "argc " << argc << std::endl;

  // Command-line options
  if (argc == 3) {
    std::string filter_filename, filter_filepath;
    int sn = std::stoi(argv[1]);
    int cycle = std::stoi(argv[2]);

    //std::cout << "cycle " << cycle << std::endl;
    //std::cout << "Pausing" << std::endl;
    //std::this_thread::sleep_for(std::chrono::seconds(10));
    //filter_filename = std::format("{:04d}_{:03d}.hex",sn,cycle); // S2_Decoder [sn] [cycle]
    //filter_filepath = std::format("{}/data/{:d}/hex/{}",S2_PATH,sn,filter_filename);
    std::ostringstream ss;
    ss.str("");
    ss.clear();
    if (cycle<0) {
      ss << std::setw(4) << std::setfill('0') << std::to_string(sn) << "_-001.hex"; // S2_Decoder [sn] [cycle]
    } else {
      ss << std::setw(4) << std::setfill('0') << std::to_string(sn) << "_" << std::setw(4) << std::setfill('0') << std::to_string(cycle) << ".hex"; // S2_Decoder [sn] [cycle]
    }
    filter_filename = ss.str();
    ss.str("");
    ss.clear();
    ss << S2_PATH << "/data/" << std::setw(4) << std::setfill('0') << std::to_string(sn) << "/hex/" << filter_filename;
    filter_filepath = ss.str();
    ss.str("");
    ss.clear();
    if (std::filesystem::exists(filter_filepath)) {
        //log( std::format("* Processing {} using command-line filter",filter_filename));
        std::filesystem::copy(filter_filepath,incoming_dir); // copy hexfile from float subdirectory to incoming
    }
    else {
        //log( std::format("* warning - unable to find {}",filter_filename) );
    }
  }

  // read and process each hex file in hex directory
  std::copy(std::filesystem::directory_iterator(incoming_dir),std::filesystem::directory_iterator(),std::back_inserter(hexfiles));
  std::sort(hexfiles.begin(),hexfiles.end());
  for (const std::filesystem::path &filepath : hexfiles) {
    //std::cout << "Processing main " << std::endl;
    //std::cout << filepath.string() << std::endl;
    hexfile h(filepath.string());
    //std::cout << "Pre Decode " << std::endl;
    h.Decode();
    //std::cout << "Post Decode " << std::endl;
	h.archive();
    //std::cout << "Post archive " << std::endl;
   	h.write_JSON();
    //std::cout << "Post JSON " << std::endl;

//	if (h.cycle == -1 && config.contains("email") ) {
//     string cmd = std::format("python3 {} 'S2 #{:d} startup' '{}' '{}'", std::string(config["email"]["python_script"]), h.sn, h.jsonpath, std::string(config["email"]["alert_recipients"]) );
//      system(cmd.c_str());
//      log( std::format("* Send startup message SUBJECT: 'S2 #{:d} startup' to {}",h.sn,std::string(config["email"]["alert_recipients"])));
//	}
//	if (h.cycle == 0 && config.contains("email") ) {
//      string cmd = std::format("python3 {} 'S2 #{:d} cycle 0' '{}' '{}'", std::string(config["email"]["python_script"]), h.sn, h.jsonpath, std::string(config["email"]["alert_recipients"]) );
//      system(cmd.c_str());
//      log( std::format("* Send startup message SUBJECT: 'S2 #{:d} cycle 0' to {}",h.sn,std::string(config["email"]["alert_recipients"])));
//	}

  }

  //log("Finished.");
  return 0;
}
