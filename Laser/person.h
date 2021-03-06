#pragma once

#include <map>
#include "lo/lo.h"
#include "point.h"
#include "drawing.h"

class Leg {
    Point position;
 public:
    Leg() {;}
    void set(Point pos) {
	position=pos;
    }
    Point get() const { return position; }
};


class Person {
    int id;
    Point position;
    Leg legs[2];
    float legDiam, legSep;
    float facing;
    int gid, gsize;
    int age; 	// Age counter -- reset whenever something is set, increment when aged
    Attributes attributes;
    std::shared_ptr<Drawing> visual;
    Point visPosition;  // Location visual is currently positioned -- allows it to be incrementally translated to follow position
 public:
    Person(int _id) {id=_id; age=0; gid=0; gsize=1; set("allpeople",1.0,0.0); visual=std::shared_ptr<Drawing>(); }
    void incrementAge() {
	age++;
    }
    int getAge() const { return age; }
    void set(Point pos) {
	position=pos;
	age=0;
    }
    Point get() const { return position; }
    void setLeg(int leg,Point pos) {
	legs[leg].set(pos);
	age=0;
    }
    Point getLeg(int leg) { return legs[leg].get(); }
    void setStats(float _legDiam, float _legSep) {
	legDiam=_legDiam;
	legSep=_legSep;
    }
    void setGrouping(int _gid, int _gsize) {
	gid=_gid;
	gsize=_gsize;
    }
    float getLegDiam() const { return legDiam; }
    float getLegSep() const { return legSep; }
    float getBodyDiam() const { return legDiam+legSep; }
    int getGroupID() const { return gid; }
    int getGroupSize() const { return gsize; }

    // Get direction they are facing, in degrees;  0 is in the (0,1) direction, 90 is in the (-1,0) direction
    float getFacing() const { return facing; }
    void setFacing(float f) { facing=f; }
    
    void drawBody(Drawing &d) const ;
    void drawLegs(Drawing &d) const ;
    void draw(Drawing &d) const ;
    void set(std::string key, float value, float time) {
	if (value==0) {
	    dbg("Person.set",1) << "Removing " << key << " from " << id << std::endl;
	    attributes.erase(key);
	} else
	    attributes.set(key,Attribute(value,time));
	age=0;
    }
    Attributes getAttributes() const { return attributes; }
    void setVisual(const Drawing &d) {
	dbg("Person.setVisual",1) << "Set visual for " << id << " to drawing with " << d.getNumElements() << " elements." << std::endl;
	visual=std::shared_ptr<Drawing>(new Drawing(d));
	visPosition=Point(0,0);  
    }
    void clearVisuals() {
	visual.reset();
    }
};

class People {
    static const int MAXAGE=10;
    static People *theInstance;   // Singleton
    std::map<int,Person> p;
    Person *getOrCreatePerson(int id);

    People() {;}
    int handleOSCMessage_impl(const char *path, const char *types, lo_arg **argv,int argc,lo_message msg);
    void incrementAge_impl();
    void draw_impl(Drawing &d) const;
public:
    static int handleOSCMessage(const char *path, const char *types, lo_arg **argv,int argc,lo_message msg) {
	return instance()->handleOSCMessage_impl(path,types,argv,argc,msg);
    }
    static People *instance() {
	if (theInstance == NULL)
	    theInstance=new People();
	return theInstance;
    }
    static bool personExists(int id)  {
	return instance()->p.count(id)>0;
    }
    static void incrementAge() { instance()->incrementAge_impl(); }
    // Image onto drawing
    static void draw(Drawing &d)  { instance()->draw_impl(d); }
    // Set the drawing commands to image a person rather than using internal drawing routines
    static void setVisual(int uid, const Drawing &d) {
	if (personExists(uid)) {
	    dbg("People.setVisual",1) << "Adding visual for ID " << uid << std::endl;
	    instance()->p.at(uid).setVisual(d);
	} else {
	    dbg("People.setVisual",1) << "Missing ID " << uid << std::endl;
	}
    }
    static void clearVisuals() {
	for (std::map<int,Person>::iterator a=instance()->p.begin(); a!=instance()->p.end();a++)
	    a->second.clearVisuals();
    }
    std::vector<int> getIDs() const {
	std::vector<int> result;
	for (std::map<int,Person>::const_iterator a=p.begin(); a!=p.end();a++) {
	    result.push_back(a->first);
	}
	std::sort(result.begin(),result.end());
	return result;
    }

    Attributes getAttributes(int uid) const { 
	return p.at(uid).getAttributes();
    }

    int attrCount(std::string attr) const {
	int cnt=0;
	for (std::map<int,Person>::iterator a=instance()->p.begin(); a!=instance()->p.end();a++)
	    if (a->second.getAttributes().isSet(attr))
		cnt++;
	return cnt;
    }

    Person *getPerson(int id);
};
