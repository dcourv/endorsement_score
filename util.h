#include <string>

// @TODO param to const char *
std::string get_user_string(std::string prompt);

void write_string_to_file(std::string filename, std::string data);

std::string read_file_to_string(std::string filename);