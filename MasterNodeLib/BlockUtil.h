#pragma once
#include <ctime>
#include <iostream>
#include <fstream>
#include <opendht.h>
#include <common/functions.h>

class Block {

private:
	long number;
	std::string hash;
	std::string previousHash;
	json data;
	std::time_t time;
public:
	Block(long number, std::string hash, std::string previousHash, json data, std::time_t time) {
		this->number = number;
		this->hash = hash;
		this->previousHash = previousHash;
		this->data = data;
		this->time = time;
	}
	std::string getPreviousHash() {
		return this->previousHash;
	}
	std::string getHash() {
		return this->hash;
	}
	long getNumber() {
		return this->number;
	}
	json getData() {
		return this->data;
	}
	std::string toJSONString() {
		json _json;
		_json["n"] = this->number;
		_json["h"] = this->hash;
		_json["p"] = this->previousHash;
		_json["data"] = this->data;
		_json["tm"] = this->time;
		return _json.dump();
	}
};


class BlockUtil {
public:
	BlockUtil(dht::DhtRunner* dht) {
		this->dht = dht;
	}
	Block getBlock(std::vector<Transaction> txns) {
		std::string previousHash;
		long number;

		// Previous blockno will be empty in case of genesis block
		std::string bnstr = dhtGet(this->dht, "blockno");
		if (bnstr.empty()) {
			number = 1;
		} else {
			number = stol(bnstr) + 1;
		}
		// Update dht with new blocknumber 
		this->dht->put("blockno", number);

		//update block number in transactions
		std::vector<Transaction>::iterator it = txns.begin();
		while (it != txns.end()) {
			it->setBlockId(number);
		}
		json data = txns;

		// PreviousHash will be empty in case of genesis block
		previousHash = dhtGet(this->dht, "hash");
		if (previousHash.empty()) {
			previousHash = "00000000000000000000000000000000";
		}

		std::time_t time = std::time(nullptr);

		// Do required validation here
		std::string currentBlockHash = calculateBlockHash(number, previousHash, txns, time);
		// Update dht with new block hash 
		this->dht->put("hash", currentBlockHash);

		Block block(number, currentBlockHash, previousHash, data, time);
		return block;
	}
	
private:
	dht::DhtRunner* dht;
	std::string calculateBlockHash(long number, std::string previousHash, std::vector<Transaction>& txns, std::time_t time) {
		std::string _data;
		std::vector<Transaction>::iterator it = txns.begin();
		while (it != txns.end()) {
			_data += it->getTxId();
		}
		dht::InfoHash infoHash;
		return infoHash.get(std::to_string(number) + previousHash + std::to_string(time) + _data).toString();
	}
};