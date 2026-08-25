#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <map>
#include <filesystem>
#include <unordered_set>
#include <numeric>
#include <cmath>
#include <set>
#include <chrono>
#include <ctime>
#include <fstream>
#include <stdexcept>
#include "../json/json.hpp"  // Include the Nlohmann JSON library
//#include "./src/json/json.hpp"  // Include the Nlohmann JSON library

//global variables
using json = nlohmann::ordered_json;

namespace fs = std::filesystem;

// container to hold all requested fields
struct SbdRecord {
    std::string raw_line;
    int cycle;
    int momsn;
    std::tm parsed_gmt;
    long long seconds_since_1950;
    double latitude;
    double longitude;
    int error_iridium;
    std::string formatted_GMT; //2022-11-18T19:56:00Z
};

// container to hold weighted fields
struct WeightedResult {
    double latitude;
    double longitude;
    double error;
    double juld;
    int subsamp;
};

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

// Function : Fetches the file modification time and populates a GMT tm structure (Gemini)
bool get_file_mtime_gmt(const fs::path& file_path, std::tm& out_gmt) {
    if (!fs::exists(file_path)) {
        return false; 
    }

    auto ftime = fs::last_write_time(file_path);
    
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    
    std::time_t unix_seconds = std::chrono::system_clock::to_time_t(sctp);
    
    std::tm* gmt_ptr = std::gmtime(&unix_seconds);
    if (!gmt_ptr) {
        return false;
    }

    out_gmt = *gmt_ptr; 
    return true;
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

// Custom comparator for natural sorting
bool natural_sort_compare(const std::string& a, const std::string& b) {
    size_t i = 0, j = 0;
    while (i < a.length() && j < b.length()) {
        if (std::isdigit(a[i]) && std::isdigit(b[j])) {
            // Both are digits, parse as numbers
            int num_a = 0, num_b = 0;
            while (i < a.length() && std::isdigit(a[i])) {
                num_a = num_a * 10 + (a[i++] - '0');
            }
            while (j < b.length() && std::isdigit(b[j])) {
                num_b = num_b * 10 + (b[j++] - '0');
            }
            if (num_a != num_b) return num_a < num_b;
        } else {
            // Character comparison
            if (a[i] != b[j]) return a[i] < b[j];
            i++;
            j++;
        }
    }
    return a.length() < b.length();
}

// Helper to check if a line starts with a specific prefix (ignoring leading whitespace if necessary)
bool starts_with_sbd(const std::string& line) {
    // If you have strict formatting with zero leading spaces:
    return (line.rfind("SBD", 0) == 0);
}

// Parses file and returns a sorted vector of records with SBD blocks
std::vector<SbdRecord> parse_sbd_file_sorted(const std::string& file_path) {
    // Intermediate storage container that self-sorts by the key (long long seconds)
    std::multimap<long long, SbdRecord> sorted_map;    
    std::ifstream infile(file_path);

    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << file_path << "\n";
        return std::vector<SbdRecord>();
    }

    std::string line;
    while (std::getline(infile, line)) {
        // Step 1: Strict prefix check—only handle lines starting with "SBD"
        if (!starts_with_sbd(line)) {
            continue;
        }

        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> tokens;
	// Step 2: Split the line into tokens via commas
        while (std::getline(ss, cell, ',')) {
            tokens.push_back(cell);
        }

        // Target index layout maps to these columns: Index 5:Momsn
        // Index 8: Year   | Index 9: Month | Index 10: Day
        // Index 11: Hour  | Index 12: Min   | Index 13: Sec
        // Index 14: Lat   | Index 15: Long  | Index 16: Err
        if (tokens.size() >= 16) {
            try {
                std::tm tm_struct = {};

                // Step 3: Parse dates to integers with standard std::tm normalization
                tm_struct.tm_year = std::stoi(tokens[8]) - 1900;
                tm_struct.tm_mon  = std::stoi(tokens[9]) - 1;
                tm_struct.tm_mday = std::stoi(tokens[10]);
                tm_struct.tm_hour = std::stoi(tokens[11]);
                tm_struct.tm_min  = std::stoi(tokens[12]);
                tm_struct.tm_sec  = std::stoi(tokens[13]);
                tm_struct.tm_isdst = 0; // Standard GMT enforcement

                // Step 4: Parse coordinate fields as decimals
                // std::stod natively handles string whitespace trimming
                double lat  = std::stod(tokens[14]);
                double lon = std::stod(tokens[15]);
                int error = std::stoi(tokens[16]);
                int mom = std::stoi(tokens[5]);
                int cyc = std::stoi(tokens[2]);

                // Step 4.5:  Rudimentary error checking
		if ( lon < -360 ) { lon += 360; } //allow 1 shift
		if ( lon >= 360 ) { lon -= 360; } //allow 1 shift
			//std::cout << "Hex Position " << lon << " " << lat << " " << error << std::endl;
                if ( error < 1 || error > 1000 || lat < -90 || lat > 90 || lon < -180 || lon >= 360 ) { // 1km is bottom of valid error
			//std::cout << "Flagged " << lon << " " << lat << " " << error << std::endl;
			lat = 99999;
			lon = 99999;
			error = 99999; 
		}

		// Step 5: Convert the std::tm structure to an ISO 8601 string
                std::ostringstream time_oss;
                time_oss << std::put_time(&tm_struct, "%Y-%m-%dT%H:%M:%SZ");

                // Step 6: Bundle and append the complete record structure
                SbdRecord record;
                record.raw_line = line;
                record.cycle = cyc;
                record.momsn = mom;
                record.parsed_gmt = tm_struct;
                record.seconds_since_1950 = get_seconds_since_1950(tm_struct);
                record.latitude = lat;
                record.longitude = lon;
                record.error_iridium = error;
		record.formatted_GMT = time_oss.str();

		// Insert into the multimap.
                // It automatically places this item into the mathematically correct sort order.
                sorted_map.insert({record.seconds_since_1950, record});
            }
            catch (const std::exception& e) {
                // Ignore any malformed numeric data inputs cleanly
                continue;
            }
        }
    }

    //Convert the sorted map back into a standard vector to maintain API compatibility
    //Here, The sorting only helps within a hex file (so a bit redundant with addition of final sort)
    std::vector<SbdRecord> results;
    results.reserve(sorted_map.size()); // Pre-allocate memory for speed
    for (const auto& [seconds, record] : sorted_map) {
        results.push_back(record);
    }
    
    return results;
}

