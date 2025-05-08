#pragma once

#include <atomic>
#include <string>
#include <queue>

enum SignalConsumer {
	ALL,
	ATLAS_LOADER,
};

struct Signal {
	std::string message;
	SignalConsumer receiver;
};

class SignalBus {
  public:
	static SignalBus &getInstance();

	std::atomic<bool> appRunningM{ true };

  private:
	SignalBus() = default;
	SignalBus(const SignalBus &) = delete;
	SignalBus &operator=(const SignalBus &) = delete;

	static SignalBus &instance;
};