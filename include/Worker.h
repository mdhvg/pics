#include <condition_variable>
#include <mutex>
#include <thread>
#include <functional>
#include <string>

// Variadic args variant
// template <class... Args>
class Worker {
  public:
	Worker(Worker &&) = delete;
	Worker(const Worker &) = delete;

	Worker(std::function<void()> fn,
		   const std::string name = "",
		   bool timeIt = false);
	~Worker();

	// Variadic args variant
	// Worker(std::function<void(Args... args)> fn,
	// 	   const std::string name = "",
	// 	   bool timeIt = false);
	// void run(Args... args);

	void run();
	void stopWorker();

  private:
	void worker_thread();

	bool ready{ false };
	bool stop{ false };
	std::mutex mutex;
	std::condition_variable cv;
	std::thread thread;
	std::function<void()> fn;

	// Variadic args variant
	// std::function<void(Args... args)> fn;
	// std::tuple<Args...> argsTuple;
	// bool argsReady{ false };

	const std::string funcName;
	bool clock;
};