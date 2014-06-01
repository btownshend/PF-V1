#pragma once 
#include <ostream>
#include <vector>
#include <map>
#include <set>

#include "point.h"
#include "etherdream_bst.h"
#include "dbg.h"
#include "bezier.h"
#include "color.h"
#include "attributes.h"

class Transform;
class Drawing;

class Primitive {
protected:
    Color c;
    Attributes attrs;
 public:
    Primitive(Color _c): c(_c) { }
    void setAttributes(const Attributes &a) { attrs=a; }
    virtual ~Primitive() { ; }
    // Get list of discrete points spaced approximately by pointSpacing,  optimizes based on minimizing distance from priorPoint to first point
    // Converts from floor space to device coordinates in the process
    virtual std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const {
	return std::vector<etherdream_point>(0);
    }
    // Convert from a Point vector to an etherdream vector, applying the current transform and any attributes in attrs
    std::vector<etherdream_point> convert(const std::vector<Point> &pts, const Transform &transform) const;

    // Get score for rendering using givent transform -- higher is better
    // A shape which is completely on-screen has a score equal to 1.0+1.0/(average line-width)
    // If part of it is off-screen, then the score is frac(oncreen) (goes from 0.0 to 1.0)
    // Full offscreen has a score of 0
    virtual float getShapeScore(const Transform &transform) const {
	// Should be overridden
	assert(0);
    }

    virtual float getLength() const { return 0.0; }
};

class Circle: public Primitive {
    Point center;
    float radius;
 public:
    Circle(Point _c, float r, Color c): Primitive(c) {
	dbg("Circle",2) << "c=" <<_c << ", r=" << r << std::endl;
	center=_c; radius=r;
    }
    std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const;
    float getLength() const { return radius*2*M_PI; }
    float getShapeScore(const Transform &transform) const;
};

class Arc: public Primitive {
    Point center, p;
    float angle;
 public:
    Arc(Point _center, Point _p, float _angle, Color c): Primitive(c) { center=_center; p=_p; angle=_angle; }
    std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const;
    //    float getShapeScore(const Transform &transform) const;
};

class Line:public Primitive {
    Point p1,p2;
 public:
    Line(Point _p1, Point _p2, Color c): Primitive(c) { p1=_p1; p2=_p2;  }
    std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const;
    float getLength() const { return (p1-p2).norm(); }
    float getShapeScore(const Transform &transform) const;
};

class Cubic:public Primitive {
    Bezier b;
 public:
    Cubic(const std::vector<Point> &pts, Color c): Primitive(c), b(pts) {;}
    std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const;
    float getLength() const { return b.getLength(); }
    float getShapeScore(const Transform &transform) const;
};

class Polygon: public Primitive {
    std::vector<Point> points;
 public:
    Polygon(const std::vector<Point> _points,Color c): Primitive(c), points(_points) {;}
    std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const;
    float getLength() const;
    //    float getShapeScore(const Transform &transform) const;
};

class Composite: public Primitive {
    std::vector<std::shared_ptr<Primitive> > elements;
 public:
 Composite(): Primitive(Color(1,1,1)) {;}
    Composite(const Drawing &d);
    void append(std::shared_ptr<Primitive> p) { elements.push_back(p); }
    std::vector<etherdream_point> getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const;
    float getLength() const {
	float len=0;
	for (unsigned int i=0;i<elements.size();i++) {
	    len+=elements[i]->getLength();
	}
	return len;
    }
    float getShapeScore(const Transform &transform) const;
};


class Drawing {
    std::vector<std::shared_ptr<Primitive> > elements;
    int frame;  // Frame number that this drawing corresponds to (or -1 if unknown)
    bool inComposite;
    Attributes currentAttributes;
 public:
    Drawing() { frame=-1; inComposite=false; }

    void setFrame(int _frame) { frame=_frame; }
    int getFrame() const { return frame; }

    // Number of primitives (which gives number of jumps)
    int getNumElements() const { return elements.size(); }

    // Get length of current drawing in floor space
    float getLength() const {
	float len=0;
	for (unsigned int i=0;i<elements.size();i++) {
	    len+=elements[i]->getLength();
	}
	dbg("Drawing.getLength",3) << "length=" << len << " for " << elements.size() << " elements." << std::endl;
	return len;
    }

