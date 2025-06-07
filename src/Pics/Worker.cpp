#include "spdlog/spdlog.h"

#include "Worker.h"

#include <mutex>

Worker::Worker(std::function<void()> fn, const std::string name, bool timeIt)
	: thread(&Worker::worker_thread, this), fn(std::move(fn)), funcName(name),
	  clock(timeIt) {
}

Worker::~Worker() {
	stopWorker();
	if (thread.joinable()) {
		thread.join();
	}
}

void Worker::run() {
	// Variadic args variant
	// {
	//     std::lock_guard<std::mutex> lock(mutex);
	//     stop = true;
	//     // in case the thread is waiting
	//     argumentsReady = true;
	// }
	{
		std::lock_guard<std::mutex> lock(mutex);
		ready = true;
	}
	cv.notify_one();
}

void Worker::stopWorker() {
	{
		std::lock_guard<std::mutex> lock(mutex);
		stop = true;
	}
	cv.notify_one();
}

void Worker::worker_thread() {
	std::unique_lock lock(mutex);
	while (true) {
		cv.wait(lock, [this] {
			return ready || stop;
		});
		if (stop)
			return;

		ready = false;

		lock.unlock();
		if (clock) {
			auto start = std::chrono::high_resolution_clock::now();
			fn();
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double> duration = end - start;
			SPDLOG_INFO("{} executed in: {:.6f} seconds",
						(funcName.size() ? funcName : "Function"),
						duration.count());
		} else {
			fn();
		}
		lock.lock();
	}
}

template <class T> void ThreadSafeQ<T>::push(std::shared_ptr<T> job) {
	std::lock_guard<std::mutex> lock(qMutex);
	Q.push(job);
}

template <class T> bool ThreadSafeQ<T>::empty() {
	std::lock_guard<std::mutex> lock(qMutex);
	return Q.empty();
}

template <class T> std::shared_ptr<T> ThreadSafeQ<T>::pop() {
	std::lock_guard<std::mutex> lock(qMutex);
	if (Q.empty())
		return nullptr;
	auto job = Q.front();
	Q.pop();
	return job;
}
