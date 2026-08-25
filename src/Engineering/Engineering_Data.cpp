#include "Engineering_Data.h"
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
//#include <format>
#include "../output/write_log.h"

#include "../json/json.hpp"
using json = nlohmann::ordered_json;
extern json config;

extern std::string S2_PATH;


#define PARAMETER_VALUE_WIDTH 8
#define PARAMETER_NAME_WIDTH  8
#define PARAMETER_UNIT_WIDTH  6

// Mission Parameters are sent along with human readable variable names
void mission::parse(std::vector<uint8_t> d) {
	//int ptype = d[0] & 0xF;
	//int pseg  = d[4];
        //log( std::format("Packet[{:2X}] Mission ({:d})",ptype,pseg) );

	std::ifstream f(S2_PATH + "/config/mission.json",std::ios::in);
	json params = json::parse(f);

	std::string pname,tval,val,unit="",desc="";
	int n = 3; //int n = 6;

	// Read in parameters one at a time
	while ( d[n] != ';' ) {
		pname.clear();
		unit.clear();
		desc.clear();
		// Read in parameter name until '=' sign. Note that parameter name legth varies from packet to packet!
		while (d[n] != '=') {
	       	if (d[n] != 32 && d[n] != 10 && d[n] != 124) // ignore spaces, CR, '|'
				pname.push_back(d[n]);
			n++;
		}
		n++; // skip next character '='
                val = "";
        // Read in parameter value until '|' character. Ignore space characters.
		while (d[n] != '|') {
                  if (d[n] != 32)
	            val.push_back(d[n]);
                  n++;
		}
                while (d[n] == '|' || d[n] == 0 || d[n] == 10) {
			//std::cout << "ignoring '" << std::hex << std::setw(2) << std::setfill('0') << int(d[n]) << "." << std::endl << std::dec << std::setfill(' ');
	          n++; // skip next character '|'
		}

		// Match mission parameter to known parameters as defined in config/mission.json
		if ( params.contains(pname) ) {
			unit = params[pname]["units"];
			desc = params[pname]["description"];
                        tval = val;
                        if (params[pname].contains("scale")) {
                           double v = std::stod(val) / int(params[pname]["scale"]);
                           std::stringstream ss;
                           ss << std::fixed << std::setprecision( params[pname]["prec"] ) << v;
                           val = ss.str();
                        }
		}
		else {
            //log( std::format("* Unknown mission parameter: {}, {}",pname,val) );
		}

		list.push_back({pname,unit,desc,val,tval});
    }
}


