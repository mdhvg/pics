#ifndef SQLITE_HELPER
#define SQLITE_HELPER

#include <mutex>
#include <sqlite3.h>
#include <string>

class DBWrapper {
  public:
	sqlite3 *dbP = nullptr;
	sqlite3 **dbPP = &dbP;
	std::mutex dbMutex;

	DBWrapper();
	~DBWrapper();
	void init(const char *filename);
	void executeCommand(const std::string command, sqlite3_callback callback = nullptr, void *data = nullptr);
};

#endif // SQLITE_HELPER