// Write the Weighted Iridium Position
bool write_IrrPos(const std::string& output_path, const WeightedResult& record, bool is_Size0) {
    std::ofstream outfile(output_path, std::ios::app); // // Opens file in strict append mode
    if (!outfile.is_open()) return false;

    if ( !is_Size0 ) {
      outfile << "\n  \"sbd_weighted_iridium_position\": [\n "
              << "    { \"JULD\": " << std::setprecision(10) << record.juld/86400. << ", "
              << "\"LATITUDE\": " << std::setprecision(5) << record.latitude << ", "
              << "\"LONGITUDE\": " << std::setprecision(6) << record.longitude << ", ";
      if (std::round(record.error) == 99999 ) {
        outfile <<  "\"POSITION_ERROR_ESTIMATED\": " << record.error << ",";
        outfile  << "\n       \"POSITION_ERROR_COMMENT\": \"\" } ";
      } else {
        outfile <<  "\"POSITION_ERROR_ESTIMATED\": " << round(record.error*1000) << ",";
        outfile  << "\n       \"POSITION_ERROR_COMMENT\": " << "\"A subset of the available Iridium Positions were used that are deemed 'independent': " << record.subsamp <<"; Error was computed as the square root of the inverse sum of the weights, where each weight is the inverse square of the individual measurement error: Weights=1/(error^2)\"" << " } ";
      }
	outfile  << "\n  ]\n}";
    } else {
      outfile << "\n  \"sbd_weighted_iridium_position\": [\n "
              << "    { \"JULD\": 999999, "
      //        << "\"weighted_subset_count\": 0, "
              << "\"LATITUDE\": 99999, "
              << "\"LONGITUDE\": 99999, "
              << "\"POSITION_ERROR_ESTIMATED\": 99999, "
              << "\"POSITION_ERROR_COMMENT\": \"\"  } "
	      << "\n  ]\n}";
    }
    return true;
}

