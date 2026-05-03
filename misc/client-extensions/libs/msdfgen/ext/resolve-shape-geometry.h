
#pragma once

#include "../core/Shape.h"

namespace msdfgen {

/// Resolves any intersections within the shape by subdividing its contours using the native system library (Direct2D on Windows) and makes sure its contours have a consistent winding.
bool resolveShapeGeometry(Shape &shape);

}
