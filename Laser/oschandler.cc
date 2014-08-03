#include <iomanip>
#include <set>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <strings.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <dbg.h>
#include "oschandler.h"
#include "lasers.h"
#include "video.h"
#include "point.h"
#include "touchosc.h"
#include "connections.h"
#include "person.h"
#include "groups.h"
#include "music.h"
#include "svg.h"

static void error(int num, const char *msg, const char *path)
{
	fprintf(stderr,"liblo server error %d in path %s: %s\n", num, path, msg);
}

/* catch any incoming messages and display them. returning 1 means that the
 * message has not been fully handled and the server should try other methods */
static int generic_handler(const char *path, const char *types, lo_arg **argv,int argc, lo_message msg , void *user_data) {
    dbg("generic_handler",5) << "Received message: " << path << ", with types: " << types << std::endl;
    int nothandled=1;
    if (strncmp(path,"/ui/",4)==0) {
	nothandled=TouchOSC::handleOSCMessage(path,types,argv,argc,msg);
    } else if (strncmp(path,"/conductor/",11)==0) {
	nothandled= Connections::handleOSCMessage(path,types,argv,argc,msg);
    } else if (strncmp(path,"/soundui/",9)==0) {
	nothandled= Music::instance()->handleOSCMessage(path,types,argv,argc,msg);
    }
    if (nothandled) {
	static std::set<std::string> noted;  // Already noted
	if (noted.count(std::string(path)+types) == 0) {
	    noted.insert(std::string(path)+types);
	    int i;
	    printf( "Unhandled Message Rcvd: %s (", path);
	    for (i=0; i<argc; i++) {
		printf("%c",types[i]);
	    }
	    printf( "): ");
	    for (i=0; i<argc; i++) {
		lo_arg_pp((lo_type)types[i], argv[i]);
		printf( ", ");
	    }
	    printf("\n");
	    fflush(stdout);
	}
    }
    return nothandled;
}

static bool doQuit = false;

static int quit_handler(const char *, const char *, lo_arg **, int, lo_message , void *) {
	printf("Received /quit command, quitting\n");
	doQuit = true;
	return 0;
}

/* Handler stubs */
static int ping_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->ping(msg,argv[0]->i); return 0; }
static int start_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->startStop(true); return 0; }
static int stop_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->startStop(false); return 0; }

// Laser settings
static int setPPS_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setPPS((int)argv[0]->f); return 0; }
static int setPreBlanking_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setPreBlanking((int)argv[0]->f); return 0; }
static int setPostBlanking_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setPostBlanking((int)argv[0]->f); return 0; }
static int setPoints_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setPoints((int)argv[0]->f); return 0; }
static int setSkew_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setSkew((int)argv[0]->f); return 0; }

// Attributes
static int setColor_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setColor(Color(argv[0]->f,argv[1]->f,argv[2]->f)); return 0; }
static int setDensity_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setDensity(argv[0]->f); return 0; }

// Primitives
static int conx_begin_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->conxBegin(&argv[0]->s); return 0; }
static int conx_end_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->conxEnd(&argv[0]->s); return 0; }
static int cell_begin_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->cellBegin(argv[0]->i); return 0; }
static int cell_end_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->cellEnd(argv[0]->i); return 0; }
static int bg_begin_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->bgBegin(); return 0; }
static int bg_end_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->bgEnd(); return 0; }
static int circle_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->circle(Point(argv[0]->f,argv[1]->f),argv[2]->f); return 0; }
static int arc_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->arc(Point(argv[0]->f,argv[1]->f),Point(argv[2]->f,argv[3]->f),argv[4]->f); return 0; }
static int cubic_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->cubic(Point(argv[0]->f,argv[1]->f),Point(argv[2]->f,argv[3]->f),Point(argv[4]->f,argv[5]->f),Point(argv[6]->f,argv[7]->f)); return 0; }
static int line_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->line(Point(argv[0]->f,argv[1]->f),Point(argv[2]->f,argv[3]->f)); return 0; }
static int svgfile_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->svgfile(&argv[0]->s,Point(argv[1]->f,argv[2]->f),argv[3]->f,argv[4]->f); return 0; }

// Transforms
static int map_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->map(argv[0]->i,argv[1]->i,Point(argv[2]->f,argv[3]->f),Point(argv[4]->f,argv[5]->f)); return 0; }
//static int setTransform_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setTransform(argv[0]->i); return 0; }

// Sound
static int beat_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    Music::instance()->setBeat(argv[0]->i,argv[1]->i); return 0; }
static int tempo_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    Music::instance()->setTempo(argv[0]->f); return 0; }