// CALL ONCE: Starts a brand new file and initializes the root JSON node structure
bool open_json_summary(const std::string& output_path) {
    std::ofstream outfile(output_path, std::ios::trunc); // Overwrites old files entirely
    if (!outfile.is_open()) return false;

    outfile << "{\n  \"sbd_iridium_position_summary\": [";
    return true;
}

//  CALL MULTIPLE TIMES: Appends an individual record subset to the array stream
// The 'is_first_item' parameter decides whether to prefix a comma delimiter row
bool append_sbd_to_json(const std::string& output_path, const SbdRecord& record, bool is_first_item) {
    std::ofstream outfile(output_path, std::ios::app); // Opens file in strict append mode
    if (!outfile.is_open()) return false;

    // Set fixed double precision layout (.5 decimals)
    outfile << std::fixed << std::setprecision(5);

    // If it's not the first item, append a comma onto the previous item's line first
    if (!is_first_item) {
        outfile << ",";
    }

    // Write out the specific structural data subsets
    outfile << "\n    { "
            << "\"source\": \"SBD\", "
            << "\"TIME\": \"" << record.formatted_GMT << "\", "
            << "\"JULD\": " << std::setprecision(5) << record.seconds_since_1950/86400. << ", "
            << "\"momsn\": " << record.momsn << ", "
            << "\"LATITUDE_iridium\": " << std::setprecision(5) << record.latitude << ", "
            << "\"LONGITUDE_iridium\": " << std::setprecision(5) << record.longitude << ", ";
   if ( record.error_iridium == 99999 ) { // if fillvalue keep as fillvalue
      outfile << "\"error_iridium\": " << record.error_iridium << ", ";
   } else {
      outfile << "\"error_iridium\": " << record.error_iridium*1000  << ", ";
   }
   outfile << "\"sbd_contains_cycle\": " << record.cycle 
            << " }"; // No newline here yet, in case another item is appended next

    return true;
}

// CALL ONCE: Appends closing formatting wrappers to ensure valid JSON tracking
bool close_json_summary(const std::string& output_path) {
    std::ofstream outfile(output_path, std::ios::app);
    if (!outfile.is_open()) return false;

    //outfile << "\n  ]\n}"; Without Weighted Iridium Position After
    outfile << "\n  ],"; // If there is a Weighted Iridium Position Afterwards;
    return true;
}

// Helper to convert degrees to radians
inline double degreesToRadians(double degrees) {
    return degrees * M_PI / 180.0;
}

// Helper to convert radians to degrees
inline double radiansToDegrees(double radians) {
    return radians * 180.0 / M_PI;
}

// Structure to hold our unique positions
struct Position {
    double lat;
    double lon;

    // Required for std::set to sort and identify duplicates
    bool operator<(const Position& other) const {
        if (lat != other.lat) return lat < other.lat;
        return lon < other.lon;
    }
};

// struct to hold the final combined unique Iridium positions
struct PositionData {
    double lat;
    double lon;
    double error;
};

