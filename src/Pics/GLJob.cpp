#include "GLJob.h"
#include "SignalBus.h"

#include "spdlog/spdlog.h"

#include <chrono>
#include <mutex>

GLJob::GLJob(std::function<void()> func,
	const std::string name,
	std::mutex *end)
	: funcName(name), clock(!name.empty()), endMutex(end), jobFunc(func) {
}

void GLJob::execute() {
	if (clock) {
		auto start = std::chrono::high_resolution_clock::now();
		jobFunc();
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> duration = end - start;
		SPDLOG_INFO("{} GLJob executed in: {:.6f} seconds",
			(funcName.size() ? funcName : "OpenGL Function"),
			duration.count());
	} else {
		jobFunc();
	}
	if (endMutex)
		endMutex->unlock();
}

void GLJob::reject() {
	if (endMutex)
		endMutex->unlock();
}

void GLJobQ::push(std::shared_ptr<GLJob> job) {
	std::lock_guard<std::mutex> lock(qMutex);
	SignalBus &bus = SignalBus::getInstance();
	if (!bus.appRunningM) {
		job->reject();
	} else {
		jobQ.push(job);
	}
}

bool GLJobQ::empty() {
	std::lock_guard<std::mutex> lock(qMutex);
	return jobQ.empty();
}

std::shared_ptr<GLJob> GLJobQ::pop() {
	std::lock_guard<std::mutex> lock(qMutex);
	if (jobQ.empty())
		return nullptr;
	auto job = jobQ.front();
	jobQ.pop();
	return job;
}
