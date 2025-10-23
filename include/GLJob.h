#include <functional>
#include <memory>
#include <mutex>
#include <queue>

class GLJob {
  public:
	GLJob(std::function<void()> func,
		std::string name = "",
		std::mutex *end = nullptr);

	void execute();
	void reject();

  private:
	std::string funcName;
	bool clock;
	std::mutex *endMutex;
	std::function<void()> jobFunc;
};

// TODO: Make this a derived class from ThreadSafeQ
class GLJobQ {
  public:
	void push(std::shared_ptr<GLJob> job);
	bool empty();
	std::shared_ptr<GLJob> pop();

  private:
	std::mutex qMutex;
	std::queue<std::shared_ptr<GLJob>> jobQ;
};