    // Get attributes, allowing them to be modified
    Attributes &getAttributes() { return currentAttributes; }

    // Clear drawing
    void clear() {
	dbg("Drawing.clear",5) << "Clearing " << elements.size() << " elements from frame " << frame << std::endl;
	elements.clear();  
	currentAttributes.clear();
	inComposite=false;
	frame=-1;
    }

    // Get quality score of reproduction for each of the elements within the drawing using the given transform
    std::map<int,float> getShapeScores(const Transform &transform) const;

    // Get a subset of the drawing containing only the given elements
    Drawing select(std::set<int> elements) const ;


    void append(std::shared_ptr<Primitive> prim) {
	prim->setAttributes(currentAttributes);
	if (inComposite) {
	    // Append to current composite 
	    if (elements.size()==0) {
		dbg("Drawing.append",1) << "inComposite but no elements" << std::endl;
		inComposite=false;
	    } else {
		std::shared_ptr<Composite> c=std::dynamic_pointer_cast<Composite>(elements.back());
		if (c==std::shared_ptr<Composite>()) {
		    dbg("Drawing.append",1) << "Last element is not a composite" << std::endl;
		    inComposite=false;
		} else {
		    c->append(prim);
		    return;
		}
	    }
	}
	elements.push_back(prim);
    }

    // Start a new shape
    void shapeBegin() {
	dbg("Drawing.shapeBegin",8) << "begin" << std::endl;
	if (inComposite) {
	    dbg("Drawing.shapeBegin",1) << "Was already in a composite -- ignoring" << std::endl;
	} else {
	    append(std::shared_ptr<Primitive>(new Composite()));
	    inComposite=true;
	}
    }

    void shapeEnd() {
	dbg("Drawing.shapeBegin",8) << "end" << std::endl;
	if (!inComposite) {
	    dbg("Drawing.shapeBegin",1) << "Was not in a composite -- ignoring" << std::endl;
	} else 
	    inComposite=false;
    }

    // Append another drawing to this one 
    void append(const Drawing &d) {
	for (int i=0;i<d.elements.size();i++)
	    elements.push_back(d.elements[i]);
    }

    // Add a circle to current drawing
    void drawCircle(Point center, float r, Color c) {
	dbg("Drawing.drawCircle",2) << "center=" << center << ", radius=" << r << ", color=" << c << std::endl;
	append(std::shared_ptr<Primitive>(new Circle(center,r,c)));
    }

    // Add a polygon  to current drawing
    void drawPolygon(const std::vector<Point> &pts, Color c) {
	append(std::shared_ptr<Primitive>(new Polygon(pts,c)));
    }

    // Add a arc to current drawing
    void drawArc(Point center, Point p, float angle, Color c) {
	dbg("Drawing.drawArc",2) << "center=" << center << ", Point =" << p << ", angleCW=" << angle << ", color=" << c << std::endl;
	append(std::shared_ptr<Primitive>(new Arc(center,p,angle,c)));
    }

    // Add a line to current drawing
    void drawLine(Point p1, Point p2, Color c) {
	append(std::shared_ptr<Primitive>(new Line(p1,p2,c)));
    }

    // Add a cubic to current drawing
    void drawCubic(Point p1, Point p2, Point p3, Point p4, Color c) {
	std::vector<Point> pts(4);
	pts[0]=p1;pts[1]=p2;pts[2]=p3;pts[3]=p4;
	append(std::shared_ptr<Primitive>(new Cubic(pts,c)));
    }

    // Convert drawing into a set of etherdream points
    // Takes into account transformation to make all lines uniform brightness (i.e. separation of points is constant in floor dimensions)
    std::vector<etherdream_point> getPoints(int targetNumPoints,const Transform &transform, float &spacing) const;

    // Convert to points using given floorspace spacing
    std::vector<etherdream_point> getPoints(float spacing,const Transform &transform) const;

    // Prune points that are not visible
    std::vector<etherdream_point> prune(const std::vector<etherdream_point> pts) const;
};
