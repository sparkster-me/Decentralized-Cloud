#include "Block.h"

Block::Block(const int number, const std::string& hash) {
	this->number = number;
	this->hash = hash;	
	this->totalVerified = 1;
}

Block::Block(const int number, const std::string& hash, const std::string& data) {
	this->number = number;
	this->hash = hash;
	this->jsonString = data;
	this->totalVerified = 1;
}

std::string Block::getString() {
	return this->jsonString;
}

std::string Block::getHash() {
	return this->hash;
}

void Block::setString(const std::string& data) {
	this->jsonString = data;
}

void Block::incrementTotalVerified() {
	totalVerified++;
}

int Block::getTotalVerified() const {
	return totalVerified;
}

bool Block::verifyHash(const std::string& hash) {
	if (this->hash == hash) {
		incrementTotalVerified();
		return true;
	}
	return false;
}




