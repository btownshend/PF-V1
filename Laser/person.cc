
#include <string>
#include "person.h"
#include "dbg.h"

People *People::theInstance=NULL;

Person *People::getPerson(int id) {
    if (p.count(id)==0)
	p.insert(std::pair<int,Person>(id,Person(id)));
    return &p.at(id);
}

int People::handleOSCMessage(const char *path, const char *types, lo_arg **argv,int argc,lo_message msg) {
    dbg("People.handleOSCMessage",1)  << "Got message: " << path << "(" << types << ") from " << lo_address_get_url(lo_message_get_source(msg)) << std::endl;

    const char *tok=strtok((char *)path,"/");
    bool handled=false;
    if (strcmp(tok,"pf")==0) {
	tok=strtok(NULL,"/");
	if (strcmp(tok,"body")==0) {
	    if (strcmp(types,"iifffffffffffffffi")!=0) {
		dbg("People.handleOSCMessage",1) << path << " has unexpected types: " << types << std::endl;
		return 0;
	    }
	    int id=argv[1]->i;
	    Point position(argv[2]->f,argv[3]->f);
	    dbg("People.handleOSCMessage",1) << "id=" << id << ",pos=" << position << std::endl;
	    Person *person=getPerson(id);
	    person->set(position);
	    person->setStats(argv[12]->f,argv[14]->f);
	    handled=true;
	}
	else if (strcmp(tok,"leg")==0) {
	    int id=argv[1]->i;
	    int leg=argv[2]->i;
	    Point position(argv[4]->f,argv[5]->f);
	    Person *person=getPerson(id);
	    person->setLeg(leg,position);
	    dbg("People.handleOSCMessage",1) << "id=" << id << ", leg=" << leg << ", pos=" << position << std::endl;
	    handled=true;
	}
	else if (strcmp(tok,"body")==0) {
	    int id=argv[2]->i;
	    int gid=argv[9]->i;
	    int gsize=argv[10]->i;
	    Person *person=getPerson(id);
	    person->setGrouping(gid,gsize);
	    dbg("People.handleOSCMessage",1) << "id=" << id << ", group=" << gid << " with " << gsize << " people" << std::endl;
	    handled=true;
	}
	else if (strcmp(tok,"update")==0) {
	    // Not needed
	    handled=true;
	}
    }
    if (!handled) {
	dbg("People.handleOSCMessage",1) << "Unhanded message: " << path << ": parse failed at token: " << tok << std::endl;
    }
    
    return handled?0:1;
}

void People::incrementAge() {
    for (std::map<int,Person>::iterator a=p.begin(); a!=p.end();a++) {
	a->second.incrementAge();
	if (a->second.getAge() > MAXAGE) {
	    dbg("People.incrementAge",1) << "Connection " << a->first << " has age " << a->second.getAge() << "; deleting." << std::endl;
	    p.erase(a);
	}
    }
}
