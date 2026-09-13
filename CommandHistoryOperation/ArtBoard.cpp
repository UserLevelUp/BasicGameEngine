#include "pch.h"
#include "ArtBoard.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>

namespace {
	std::string Required(const OpNode& node, const std::string& key) {
		auto value = node.GetValue(key);
		if (value.empty()) {
			throw std::invalid_argument("Missing command attribute: " + key);
		}
		return value;
	}

	std::vector<double> Numbers(std::string text) {
		for (auto& character : text) {
			if (character == '[' || character == ']' || character == ',' || character == ';') {
				character = ' ';
			}
		}
		std::istringstream stream(text);
		stream.imbue(std::locale::classic());
		std::vector<double> values;
		while (stream >> std::ws && !stream.eof()) {
			double value;
			if (!(stream >> value) || !std::isfinite(value)) {
				throw std::invalid_argument("Command attributes must contain finite numbers.");
			}
			values.push_back(value);
		}
		return values;
	}

	double Number(const std::string& text) {
		auto values = Numbers(text);
		if (values.size() != 1) {
			throw std::invalid_argument("Expected one number.");
		}
		return values[0];
	}

	Point2 Point(const std::string& text) {
		auto values = Numbers(text);
		if (values.size() != 2) {
			throw std::invalid_argument("Expected two point coordinates.");
		}
		return { values[0], values[1] };
	}

	MatrixWithTime Transform(const OpNode& node) {
		MatrixWithTime transform;
		auto matrix = node.GetValue("Matrix");
		if (!matrix.empty()) {
			auto values = Numbers(matrix);
			if (values.size() == 4) {
				transform.matrix[0] = values[0];
				transform.matrix[1] = values[1];
				transform.matrix[4] = values[2];
				transform.matrix[5] = values[3];
			}
			else if (values.size() == 16) {
				std::copy(values.begin(), values.end(), transform.matrix.begin());
			}
			else {
				throw std::invalid_argument("Matrix must contain 4 or 16 numbers.");
			}
			if (transform.matrix[12] != 0 || transform.matrix[13] != 0
				|| transform.matrix[14] != 0 || transform.matrix[15] != 1) {
				throw std::invalid_argument("Expected an affine matrix.");
			}
		}
		auto time = node.GetValue("TimeComponent");
		if (!time.empty()) {
			if (time.back() == 's') {
				time.pop_back();
			}
			transform.timeSeconds = Number(time);
			if (transform.timeSeconds < 0) {
				throw std::invalid_argument("Time must not be negative.");
			}
		}
		return transform;
	}

	ArtBoard& Board(ArtBoardDocument& document, const std::string& name) {
		auto board = document.boards.find(name);
		if (board == document.boards.end()) {
			throw std::invalid_argument("Unknown artboard: " + name);
		}
		return board->second;
	}

	Shape* FindShape(ArtBoard& board, const std::string& id) {
		for (size_t i = 0; i < board.shapes.Size(); ++i) {
			if (board.shapes.At(i).id == id) {
				return &board.shapes.At(i);
			}
		}
		return nullptr;
	}

	Shape& TargetShape(ArtBoard& board, const OpNode& node) {
		auto shape = FindShape(board, Required(node, "ShapeId"));
		if (!shape) {
			throw std::invalid_argument("Unknown shape.");
		}
		return *shape;
	}

	void Draw(ArtBoardDocument& document, const OpNode& node, const std::string& type) {
		auto& board = Board(document, Required(node, "Board"));
		Shape shape;
		shape.id = Required(node, "ShapeId");
		if (FindShape(board, shape.id)) {
			throw std::invalid_argument("Shape identifiers must be unique within an artboard.");
		}
		shape.transform = Transform(node);
		auto color = node.GetValue("Color");
		if (!color.empty()) {
			auto values = Numbers(color);
			if (values.size() != 4 || std::any_of(values.begin(), values.end(), [](double value) { return value < 0 || value > 1; })) {
				throw std::invalid_argument("Color must contain four components between zero and one.");
			}
			std::copy(values.begin(), values.end(), shape.color.begin());
		}
		if (type == "DrawCircle") {
			auto radius = Number(Required(node, "Radius"));
			if (radius <= 0) {
				throw std::invalid_argument("Circle radius must be positive.");
			}
			shape.geometry = Circle{ Point(Required(node, "Center")), radius };
		}
		else if (type == "DrawLine") {
			shape.geometry = Line{ Point(Required(node, "Start")), Point(Required(node, "End")) };
		}
		else {
			auto dimensions = Point(Required(node, "Dimensions"));
			if (dimensions.x <= 0 || dimensions.y <= 0) {
				throw std::invalid_argument("Rectangle dimensions must be positive.");
			}
			shape.geometry = RectangleShape{ Point(Required(node, "TopLeft")), dimensions };
		}
		board.shapes.Add(shape);
	}