// Draw
static int update_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->update(argv[0]->i); return 0; }

// pf 
//static int pfupdate_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {   /* ((OSCHandler *)user_data)->circle(Point(argv[3]->f,argv[4]->f),.30);  */ return 0; }
//static int pfbody_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {   ((OSCHandler *)user_data)->pfbody(Point(argv[2]->f,argv[3]->f)); return 0; }
//static int pfleg_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->pfleg(Point(argv[4]->f,argv[5]->f)); return 0; }
static int pfframe_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->pfframe(argv[0]->i); return 0; }
static int pfsetminx_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setMinX(argv[0]->f);  return 0; }
static int pfsetminy_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setMinY(argv[0]->f); return 0; }
static int pfsetmaxx_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setMaxX(argv[0]->f); return 0; }
static int pfsetmaxy_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->setMaxY(argv[0]->f); return 0; }
static int pfbackground_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->pfbackground(argv[0]->i,argv[1]->i,argv[2]->f,argv[3]->f); return 0; }

static int person_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {   return People::handleOSCMessage(path,types,argv,argc,msg);  }
static int group_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {   return Groups::handleOSCMessage(path,types,argv,argc,msg);  }
static int range_handler(const char *path, const char *types, lo_arg **argv, int argc,lo_message msg, void *user_data) {    ((OSCHandler *)user_data)->range(argv[0]->i,argv[1]->i,argv[2]->i,argv[3]->i,argv[4]->i,argv[5]->i,argv[6]); return 0; }

void OSCHandler::range(int id, int frame, int sec, int usec, int echo, int nmeasure, lo_blob data) {
    dbg("OSCHandler.range",3) << "Got range(id=" << id << ", frame=" << frame << ", T=(" << sec << "," << usec << "), echo=" << echo << ", n=" << nmeasure << ", blob size=" << lo_blob_datasize(data) << std::endl;
    if (lo_blob_datasize(data) != nmeasure*sizeof(ranges.ranges[0])) {
	dbg("OSCHandler.range",1) << "Unexpected blob size: " << lo_blob_datasize(data) << " (expected " << nmeasure*4 << ")" << std::endl;
	return;
    }
    if (echo==0) {
	// Save the range data
	ranges.ranges.resize(nmeasure);
	for (int i=0;i<nmeasure;i++)
	    ranges.ranges[i]=((unsigned int *)data)[i]/1000.0;
	dbg("OSCHandler.range",3) << "Save " << ranges.size() << " ranges (max="  << *std::max_element(ranges.ranges.begin(),ranges.ranges.end()) << ")" << std::endl;
    }
};

