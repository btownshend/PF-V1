#include "point.h"

std::ostream& operator<<(std::ostream &s, const Point &p) {
    s << "(" << p.x << "," << p.y << ")";
    return s;
}


// Calculate distance from a line segment define by two points and another point
float  segment2pt(const Point &l1, const Point &l2, const Point &p) {
    Point D=l2-l1;
    Point p1=p-l1;
    float u=D.dot(p)/D.dot(D);
    if (u<=0)
	return p1.norm();
    else if (u>=1)
	return (p-l2).norm();
    else 
	return (p1-D*u).norm();
}