	int TickNumber(const std::string& text) {
		int tick = 0;
		auto result = std::from_chars(text.data(), text.data() + text.size(), tick);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || tick < 0) {
			throw std::invalid_argument("Tick numbers must be nonnegative integers.");
		}
		return tick;
	}

	void AppendTick(Shape& shape, int number, Point2 position, const MatrixWithTime& transform) {
		if (shape.animation.Size() && number <= shape.animation.At(shape.animation.Size() - 1).number) {
			throw std::invalid_argument("Animation ticks must be strictly increasing.");
		}
		if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
			throw std::invalid_argument("Animation positions must be finite.");
		}
		shape.animation.Add(AnimationTick{ number, position, transform });
	}

	using Mutation = std::function<void(ArtBoardDocument&, const OpNode&)>;

	void RegisterMutation(CommandHistoryOperation& history, const std::shared_ptr<ArtBoardDocument>& document,
		const std::string& type, Mutation mutation) {
		history.RegisterCommand(type, [document, mutation](const std::shared_ptr<OpNode>& node) {
			auto before = *document;
			auto after = before;
			mutation(after, *node);
			return CommandAction{
				[document, after] { *document = after; },
				[document, before] { *document = before; }
			};
		});
	}
}

void RegisterArtBoardCommands(CommandHistoryOperation& history, const std::shared_ptr<ArtBoardDocument>& document) {
	if (!document) {
		throw std::invalid_argument("An artboard document is required.");
	}
	RegisterMutation(history, document, "CreateArtBoard", [](ArtBoardDocument& state, const OpNode& node) {
		auto name = Required(node, "Board");
		auto animated = node.GetValue("Animated");
		if (!animated.empty() && animated != "true" && animated != "false") {
			throw std::invalid_argument("Animated must be true or false.");
		}
		if (!state.boards.emplace(name, ArtBoard{ name, animated == "true", {} }).second) {
			throw std::invalid_argument("Artboard already exists: " + name);
		}
	});
	for (const std::string type : { "DrawCircle", "DrawLine", "DrawRectangle" }) {
		RegisterMutation(history, document, type, [type](ArtBoardDocument& state, const OpNode& node) { Draw(state, node, type); });
	}
	RegisterMutation(history, document, "TranslationCommand", [](ArtBoardDocument& state, const OpNode& node) {
		Required(node, "Matrix");
		Required(node, "TimeComponent");
		auto& shape = TargetShape(Board(state, Required(node, "Board")), node);
		shape.transform = Transform(node);
	});
	RegisterMutation(history, document, "CombineArtBoards", [](ArtBoardDocument& state, const OpNode& node) {
		const auto& first = Board(state, Required(node, "StaticBoard"));
		const auto& second = Board(state, Required(node, "AnimatedBoard"));
		auto destination = Required(node, "destination_board_name");
		if (state.boards.count(destination)) {
			throw std::invalid_argument("Destination artboard already exists.");
		}
		ArtBoard combined{ destination, first.animated || second.animated, first.shapes };
		for (size_t i = 0; i < second.shapes.Size(); ++i) {
			const auto& shape = second.shapes.At(i);
			if (FindShape(combined, shape.id)) {
				throw std::invalid_argument("Combined shape identifiers must be unique.");
			}
			combined.shapes.Add(shape);
		}
		state.boards.emplace(destination, combined);
	});
	RegisterMutation(history, document, "AddAnimationTick", [](ArtBoardDocument& state, const OpNode& node) {
		auto& board = Board(state, Required(node, "Board"));
		auto& shape = TargetShape(board, node);
		AppendTick(shape, TickNumber(Required(node, "TickNumber")), Point(Required(node, "Position")), Transform(node));
		board.animated = true;
	});
	RegisterMutation(history, document, "AnimateMultipleSteps", [](ArtBoardDocument& state, const OpNode& node) {
		auto& board = Board(state, Required(node, "Board"));
		auto& shape = TargetShape(board, node);
		auto start = TickNumber(Required(node, "StartTick"));
		auto end = TickNumber(Required(node, "EndTick"));
		auto startPosition = Point(Required(node, "StartPosition"));
		auto endPosition = Point(Required(node, "EndPosition"));
		auto transform = Transform(node);
		if (end < start || (end == start && (startPosition.x != endPosition.x || startPosition.y != endPosition.y))) {
			throw std::invalid_argument("Animation ranges must be ordered and single ticks must have one position.");
		}
		for (int tick = start; ; ++tick) {
			double fraction = end == start ? 0.0 : static_cast<double>(tick - start) / (end - start);
			Point2 position{ startPosition.x * (1.0 - fraction) + endPosition.x * fraction,
				startPosition.y * (1.0 - fraction) + endPosition.y * fraction };
			AppendTick(shape, tick, position, transform);
			if (tick == end) {
				break;
			}
		}
		board.animated = true;
	});
}