OSCHandler::OSCHandler(int port, std::shared_ptr<Lasers> _lasers, std::shared_ptr<Video> _video) : lasers(_lasers), video(_video),  currentColor(0.0,1.0,0.0) {
    dbg("OSCHandler",1) << "OSCHandler::OSCHandler()" << std::endl;
    currentDensity=1.0;
    minx=-5; maxx=5;
    miny=0; maxy=0;

    gettimeofday(&lastFrameTime,0);
	serverPort=port;

	/* start a new server on OSC port  */
	char cbuf[10];
	sprintf(cbuf,"%d", serverPort);
	s = lo_server_new(cbuf, error);
	if (s==0) {
		fprintf(stderr,"Unable to start server on port %d -- perhaps another instance is running\n", serverPort);
		exit(1);
	}
	printf("Started server on port %d\n", serverPort);

	/* add method that will match the path /quit with no args */
	lo_server_add_method(s, "/quit", "", quit_handler, NULL);

	/* add methods */
	lo_server_add_method(s,"/laser/start","",start_handler,this);
	lo_server_add_method(s,"/laser/stop","",stop_handler,this);

	/* Link management */
	lo_server_add_method(s,"/ping","i",ping_handler,this);

	/* Attributes */
	lo_server_add_method(s,"/laser/set/color","fff",setColor_handler,this);
	lo_server_add_method(s,"/laser/set/density","f",setDensity_handler,this);

	/* Laser settings */
	lo_server_add_method(s,"/ui/laser/pps","f",setPPS_handler,this);
	lo_server_add_method(s,"/ui/laser/points","f",setPoints_handler,this);
	lo_server_add_method(s,"/ui/laser/postblank","f",setPostBlanking_handler,this);
	lo_server_add_method(s,"/ui/laser/preblank","f",setPreBlanking_handler,this);
	lo_server_add_method(s,"/ui/laser/skew","f",setSkew_handler,this);

	/* Primitives */
	lo_server_add_method(s,"/laser/conx/begin","s",conx_begin_handler,this);
	lo_server_add_method(s,"/laser/conx/end","s",conx_end_handler,this);
	lo_server_add_method(s,"/laser/cell/begin","i",cell_begin_handler,this);
	lo_server_add_method(s,"/laser/cell/end","i",cell_end_handler,this);
	lo_server_add_method(s,"/laser/bg/begin","",bg_begin_handler,this);
	lo_server_add_method(s,"/laser/bg/end","",bg_end_handler,this);
	
	lo_server_add_method(s,"/laser/circle","fff",circle_handler,this);
	lo_server_add_method(s,"/laser/arc","fffff",arc_handler,this);
	lo_server_add_method(s,"/laser/bezier/cubic","ffffffff",cubic_handler,this);
	lo_server_add_method(s,"/laser/line","ffff",line_handler,this);
	lo_server_add_method(s,"/laser/svgfile","sffff",svgfile_handler,this);

	/* Transforms */
	lo_server_add_method(s,"/laser/map","iiffff",map_handler,this);
	//	lo_server_add_method(s,"/laser/settransform","i",setTransform_handler,this);

	/* Draw */
	lo_server_add_method(s,"/laser/update","i",update_handler,this);

	/* Sound */
	lo_server_add_method(s,"/sound/beat","ii",beat_handler,this);
	lo_server_add_method(s,"/sound/tempo","f",tempo_handler,this);
	

	/* PF */
	lo_server_add_method(s,"/pf/frame","i",pfframe_handler,this);
	//lo_server_add_method(s,"/pf/update","ififfffffiii",pfupdate_handler,this);
	//lo_server_add_method(s,"/pf/body","iifffffffffffffffi",pfbody_handler,this);
	//lo_server_add_method(s,"/pf/leg","iiiiffffffffi",pfleg_handler,this);

	lo_server_add_method(s,"/pf/update","ififfffffiii",person_handler,this);
	lo_server_add_method(s,"/pf/body","iifffffffffffffffi",person_handler,this);
	lo_server_add_method(s,"/pf/leg","iiiiffffffffi",person_handler,this);
	lo_server_add_method(s,"/pf/group","iiiffff",group_handler,this);
	lo_server_add_method(s,"/conductor/attr","siff",person_handler,this);
	lo_server_add_method(s,"/conductor/gattr","siff",group_handler,this);

	lo_server_add_method(s,"/pf/set/minx","f",pfsetminx_handler,this);
	lo_server_add_method(s,"/pf/set/maxx","f",pfsetmaxx_handler,this);
	lo_server_add_method(s,"/pf/set/miny","f",pfsetminy_handler,this);
	lo_server_add_method(s,"/pf/set/maxy","f",pfsetmaxy_handler,this);
	lo_server_add_method(s,"/pf/background","iiff",pfbackground_handler,this);

	lo_server_add_method(s,"/vis/range","iiiiiib",range_handler,this);

	/* add method that will match any path and args if they haven't been caught above */
	lo_server_add_method(s, NULL, NULL, generic_handler, this);

	/* Start incoming message processing */
	dbg("OSCHandler",1) << "Creating thread for incoming messages (server=" << s << ")..." << std::flush;
	int rc=pthread_create(&incomingThread, NULL, processIncoming, (void *)this);
	if (rc) {
	    fprintf(stderr,"pthread_create failed with error code %d\n", rc);
	    exit(1);
	}
	dbgn("OSCHandler",1) << "done." << std::endl;
	drawTarget=NONE;
}

OSCHandler::~OSCHandler() {
    int rc=pthread_cancel(incomingThread);
    if (rc)
	dbg("OSCHandler",1) << "pthread_cancel failed with error code " << rc << std::endl;
    lo_server_free(s);
}


// Processing incoming OSC messages in a separate thread
void *OSCHandler::processIncoming(void *arg) {
    SetDebug("pthread:OSCHandler");
    OSCHandler *handler = (OSCHandler *)arg;
    handler->processIncoming();
    return NULL;
}


void OSCHandler::processIncoming() {
    dbg("OSCHandler.processIncoming",1) << "Started: s=" << std::setbase(16) << s << std::setbase(10) << std::endl;
    // Process all queued messages
    while (true) {
	// TODO: Should set timeout to 0 if geometry is dirty, longer timeout if its clean
	if  (lo_server_recv_noblock(s,1) == 0) {
	    static const float FRAMETIMEOUT= 1.0f;	// Timeout to inject a fake frame to keep the UI running
	    struct timeval now;
	    gettimeofday(&now,0);
	    if ((now.tv_sec-lastFrameTime.tv_sec)+(now.tv_usec-lastFrameTime.tv_usec)/1e6 > FRAMETIMEOUT) {
		dbg("OSCHanler.processingIncoming",1) << "Faking a frame message" << std::endl;
		pfframe(lastUpdateFrame?0:1);	// Simulate a frame message
	    }
	    // Render lasrs only when nothing in queue
	    if (lasers->render()) 
		// If they've changed, mark the video for update too
		video->setDirty();
	}
	if (doQuit)
	    break;
    }
}

