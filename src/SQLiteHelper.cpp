#include "SQLiteHelper.h"
#include "Debug.h"

DBWrapper::DBWrapper(const char *filename) {
	ASSERT(sqlite3_open(filename, dbPP) == SQLITE_OK && "Couldn't load database");
}

DBWrapper::~DBWrapper() {
	ASSERT(sqlite3_close(dbP) == SQLITE_OK && "Failed to close db");
}

void DBWrapper::executeSQL(const std::string command) {
	std::lock_guard<std::mutex> lock(dbMutex);
	char *errorMessage = nullptr;
	int result = sqlite3_exec(dbP, command.c_str(), nullptr, nullptr, &errorMessage);
	if (result != SQLITE_OK) {
		SPDLOG_ERROR("[SQLite error] on Query: {} Code: {}, Message: {}", command, result, (errorMessage ? errorMessage : "Unknown error"));
		ASSERT(false);
	}
	if (errorMessage) {
		sqlite3_free(errorMessage);
	}
}