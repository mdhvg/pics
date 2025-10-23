#include "SQLiteHelper.h"
#include "Debug.h"
#include <mutex>

DBWrapper::DBWrapper() {
	dbMutex.lock();
}

DBWrapper::~DBWrapper() {
	std::lock_guard<std::mutex> lock(dbMutex);
	ASSERT(sqlite3_close(dbP) == SQLITE_OK && "Failed to close db");
}

void DBWrapper::init(const char *filename) {
	ASSERT(sqlite3_open(filename, dbPP) == SQLITE_OK && "Couldn't load database");
	dbMutex.unlock();
}

void DBWrapper::executeCommand(const std::string command, sqlite3_callback callback, void *data) {
	std::lock_guard<std::mutex> lock(dbMutex);
	char *errorMessage = nullptr;
	int result = sqlite3_exec(dbP, command.c_str(), callback, data, &errorMessage);
	if (result != SQLITE_OK) {
		SPDLOG_ERROR("[SQLite error]\nQuery:\n{}\nCode: {}, Message: {}",
			command,
			result,
			(errorMessage ? errorMessage : "Unknown error"));
		ASSERT(false);
	}
	if (errorMessage) {
		sqlite3_free(errorMessage);
	}
}