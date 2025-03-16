#pragma once

#include <filesystem>
#include <map>

class TempManager {
public:
	~TempManager();

	std::filesystem::path addFile(const std::string& key);
	std::filesystem::path getPath(const std::string& key);
	void deleteFile(const std::string& key);
	std::string generateRandomName(const std::string& prefix = "file_", const std::string& suffix = ".tmp");
	void makeNonTemp(const std::string& key);

private:
	std::map<std::string, std::filesystem::path> tempFilesPaths_;
};