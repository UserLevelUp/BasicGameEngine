#ifndef ARTBOARD_H
#define ARTBOARD_H

#include "CommandHistoryOperation.h"
#include <array>
#include <list>
#include <stdexcept>
#include <variant>

// Value semantics keep undo snapshots independent; each batch holds at most ten items.
template<typename T>
class TenItemCollection {
public:
	void Add(const T& item) {
		if (collections_.empty() || collections_.back().size() == 10) {
			collections_.emplace_back();
			collections_.back().reserve(10);
		}
		collections_.back().push_back(item);
	}

	size_t Size() const {
		size_t size = 0;
		for (const auto& collection : collections_) {
			size += collection.size();
		}
		return size;
	}

	T& At(size_t index) {
		for (auto& collection : collections_) {
			if (index < collection.size()) {
				return collection[index];
			}
			index -= collection.size();
		}
		throw std::out_of_range("Collection index is out of range.");
	}

	const T& At(size_t index) const {
		for (const auto& collection : collections_) {
			if (index < collection.size()) {
				return collection[index];
			}
			index -= collection.size();
		}
		throw std::out_of_range("Collection index is out of range.");
	}

	const std::list<std::vector<T>>& GetCollections() const { return collections_; }

private:
	std::list<std::vector<T>> collections_;
};

struct Point2 {
	double x = 0;
	double y = 0;
};

struct MatrixWithTime {
	// Row-major 4x4 matrix applied to column vectors; time is separate from spatial coordinates.
	std::array<double, 16> matrix{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
	double timeSeconds = 0;

	Point2 Apply(Point2 point) const {
		return { matrix[0] * point.x + matrix[1] * point.y + matrix[3],
			matrix[4] * point.x + matrix[5] * point.y + matrix[7] };
	}
};

struct Circle { Point2 center; double radius = 0; };
struct Line { Point2 start; Point2 end; };
struct RectangleShape { Point2 topLeft; Point2 dimensions; };

struct AnimationTick {
	int number = 0;
	Point2 position;
	MatrixWithTime transform;
};

struct Shape {
	std::string id;
	std::variant<Circle, Line, RectangleShape> geometry;
	MatrixWithTime transform;
	std::array<double, 4> color{ 0, 0, 0, 1 };
	TenItemCollection<AnimationTick> animation;

	Point2 PositionAtTick(int tick) const {
		for (size_t i = 0; i < animation.Size(); ++i) {
			const auto& sample = animation.At(i);
			if (sample.number == tick) {
				return sample.transform.Apply(sample.position);
			}
		}
		throw std::out_of_range("Animation tick does not exist.");
	}
};

struct ArtBoard {
	std::string name;
	bool animated = false;
	TenItemCollection<Shape> shapes;
};

struct ArtBoardDocument {
	std::map<std::string, ArtBoard> boards;
};

COMMANDHISTORYOPERATION_API void RegisterArtBoardCommands(CommandHistoryOperation& history,
	const std::shared_ptr<ArtBoardDocument>& document);

#endif // ARTBOARD_H
