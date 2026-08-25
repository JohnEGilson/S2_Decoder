#include "hexfile.h"
#include <sstream>
#include <algorithm>
#include "../output/write_log.h"
#include <filesystem>
//#include <format>

#include "../json/json.hpp"
using json = nlohmann::ordered_json;
extern json config;

extern std::string S2_PATH;

hexfile::hexfile( std::string f ) {
	upload_command = ""; // initialize upload command

	// Open .hex file
	filepath = f;
        filename = f.substr(f.find_last_of("/\\") + 1); // file basename
	//std::cout << "In hexfile " << filename << std::endl;

	fin.open(filepath,std::ifstream::binary);
	if ( !fin.good() ) {
		log(std::string(" -- Error; unable to open: ") + filename);
		return;
	}

	//log( std::string("ProcessingH ") + filename );
	int cnt = 0;
	message m;
	while (fin >> m) {
		if (cnt++ > 50) {
			log( std::string(" -- Error reading ") + filename + " [infinite loop]; breaking");
			break;
		}
		sn = m.SN;
		cycle = m.cycle;

		// Ignore duplicate messages (identified using PID)
		if ( messages.count(m.PID) ) {
			log(std::string(" -- warning, ignoring duplicate PID: ") + std::to_string(m.PID) );
			continue;
		}
		//std::cout << "Next step "<< sn << " " << cycle << std::endl;
		for( const auto &p : m.packets ) {
			packets.push_back(p);
		}
		messages[m.PID] = m;
	}
}

std::ostream & operator << ( std::ostream &os, hexfile &h) {
	int packet_count = 0;
	int packet_bytes = 0;
	int total_bytes = 0;
	char profile_key[7];
	string desc;

	std::map<std::string,int> packets;
	std::map<std::string,int> pcnt;


	os << "#        SN Cycle Size PID                Date Sensor_IDs" << std::endl;
	for (auto & [PID,m] : h.messages) {
		os << "SBD" << " " << std::setw(4) << h.sn << " " << std::setw(5) << h.cycle << " ";
		os << std::setw(4) << m.size << " " << std::setw(3) << PID << " ";
		os << std::setfill('0') << std::setw(4) << m.yy << "/" << std::setw(2) << m.mm << "/";
		os << std::setw(2) << m.dd << " " << std::setw(2) << m.HH << ":" << std::setw(2) << m.MM << ":" << std::setw(2) << m.SS << " ";
		os << std::setfill(' ');
		total_bytes += m.size;

		for (auto p : m.packets) {
			os << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)p.header.sensorID << ",";
			packet_count++;
			packet_bytes += p.size;

			int sensor_id = 0; //int sensor_id = p.data[0];
			int segment = ( p.data[0] & 0x0F ) % 8; //int segment = p.data[5] & 0x0F; // sub-segment index
			int data_id = p.data[0] - segment; //int data_id = p.data[3] & 0x0F;
			int pro = 0; //int pro = p.data[4];

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

		os << std::dec << std::setfill(' ') << std::endl;
	}
	os << std::endl;
	os << "# Raw packet information" << std::endl;
	os << "# Packets  Packet_bytes Total_bytes" << std::endl;
	os << "P0 " << std::setw(7) << packet_count << " " << std::setw(12) << packet_bytes << " " << std::setw(11) << total_bytes << std::endl;
	os << "# Sensor_ID bytes packets Description" << std::endl;
	for (auto & [key,value] : packets) {
		if (config["packets"].count(key))
			desc = std::string(config["packets"][key]["profile"]) + " " + std::string(config["packets"][key]["name"]);
		else
			desc = "unknown";
		os << std::setw(11) << key << " " << std::setw(5) << value << " " << std::setw(7) << pcnt[key] << " " << desc << std::endl;
	}

	return os;
}


void hexfile::print() {
	for (auto & [PID,m] : messages) {
		//std::cout << "Message PID:" << PID << " size: " << m.size << std::endl;
		for (auto p : m.packets) {
			for( int h = 0; h < p.data.size(); h++) {
				if (h > 50) {
					break;
				}
				std::cout << std::hex << std::setw(2) << std::setfill('0') << (uint16_t)p.data[h] << " ";
			}
			std::cout << std::dec << std::endl;
		}
	}
	std::cout << std::setfill(' ');
}

