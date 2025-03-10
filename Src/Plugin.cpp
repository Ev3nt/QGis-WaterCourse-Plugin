#include "pch.h"

#include "Plugin.h"

#include <filesystem>
#include <queue>
#include <fstream>

#include "Barrier.h"
#include "Timer.h"

void Plugin::process(const std::string& name, float scale, int threadsCount) {
	scale_ = scale;

	int availableThreads = std::thread::hardware_concurrency();
	//std::cout << "Available threads: " << availableThreads << std::endl;

	threadsCount = min(availableThreads, threadsCount);
	//std::cout << "Will be used threads: " << threadsCount << std::endl;

	std::cout << "Threads: " << threadsCount << "/" << availableThreads << " (" << (threadsCount * 100.f / availableThreads) << "%)" << std::endl;

	std::cout << "Opening: " << name << std::endl;

	std::cout << "Scale: " << scale << std::endl;

	GEOTIFF_READER terrainReader(new GdalTiffReader(name));

	std::cout << "Raster count: " << terrainReader->getRasterCount() << std::endl;

	RASTER_BAND terrainBand(terrainReader->getRasterBand(1));

	int width = terrainBand->getXSize(), height = terrainBand->getYSize();
	std::cout << "Raster size: " << width << "x" << height << std::endl;

	noData_ = terrainBand->getNoDataValue();
	std::cout << "Has nodata: " << (noData_ ? "yes (" + std::to_string(noData_.value()) + ")" : "no") << std::endl;

	std::filesystem::path temp_file = /*std::filesystem::temp_directory_path() /*/ "D:\\Tiles\\Test\\temp_tif_file.tif";
	std::string projection = terrainReader->getProjection();
	std::cout << "Projection: " << projection << std::endl;

	CANVAS_FLOAT terrain(new Canvas<float>(terrainBand));

	std::string sourcesFileName = "D:\\Tiles\\Test\\temp_srcs_file.srcs";
	bool interrupted = false;

	Timer timer;

	{
		GEOTIFF_READER directionsReader(new GdalTiffReader(temp_file.string(), width, height, 1));
		directionsReader->setProjection(projection);
		directionsReader->setGeoTransform(terrainReader->getGeoTransform());

		RASTER_BAND directionsBand(directionsReader->getRasterBand(1));
		CANVAS_BYTE directions(new Canvas<uint8_t>(directionsBand, true));
		directionsBand->setNoDataValue(50);

		std::vector<std::thread> threads;
		threads.reserve(threadsCount);

		std::ofstream sources(sourcesFileName, std::ios::binary | std::ios::out);

		std::cout << "---------------- FlowDirections Started! ----------------" << std::endl;

		Timer flowTimer;

		for (int i = 0; i < threadsCount; i++) {
			threads.emplace_back([this, &terrain, &directions, width, height, i, &sources, threadsCount, &interrupted]() {
				try {
					directionProcess(terrain, directions, width, height, i, sources, threadsCount);
				}
				catch (const std::runtime_error& exception) {
					progressCallback_ = [] { return 0; };

					std::cout << "<b>---------------- FlowDirections Failed! ----------------</b>" << std::endl;
					std::cout << "<b>Exception:</b> " << exception.what() << std::endl;

					interrupted = true;
				}
				catch (...) {

				}
				});
		}

		for (auto& thread : threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		if (interrupted) {
			return;
		}

		std::cout << "---------------- FlowDirections Finished! ----------------" << std::endl;
		std::cout << "Spent time: " << flowTimer.elapsedSeconds() << "s" << std::endl;
	}

	{
		GEOTIFF_READER directionsReader(new GdalTiffReader(temp_file.string()));

		RASTER_BAND directionsBand(directionsReader->getRasterBand(1));
		CANVAS_BYTE directions(new Canvas<uint8_t>(directionsBand));

		std::filesystem::path result_file = /*std::filesystem::temp_directory_path() /*/ "D:\\Tiles\\Test\\result_tif_file.tif";
		GEOTIFF_READER accumulationReader(new GdalTiffReader(result_file.string(), width, height, 1, true));
		accumulationReader->setProjection(projection);
		accumulationReader->setGeoTransform(terrainReader->getGeoTransform());

		RASTER_BAND accumaltionBand(accumulationReader->getRasterBand(1));
		CANVAS_UINT32 accumaltion(new Canvas<uint32_t>(accumaltionBand, true));

		std::vector<std::thread> threads;
		threads.reserve(threadsCount);

		std::ifstream sources(sourcesFileName, std::ios::binary | std::ios::out);

		sources.seekg(0, std::ios::end);
		size_t sourcesFileSize = sources.tellg();
		size_t sourcesCount = sourcesFileSize / sizeof(Source);
		sources.seekg(0, std::ios::beg);

		std::cout << "---------------- FlowAccumulation Started! ----------------" << std::endl;
		std::cout << "Sources count: " << sourcesCount << std::endl;

		std::queue<CHUNK_BORDERS> chunks;
		std::mutex chunkMutex;

		int chunkSize = 10000 * sizeof(Source);

		std::size_t start = 0;
		while (start < sourcesFileSize) {
			std::size_t end = min(start + chunkSize, sourcesFileSize);
			chunks.push({ start, end });
			start = end;
		}

		Timer flowTimer;

		for (int i = 0; i < threadsCount; i++) {
			threads.emplace_back([this, &accumaltion, &directions, i, &sources, threadsCount, &interrupted, &chunks, &chunkMutex, sourcesCount]() {
				try {
					while (true) {
						CHUNK_BORDERS chunk;

						{
							std::unique_lock lock(chunkMutex);

							if (chunks.empty()) {
								break;
							}

							chunk = chunks.front();
							chunks.pop();
						}

						accumulationProcess(accumaltion, directions, i, sources, chunk, threadsCount, sourcesCount);
					}
				}
				catch (const std::runtime_error& exception) {
					progressCallback_ = [] { return 0; };

					std::cout << "<b>---------------- FlowAccumulation Failed! ----------------</b>" << std::endl;
					std::cout << "<b>Exception:</b> " << exception.what() << std::endl;

					interrupted = true;
				}
				catch (...) {

				}
				});
		}

		for (auto& thread : threads) {
			if (thread.joinable()) {
				thread.join();
			}
		}

		if (interrupted) {
			return;
		}

		std::cout << "---------------- FlowAccumulation Finished! ----------------" << std::endl;
		std::cout << "Spent time: " << flowTimer.elapsedSeconds() << "s" << std::endl;
	}

	std::cout << "---------------- Finished ----------------" << std::endl;
	std::cout << "Spent time: " << timer.elapsedSeconds() << "s" << std::endl;
	std::cout << std::endl;
}

void Plugin::directionProcess(CANVAS_FLOAT& terrain, CANVAS_BYTE& directions, int width, int height, int index, std::ofstream& sources, int threadsCount) {
	static Barrier syncPoint;
	static std::mutex sourcesMutex;
	static std::atomic_bool interrupted;
	static std::atomic_int counter;

	int heightPerThread = height / threadsCount;
	int residualHeight = height - (heightPerThread * threadsCount);
	int rowOffset = heightPerThread * index;
	Source source;

	interrupted = false;
	counter = 0;

	int tileHeight = terrain->getHeight();
	progressCallback_ = [tileHeight]() -> int {
			return int(counter.load() / float(tileHeight * 2) * 100);
		};

	height = heightPerThread;
	if (index == threadsCount - 1) {
		height += residualHeight;
	}

	std::cout << "Thread Created ID: " << index << " (0x" << std::setfill('0') << std::setw(8) << std::right << std::this_thread::get_id() << ")\t Row Offset: " << rowOffset << "\t Height: " << height << std::endl;

	float precision = 0.001f;
	float border = 0.f;
	for (int j = rowOffset; j < rowOffset + height; j++) {
		for (int i = 0; i < width; i++) {
			if (interrupted.load(std::memory_order_relaxed)) {
				throw std::exception();
			}

			float from = terrain->at(i, j, index);
			auto dir = directions->at(i, j, index);
			float minSlope = 0;

			if (noData_ == from) {
				dir = 50;

				continue;
			}

			for (int dirY = 0; dirY < 3; dirY++) {
				for (int dirX = 0; dirX < 3; dirX++) {
					if (dirX == 1 && dirY == 1) {
						continue;
					}

					int curX = i + dirX - 1;
					int curY = j + dirY - 1;

					auto _to = terrain->at(curX, curY, -index);
					float to = 0;
					if (!_to.valid() || (_to.data() == noData_)) {
						border = from - precision;
						to = border;
					}
					else {
						to = _to;
					}

					float slope = scale_;

					if (!(dirX % 2) && !(dirY % 2)) {
						slope *= 1.41f;
					}

					slope = (to - from) / slope;
					if (slope < minSlope) {
						minSlope = slope;
						dir = dirY * 3 + dirX + 1;
					}
				}
			}

			if (!minSlope) {
				//interrupted = true;

				//throw std::runtime_error("Unexpected direction, min slope equals zero. Before trying again, use Fill Sinks.");
			}
		}
	
		counter.fetch_add(1, std::memory_order_relaxed);
	}

	syncPoint.wait(threadsCount, [] { return interrupted.load(std::memory_order_relaxed); });

	if (interrupted.load(std::memory_order_relaxed)) {
		throw std::exception();
	}

	std::cout << "Thread ID: " << index << " Looking for sources..." << std::endl;

	for (int j = rowOffset; j < rowOffset + height; j++) {
		for (int i = 0; i < width; i++) {
			int enters = 0;
			auto dir = directions->at(i, j, index);

			if (dir == 50) {
				continue;
			}

			for (int dirY = 0; dirY < 3; dirY++) {
				for (int dirX = 0; dirX < 3; dirX++) {
					if (dirX == 1 && dirY == 1) {
						continue;
					}

					int curX = i + dirX - 1;
					int curY = j + dirY - 1;

					auto curDir = directions->at(curX, curY, -index);
					if (!curDir.valid()) {
						continue;
					}

					if (dirY * 3 + dirX == 9 - abs(*(char*)&curDir.data())) {
						enters++;
					}
				}
			}

			if (!enters) {
				std::lock_guard lock(sourcesMutex);

				source = { i, j };
				sources.write((char*)&source, sizeof(source));
			}
			else if (enters > 1) {
				dir = -dir;
			}
		}

		counter.fetch_add(1, std::memory_order_relaxed);
	}
}

void Plugin::accumulationProcess(CANVAS_UINT32& accumulation, CANVAS_BYTE& directions, int index, std::ifstream& sourcesFile, CHUNK_BORDERS& chunk, int threadsCount, size_t sourcesCount) {
	static std::mutex sourcesMutex, writeMutex;
	static std::atomic_int64_t counter;

	size_t start = chunk.first;
	size_t end = chunk.second;

	std::vector<Source> sources((end - start) / sizeof(Source));

	if (!chunk.first) {
		counter = 0;
	}

	{
		std::lock_guard lock(sourcesMutex);

		sourcesFile.seekg(start, std::ios::beg);
		sourcesFile.read((char*)sources.data(), sources.size());
	}
	

	progressCallback_ = [sourcesCount]() -> int {
			return int(counter.load() / float(sourcesCount) * 100);
		};

	std::cout << "Thread ID: " << index << " (0x" << std::setfill('0') << std::setw(8) << std::right << std::this_thread::get_id() << ")\t Start Point: " << chunk.first << "\t End Point: " << chunk.second << std::endl;

	std::cout << sources.size() << std::endl;

	for (const auto source : sources) {
		int x = source.x;
		int y = source.y;

		{
			std::lock_guard lock(writeMutex);
			accumulation->at(x, y, index) = 1;
		}

		counter.fetch_add(1, std::memory_order_relaxed);
	}
}

int Plugin::getProgress() {
	return progressCallback_ ? progressCallback_() : 0;
}

EXPORT_API void Process(const char* name, float scale, int threadsCount) {
	try {
		Plugin::getInstance().process(name, scale, threadsCount);
	}
	catch (const std::runtime_error& exception) {
		std::cout << "<b>---------------- FlowDirections Failed! ----------------</b>" << std::endl;
		std::cout << "<b>Exception:</b> " << exception.what() << std::endl;
	}
}

EXPORT_API int GetProgress() {
	return Plugin::getInstance().getProgress();
}