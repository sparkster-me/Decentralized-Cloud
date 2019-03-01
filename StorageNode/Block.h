#pragma once

#include <common/functions.h>

using json = nlohmann::json;
class Block {

private:
	std::string number;
	std::string hash;
	std::string jsonString;
	int totalVerified;
public:
	Block(const int number, const std::string& hash);
	Block(const int number, const std::string& hash, const std::string& data);
	std::string getHash();
	std::string getString();
	void setString(const std::string& data);
	int getTotalVerified() const;
	void incrementTotalVerified();
	bool verifyHash(const std::string& hash);
};





