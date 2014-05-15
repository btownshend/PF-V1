/*
 * background.h
 *
 *  Created on: Mar 25, 2014
 *      Author: bst
 */

#pragma once
#include <vector>
#include "lo/lo.h"
#include "sickio.h"

class Background {
    static const int NRANGES=3;

    int nupdates;
    std::vector<float> range[NRANGES];   // Range in mm of background for NRANGES values/scan
    std::vector<float> freq[NRANGES];
    float scanRes;

    void swap(int k, int i, int j);
    void setup(const SickIO &sick);
public:
    Background();
    // Return probability of each scan pixel being part of background (fixed structures not to be considered targets)
    std::vector<float> like(const SickIO &sick) const;
    void update(const SickIO &sick, const std::vector<int> &assignments, bool all=false);
    mxArray *convertToMX() const;
    const std::vector<float> &getRange(int i) const { return range[i]; }
    const std::vector<float> &getFreq(int i) const { return freq[i]; }
    float getScanRes() const { return scanRes; }

    // Send /pf/background OSC message
    void sendMessages(lo_address &addr,int scanpt) const;
};
