#include "SignalBus.h"

SignalBus &SignalBus::getInstance() {
	static SignalBus instance;
	return instance;
}