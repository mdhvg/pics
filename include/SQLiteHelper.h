#ifndef SQLITE_HELPER
#define SQLITE_HELPER

#include <sqlite3.h>
#include <mutex>
#include <string>

struct DBWrapper {
	sqlite3 *dbP = nullptr;
	sqlite3 **dbPP = &dbP;
	std::mutex dbMutex;

	DBWrapper(const char *filename);
	~DBWrapper();
	void executeCommand(const std::string command,
						sqlite3_callback callback = nullptr,
						void *data = nullptr);
};

#endif // !SQLITE_HELPER