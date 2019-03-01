#pragma once
#include <fstream>
#include <common/transaction.h>
#include <common/functions.h>
namespace BlockUtil {
	class Block {

	private:
		size_t number;
		std::string hash;
		std::string previousHash;
		json data;
		std::time_t time;
	public:
		Block(const size_t& number, const std::string& hash, const std::string& previousHash, const json& data, const std::time_t& time) {
			this->number = number;
			this->hash = hash;
			this->previousHash = previousHash;
			this->data = data;
			this->time = time;
		}
		std::string getPreviousHash() const {
			return this->previousHash;
		}
		std::string getHash() const {
			return this->hash;
		}
		size_t getNumber() const {
			return this->number;
		}
		json getData() const {
			return this->data;
		}
		std::string toJSONString() const {
			json _json;
			_json["n"] = this->number;
			_json["h"] = this->hash;
			_json["p"] = this->previousHash;
			_json["data"] = this->data;
			_json["tm"] = this->time;
			return _json.dump();
		}
	};

std::string calculateBlockHash(const size_t& number, const std::string& previousHash, const std::vector<Transaction>& txns, const std::time_t& time);
Block getBlock(std::vector<Transaction>&& txns);
};