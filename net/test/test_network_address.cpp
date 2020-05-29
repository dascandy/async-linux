#include "network_address.h"
#include <iostream>
#include <sstream>

bool testcaseFailed = false;

void testcase(const std::string& str) {
  network_address addr(str);
  std::string out = to_string(addr);
  if (out != str) {
    printf("Case failed: %s != %s\n", out.c_str(), str.c_str());
    testcaseFailed = true;
  }
}

int main()
{
	//The "localhost" IPv4 address
	testcase ( "127.0.0.1" );

	//The "localhost" IPv4 address, with a specified port (80)
	testcase ( "127.0.0.1:80" );
	//The "localhost" IPv6 address
	testcase ( "::1" );
	//The "localhost" IPv6 address, with a specified port (80)
	testcase ( "[::1]:80" );
	//Rosetta Code's primary server's public IPv6 address
	testcase ( "2605:2700:0:3::4713:93e3" );
	//Rosetta Code's primary server's public IPv6 address, with a specified port (80)
	testcase ( "[2605:2700:0:3::4713:93e3]:80" );

	//ipv4 space
	testcase ( "::ffff:192.168.173.22" );
	//ipv4 space with port
	testcase ( "[::ffff:192.168.173.22]:80" );
	//trailing compression
	testcase ( "1::" );
	//trailing compression with port
	testcase ( "[1::]:80" );
	//'any' address compression
	testcase ( "::" );
	//'any' address compression with port
	testcase ( "[::]:80" );

	return testcaseFailed;
}

