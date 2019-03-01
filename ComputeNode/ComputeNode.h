#pragma once
#pragma warning (disable: 4996)
#include <v8.h>
#include "libplatform/libplatform.h"
#include "../ComputeNodeLib/JsProcessor.h"
#include "TransactionResult.h"
#include "RequestData.h"
#include <unordered_map>
#include <common/functions.h>
#include <mutex>
#include <condition_variable>

using namespace v8;
using namespace std;

/*
Type of the node
*/
enum class NodeType {
	Compute,
	Verifier
};

std::mutex m;
std::condition_variable cv;
dht::crypto::Identity* nodeIdentity = nullptr;

typedef std::unordered_map<std::string, TransactionResult> TransactionResultMap; // Mapping of txId..Transaction
TransactionResultMap trs;

typedef std::unordered_map<std::string, RequestData> RequestDataMap; // Mapping of txId..User Request Data
RequestDataMap rds;

std::mutex rdsMutex;

std::mutex txMutex;

typedef std::unordered_map<std::string, std::string> DocumentDataMap; // Mapping of txId..User Request Data
DocumentDataMap dds;

std::string hashCode(std::string& data);


