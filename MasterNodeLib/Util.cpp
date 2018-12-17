#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <map>

using namespace std;

class Util
{
	// Access specifier 
public:

	// Data Members 


	// Member Functions() 
	std::map<std::string, std::string> stringToMap(const std::string& s, char delimiter)
	{
		std::vector<std::string> list = split(s, delimiter);
		std::map<std::string, std::string> inputMap;
		for (int i = 0; i < list.size(); ++i) {
			std::vector<std::string> data = split(list[i], ':');
			inputMap.insert(std::make_pair(data[0], data[1]));
		}
		return inputMap;
	}

	std::vector<std::string> split(const std::string& s, char delimiter)
	{
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream tokenStream(s);
		while (std::getline(tokenStream, token, delimiter))
		{
			tokens.push_back(token);
		}
		return tokens;
	}
};
