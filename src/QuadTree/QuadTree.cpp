#include "QuadTree/QuadTree.h"
#include <stdexcept>


QuadTree::QuadTree(const Rectangle& boundary, int capacity, int depth)
    : boundary(boundary), capacity(capacity), divided(false), depth(depth) {}

void QuadTree::clear() {
    points.clear();
    children.clear();
    divided = false;
}

bool QuadTree::insert(const Point& point) {
    if (!contains(point)) {
        return false;
    }

    if (!divided) {
        if (static_cast<int>(points.size()) < capacity || depth == MAX_DEPTH) {
            points.push_back(point);
            return true;
        }

        subdivide();
    }

    if (children[0].insert(point)) return true;
    if (children[1].insert(point)) return true;
    if (children[2].insert(point)) return true;
    if (children[3].insert(point)) return true;

    return false;
}

std::vector<Point> QuadTree::query(const Rectangle& range) const {
    std::vector<Point> found;

    if (!intersects(range)) {
        return found;
    }

    for (const Point& p : points) {
        if (range.contains(p)) {
            found.push_back(p);
        }
    }

    if (divided) {
        for (const QuadTree& child : children) {
            std::vector<Point> childPoints = child.query(range);
            found.insert(found.end(), childPoints.begin(), childPoints.end());
        }
    }

    return found;
}

void QuadTree::deleteInRange(const Rectangle& range) {
    points.erase(std::remove_if(points.begin(), points.end(),
        [&](const Point& p) { return range.contains(p); }), points.end());

    if (divided) {
        for (QuadTree& child : children) {
            child.deleteInRange(range);
        }
    }
}

std::vector<Point> QuadTree::closest(const Point& searchPoint, int maxCount, double maxDistance) const {
    double sqMaxDistance = maxDistance * maxDistance;
    std::vector<Point> found;
    double furthestSqDistance = INFINITY;
    int foundSoFar = 0;
    kNearest(searchPoint, maxCount, sqMaxDistance, furthestSqDistance, foundSoFar, found);
    return found;
}

void QuadTree::forEach(void (*fn)(const Point&)) {
    for (const Point& p : points) {
        fn(p);
    }

    if (divided) {
        for (QuadTree& child : children) {
            child.forEach(fn);
        }
    }
}

QuadTree QuadTree::filter(bool (*fn)(const Point&)) const {
    QuadTree filtered(boundary, capacity, depth);

    for (const Point& p : points) {
        if (fn(p)) {
            filtered.insert(p);
        }
    }

    if (divided) {
        for (const QuadTree& child : children) {
            QuadTree childFiltered = child.filter(fn);
            filtered.merge(childFiltered, capacity);
        }
    }

    return filtered;
}

QuadTree QuadTree::merge(const QuadTree& other, int capacity) const {
    double left = std::min(boundary.left, other.boundary.left);
    double right = std::max(boundary.right, other.boundary.right);
    double top = std::min(boundary.top, other.boundary.top);
    double bottom = std::max(boundary.bottom, other.boundary.bottom);

    double height = bottom - top;
    double width = right - left;

    double midX = left + width / 2;
    double midY = top + height / 2;

    Rectangle mergedBoundary(midX, midY, width, height);
    QuadTree merged(mergedBoundary, capacity);

    merged.points = points;
    merged.divided = divided;

    if (divided) {
        for (int i = 0; i < 4; i++) {
            QuadTree child = children[i];
            QuadTree otherChild = other.children[i];
            QuadTree mergedChild = child.merge(otherChild, capacity);
            merged.children.push_back(mergedChild);
        }
    }

    return merged;
}

int QuadTree::length() const {
    int count = points.size();
    if (divided) {
        for (const QuadTree& child : children) {
            count += child.length();
        }
    }
    return count;
}

void QuadTree::subdivide() {
    double x = boundary.x;
    double y = boundary.y;
    double w = boundary.w / 2;
    double h = boundary.h / 2;

    children.push_back(QuadTree(Rectangle(x + w / 2, y - h / 2, w, h), capacity, depth + 1));
    children.push_back(QuadTree(Rectangle(x - w / 2, y - h / 2, w, h), capacity, depth + 1));
    children.push_back(QuadTree(Rectangle(x + w / 2, y + h / 2, w, h), capacity, depth + 1));
    children.push_back(QuadTree(Rectangle(x - w / 2, y + h / 2, w, h), capacity, depth + 1));

    divided = true;

    // Move points to children.
    for (const Point& p : points) {
        bool inserted = false;
        for (QuadTree& child : children) {
            if (child.insert(p)) {
                inserted = true;
                break;
            }
        }

        if (!inserted) {
            throw std::range_error("Capacity must be greater than 0");
        }
    }

    points.clear();
}

bool QuadTree::contains(const Point& point) const {
    return boundary.contains(point);
}

bool QuadTree::intersects(const Rectangle& range) const {
    return boundary.intersects(range);
}

double QuadTree::sqDistanceFrom(const Point& point) const {
    double dx = point.x - boundary.x;
    double dy = point.y - boundary.y;
    return dx * dx + dy * dy;
}

double QuadTree::distanceFrom(const Point& point) const {
    return std::sqrt(sqDistanceFrom(point));
}

std::vector<Point> QuadTree::kNearest(const Point& searchPoint, int maxCount, double sqMaxDistance, double furthestSqDistance, int foundSoFar, std::vector<Point>& found) const {
    // Note that there's no need to declare 'std::vector<Point> found' here.

    if (divided) {
        std::vector<QuadTree> sortedChildren = children; // Make a copy of children
        std::sort(sortedChildren.begin(), sortedChildren.end(), [&](const QuadTree& a, const QuadTree& b) {
            return a.boundary.sqDistanceFrom(searchPoint) < b.boundary.sqDistanceFrom(searchPoint);
        });

        for (const QuadTree& child : sortedChildren) {
            double sqDistance = child.boundary.sqDistanceFrom(searchPoint);
            if (sqDistance > sqMaxDistance) {
                continue;
            } else if (foundSoFar < maxCount || sqDistance < furthestSqDistance) {
                child.kNearest(searchPoint, maxCount, sqMaxDistance, furthestSqDistance, foundSoFar, found);
                furthestSqDistance = found.empty() ? 0.0 : found.back().sqDistanceFrom(searchPoint);
            }
        }
    } else {
        std::vector<Point> sortedPoints = points; // Make a copy of points
        std::sort(sortedPoints.begin(), sortedPoints.end(), [&](const Point& a, const Point& b) {
            return a.sqDistanceFrom(searchPoint) < b.sqDistanceFrom(searchPoint);
        });

        for (const Point& p : sortedPoints) {
            double sqDistance = p.sqDistanceFrom(searchPoint);
            if (sqDistance > sqMaxDistance) {
                continue;
            } else if (foundSoFar < maxCount || sqDistance < furthestSqDistance) {
                found.push_back(p);
                furthestSqDistance = found.empty() ? 0.0 : found.back().sqDistanceFrom(searchPoint);
                foundSoFar++;
            }
        }
    }

    // No need to sort 'found' here.

    return std::vector<Point>(found.begin(), found.begin() + std::min(maxCount, int(found.size())));
}

