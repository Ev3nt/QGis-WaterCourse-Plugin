#include "pch.h"

#include "TempManager.h"

#include <filesystem>
#include <random>

TempManager::~TempManager() {
	for (const auto& path : tempFilesPaths_) {
		std::filesystem::remove(path.second);
	}

	tempFilesPaths_.clear();
}

std::filesystem::path TempManager::addFile(const std::string& key) {
	return tempFilesPaths_.find(key) == tempFilesPaths_.end() ? tempFilesPaths_[key] = generateRandomName() : tempFilesPaths_[key];
}

std::filesystem::path TempManager::getPath(const std::string& key) {
	return tempFilesPaths_.at(key);
}

void TempManager::deleteFile(const std::string& key) {
	std::filesystem::remove(tempFilesPaths_[key]);
	tempFilesPaths_.erase(key);
}

std::string TempManager::generateRandomName(const std::string& prefix, const std::string& suffix) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(0, 15);

	std::string random_part;
	for (int i = 0; i < 8; ++i) {
		random_part += "0123456789ABCDEF"[dis(gen) % 16];
	}

	return prefix + random_part + suffix;
}

void TempManager::makeNonTemp(const std::string& key) {
	tempFilesPaths_.erase(key);
}