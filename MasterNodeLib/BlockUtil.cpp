#include "BlockUtil.h"
namespace BlockUtil {
	size_t nextBlockNumber= 0;
	std::string previousHash = "";
	size_t getNextBlockNumber() {
		// If nextBlockNumber is 0, we're either the first master node to spawn or we simply haven't fetched yet.
		// A master node can get the next block number of the network by calculating the number of seconds between the current timestamp and the genesis timestamp.
		// The genesis timestamp is the timestamp at which the first node in the network spawned. Since blocks are generated every second, all a master node needs is the
		// genesis timestamp to find out which block number to start at.
		if (nextBlockNumber == 0) {
			std::string bnstr = dhtGet("genesisStamp");
			if (bnstr.empty()) { // We're the first master node.
				dhtPut("genesisStamp", std::to_string(currentTimestamp), true);
				nextBlockNumber = 1;
				return 0; // Block numbers start from 0.
			} else { // We've fetched a genesis time from the dht.
				nextBlockNumber = currentTimestamp - std::stoull(bnstr);
			}
		}
		return nextBlockNumber++;
	}

	std::string getPreviousHash() {
		std::string hash = dhtGet("hash");
		if (hash.empty()) {
			return "00000000000000000000000000000000";
		}
		return std::string(hash);
	}

	std::string calculateBlockHash(const size_t& number, const std::string& previousHash, const std::vector<Transaction>& txns, const std::time_t& time) {
		std::string _data;
		for (std::vector<Transaction>::const_iterator it = txns.cbegin(); it != txns.cend(); it++)
			_data += it->getTxId();
		dht::InfoHash infoHash;
		return infoHash.get(std::to_string(number) + previousHash + std::to_string(time) + _data).toString();
	}

	Block getBlock(std::vector<Transaction>&& txns) {
		size_t nextBlockNumber = getNextBlockNumber();
		//update block number in transactions
		for (std::vector<Transaction>::iterator it = txns.begin(); it != txns.end(); it++)
			it->setBlockId(nextBlockNumber);
		json data = txns;
		logDebug("Transaction vector length %d , json data: %s",txns.size(),data.dump());
		// PreviousHash will be empty in case of genesis block or if we haven't fetched the hash yet.
		if (previousHash.empty())
			previousHash = getPreviousHash();
		// Do required validation here
		std::string currentBlockHash = calculateBlockHash(nextBlockNumber, previousHash, txns, currentTimestamp);
		dhtPut("hash", currentBlockHash, false);
		Block block(nextBlockNumber, currentBlockHash, previousHash, data, currentTimestamp);
		// Next, we need to fast-forward the block state so the next block is generated using the values generated here.
		previousHash = currentBlockHash;
		return block;
	}
};