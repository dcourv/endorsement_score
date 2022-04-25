#include <iostream>
#include <fstream>
#include <sstream>

#include "util.h"

using std::string;
using std::cout;
using std::cin;
using std::ofstream;

string get_user_string(string prompt) {
	cout << prompt << " ";

	string res;
	cin >> res;
	cout << '\n';
	return res;
}

void write_string_to_file(string filename, string data) {
	ofstream output_filestream(filename);
	output_filestream << data;
	output_filestream.close();
}

// @TODO handle nonexistant file?
string read_file_to_string(string filename) {
	std::ifstream input_file_stream(filename);
	std::stringstream string_stream;
	string_stream << input_file_stream.rdbuf();
	return string_stream.str();
}


