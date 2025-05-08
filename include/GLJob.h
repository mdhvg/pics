#include <functional>
#include <mutex>
#include <queue>
#include <memory>

class GLJob {
  public:
	GLJob(std::function<void()> func,
		  const std::string name = "",
		  bool timeIt = false,
		  std::mutex *end = nullptr);

	void execute();
	void reject();

  private:
	const std::string funcName;
	bool clock;
	std::mutex *endMutex;
	std::function<void()> jobFunc;
};

class GLJobQ {
  public:
	void push(std::shared_ptr<GLJob> job);
	bool empty();
	std::shared_ptr<GLJob> pop();

  private:
	std::mutex qMutex;
	std::queue<std::shared_ptr<GLJob>> jobQ;
};