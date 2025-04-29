#include "GLJob.h"
#include "spdlog/spdlog.h"

#include <chrono>

GLJob::GLJob(std::function<void()> func, const std::string name, bool timeIt)
	: funcName(name), timeIt(timeIt), jobFunc(func) {
}

void GLJob::execute() {
	if (timeIt) {
		auto start = std::chrono::high_resolution_clock::now();
		jobFunc();
		auto end = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> duration = end - start;
		SPDLOG_INFO("{} executed in: {:.6f} seconds", (funcName.size() ? funcName : "OpenGL Function"), duration.count());
		return;
	}
	jobFunc();
}

void GLJobQ::push(std::shared_ptr<GLJob> job) {
	std::lock_guard<std::mutex> lock(qMutex);
	jobQ.push(job);
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
