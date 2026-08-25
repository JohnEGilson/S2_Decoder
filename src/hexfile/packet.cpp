#include "packet.h"

std::ostream & operator << ( std::ostream &os, packet &p ) {
    std::cout << "sensorID : " << (int)p.header.sensorID << std::endl;
    std::cout << "ver      : " << (int)p.header.ver << std::endl;
    std::cout << "nDatSize : " << (int)p.header.nDat1 * 256 + (int)p.header.nDat2 << std::endl;
    std::cout << "packType : " << (int)p.header.packType << std::endl;
    std::cout << "data_ID  : " << (int)p.header.data_ID << std::endl;
    std::cout << "proType  : " << (int)p.header.proType << std::endl;
    std::cout << "proDir   : " << (int)p.header.proDir << std::endl;
    std::cout << "pSame    : " << (int)p.header.pSame << std::endl;
    std::cout << "subBlk   : " << (int)p.header.subBlk << std::endl;
    return os;
}

std::istream & operator >> ( std::istream &is, packet &p ) {
    is.read( (char *)&p.header, sizeof(packet_header) );
    std::cout << p;
    return is;
}

// Define packet priorities
int packet::priority() const {
    // Argo Packet includes CTD gain,offsets; process before CTD profiles and engineering (drift averages)
	if (header.sensorID == 240) { //S2 F0
		return 1;
	} else if (header.sensorID == 208) { //Sorting the D0 Config Dump so in correct order
		return 2;
	} else if (header.sensorID == 209) {
		return 3;
	} else if (header.sensorID == 210) {
		return 4;
	} else if (header.sensorID == 211) {
		return 5;
	} else if (header.sensorID == 212) {
		return 6;
	} else {
		return 9;
        }
}

// Used to sort packets by priority
bool packet::operator<(const packet &rhs) const {
	return priority() < rhs.priority();
}
