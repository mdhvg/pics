#include <functional>
#include <mutex>
#include <queue>
#include <memory>

class GLJob {
  public:
	GLJob(std::function<void()> func,
		  const std::string name = "",
		  bool timeIt = false);
	void execute();

  private:
	const std::string funcName;
	bool timeIt;
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