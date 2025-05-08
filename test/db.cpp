#include <gtest/gtest.h>
#include <fmt/core.h>

#include "SQLiteHelper.h"
#include "fmt/base.h"

class TestQueries : public testing::Test {
  public:
	TestQueries() : db(ROOT_DIR "/pics.sqlite") {
	}

  protected:
	DBWrapper db;
};

TEST_F(TestQueries, CreateAndCloseDB) {
	EXPECT_NE(db.dbP, nullptr);
}

TEST_F(TestQueries, SelectAtlas) {
	std::string result;
	const std::string query = "SELECT * FROM Atlas";
	db.executeCommand(
		query,
		[](void *data, int argc, char **argv, char **) {
			std::string *result = (std::string *)data;
			for (int i = 0; i < argc; i++) {
				result->append(argv[i]);
				result->push_back('\t');
			}
			result->push_back('\n');
			return 0;
		},
		&result);
	fmt::println("Result\n{}", result);
	EXPECT_GE(result.size(), 0);
}

TEST_F(TestQueries, SelectImages) {
	std::string result;
	const std::string query = "SELECT * FROM Images";
	db.executeCommand(
		query,
		[](void *data, int argc, char **argv, char **) {
			std::string *result = (std::string *)data;
			for (int i = 0; i < argc; i++) {
				result->append(argv[i]);
				result->push_back('\t');
			}
			result->push_back('\n');
			return 0;
		},
		&result);
	fmt::println("Result\n{}", result);
	EXPECT_GE(result.size(), 0);
}

TEST_F(TestQueries, getAtlasPaths) {
	std::vector<std::string> atlasPaths;
	db.executeCommand(
		"SELECT atlas_path FROM Atlas",
		[](void *data, int, char **argv, char **) -> int {
			auto *atlasPaths = (std::vector<std::string> *)data;
			atlasPaths->push_back(argv[0]);
			return 0;
		},
		&atlasPaths);
	for (const auto &path : atlasPaths) {
		fmt::println("Path: {}", path);
	}
	EXPECT_GE(atlasPaths.size(), 0);
}