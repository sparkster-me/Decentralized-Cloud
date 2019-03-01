#pragma once
#include <iostream>
#include <future>
#include <common/types.h>
#include <queue>

typedef long long DoubleLong;

class CommandInfo{
public:
	CommandInfo();
	CommandInfo(FieldValueMap fields, DoubleLong timestamp,std::vector<std::string> command);
	~CommandInfo();
	DoubleLong& getTimestamp();
	FieldValueMap& getFields();
	std::vector<std::string>& getCommand();
private:
	FieldValueMap _fields;
	DoubleLong _timestamp;
	std::vector<std::string> _command;
};

struct OrderByTimeStamp {
	bool operator()(CommandInfo& a, CommandInfo& b) const noexcept {
		return a.getTimestamp() > b.getTimestamp();
	}
};

typedef std::priority_queue<CommandInfo, std::vector<CommandInfo>, OrderByTimeStamp> OrderedPriorityQueue;
typedef std::unordered_map<std::string, OrderedPriorityQueue*> UnorderedQueueMap;