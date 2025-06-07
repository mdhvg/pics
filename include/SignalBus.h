#pragma once

#include "Worker.h"

#include <atomic>

class SignalBus {
  public:
	static SignalBus &getInstance();

	std::atomic<bool>		 appRunningM{ true };
	ThreadSafeQ<std::string> textEncodeQ;

  private:
	SignalBus() = default;
	SignalBus(const SignalBus &) = delete;
	SignalBus &operator=(const SignalBus &) = delete;

	static SignalBus &instance;
};