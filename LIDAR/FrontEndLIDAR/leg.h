#pragma once

#include <ostream>
#include <vector>
#include "point.h"
#include "kalmanfilter.h"

class Person;
class LegStats;
class Vis;

class Leg {
    friend class Person;
    Point measurement;  // Current measurement
    Point position;   // Smoothed estimate of position
    float posvar;
    Point prevPosition;
    float prevposvar;
    Point velocity;
    std::vector<int> scanpts;
    float maxlike; 	   // Likelihood of maximum likelihood estimator
    int consecutiveInvisibleCount;
    // Keep the likelihood map so we can dump to matlab
    std::vector<float> like;
    int likenx, likeny;
    Point minval, maxval;
    void init(const Point &pt);
    KalmanFilter kf;
 public:
    Leg();
    Leg(const Point &pos);
    friend std::ostream &operator<<(std::ostream &s, const Leg &l);
    float getObsLike(const Point &pt, int frame,const LegStats &ls) const;
    Point getPosition() const { return position; }
    void predict(int nstep, float fps);
    void update(const Vis &vis, const std::vector<float> &bglike, const std::vector<int> fs, int nstep,float fps, const LegStats &ls, const Leg *otherLeg=0);
    void updateVisibility();
    void updateDiameterEstimates(const Vis &vis, LegStats &ls) const;   // Update given legstats diameter if possible
    void sendMessages(lo_address &addr, int frame, int id, int legnum) const;
    bool isVisible() const { return consecutiveInvisibleCount==0; }
};