// Function to calculate the weighted average based on (1/error)^2
WeightedResult WeightedIridiumPosition(const std::vector<SbdRecord>& data) {
    if (data.empty()) {
        throw std::invalid_argument("Vector cannot be empty.");
    }

    double total_weight = 0.0;
    double total_weightj = 0.0;
    double weighted_lat_sum = 0.0;
    double weighted_sin_lon_sum = 0.0;
    double weighted_cos_lon_sum = 0.0;
    long long weighted_juld_sum = 0.0;

    // Map Position -> lowest error
    std::map<Position, double> unique_map;

    for (const auto& point : data) { //form unique position and average JULD (All)
        Position pos{point.latitude, point.longitude};
        double current_error = point.error_iridium;


	// If the position is new, OR we found a lower error for an existing position
        if (unique_map.find(pos) == unique_map.end() || current_error < unique_map[pos]) {
            unique_map[pos] = current_error; // Insert or overwrite with lower error
        }

	total_weightj += 1;
	weighted_juld_sum += point.seconds_since_1950;
    }

    // Form the new vector of unique positions with their best errors
    std::vector<PositionData> unique_positions;
    unique_positions.reserve(unique_map.size());

    for (const auto& [pos, err] : unique_map) {
        unique_positions.push_back({pos.lat, pos.lon, err});
    }

    for (const auto& loop : unique_positions) {

        //std::cout << "Unique Positions " << loop.lat << " " << loop.lon << " " << loop.error << std::endl;

	// Prevent division by zero
        if (loop.error <= 0) {
            continue;
        }

        // Weight is inversely proportional to the square error: w = 1 / (error^2)
        double weight = 1.0 / std::pow(loop.error,2); //error_iridium in km here

        total_weight += weight;

	weighted_lat_sum += loop.lat * weight;        
        // Convert longitude to radians and find its circular components
        double lon_rad = degreesToRadians(loop.lon);
        weighted_sin_lon_sum += std::sin(lon_rad) * weight;
        weighted_cos_lon_sum += std::cos(lon_rad) * weight;
    }

    if (total_weight == 0) {
        throw std::runtime_error("Total weight is zero. Check your error values.");
    }

    // Compute the weighted mean components for longitude
    double avg_sin_lon = weighted_sin_lon_sum / total_weight;
    double avg_cos_lon = weighted_cos_lon_sum / total_weight;

    // Save the subsampled count for return
    int subsampcount=unique_positions.size();

    // Reconstruct the average longitude using atan2 to resolve the correct quadrant
    double avg_lon_rad = std::atan2(avg_sin_lon, avg_cos_lon);
    return {
        weighted_lat_sum / total_weight,
        radiansToDegrees(avg_lon_rad),
        1. / sqrt(total_weight),
        weighted_juld_sum / total_weightj,
        subsampcount
    };
}


