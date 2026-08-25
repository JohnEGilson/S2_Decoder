#include "gps.h"
using boost::gregorian::date;
using boost::gregorian::days;
using boost::posix_time::hours;
using boost::posix_time::minutes;

#include <boost/date_time.hpp>

//#include <format>
#include "../output/write_log.h"

GPS::GPS(std::vector<uint8_t> data) {
	//int packet = data[0] & 0x0F;
	phase = data[0];
	valid = data[3];
	if (valid > 0x80)
		valid -= 0x100;
	lat = *reinterpret_cast<int32_t *>(&data[4]);
	lat = __builtin_bswap32(lat);
	flat = lat * 1e-7;
	lon = *reinterpret_cast<int32_t *>(&data[8]);
	lon = __builtin_bswap32(lon);
	flon = lon * 1e-7;

	// parse date, convert GPS week,day,hr,min to boost posix_time
	week = *reinterpret_cast<uint16_t *>(&data[12]);
	week = __builtin_bswap16(week);
	wDay = data[14];
	hour = data[15];
	mins = data[16];
	gps_time = ptime(date(1980,1,6))+days((week*7)+wDay)+hours(hour)+minutes(mins);

	time2fix = data[17]*10;
	num_sat = data[18];
	snr_min = data[19];
	snr_mean = data[20];
	snr_max = data[21];
	hdop = data[22]*0.1;

	std::map<int,std::string> gps_phase = {{0,"GPS BIST"},{1,"GPS START"},{2,"GPS END"},{3,"GPS ABORT"},{5,"GPS BITPASS"},{6,"GPS BITFAIL"}};

    tm d = boost::posix_time::to_tm(gps_time);
	//log( std::format("Packet[{:2X}] {:s} - {:4d}/{:02d}/{:02d} {:02d}:{:02d} lat:{:-.5f} lon:{:-.5f} sats:{:d}",packet,gps_phase[phase],d.tm_year+1900,d.tm_mon+1,d.tm_mday,d.tm_hour,d.tm_min,flat,flon,num_sat) );
}

std::ostream & operator << ( std::ostream &os, GPS &g) {
	os << "G xx " << std::setw(2) << g.phase << " " << g.gps_time << " ";
	os << std::fixed << std::setw(9) << std::setprecision(5) << g.flat << " ";
	os << std::fixed << std::setw(10) << std::setprecision(5) << g.flon << " ";
	os << std::setw(4) << g.time2fix << " " << std::setw(2) << g.num_sat << " ";
	os << std::setw(2) << g.snr_min << " " << std::setw(2) << g.snr_mean << " " << std::setw(2) << g.snr_max << " ";
	os << std::setw(4) << std::setprecision(1) << g.hdop << std::endl;
	return os;
}
