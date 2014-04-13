#ifndef LEGSTATS_H_
#define LEGSTATS_H_

#include <ostream>

class Person;

// Statistics shared between 2 legs
class LegStats {
    float diam,diamSigma;
    float sep,sepSigma; 	// average leg separation in meters
    float leftness;
    float facing,facingSEM;	// Direction in radians they are facing
 public:
    LegStats();
    float getDiam() const { return diam; }
    float getDiamSigma() const { return diamSigma; }
    float getSep() const { return sep; }
    float getSepSigma() const { return sepSigma; }
    float getLeftness() const { return leftness; }
    float getFacing() const { return facing; }
    float getFacingSEM() const { return facingSEM; }

    void update(const Person &p);
    friend std::ostream &operator<<(std::ostream &s, const LegStats &ls);
};

#endif /* LEGSTATS_H_ */