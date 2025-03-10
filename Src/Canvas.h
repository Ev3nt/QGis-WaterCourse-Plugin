#pragma once

#include <map>

#include "GdalTiffReader.h"
#include "Grid.hpp"

typedef std::shared_ptr<IGeoTiffReader> GEOTIFF_READER;
typedef std::shared_ptr<IRasterBand> RASTER_BAND;

template<typename T>
class Slot;

template<typename T>
class DataHolder {
public:
	DataHolder(T* value, Slot<T>* slot);
	DataHolder() = default;
	~DataHolder();

	DataHolder<T>& operator=(const DataHolder<T>& self);
	T operator=(const T value);
	operator T();
	const T& data();

	bool valid();

private:
	T previousValue_ = T();
	T* value_ = nullptr;
	Slot<T>* slot_;
};

template<typename T>
class Slot {
public:
	Slot() = default;

	Grid<T>& getGrid();

	void setIndex(int index);
	int getIndex();

	void setHeight(int height);
	int getHeight();

	void setOffsetY(int offsetY);
	int getOffsetY();

	int& getChangesCount();

private:
	Grid<T> grid_;

	int index_ = 0;
	int height_ = 0;
	int offsetY_ = 0;

	int changesCounter_ = 0;

	friend DataHolder;
};

template<typename T>
class Canvas {
public:
	typedef std::shared_ptr<Slot<T>> SLOT;

	Canvas(RASTER_BAND band, bool dumping = false);
	~Canvas();

	DataHolder<T> at(int x, int y, int index = 0);

	int getWidth();
	int getHeight();

private:
	RASTER_BAND band_;
	std::vector<SLOT> slots_;
	std::map<int, SLOT> users_;

	int tileWidth_ = 0;
	int tileHeight_ = 0;
	int step_ = 1000;

	bool dumping_;

	std::mutex slotsMtx_;
};