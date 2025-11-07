#include "SQLiteHelper.h"
#include "Debug.h"
#include <mutex>

DBWrapper::~DBWrapper() {
	std::lock_guard<std::mutex> lock(db_mutex);
	if (dbP) {
		ASSERT(sqlite3_close(dbP) == SQLITE_OK && "Failed to close db");
		dbP = NULL;
	}
}

void DBWrapper::init(const char *filename) {
	std::unique_lock<std::mutex> lock(db_mutex);
	ASSERT(sqlite3_open(filename, &dbP) == SQLITE_OK && "Couldn't load database");
	initialized = true;
	db_cv.notify_all();
}

void DBWrapper::execute_command(const std::string command, sqlite3_callback callback, void *data) {
	std::unique_lock<std::mutex> lock(db_mutex);
	db_cv.wait(lock, [this]() {
		return initialized;
	});

	char *errorMessage = nullptr;
	int	  result = sqlite3_exec(dbP, command.c_str(), callback, data, &errorMessage);
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