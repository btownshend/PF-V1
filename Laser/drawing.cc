#include <iostream>
#include <cmath>
#include "drawing.h"
#include "dbg.h"
#include "opencv2/imgproc/imgproc.hpp"

std::ostream& operator<<(std::ostream& s, const Color &col) {
    return s << "[" << col.r << "," << col.g << "," << col.b << "]";
}

Transform::Transform() {
    transform.resize(6);
    clear();
}

void Transform::clear() {
    transform=cv::Mat::eye(3,3,CV_32F);
}

// Find the perspective transform that best matches the 3 points loaded
void Transform::setTransform() {
    if (floorpts.size() != 4) {
	std::cerr << "setTransform() called after " <<floorpts.size() << " points added -- must be exactly 4" << std::endl;
    } else {
	transform=cv::getPerspectiveTransform(floorpts,devpts);
    }
    floorpts.clear();
    devpts.clear();
}

etherdream_point Transform::mapToDevice(Point floorPt,Color c) const {
    etherdream_point p;
    std::vector<cv::Point2f> src(1);
    src[0].x=floorPt.X();
    src[0].y=floorPt.Y();
    std::vector<cv::Point2f> dst;
    cv::perspectiveTransform(src,dst,transform);
    int x=round(dst[0].x);
    if (x<-32768)
	p.x=-32768;
    else if (x>32767)
	p.x=32767;
    else
	p.x=x;

    int y=round(dst[0].y);
    if (y<-32768)
	p.y=-32768;
    else if (y>32767)
	p.y=32767;
    else
	p.y=y;

    p.r=(int)(c.red() * 65535);
    p.g=(int)(c.green() * 65535);
    p.b=(int)(c.blue() * 65535);

    dbg("Transform.mapToDevice",4) << floorPt << " -> " << "[" << p.x << "," <<p.y << "]" << std::endl;
    return p;
}

std::vector<etherdream_point> Transform::mapToDevice(std::vector<Point> floorPts,Color c) const {
    std::vector<etherdream_point> result(floorPts.size());
    for (unsigned int i=0;i<floorPts.size();i++)
	result[i]=mapToDevice(floorPts[i],c);
    return result;
}

std::vector<etherdream_point> Circle::getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const {
    int npoints=std::ceil(getLength()/pointSpacing)+1;
    if (npoints < 5) {
	dbg("Circle.getPoints",1) << "Circle of radius " << radius << " with point spacing of " << pointSpacing << " only had " << npoints << " points; increasing to 5" << std::endl;
	npoints=5;
    }
    std::vector<etherdream_point> result(npoints);
    struct etherdream_point *pt = &result[0];
    float initphase;
    etherdream_point devcenter=transform.mapToDevice(center,c);
    if (priorPoint==0 || (priorPoint->x==devcenter.x && priorPoint->y==devcenter.y))
	initphase=0;
    else {
	// Find phase closest to prior point
	Point delta(priorPoint->x-devcenter.x,priorPoint->y-devcenter.y);
	initphase=atan2(delta.Y(),delta.X());
	dbg("Circle.getPoints",3) << "Delta=" << delta << ", initial phase = " << initphase << std::endl;
    }
    for (int i = 0; i < npoints; i++,pt++) {
	float phase = i * 2.0 * M_PI /(npoints-1)+initphase;
	*pt = transform.mapToDevice(Point(cos(phase) * radius + center.X(), sin(phase) * radius + center.Y()),c);
    }
    dbg("Circle.getPoints",2) << "Converted to " << result.size() << " points." << std::endl;
    return result;
}

std::vector<etherdream_point> Line::getPoints(float pointSpacing,const Transform &transform,const etherdream_point *priorPoint) const {
    int npoints=std::max(2,(int)std::ceil(getLength()/pointSpacing)+1);
    bool swap=false;
    if (priorPoint!=NULL) {
	// Check if line direction should be swapped
	etherdream_point devp1=transform.mapToDevice(p1,c);
	etherdream_point devp2=transform.mapToDevice(p2,c);
	int d1=std::max(abs(priorPoint->x-devp1.x),abs(priorPoint->y-devp1.y));
	int d2=std::max(abs(priorPoint->x-devp2.x),abs(priorPoint->y-devp2.y));
	if (d2<d1) {
	    dbg("Line.getPoints",3) << "Swapping line endpoints; d1=" << d1 << ", d2=" << d2 << std::endl;
	    swap=true;
	}
    }
    std::vector<etherdream_point> result(npoints);
    for (int i = 0; i < npoints; i++) {
	float rpos=i*1.0/(npoints-1);
	if (swap) rpos=1-rpos;
	result[i] = transform.mapToDevice(p1+(p2-p1)*rpos,c);
    }
    dbg("Line.getPoints",2) << "Converted to " << result.size() << " points." << std::endl;
    return result;
}


// Convert to points using given floorspace spacing
std::vector<etherdream_point> Drawing::getPoints(float spacing) const {
    dbg("Drawing.getPoints",2) << "getPoints(" << spacing << ")" << std::endl;
    std::vector<etherdream_point>  result;
    for (unsigned int i=0;i<elements.size();i++) {
	std::vector<etherdream_point> newpoints;
	if (result.size()>0)
	    newpoints = elements[i]->getPoints(spacing,transform,&result.back());
	else
	    newpoints = elements[i]->getPoints(spacing,transform,NULL);
	if (result.size()>0 && newpoints.size()>0)  {
	    // Insert blanks first
	    std::vector<etherdream_point> blanks = Laser::getBlanks(result.back(),newpoints.front());
	    result.insert(result.end(), blanks.begin(), blanks.end());
	}
	result.insert(result.end(), newpoints.begin(), newpoints.end());
    }
    // Add blanks for final skew back to start of figure
    if (result.size()>0) {
	std::vector<etherdream_point> blanks = Laser::getBlanks(result.front(),result.back());
	result.insert(result.end(), blanks.begin(), blanks.end());
    }
    dbg("Drawing.getPoints",2) << "Converted to " << result.size() << " points." << std::endl;
    for (unsigned int i=0;i<result.size();i++) {
	dbg("Drawing.getPoints",5)  << "pt[" << i << "] = " << result[i].x << "," << result[i].y << " G=" << result[i].g << std::endl;
    }
    return result;
}

// Convert drawing into a set of etherdream points
// Takes into account transformation to make all lines uniform brightness (i.e. separation of points is constant in floor dimensions)
std::vector<etherdream_point> Drawing::getPoints(int targetNumPoints) const {
    std::vector<etherdream_point> result = getPoints(getLength()/targetNumPoints);
    dbg("Drawing.getPoints",2) << "Initial point count = " << result.size() << " compared to planned " << targetNumPoints << " for " << elements.size() << " elements." << std::endl;
    if (targetNumPoints < (int)result.size()) {
	// Redo with a reduced number of desired points (to account for inserted blanks)
	int requestPoints =targetNumPoints-(result.size()-targetNumPoints);
	if (requestPoints < targetNumPoints/2) {
	    // Would be too few, leave as in and run slower
	    dbg("Drawing.getPoints",2) << "Fully compensating for blanks would not leave enough points" << std::endl;
	    requestPoints = targetNumPoints/2;
	}
	result = getPoints(getLength()/requestPoints);
	dbg("Drawing.getPoints",2) << "Final point count = " << result.size() << std::endl;
    }
    return result;
}
