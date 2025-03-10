#pragma once

#include <string>
#include <vector>
#include <optional>

class IRasterBand {
public:
	virtual int rasterByte(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;
	virtual int rasterInt(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;
	virtual int rasterUInt32(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;
	virtual int rasterUInt64(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;
	virtual int rasterFloat(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;
	virtual int rasterDouble(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;

	virtual int raster(int offsetX, int offsetY, int xSize, int ySize, void* buffer, int xBufferSize, int yBufferSize) = 0;

	virtual int getXSize() = 0;
	virtual int getYSize() = 0;
	virtual int getBand() = 0;

	virtual std::optional<double> getNoDataValue() = 0;
	virtual int setNoDataValue(double value) = 0;

	virtual std::pair<double, double> getRasterMinMax(bool approx) = 0;
	virtual int setStatistics(double min, double max, double mean, double stdDev) = 0;
	virtual int computeRasterMinMax() = 0;
};

class IGeoTiffReader {
public:
	virtual IRasterBand* getRasterBand(int num) = 0;
	virtual int getRasterCount() = 0;

	virtual std::string getProjection() = 0;
	virtual void setProjection(const std::string& projection) = 0;

	virtual std::vector<double> getGeoTransform() = 0;
	virtual void setGeoTransform(const std::vector<double>& geoTransform) = 0;
};