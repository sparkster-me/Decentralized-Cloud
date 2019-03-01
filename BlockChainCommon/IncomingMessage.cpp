#include <common/IncomingMessage.h>
#define DEFAULT_WAIT_TIME 1000
IncomingMessage::IncomingMessage() {
	arrivalTime = std::chrono::system_clock::now();
	howManyReceived = 0;
	numberOfMaydays = 0;
	timeToWait = DEFAULT_WAIT_TIME;
}
IncomingMessage::IncomingMessage(const std::vector<std::string>& fragments):IncomingMessage() {
	messages = fragments;
	lastPacketIndex = messages.size() - 1;
	howManyReceived = messages.size();
}
IncomingMessage::IncomingMessage(const size_t& lastPacketIndex, const std::string& originator):IncomingMessage() {
	this->lastPacketIndex = lastPacketIndex;
	this->originator = originator;
	messages.resize(lastPacketIndex + 1);
}

std::string IncomingMessage::add(const size_t& index, const std::string& fragment) {
	if (index >= messages.size())
		return "";
	if (messages[index].empty()) {
		messages[index] = fragment;
		// We just received a packet, so reset the backoff timer since we're clearly receiving stuff.
		lastPacketTime = std::chrono::system_clock::now();
		timeToWait = DEFAULT_WAIT_TIME;
		numberOfMaydays = 0;
	}  else // We already have this packet, so just ignore it.
			return "";
		// Since we've received another packet, increase our score!
	if (++howManyReceived > lastPacketIndex) {
		std::string assembledMessage;
		size_t startOfPayload = 0;
		// Since we save the entire segment including the header in case we need to retransmit, we'll assemble the string by finding the start of the payload and grabbing everything from there onward.
		// We shouldn't include the header since this is not part of the message.
		for (std::vector<std::string>::iterator it = messages.begin(); it != messages.end(); it++) {
			startOfPayload = it->find(':') + 1;
			// We promise, we didn't plan for the :'). It just worked out that way.
			assembledMessage.append(it->substr(startOfPayload, std::string::npos));
		}
		return std::string(assembledMessage);
	}
	return "";
}

time_t IncomingMessage::getSecondsElapsed(const std::chrono::time_point<std::chrono::system_clock>& now) const {
	std::chrono::seconds sec = std::chrono::duration_cast<std::chrono::seconds>(now - arrivalTime);
	return sec.count();
}

bool IncomingMessage::isMissing() const {
	return howManyReceived <= lastPacketIndex;
}

bool IncomingMessage::canSendMayday(const std::chrono::time_point < std::chrono::system_clock>& now) {
	std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPacketTime);
	if (ms.count() >= timeToWait) {
		numberOfMaydays++;
		timeToWait += DEFAULT_WAIT_TIME * numberOfMaydays;
		return true;
	}
	return false;
}

std::string IncomingMessage::getMaydayPacket() {
	std::string packet;
	int index = 0;
	for (std::vector<std::string>::const_iterator it = messages.cbegin(); it != messages.cend(); it++) {
		if (it->empty()) {
			if (!packet.empty())
				packet += ",";
			packet += std::to_string(index);
		}
		index++;
	}
	return packet;
}

std::string IncomingMessage::getSegmentAt(const size_t& index) const {
	if (index >= messages.size()) // Shouldn't happen, but you never know.
		return "";
	return messages[index];
}
std::string IncomingMessage::getOriginator() const {
	return originator;
}