void OSCHandler::startStop(bool start) {
	printf("OSCHandler: %s\n", start?"start":"stop");
}


void OSCHandler::setPPS(int pps) {
    dbg("OSCHandler.setPPS",1) << "Setting PPS to " << pps << " PPS" << std::endl;
    lasers->setPPS(pps);
}

void OSCHandler::setPoints(int n) {
    dbg("OSCHandler.setPPS",1) << "Setting points/frame to " << n << std::endl;
    lasers->setPoints(n);
}

void OSCHandler::setPreBlanking(int nblank) {
    dbg("OSCHandler.setBlanking",1) << "Setting pre blanking to " << nblank << std::endl;
    lasers->setPreBlanks(nblank);
}

void OSCHandler::setPostBlanking(int nblank) {
    dbg("OSCHandler.setBlanking",1) << "Setting post blanking to " << nblank << std::endl;
    lasers->setPostBlanks(nblank);
}

void OSCHandler::setSkew(int skew) {
    dbg("OSCHandler.setSkew",1) << "Setting skew to " << skew  << std::endl;
    lasers->setSkew(skew);
}


void OSCHandler::ping(lo_message msg, int seqnum) {
    char *url=lo_address_get_url(lo_message_get_source(msg));
    printf("Got ping from %s\n",url);
    lo_address addr = lo_address_new_from_url(url);
    lo_send(addr,"/ack","i",seqnum);
    lo_address_free(addr);
}

void OSCHandler::setColor(Color c ) {
    currentColor=c;
}

void OSCHandler::setDensity(float d ) {
    currentDensity=d;
}

void OSCHandler::cellBegin(int uid) {
    dbg("OSCHandler.cellBegin",3) << "UID " << uid << std::endl;
    if (drawTarget!=NONE) {
	dbg("OSCHandler.cellBegin",1) << "Already drawing to a target: " <<  drawTarget << "; switching to CELL" << std::endl;
    }
	
    if (cellDrawing.getNumElements() > 0) {
	dbg("OSCHandler.cellBegin",1) << "Cell drawing is not empty - clearing" << std::endl;
	cellDrawing.clear();
    }
    drawTarget=CELL;
}

void OSCHandler::cellEnd(int uid) {
    dbg("OSCHandler.cellEnd",3) << "UID " << uid << std::endl;
    if (drawTarget!=CELL) {
	dbg("OSCHandler.cellEnd",1) << "Not in CELL drawing state" << std::endl;
	return;
    }
	
    People::setVisual(uid,cellDrawing);
    cellDrawing.clear();
    drawTarget=NONE;
}

void OSCHandler::conxBegin(const char *cid) {
    dbg("OSCHandler.conxBegin",3) << "CID " << cid << std::endl;
    if (drawTarget!=NONE) {
	dbg("OSCHandler.conxBegin",1) << "Already drawing to a target: " <<  drawTarget << "; switching to CONX" << std::endl;
    }
    if (!Connections::instance()->connectionExists(cid))
	dbg("OSCHandler.conxBegin",1) << "CID " << cid << " does not exist" << std::endl;

    if (conxDrawing.getNumElements() > 0) {
	dbg("OSCHandler.conxBegin",1) << "Drawing is not empty - clearing" << std::endl;
	conxDrawing.clear();
    }
    drawTarget=CONX;
}

void OSCHandler::conxEnd(const char *cid) {
    dbg("OSCHandler.conxEnd",3) << "CID " << cid << std::endl;
    if (drawTarget!=CONX) {
	dbg("OSCHandler.conxEnd",1) << "Not in CONX drawing state" << std::endl;
	return;
    }
    if (!Connections::instance()->connectionExists(cid)) {
	dbg("OSCHandler.conxEnd",1) << "CID " << cid << " does not exist" << std::endl;
    } else
	Connections::setVisual(cid,conxDrawing);
    conxDrawing.clear();
    drawTarget=NONE;
}

