/*
 * likelihood.h
 *
 *  Created on: Mar 30, 2014
 *      Author: bst
 */

#ifndef LIKELIHOOD_H_
#define LIKELIHOOD_H_

#include <vector>
#include <assert.h>
#include <ostream>

class Target;

class Assignment {
 public:
    int track;
    const Target *target1, *target2;
    float like;
    Assignment(int t, const Target *t1, const Target *t2, float l) { track=t; target1=t1; target2=t2; like=l; }
    friend std::ostream& operator<<(std::ostream &s, const Assignment &a);
};

class Likelihood {
    std::vector<Assignment> entries;
public:
    void add(const Assignment &a) {
	entries.push_back(a);
    }
    Assignment maxLike() const { 
	assert (entries.size()>0);
	float best=0;
	for (unsigned int i=1;i<entries.size();i++)
	    if (entries[i].like>entries[best].like)
		best=i;
	return entries[best];
    }
    void remove(Assignment a) {
	std::vector<Assignment>::iterator iter;
	for (iter = entries.begin(); iter != entries.end(); ) {
	    if ((a.target1!=NULL && (a.target1==iter->target1 || a.target1==iter->target2)) || (a.target2!=NULL && (a.target2==iter->target2 || a.target2==iter->target1)) || a.track==iter->track)
		iter = entries.erase(iter);
	    else
		++iter;
	}
    }
    const Assignment &operator[](int i) { return entries[i]; }
    int size() const { return entries.size(); }
    friend std::ostream& operator<<(std::ostream &s, const Likelihood &l);
    
    Likelihood greedy();
    Likelihood smartassign();
};

#endif /* LIKELIHOOD_H_ */