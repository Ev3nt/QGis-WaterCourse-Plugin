#include "pch.h"

#include "Canvas.h"

template<typename T>
int Slot<T>::getIndex() {
	return index_;
}

template<typename T>
void Slot<T>::setIndex(int index) {
	index_ = index;
}

template<typename T>
void Slot<T>::setHeight(int height) {
	height_ = height;
}

template<typename T>
int Slot<T>::getHeight() {
	return height_;
}

template<typename T>
void Slot<T>::setOffsetY(int offsetY) {
	offsetY_ = offsetY;
}

template<typename T>
int Slot<T>::getOffsetY() {
	return offsetY_;
}

template<typename T>
Grid<T>& Slot<T>::getGrid() {
	return grid_;
}

template<typename T>
int& Slot<T>::getChangesCount() {
	return changesCounter_;
}

template<typename T>
Canvas<T>::Canvas(RASTER_BAND band, bool rareLocking, bool dumping) : band_(band), rareLocking_(rareLocking), dumping_(dumping) {
	if (!tileWidth_) {
		tileWidth_ = band->getXSize();
		tileHeight_ = band->getYSize();
	}

	step_ = (100 * 1024 * 1024) / (tileWidth_ * sizeof(T)); // 10MB
	
	//MEMORYSTATUSEX memory;
	//GlobalMemoryStatusEx(&memory);

	//slotLimit_ = (4ll * 1024 * 1024 * 1024 /*memory.ullAvailPhys * 0.8*/) / (tileWidth_ * sizeof(T) * step_);
}

template<typename T>
Canvas<T>::~Canvas() {
	if (dumping_) {
		for (const auto slot : slots_) {
			if (slot->getChangesCount()) {
				band_->raster(0, slot->getOffsetY(), tileWidth_, slot->getHeight(), slot->getGrid().data(), tileWidth_, slot->getHeight());
			}
		}

		band_->computeRasterMinMax(); // Should be optimized
	}
}

template<typename T>
DataHolder<T> Canvas<T>::at(int x, int y, int index) {
	int tileIndex = y / step_;
	int offsetY = tileIndex * step_;

	SLOT result;
	SLOT freeSlot;

	auto& picked = users_[index];

	std::unique_lock<Spinlock> lock;

	if (!rareLocking_) {
		lock = std::unique_lock(slotsMtx_);
	}

	if (!picked || picked->getIndex() != tileIndex) {
		if (rareLocking_) {
			lock = std::unique_lock(slotsMtx_);
		}

		for (auto& slot : slots_) {
			if (slot->getIndex() == tileIndex) {
				picked = result = slot;

				break;
			}
			else if (!freeSlot && slot.use_count() == 1) {
				freeSlot = slot;
			}
		}
	}
	else {
		result = picked;
	}

	if (!result) {
		if (!freeSlot) {
			freeSlot = std::make_shared<Slot<T>>();
			slots_.push_back(freeSlot);
		}
		else if (dumping_ && freeSlot->getChangesCount()) {
			band_->raster(0, freeSlot->getOffsetY(), tileWidth_, freeSlot->getHeight(), freeSlot->getGrid().data(), tileWidth_, freeSlot->getHeight());
		}

		picked = result = freeSlot;

		result->setIndex(tileIndex);
		result->setOffsetY(offsetY);

		auto& grid = result->getGrid();

		int height = min(tileHeight_ - offsetY, step_);
		if (grid.getWidth() != tileWidth_ || grid.getHeight() != height) {
			grid.resize(tileWidth_, height);
			result->setHeight(height);
		}

		auto& type = typeid(T);
		if (type == typeid(float)) {
			band_->rasterFloat(0, offsetY, tileWidth_, height, grid.data(), tileWidth_, height);
		}
		else if (type == typeid(int8_t)) {
			band_->rasterByte(0, offsetY, tileWidth_, height, grid.data(), tileWidth_, height);
		}
		else if (type == typeid(uint32_t)) {
			band_->rasterUInt32(0, offsetY, tileWidth_, height, grid.data(), tileWidth_, height);
		}
		else if (type == typeid(uint64_t)) {
			band_->rasterUInt64(0, offsetY, tileWidth_, height, grid.data(), tileWidth_, height);
		}
	}

	return DataHolder<T>(picked->getGrid().at(x, y - offsetY), picked.get());
}

template<typename T>
int Canvas<T>::getWidth() {
	return tileWidth_;
}

template<typename T>
int Canvas<T>::getHeight() {
	return tileHeight_;
}

template<typename T>
DataHolder<T>::DataHolder(T* value, Slot<T>* slot) : value_(value), slot_(slot) {
	if (value_) {
		//std::unique_lock lock(mtx_);

		previousValue_ = *value_;
	}
}

template<typename T>
DataHolder<T>::~DataHolder() {
	//mtxMap_.erase(value_);
}

template<typename T>
T DataHolder<T>::operator=(const T value) {
	//std::lock_guard lock(mtx_);
	if (value == previousValue_) {
		slot_->changesCounter_--;
	}
	else if (value != *value_) {
		if (*value_ == previousValue_) {
			slot_->changesCounter_++;
		}
	}

	(*value_) = value;

	return value;
}

template<typename T>
DataHolder<T>& DataHolder<T>::operator=(const DataHolder<T>& self) {
	previousValue_ = self.previousValue_;
	value_ = self.value_;

	return *this;
}

template<typename T>
DataHolder<T>::operator T() {
	//std::lock_guard lock(mtx_);

	return *value_;
}

template<typename T>
const T& DataHolder<T>::data() {
	//std::lock_guard lock(mtx_);

	return *value_;
}

template<typename T>
bool DataHolder<T>::valid() {
	return value_ != nullptr;
}


template class DataHolder<float>;
template class DataHolder<uint8_t>;
template class DataHolder<int8_t>;
template class DataHolder<uint64_t>;
template class DataHolder<uint32_t>;
template class DataHolder<double>;

template class Slot<float>;
template class Slot<uint8_t>;
template class Slot<int8_t>;
template class Slot<uint64_t>;
template class Slot<uint32_t>;
template class Slot<double>;

template class Canvas<float>;
template class Canvas<uint8_t>;
template class Canvas<int8_t>;
template class Canvas<uint64_t>;
template class Canvas<uint32_t>;
template class Canvas<double>;