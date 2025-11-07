#ifndef SQLITE_HELPER
#define SQLITE_HELPER

#include <condition_variable>
#include <mutex>
#include <sqlite3.h>
#include <string>

class DBWrapper {
  public:
	DBWrapper() = default;
	~DBWrapper();
	void init(const char *filename);
	void execute_command(const std::string command, sqlite3_callback callback = nullptr, void *data = nullptr);

  private:
	sqlite3				   *dbP = nullptr;
	std::mutex				db_mutex;
	std::condition_variable db_cv;
	bool					initialized = false;
};

#endif // SQLITE_HELPER