//*********************************************************************************************
int main( int argc, char* argv[] ) {
 
    const char *s2_PATH = getenv("S2_PATH");
    if (s2_PATH == NULL) {
        // If S2_PATH env variable is not defined, exit. Write to stdout becuase log() requires S2_PATH
      std::cout << "Unable to load S2_PATH environment variable; exiting." << std::endl;
      return 0;
    }

    std::cout << "" << std::endl;
    std::cout << "Program Name: " << argv[0] << std::endl;

    if (argc > 2) {
      std::cout << "Number of arguments: " << argc - 1 << std::endl;
      std::cout << "Arguments:" << std::endl;
      for (int i = 1; i < argc; ++i) {
        std::cout << "  Argument " << i << ": " << argv[i] << std::endl;
      }
    } else {
      std::cout << "2 Argments mandatory: float SN; cycle number" << std::endl;
      return 0;
    }

    std::string s2_path = s2_PATH;

 //process the argments 
    int value = std::stoi(argv[1]);
    int padwidth=4;
    if ( value >= 10000 ) { padwidth=5; }
    std::ostringstream padossn;
    padossn << std::setw(padwidth) << std::setfill('0') << value;
    std::string padsn=padossn.str();
    value = std::stoi(argv[2]);
    std::string padcy;
    if ( value == -1 ) {
      padcy="-01";
    } else {
      padwidth=3;
      if ( value >= 1000 ) { padwidth=4; }
      std::ostringstream padoscy;
      padoscy << std::setw(padwidth) << std::setfill('0') << value;
      padcy=padoscy.str();
    }

    //confirm the json/SBD directory exists; if not create it
    std::string output_json = s2_path + "/data/" + padsn + "/json/SBD";
    if (std::filesystem::create_directory(output_json)) {
      std::cout << "  Directory '" << output_json << "' created successfully." << std::endl;
    }

    //Determine the L0 json file the program has been asked to process: save name to SBD_for_file
    fs::path SBD_for_file;
    std::string metafile = s2_path + "/data/" + padsn + "/" + padsn + "_meta.json"; // Must get SN_meta.json to get AOMLid && transmission ID
    if (std::filesystem::exists(metafile)) {  //check to see if float specific meta.json exists; if not exit
      std::ifstream tempmeta(metafile, std::ifstream::binary);
      json Meta;
      tempmeta >> Meta; //read in json of meta
      if ( Meta.contains("DAC_ID_NUMBER") && Meta.contains("TRANSMISSION_ID_NUMBER") ) {
        int AOMLid = Meta.at("DAC_ID_NUMBER"); 
        padwidth=4;
        if ( AOMLid >= 10000 ) { padwidth=5; }
        std::ostringstream padosao;
        padosao << std::setw(padwidth) << std::setfill('0') << AOMLid;
        std::string padaoml=padosao.str();
        AOMLid = Meta.at("TRANSMISSION_ID_NUMBER"); 
        padwidth=6;
        padosao.str("");
        padosao.clear();
        padosao << std::setw(padwidth) << std::setfill('0') << AOMLid;
        std::string padtrans=padosao.str();

        SBD_for_file = s2_path + "/data/" + padsn + "/json/L0/" + padaoml + "_" + padtrans + "_L0_" + padcy + ".json";  //name of L0 json we are processing
        output_json = s2_path + "/data/" + padsn + "/json/SBD/" + padaoml + "_" + padtrans + "_SBD_" + padcy + ".json";  //name of SBD json we will be writing

      } else {
        std::cout << "No DAC_ID or TRANSMISSION_ID  within meta-json: exiting" << std::endl;
        return 0;
      }
    } else {
      std::cout << "No meta-json yet written in ./data/SN: exiting" << std::endl;
      return 0;
    }
    //std::cout << SBD_for_file << std::endl;

// OPEN L0 json and store the start of surface interval in SBDtime
    json L0_json;
    int NearSurface = 0; //flag to indicate whether the SBDtime is close to surface...if not need to extend span of search
    long long seconds_ERR; //seconds of "telemetry_summary" to compare with collected start TIME
    std::string SBDtime="";
    std::string ERRtime="";
    if (std::filesystem::exists(SBD_for_file)) {  //check to see if L0 json exists
      std::ifstream tempjson(SBD_for_file, std::ifstream::binary);
      tempjson >> L0_json; //read in L0 json

      // As an error check load in earliest date from "telemetry_summary"
      // telemetry_summary is not the best time to use for gathering SBD...but can be estimate and error check
      seconds_ERR= -1;
      if ( L0_json.contains("telemetry_summary") ) { 
        const auto& list = L0_json.at("telemetry_summary");
        std::string tmpERRtime;
        long long tmpseconds_ERR;
        std::tm result;
	for (const auto& row : list) {
          tmpERRtime = row["TIME"].get<std::string>();
          result = string_to_tm(tmpERRtime); //convert to structure for the subroutine
          tmpseconds_ERR = get_seconds_since_1950(result);
	  if ( seconds_ERR < 0 || tmpseconds_ERR < seconds_ERR ) {
	    seconds_ERR = tmpseconds_ERR;
	    ERRtime = tmpERRtime;
          } 
        } 
	std::cout << "ERRtime: telemetry_summary " << ERRtime << std::endl;
      } 

      if ( L0_json.contains("GPS") ) {
        const auto& list = L0_json.at("GPS");
        for (const auto& row : list) {
          if ( row["description"] != "GPS_START" ) { //Accept any GPS that is NOT GPS_START
            int GPSvalid = row["valid"].get<int>();
	    if ( std::abs(GPSvalid) == 2 ) {
              SBDtime = row["TIME"].get<std::string>();
	      std::cout << "SBDtime: GPS_END " << SBDtime << std::endl;
	    }
	  }
	}
      }
      if ( L0_json.contains("Rise") && SBDtime=="" ) { //Must move to Rise
        //std::cout << "Entering RISE " << std::endl;
        const auto& list = L0_json.at("Rise");
        for (const auto& row : list) {
          if ( row["PRES"].get<int>() <= 9999. ) {
            SBDtime = row["TIME"].get<std::string>(); //Will save most recent (shallowest)
	    //std::cout << "SBDtime: Rise " << SBDtime << std::endl;
            if ( row["PRES"].get<int>() >= 50. ) { //assume that deeper than 50dbar is enough to need longer surface
		    NearSurface = 1;
	    }
	  }
	}
      }
      if ( L0_json.contains("Fall") && SBDtime=="" ) { //Must move to Fall
        //std::cout << "Entering Fall " << std::endl;
        const auto& list = L0_json.at("Fall");
        for (const auto& row : list) {
          if ( row["PRES"].get<int>() <= 9999. ) {
            SBDtime = row["TIME"].get<std::string>(); //Will save most recent (deepest)
	    NearSurface = 2;
	    //std::cout << "SBDtime: Fall " << SBDtime << std::endl;
	  }
	}
      }
      if ( L0_json.contains("GPS") && SBDtime=="" ) { //Finally go back to GPS and take GPS_START (which is from previous cycle after transmission)
        const auto& list = L0_json.at("GPS");
        for (const auto& row : list) {
          if ( row["description"] == "GPS_START" ) {
            int GPSvalid = row["valid"].get<int>();
	    if ( std::abs(GPSvalid) == 2 ) {
              SBDtime = row["TIME"].get<std::string>();
	      NearSurface = 3;
	      //std::cout << "SBDtime: GPS_START " << SBDtime << std::endl;
	    }
	  }
	}
      }

    } else {
      std::cout << "Requested L0 json file does not exist: Nothing to do so exiting" << std::endl;
      return 0;
    }

    long long seconds_L0_json_start; //Define the start of the surface interval based on L0 in seconds
    long long seconds_L0_json_end; //Define the end of the surface interval based on L0 in seconds
    if ( SBDtime=="" && ERRtime=="" ) { //Check to see if there is NO time information in json
      std::cout << "After reading L0 json, no start of surface time found: exiting " << SBDtime << std::endl;
      return 0;
    } else { //Compute the seconds since 1950

// Define the surface interval
      std::tm result = string_to_tm(SBDtime); //convert to structure for the subroutine
      seconds_L0_json_start = get_seconds_since_1950(result);
      //compare the start to seconds_ERR: if ERR prior by 20 days or after 3 years assume telemetry is better
      std::cout << "Error Check: compare surface start JULD with earliest telemetry " << std::setprecision(11) << seconds_L0_json_start/86400. << " " << seconds_ERR/86400. << std::endl;
      if ( seconds_ERR > 0 ) { // enter if telemetry_summary has data
        long long difftime = seconds_L0_json_start - seconds_ERR;
        if ( difftime > ( 20 * 86400 ) || difftime < ( -3 * 365 * 86400) ) {
	    NearSurface = 0;
            seconds_L0_json_start = seconds_ERR - ( 5 * 60 ); //assume 5 minutes prior
            std::cout << "Defaulting to telemetry summary " << std::setprecision(11) << seconds_L0_json_start/86400. << std::endl;
        }
      }
      //std::cout << "Start of surface interval " << std::setprecision(11) << seconds_L0_json_start/86400. << std::endl;

      if ( L0_json.contains("ARGO_Mission") ) { 
        seconds_L0_json_end = seconds_L0_json_start + L0_json["ARGO_Mission"]["target_surface_second"].get<int>();
	if ( NearSurface >= 1 ) { // Add time to ascend from drift
          seconds_L0_json_end = seconds_L0_json_end + 60 * ( L0_json["ARGO_Mission"]["max_rise_minute"].get<int>() + L0_json["ARGO_Mission"]["max_fall_to_profile_minute"].get<int>() );
	} 
	if ( NearSurface >= 2 ) { // Add time to drift and descend to profile
          seconds_L0_json_end = seconds_L0_json_end + 60 *  ( 5 * L0_json["ARGO_Mission"]["target_drift_5minute"].get<int>() + L0_json["ARGO_Mission"]["seek_periods"].get<int>() * L0_json["ARGO_Mission"]["seek_minute"].get<int>() );
	} 
	if ( NearSurface >= 3 ) { // Add time to drift and descend to profile
          seconds_L0_json_end = seconds_L0_json_end + 60 * ( L0_json["ARGO_Mission"]["max_fall_to_park_minute"].get<int>() );
	} 
      } else {
        seconds_L0_json_end = seconds_L0_json_start + 60*60; //assume 60 minutes
      }
      //std::cout << "End of surface interval " << std::setprecision(11) << seconds_L0_json_end/86400. << std::endl;
    }

// Moving on to the hex files

    fs::path hexpath = s2_path + "/data/" + padsn + "/hex";  //path for this floats hex files
    //std::cout << hexpath << std::endl;

    // Sort hex files
    std::vector<std::filesystem::path> files_in_directory;
    try {
         for (const auto& entry : std::filesystem::directory_iterator(hexpath)) {
            if (entry.is_regular_file()) {
                if (entry.path().extension() == ".hex") {
                        files_in_directory.push_back(entry.path());
                }
            }
         }
    } catch (const std::filesystem::filesystem_error& ex) {
          std::cerr << "Filesystem error: " << ex.what() << std::endl;
    }

    //exit if there are no files in L0 directory (Can this happen?)
    if ( files_in_directory.size() == 0 ) {
          std::cout << "No hex files in directory: exiting " << std::endl;
          return 0;
    }

    std::sort(files_in_directory.begin(), files_in_directory.end(), natural_sort_compare); //sort the hex file names dealing with 3 and 4 digit cycle#

    // Initialize the SBD json document structure skeleton
    bool SBDfirst=true; //counter to keep track of the first record
    open_json_summary(output_json);

    int hexcy;
    long long MinTimeLaterCy=9999999999; //Save the minimum time of transmissions in LATER cycles
    std::multimap<long long, SbdRecord> sorted_main_map; //Sorting structure within main

    for (auto it = files_in_directory.rbegin(); it != files_in_directory.rend(); ++it) { //Loop from last natural sort to first
      std::string hexfile = *it;
      //std::cout << "Looping check for " << hexfile << std::endl;

      size_t posst = hexfile.find_last_of('_')+1;
      if ( hexfile.substr(posst,1) == "-" ) {
	hexcy=-1;
      } else {
        hexcy = std::stoi(hexfile.substr(posst));
      }

      if ( hexcy > std::stoi(argv[2]) ) { 
	//IF cycle# > target, enter block and collect the MINIMUM time of SBD.  This is the upper cutoff for target json.
	//Uses the fact that the float sends present data BEFORE, previous data (target).  
        fs::path p = hexfile;
        std::vector<SbdRecord> records = parse_sbd_file_sorted(hexfile); 
        for (const auto& record : records) {
     	  if ( record.seconds_since_1950 < MinTimeLaterCy ) { 
	    MinTimeLaterCy = record.seconds_since_1950;
	  }
	}
      }

      fs::path p = hexfile;

      std::tm file_gmt = {};

      if (get_file_mtime_gmt(p, file_gmt)) {

        //std::cout << "GMT File Time: " 
        //          << (file_gmt.tm_year + 1900) << "-" 
        //          << (file_gmt.tm_mon + 1) << "-" 
        //          << file_gmt.tm_mday << " " 
        //          << file_gmt.tm_hour << ":" 
        //          << file_gmt.tm_min << ":" 
        //          << file_gmt.tm_sec << " GMT\n";

        long long seconds_hex = get_seconds_since_1950(file_gmt);
        //std::cout << "Seconds since Jan 1, 1950: " << seconds_hex << "\n";
        //std::cout << "Days since Jan 1, 1950: " << std::setprecision(11) << seconds_hex/86400. << "\n";

	if ( seconds_hex >= seconds_L0_json_start ) { //Hex file is written since json surface interval...so could contain surface SBD
            //std::cout << "Hex more recent than surface interval in L0 json: Processing " << hexfile << std::endl;

                std::vector<SbdRecord> records = parse_sbd_file_sorted(hexfile); //subroutine only sorted within hex file, now make sure sorted across hex files

                for (const auto& record : records) {
     		  if ( record.seconds_since_1950 > seconds_L0_json_start && record.seconds_since_1950 < seconds_L0_json_end && record.seconds_since_1950 < MinTimeLaterCy) { // Check if SBD record is between surface interval start/end : Also less than the MinTimeLaterCy
                    sorted_main_map.insert({record.seconds_since_1950, record}); //Assign to matrix for writing to json

  	            // Verify properties print correctly
                    //std::cout << "ISO Formatted Time: " << record.formatted_GMT << "\n";
                    //std::cout << "Time Since 1950: " << record.seconds_since_1950 << " seconds\n";
                    //std::cout << "Days Since 1950: " << record.seconds_since_1950/86400. <<  "\n";
                    //std::cout << "Iridium Position:  GMT:" << std::left << record.formatted_GMT << " JULD:" << std::setprecision(11) << std::setw(12) << record.seconds_since_1950/86400. 
	            // 	      <<  " Lat: " << std::setw(9) << record.latitude << " Lon: " << std::setw(10) << record.longitude << " Error: " << std::setw(4) << record.error_iridium << "\n";
		  }
                }	
	}

      } else {
        std::cout << "Error: File could not be found or processed.\n";
      }

    }

    std::vector<SbdRecord> results;
    results.reserve(sorted_main_map.size()); // Pre-allocate memory for speed
    for (const auto& [seconds, record] : sorted_main_map) {
      results.push_back(record);
    }
    for (const auto& record : results ) { //write to json;
        SbdRecord single_record = record;  //write single record to json
        if ( SBDfirst ) {  
          append_sbd_to_json(output_json, record, SBDfirst);  // True indicates the first item (no leading comma)
	  SBDfirst = false; //no longer the first record
        } else {
          append_sbd_to_json(output_json, record, SBDfirst); // False prints a comma before writing this item
        }

    }

    // Seal file tags to finish output validation
    close_json_summary(output_json);

    // Call the weighted average subroutine
    bool is_Size0 = true; //boolean to determin if there are records in results
    WeightedResult result;
    if ( results.size() > 0 ) {	
      is_Size0 = false;

      //structure has size...but make sure there are some valid error estimates
         auto max_it = std::max_element(results.begin(), results.end(),
	   [](const SbdRecord& a, const SbdRecord&b) {
		   return a.error_iridium < b.error_iridium; // return true if 'a' is less than 'b'
	   });
      	 if ( max_it != results.end()) { //empty
           //std::cout << max_it->error_iridium << std::endl;
	   if ( max_it->error_iridium != 0 && max_it->error_iridium != 99999 ) {
                 //std::cout << "Entering Weighted Routine" << std::endl;
             result = WeightedIridiumPosition(results); //enter routine
           } else { // dont enter weighted average routine:  Just simple average of juld
      		 double sum = std::accumulate(results.begin(), results.end(), 0.0,
			   [](long long acc, const SbdRecord& record) {
			        return acc+record.seconds_since_1950;
			   });
	     result.juld = sum / results.size();
       	     result.latitude = 99999.; 
             result.longitude = 99999.; 
             result.error = 99999; 
	   }
         }
    }

    write_IrrPos(output_json, result, is_Size0);  

    //output_json << "\n";
        //output_json << "  \"sbd_weighted_iridium_position\": ";
        //output_json << "\n    { "
        //    << "\"JULD\": \"" << std::setprecision(11) << result.juld/86400. << ", "
        //    << "\"LATITUDE\": \"" << result.latitude << ", "
        //    << "\"LONGITUDE\": \"" << result.longitude << ", "
        //    << "\"POSITION_ERROR\": \"" << result.error*1000. << " }"
	//    << "\n}"

    return 0;
}