// Parses Engineering Diagnostic-Dive [0xe0], Profile-Mode [0xe2] and Engineering Beacon [0xe5]
// Requires a separate pfile.json that describes variable names, order, types, descriptions
// Only parameters sent. Use pfile.json to interpret parameter data
void Engineering_Data::parse_pfile(std::vector<uint8_t> d,std::string pfile) {

	int ptype = d[0] & 0xF;
	int EngVer  = d[3];
	//std::cout << "Engineering_Data " << ptype << EngVer << std::endl;
        if ( ptype == 0 | ptype == 2 ) { //if Engineering 0xE0 or 0xE2 change the version :: JG addition
		size_t pos = pfile.rfind('.');
                pfile = pfile.substr(0,pos) + "_" + std::to_string(EngVer) + ".json";
        }
	//std::cout << "Config " << pfile << std::endl;

    	std::ifstream f(pfile);
        json params = json::parse(f);

        int dval;
	int n = 4; //int n = 6;
	double val;
	int prec;
	std::stringstream unit,vstr,pnamestr;

	pnamestr << std::setw(PARAMETER_NAME_WIDTH) << "\"Eng_ver\"";
	vstr << std::setw(PARAMETER_VALUE_WIDTH) << (uint16_t)d[3];
	int vs = d[3];

	unit << std::setw(PARAMETER_UNIT_WIDTH) << "\"\"";
	list.push_back({pnamestr.str(),unit.str(),"Engineering Packet software version",vstr.str()});
	pnamestr.str("");
	vstr.str("");
	unit.str("");

	for(auto &[pname,patts] : params.items()) {
		prec = 0;
		unit.str("");
		unit << "\"";
		//std::cout << "Eng Parsing: " << pname << std::endl;
		if (patts["type"] == "U8") {
		          if (patts.contains("mask")) {
			    dval=d[n];
                            std::string hexbits = patts["mask"];
                            int bits = std::stoi(hexbits,nullptr,16);
                            bool lsbbits = (bits & 1);
			    if ( lsbbits ) {
			      val = ( ( dval & bits ) );
                              n++;
			    } else {
			      int bitshift = log2( bits & (-1)*bits );
			      val = ( ( dval & bits ) >> bitshift );
			    }
			  } else {
			    val = d[n];
			    n++;
		          }
		}
		else if (patts["type"] == "U16") {
		          if (patts.contains("mask")) {
			    dval=(d[n]<<8) + d[n+1];
                            std::string hexbits = patts["mask"];
                            int bits = std::stoi(hexbits,nullptr,16);
                            bool lsbbits = (bits & 1);
			    if ( lsbbits ) {
			      val = ( ( dval & bits ) );
                              n+=2;
			    } else {
			      int bitshift = log2( bits & (-1)*bits );
			      val = ( ( dval & bits ) >> bitshift );
			    }
			  } else {
			    val = (d[n]<<8) + d[n+1];
			    n+=2;
		          }
		}
		else if (patts["type"] == "I16") {
			val = (d[n]<<8) + d[n+1];
			if (val > 32767) // 2025/09/10 BG convert uint16 to int16
				val = val - 65536;
			n+=2;
		}
		else if (patts["type"] == "U24") {
			val = (d[n]<<16) + (d[n+1]<<8) + d[n+2];
			n+=3;
		}

		else {
			std::cout << "unknown parameter" << std::endl;
			val = (d[n]<<8) + d[n+1];
			n+=2;
		}
		if (patts.contains("scale")) {
			if (patts["scale"] == "pres") {
				val /= double(config["prof"]["CTD_Binned"]["PRES"]["gain"]);
			        prec = int(config["prof"]["CTD_Binned"]["PRES"]["col_precision"]);
			} else if (patts["scale"] == "temp") {
				val /= double(config["prof"]["CTD_Binned"]["TEMP"]["gain"]);
			        prec = int(config["prof"]["CTD_Binned"]["TEMP"]["col_precision"]);
			} else if (patts["scale"] == "psal") {
				val /= double(config["prof"]["CTD_Binned"]["PSAL"]["gain"]);
			        prec = int(config["prof"]["CTD_Binned"]["PSAL"]["col_precision"]);
			} else {
				val *= (float)patts["scale"];
			        prec = (int)patts["prec"]; // precision for output (list files and json); every parameter with value scale needs "prec" defined
			}
		}
		if (patts.contains("offset")) {
		  if (patts["offset"] == "pres")
		  	val -= double(config["prof"]["CTD_Binned"]["PRES"]["offset"]);
		  else if (patts["offset"] == "temp")
			val -= double(config["prof"]["CTD_Binned"]["TEMP"]["offset"]);
		  else if (patts["offset"] == "psal")
			val -= double(config["prof"]["CTD_Binned"]["PSAL"]["offset"]);
                  else
		        val -= (float)patts["offset"];
		}
		if (patts.contains("units")) {
			unit << (std::string)patts["units"] << "\"";
		} else {
			//unit << "1\"";
			unit << "\"";
		}

		vstr.str("");
		pnamestr.str("");
		pnamestr << "\"" << pname << "\"";
		vstr << std::setw(PARAMETER_VALUE_WIDTH) << std::fixed << std::setprecision(prec) << val;
		list.push_back({pnamestr.str(),unit.str(),(std::string)patts["description"],vstr.str()});
               
		if ( ( ptype == 0 ) && ( pname == "nQueued" ) ) { //in S2 there is a gap
			n+=6;
		}

	}
}