void hexfile::Decode() {
	char key[7];
        packets.sort(); // sort by priority

        std::vector<uint8_t> catdata; //vector for storing all CONFIG dump data
	for (auto p : packets) {
		        int sdata = p.data[0];
			//std::cout << "original header.sensorID = " << sdata << std::endl;
			int sjg;
                        sjg = ( p.data[0] & 0xF0 ); //pull out the data_id
			//std::cout << "header.sensorID = " << sjg << std::endl;
			if ( (( p.data[0] & 0xF0 ) >> 4 ) >= 9 && (( p.data[0] & 0xF0 ) >> 4) <= 11 ) { 
				//std::cout << "multiuse " << std::endl;
			        int sjg2 = ( p.data[0] & 0x0F ) % 8; //JG
			        sjg = p.data[0] - sjg2; //JG
		                //std::cout << "Processing multiuse key " << sjg << " " << sjg2 << std::endl;
			}
	                sprintf(key,"%02X",sjg); // JG
		//std::cout << "Processing Packet[" << key << "]" << std::endl;

     	    if (p.header.sensorID < 9) { // GPS JG
		    //std::cout << "Found GPS" << std::endl;
				gps.push_back(GPS(p.data));
	    }
            else if (p.header.sensorID == 240) { // Argo Mission JG
		    //std::cout << "Found Argo Mission" << std::endl;
				argo.Decode(p.data);
            }
            else if (p.header.sensorID == 222) { // upload command dump id="DE": Must be listed before so it enters prior to 0xF0 bit match p.header.sensorID == 208
					for(int c = 3; c < p.header.nDat2 - 1; c++) {
						if ( 32 <= p.data[c] && p.data[c] <= 126 )
							upload_command += p.data[c];
						else
							upload_command += '*';
					}
            }
            else if ( ( p.data[0] & 0xF0 ) == 208) { // parameter dump id="D0 etc" : Must be listed after p.header.sensorID == 222
		    //S2 CONFIG dump does not necessarily divide packets by full-width config items.  Thus packets (D0, D1, D2, etc) cannot run independently.
		    //Must priority sort the D[0-9] family messages; and then cat together prior to parsing.  Sorting messages also fixes out of order CONFIG in json;
		    //Once the data aligns with full CONFIG, the stored data is cleared so it isn't parsed multipe times.
		    size_t lengthc = std::size(p.data);
		    //if the original packet is incomplete (!end with "|;" ) or previous stored data exists, enter JG stuff
		    if ( !catdata.empty() | ( p.data[lengthc-2] != 124 | p.data[lengthc-1] != 59 ) ) { 
		      if ( catdata.empty() ) {
                        catdata = p.data;
		      } else {
		        catdata.reserve(std::size(catdata) + p.data.size() - 4 );
		        catdata.insert(catdata.end() - 1, p.data.begin() + 3, p.data.end() - 1);
		      }
		      lengthc = std::size(catdata);
		      if ( catdata[lengthc-2] == 124 && catdata[lengthc-1] == 59 ) {  //if catdata ends with "|;" then full and can run Ben's code
     		        p.data = catdata;  
		        miss.parse(p.data);
		        catdata.clear(); //clear catdata so that we start fresh
                      }
		    } else {
		      miss.parse(p.data);
	            }
	    }
	    else if ( ( p.data[0] & 0xF0 ) == 64) { // Fall
					fall.Decode(p.data);
	    }
	    else if ( ( p.data[0] & 0xF0 ) == 80) { // Rise
					rise.Decode(p.data);
	    }
	    else if ( ( p.data[0] & 0xF0 ) == 96) { // Pump
					pump.Decode(p.data);
	    }
	    else if (p.header.sensorID == 224 ) { // diagnostic engineering data
					eng_data.parse_pfile(p.data,S2_PATH + "/config/diagnostic.json");
					//log("Packet[10] Diagnostic");
	    }
	    else if (p.header.sensorID == 226 ) { // diagnostic engineering data
					eng_data.parse_pfile(p.data,S2_PATH + "/config/Engineering_Data.json");
					//log("Packet[10] Diagnostic");
	    }
	    else if (p.header.sensorID == 227 ) { // BIT Beacon
                    beacon.parse(p.data);
            }
	    else if (p.header.sensorID == 229 || p.header.sensorID == 230 ) { // BIT OK [13] and FAIL [14]
		bit.parse(p.data);
		std::cout << "back from BIT from hexfile " << std::endl;
            }
			else if ( config["packets"].contains(key) ) {
				string pname = config["packets"][key]["profile"];
				string vname = config["packets"][key]["name"];
				prof[pname].insert(p.data);
			}
	}

	// Convert raw counts to SI units
	for (auto &[pname,vdict] : config["prof"].items()) {
		prof[pname].Conv_Cnts2SI();
		//prof[pname].print_stats();
	}
}

void hexfile::archive() {
	// check if float subdirectories exist. If not, create empty skeleton
	std::ostringstream ss;
        ss.str("");
        ss.clear();
        ss << std::string(config["directories"]["output"]) << "/" << std::setw(4) << std::setfill('0') << std::to_string(sn);
        std::string floatdir = ss.str();	
	if (!std::filesystem::exists(floatdir)) {
		std::filesystem::create_directory(floatdir);
		//log(std::format("+ Creating float {} subdirectory",sn));
	}
	if (!std::filesystem::exists(floatdir+"/hex")) {
		std::filesystem::create_directory(floatdir + "/hex");
		//log(std::format("+ Creating float {} hex subdirectory",sn));
	}
	std::filesystem::rename(filepath,floatdir+"/hex/"+filename); // move processed .hex file from incoming to float hex subdirectory
}