void OSCHandler::bgBegin() {
    dbg("OSCHandler.bgBegin",3) << "bgBegin" << std::endl;
    if (drawTarget!=NONE) {
	dbg("OSCHandler.bgBegin",1) << "Already drawing to a target: " <<  drawTarget << "; switching to BACKGROUND" << std::endl;
    }
    if (bgDrawing.getNumElements() > 0) {
	dbg("OSCHandler.bgBegin",1) << "Drawing is not empty - clearing" << std::endl;
	bgDrawing.clear();
    }
    drawTarget=BACKGROUND;
}

void OSCHandler::bgEnd() {
    dbg("OSCHandler.bgEnd",3) << "bgEnd"<< std::endl;
    if (drawTarget!=BACKGROUND) {
	dbg("OSCHandler.bgEnd",1) << "Not in BACKGROUND drawing state" << std::endl;
	return;
    }
    lasers->setVisual(bgDrawing);
    bgDrawing.clear();
    drawTarget=NONE;
}

Drawing *OSCHandler::currentDrawing() {
    if (drawTarget==CELL)
	return &cellDrawing;
    else if (drawTarget==CONX)
	return &conxDrawing;
    else if (drawTarget==BACKGROUND)
	return &bgDrawing;
    else {
	dbg("OSCHandler.currentDrawing",1) << "No current drawing set" << std::endl;
	return NULL;
    }	
}

void OSCHandler::circle(Point center, float radius ) {
    Drawing *d=currentDrawing();
    if (d!=NULL)
	d->drawCircle(center,radius,currentColor);
}

void OSCHandler::arc(Point center, Point pt, float angle ) {
    Drawing *d=currentDrawing();
    if (d!=NULL)
	d->drawArc(center,pt,angle,currentColor);
}

void OSCHandler::line(Point p1, Point p2) {
    dbg("OSCHandler.line",3) << "line(" << p1 << "," << p2 << ")" << std::endl;
    Drawing *d=currentDrawing();
    if (d!=NULL)
	d->drawLine(p1,p2,currentColor);
}

void OSCHandler::svgfile(std::string filename,Point origin, float scaling,float rotateDeg) {
    static const std::string SVGDIRECTORY("Images");
    dbg("OSCHandler.svgfile",3) << "svgfile(" << filename << ", " << origin << ", " << scaling << ", " << rotateDeg << ")" << std::endl;
    Drawing *d=currentDrawing();
    if (d!=NULL) {
	std::shared_ptr<SVG> s=SVGs::get(SVGDIRECTORY+"/"+filename);
	if (s!=nullptr)
	    s->addToDrawing(*d,origin,scaling,rotateDeg,currentColor);
    }
}

void OSCHandler::cubic(Point p1, Point p2, Point p3, Point p4) {
    if (p1==p2 && p2==p3 && p3==p4) {
	dbg("OSCHandler.cubic",1) << "Cubic with identical points at  " << p1 << " ignored" << std::endl;
	return;
    }
    Drawing *d=currentDrawing();
    if (d!=NULL)
	d->drawCubic(p1,p2,p3,p4,currentColor);
}

void OSCHandler::map(int unit,  int pt, Point devpt, Point floorpt) {
    if (unit<0 || unit>=(int)lasers->size()) {
	dbg("OSCHandler.map",1)  << "Bad unit: " << unit << std::endl;
	return;
    }
    if (pt<0 || pt>3) {
	dbg("OSCHandler.map",1)  << "Invalid point: " << pt << std::endl;
	return;
    }
    
    lasers->getLaser(unit)->getTransform().setFloorPoint(pt,floorpt);
    lasers->getLaser(unit)->getTransform().setDevPoint(pt,devpt);
}

void OSCHandler::update(int frame) {
    // Not needed -- using frame for updates
}

void OSCHandler::pfframe(int frame) {
    dbg("OSCHandler.pfframe",1) << "pfframe(" << frame << "), lastUpdateFrame=" << lastUpdateFrame << std::endl;
    gettimeofday(&lastFrameTime,0);
    lastUpdateFrame=frame;
    lasers->setFrame(frame);

    if (!TouchOSC::instance()->isFrozen()) {
	// Age all the connections and people
	Connections::incrementAge();
	People::incrementAge();
	Groups::incrementAge();
    }

    // UI Tick
    TouchOSC::frameTick(frame);
    Music::instance()->frameTick(frame);

    //    if (frame%1000 == 0)
    //	lasers->dumpPoints();
}

// Called when any of the bounds have been possibly changed
void OSCHandler::updateBounds() {
    std::vector<Point> bounds(4);
    bounds[0]=Point(minx,miny);
    bounds[1]=Point(maxx,miny);
    bounds[2]=Point(maxx,maxy);
    bounds[3]=Point(minx,maxy);

    video->setBounds(bounds);  /// This will handle checking if there is any real change
}
