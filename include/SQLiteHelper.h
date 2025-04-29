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
	void executeSQL(const std::string command);
};

#endif // !SQLITE_HELPER