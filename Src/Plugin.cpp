#include "pch.h"

#include "Plugin.h"

#include <filesystem>
#include <queue>
#include <fstream>

#include "Barrier.h"
#include "Timer.h"

int Plugin::getDirection(int i, int j) {
	return i || j ? (i + 1) * 3 + (j + 1) + 1 : 0;
}

void Plugin::getOffsets(int direction, int* i, int* j) {
	if (direction) {
		direction--;

		*i = direction / 3 - 1;
		*j = direction % 3 - 1;
	}
	else {
		*i = 0;
		*j = 0;
	}
}

int Plugin::calculateFlowDirection(CANVAS_FLOAT& terrain, int x, int y, int index) {
	float maxSlope = -1;
	int direction = directionNoData_.value_or(0);

	auto from = terrain->at(x, y, index);
	if (!terrainNoData_.has_value() || from.data() != terrainNoData_) {
		direction = 0;

		for (int j = -1; j <= 1; j++) {
			for (int i = -1; i <= 1; i++) {
				if (i == 0 && j == 0) {
					continue;
				}

				int nx = x + i;
				int ny = y + j;

				auto to = terrain->at(nx, ny, -index);
				if (!to.valid() || to.data() == terrainNoData_) {
					continue;
				}

				float deltaZ = from - to;
				if (deltaZ <= 0) {
					continue;
				}

				float scale = 1.f;
				float distance = (abs(nx - x) + abs(ny - y) == 2) ? 1.41f : 1.f;
				float slope = deltaZ / (distance * scale);

				if (slope > maxSlope) {
					maxSlope = slope;
					direction = getDirection(i, j);
				}
			}
		}
	}

	return direction;
}

int Plugin::calculateEnters(CANVAS_BYTE& directions, int x, int y, int index) {
	int noData = directionNoData_.value();
	int enters = -1;

	auto direction = directions->at(x, y, index);
	if (direction != noData && direction != 0) {
		enters = 0;

		for (int j = -1; j <= 1; ++j) {
			for (int i = -1; i <= 1; ++i) {
				if (i == 0 && j == 0) {
					continue;
				}

				int nx = x + i;
				int ny = y + j;

				auto neighbour = directions->at(nx, ny, -index);
				if (!neighbour.valid() || neighbour == noData || direction == 0) {
					continue;
				}

				if (abs(neighbour) == getDirection(-i, -j)) {
					enters++;
				}
			}
		}
	}

	return enters;
}

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

	terrainNoData_ = terrainBand->getNoDataValue();
	std::cout << "Has nodata: " << (terrainNoData_ ? "yes (" + std::to_string(terrainNoData_.value()) + ")" : "no") << std::endl;

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
		directionsBand->setNoDataValue(directionNoData_.value());

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

	for (int j = rowOffset; j < rowOffset + height; j++) {
		for (int i = 0; i < width; i++) {
			if (interrupted.load(std::memory_order_relaxed)) {
				throw std::exception();
			}
			
			directions->at(i, j, index) = calculateFlowDirection(terrain, i, j, index);
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
			auto direction = directions->at(i, j, index);
			int enters = calculateEnters(directions, i, j, index);

			if (!enters) {
				std::lock_guard lock(sourcesMutex);

				source = { i, j };
				sources.write((char*)&source, sizeof(source));
			}
			else if (enters > 1) {
				direction = -direction;
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

		uint32_t value = 1;

		while (true) {
			auto neighbourHolder = directions->at(x, y, index);

			if (!neighbourHolder.valid()) {
				break;
			}

			int direction = *(char*)&neighbourHolder.data();

			std::unique_lock<std::mutex> lock;
			
			auto data = accumulation->at(x, y, index);

			if (direction < 0) {
				bool isOwner = true;

				lock = std::unique_lock(writeMutex);

				for (int j = -1; j <= 1; j++) {
					for (int i = -1; i <= 1; i++) {
						if (!isOwner) {
							break;
						}

						if (i == 0 && j == 0) {
							continue;
						}

						int nx = x + i;
						int ny = y + j;

						auto neighbourDirection = directions->at(nx, ny, -index);
						if (!neighbourDirection.valid()) {
							continue;
						}

						if (abs(*(char*)&neighbourDirection.data()) != getDirection(-i, -j)) {
							continue;
						}

						auto neighbour = accumulation->at(nx, ny, -index);

						if (neighbour == 0) {
							isOwner = false;

							break;
						}
						else {
							value += neighbour;
						}
					}
				}

				if (!isOwner) {
					break;
				}

				value++;
			}

			data = value++;

			int i, j;
			getOffsets(direction, &i, &j);

			x += i;
			y += j;
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