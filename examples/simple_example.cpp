#include <v3crtplib/version.h>

#include <iostream>

int main() {
	std::cout << "V3C rtp lib version: " << v3cRTPLib::get_version() << std::endl;
	return 0;
}