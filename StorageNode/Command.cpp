#include "Command.h"
#pragma warning (disable: 4996)
CommandInfo::CommandInfo() {}
CommandInfo::CommandInfo(FieldValueMap fields,DoubleLong timestamp,std::vector<std::string> command){
	_timestamp = timestamp;
	_fields = fields;
	_command = command;
}

CommandInfo::~CommandInfo(){
}

FieldValueMap& CommandInfo::getFields() {
	return _fields;
}

DoubleLong& CommandInfo::getTimestamp() {
	return _timestamp;
}

std::vector<std::string>& CommandInfo::getCommand() {
	return _command;
}

