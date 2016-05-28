#include <algorithm>
#include "calibration.h"
#include "trackerComm.h"
#include "dbg.h"
#include "opencv2/calib3d/calib3d.hpp"

class Transform;

static const float WORLDSIZE=30;   // Maximum extents of world (in meters)

std::shared_ptr<Calibration> Calibration::theInstance=NULL;   // Singleton

int RelMapping::send(std::string path, float value) const {
    return Calibration::instance()->send(path,value);
}

int RelMapping::send(std::string path, float v1, float v2) const {
    return Calibration::instance()->send(path,v1,v2);
}
int RelMapping::send(std::string path, std::string value) const {
    return Calibration::instance()->send(path,value);
}

int Calibration::send(std::string path, float value) const {
    dbg("Calibration.send",4) << "send " << path << "," << value << std::endl;
    if (lo_send(tosc,path.c_str(),"f",value) <0 ) {
	dbg("Calibration.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(tosc) << ": " << lo_address_errstr(tosc) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

int Calibration::send(std::string path, float v1, float v2) const {
    dbg("Calibration.send",4) << "send " << path << "," << v1 << ", " << v2 << std::endl;
    if (lo_send(tosc,path.c_str(),"ff",v1,v2) <0 ) {
	dbg("Calibration.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(tosc) << ": " << lo_address_errstr(tosc) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

int Calibration::send(std::string path, std::string value) const {
    dbg("Calibration.send",4) << "send " << path << "," << value << std::endl;
    if (lo_send(tosc,path.c_str(),"s",value.c_str()) <0 ) {
	dbg("Calibration.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(tosc) << ": " << lo_address_errstr(tosc) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

// Convert from logarithmic speed (~1/1920 - 0.1) to [0,1] rotary setting
static float speedToRotary(float speed) {
    return log10(speed*1920)/log10(1000);
}

// And back
static float rotaryToSpeed(float rotary) {
    return pow(10,rotary*log10(1000))/1920;
}

// From  http://dsp.stackexchange.com/questions/1484/how-to-compute-camera-pose-from-homography-matrix
static void cameraPoseFromHomography(const cv::Mat& H, cv::Mat& pose)
{
    pose = cv::Mat::eye(3, 4, CV_64FC1); //3x4 matrix
    float norm1 = (float)norm(H.col(0)); 
    float norm2 = (float)norm(H.col(1));
    float tnorm = (norm1 + norm2) / 2.0f;

    cv::Mat v1 = H.col(0);
    cv::Mat v2 = pose.col(0);

    cv::normalize(v1, v2); // Normalize the rotation

    v1 = H.col(1);
    v2 = pose.col(1);

    cv::normalize(v1, v2);

    v1 = pose.col(0);
    v2 = pose.col(1);

    cv::Mat v3 = v1.cross(v2);  //Computes the cross-product of v1 and v2
    cv::Mat c2 = pose.col(2);
    v3.copyTo(c2);      

    pose.col(3) = H.col(2) / tnorm; //vector t [R|t]
}

bool RelMapping::handleOSCMessage(std::string tok, lo_arg **argv,float speed,bool flipX1, bool flipY1, bool flipX2, bool flipY2) {
    dbg("RelMapping",1) << "tok=" << tok << ", speed=" << speed << std::endl;
    bool handled=false;
    
    Point *pt=&pt1[selected];
    char *nexttok=strtok(NULL,"/");
    if (nexttok){
	dbg("RelMapping",1) << "tok=" << tok << ", nexttok=" << nexttok << ", speed=" << speed << std::endl;
    } else
	dbg("RelMapping",1) << "tok=" << tok << ", speed=" << speed << std::endl;
    float speedX=speed*(flipX1?-1:1);
    float speedY=speed*(flipY1?-1:1);;
    if (nexttok && strcmp(nexttok,"2")==0) {
	dbg("RelMapping",2) << "Using pt2" << std::endl;
	pt=&pt2[selected];
	speedX=speed*(flipX2?-1:1);
	speedY=speed*(flipY2?-1:1);;
    }

    if (tok=="xy") {
	// xy pad
	if (~locked[selected]) {
	    pt->setX(argv[0]->f*(speedX>0?1:-1));
	    pt->setY(argv[1]->f*(speedY>0?1:-1));
	}
	handled=true;
    } else if (tok=="lock") {
	locked[selected]=argv[0]->f > 0;
	handled=true;
    } else if (tok=="selpair") {
	if (argv[0]->f > 0)
	    selected=atoi(nexttok)-1;
	handled=true;
    } else if (tok=="left") {
	if (~locked[selected] && argv[0]->f > 0)
	    pt->setX(std::max(-1.0f,pt->X()-speedX));
	handled=true;
    } else if (tok=="right") {
	if (selected>=0 && ~locked[selected] && argv[0]->f > 0)
	    pt->setX(std::min(1.0f,pt->X()+speedX));
	handled=true;
    } else if (tok=="up") {
	if (selected>=0 && ~locked[selected] && argv[0]->f > 0)
	    pt->setY(std::min(1.0f,pt->Y()+speedY));
	handled=true;
    } else if (tok=="down") {
	if (selected>=0 && ~locked[selected] && argv[0]->f > 0)
	    pt->setY(std::max(-1.0f,pt->Y()-speedY));
	handled=true;
    } else if (tok=="est") {
	if (atoi(nexttok)==2 && ~locked[selected] && argv[0]->f > 0) {
	    setDevicePt(Calibration::instance()->map(getDevicePt(0),unit1,unit2),1);
	    dbg("RelMapping.handleOSCMessage",1) << "Estimated position for unit " << unit2 << ": " << pt2[selected] << std::endl;
	} else if (atoi(nexttok)==1 && ~locked[selected] && argv[0]->f > 0) {
	    setDevicePt(Calibration::instance()->map(getDevicePt(1),unit2,unit1),0);
	    dbg("RelMapping.handleOSCMessage",1) << "Estimated position for unit " << unit1 << ": " << pt1[selected] << std::endl;
	} else {
	    dbg("RelMapping.handleOSCMessage",1) << "Est click ignored" << std::endl;
	}
	handled=true;
    } else if (tok=="align") {
	if (selected>=0 && ~locked[selected] && argv[0]->f > 0 && isWorld) {
	    // Find nearest alignment target to current world cooords and move to it
	    std::vector<Point> a=Calibration::instance()->getAlignment();
	    float mindist=1e10;
	    int closest=-1;
	    Point dpoint=getDevicePt(1);
	    for (int i=0;i<a.size();i++) {
		float d=(a[i]-dpoint).norm();
		dbg("RelMapping.handleOSCMessage",1) << "Distance from " << a[i] << " to " << dpoint << " = " << d << std::endl;
		if (d<mindist) {
		    mindist=d;
		    closest=i;
		}
	    }
	    if (closest>=0) {
		dbg("RelMapping.handleOSCMessage",1) << "Closest of " << a.size() << " alignment points to " << pt2[selected] << " is at " << a[closest] << std::endl; 
		setDevicePt(a[closest],1);
	    } else {
		dbg("RelMapping.handleOSCMessage",1) << "No alignment point" << std::endl;
	    }
	}
	handled=true;
    }
    
    return handled;
}

static Point relToProj(Point x) {
    return Point((x.X()+1)*1920/2,(x.Y()+1)*1080/2);
}

static Point projToRel(Point x) {
    return Point(x.X()/(1920/2)-1,x.Y()/(1080/2)-1);
}

// Get coordinate of pt[i] in device [0,0]-[1919,1079] or world ([-WORLDSIZE,WORLDSIZE],[0,WORLDSIZE])
Point RelMapping::getDevicePt(int i,int which,bool doRound) const {
    Point res;
    if (which==-1)
	which=selected;
    if (i==0)
	res=relToProj(pt1[which]);
    else if (isWorld) {
	res=Point(pt2[which].X()*WORLDSIZE,(pt2[which].Y()+1)*WORLDSIZE/2);
	if (doRound)
	    res=Point(std::round(res.X()*100)/100,std::round(res.Y()*100)/100);
	return res;
    } else
	res=relToProj(pt2[which]);
    if (doRound)
	res=Point(std::round(res.X()),std::round(res.Y()));
    return res;
}
       
// Set coordinate of pt[i] in device or world
void  RelMapping::setDevicePt(Point p, int i,int which)  {
    dbg("RelMapping.setDevicePt",2) <<"setDevicePt(" << p << "," << i << "," << which << ")" << std::endl;
    Point res;
    if (which==-1)
	which=selected;
    if (i==0)
	pt1[which]=projToRel(p);
    else if (isWorld) {
	pt2[which]=Point(p.X()/WORLDSIZE,p.Y()*2/WORLDSIZE-1);
    } else
	pt2[which]=projToRel(p);
}
       
Calibration::Calibration(int _nunits, URLConfig&urls): homographies(_nunits+1), statusLines(3), tvecs(_nunits), rvecs(_nunits), poses(_nunits), alignCorners(0), config("settings_proj.json") {
    nunits = _nunits;
    dbg("Calibration.Calibration",1) << "Constructing calibration with " << nunits << " units." << std::endl;
    assert(nunits>0);
    for (int i=0;i<homographies.size();i++)
	homographies[i]=cv::Mat::eye(3, 3, CV_64F);	// Initialize to identity matrix
    for (int i=0;i<poses.size();i++)  {
	poses[i] = cv::Mat::zeros(3, 1, CV_64FC1);
	rvecs[i] = cv::Mat::zeros(3, 1, CV_64FC1);
	tvecs[i] = cv::Mat::zeros(3, 1, CV_64FC1);
    }
    projection = cv::Mat::zeros(3, 3, CV_64FC1); //3x4 matrix
    //float fh=547.392, fv=-547.3440, ch=0, cv=-1298.9;
    //float fh=547.392, fv=-547.3440, ch=1920/2, cv=1080/2;
    //float fh=964, fv=-fh, ch=1920/2, cv=1080/2;
    float fh=486, fv=-fh, ch=1920/2, cv=1080/2;
    cv=cv-fv*(.224+1.25)/0.56;    // Vertical offset from manual   
    //float fh=0.66/2.48*1920, fv=-fh, ch=1920/2, cv=(1.52/2+.25)/1.52*1080;
    projection.at<double>(0,0)=fh;
    projection.at<double>(1,1)=fv;
    projection.at<double>(0,2)=ch;
    projection.at<double>(1,2)=cv;
    projection.at<double>(2,2)=1;
    

    for (int i=0;i<nunits;i++)
	for (int j=i+1;j<nunits+1;j++) {
	    relMappings.push_back(std::shared_ptr<RelMapping>(new RelMapping(i,j,j==nunits)));
	}
    for (int i=0;i<nunits;i++) {
	flipX.push_back(false);
	flipY.push_back(false);
    }
    curMap=relMappings[0];
    speed=0.05;
    laserMode=CM_NORMAL;

    /* Setup touchOSC sending */
    int touchOSCPort=urls.getPort("TO");
    const char *touchOSCHost=urls.getHost("TO");
    if (touchOSCPort==-1 || touchOSCHost==0) {
	fprintf(stderr,"Unable to locate TO in urlconfig.txt\n");
	tosc=0;
    } else {
	char cbuf[10];
	sprintf(cbuf,"%d",touchOSCPort);
	tosc = lo_address_new(touchOSCHost, cbuf);
	dbg("Calibration",1)  << "Set remote to " << loutil_address_get_url(tosc) << std::endl;
	// Don't call sendOSC here as it will cause a recursive loop
    }

    for (int i=0;i<statusLines.size();i++)
	statusLines[i]="";
    showStatus("Initalized");
}

int Calibration::handleOSCMessage_impl(const char *path, const char *types, lo_arg **argv,int argc,lo_message msg) {
    dbg("Calibration.handleOSCMessage",1)  << "Got message: " << path << "(" << types << ") from " << loutil_address_get_url(lo_message_get_source(msg)) << std::endl;

    char *pathCopy=new char[strlen(path)+1];
    strcpy(pathCopy,path);
    char *tok=strtok(pathCopy,"/");

    bool handled=false;

    if (strcmp(tok,"cal")==0) {
	tok=strtok(NULL,"/");
	if (strcmp(tok,"sel")==0) {
	    if (argv[0]->f > 0) {
		std::string dir=strtok(NULL,"/");
		int which=atoi(strtok(NULL,"/"))-1;
		int cur[2];
		cur[0]=curMap->getUnit(0);
		cur[1]=curMap->getUnit(1);
		if (dir=="left") {
		    if (cur[which]>0)
			cur[which]--;
		} else {
		    if (cur[which]<nunits-1 || (which == 1 && cur[which]==nunits-1))
			cur[which]++;
		}
		dbg("Calibration",1) << "Selected " << cur[0] << ", " << cur[1] << std::endl;
		for (int i=0;i<relMappings.size();i++) {
		    std::shared_ptr<RelMapping> rm=relMappings[i];
		    if (rm->getUnit(0)==cur[0] && rm->getUnit(1)==cur[1]) {
			curMap=rm;
			dbg("Calibration",1) << "Change curMap to " << curMap->getUnit(0) << "-" << curMap->getUnit(1) << std::endl;
			break;
		    }
		}
	    }
	    handled=true;
	} else if (strcmp(tok,"speed")==0) {
	    speed=rotaryToSpeed(argv[0]->f);
	    showStatus("Set speed to " + std::to_string(std::round(speed*32767)));
	    handled=true;
	}  else if (strcmp(tok,"flipx")==0) {
	    int which=atoi(strtok(NULL,"/"))-1;
	    if (which==0)
		flipX[curMap->getUnit(0)]=argv[0]->f>0;
	    else
		flipX[curMap->getUnit(1)]=argv[0]->f>0;
	    handled=true;
	}  else if (strcmp(tok,"flipy")==0) {
	    int which=atoi(strtok(NULL,"/"))-1;
	    if (which==0)
		flipY[curMap->getUnit(0)]=argv[0]->f>0;
	    else
		flipY[curMap->getUnit(1)]=argv[0]->f>0;
	    handled=true;
	} else if (strcmp(tok,"recompute")==0) {
	    if (argv[0]->f > 0) {
		if (recompute() == 0)
		    showStatus("Updated mappings");
	    }
	    handled=true;
	} else if (strcmp(tok,"save")==0) {
	    save();
	    handled=true;
	} else if (strcmp(tok,"load")==0) {
	    load();
	    handled=true;
	} else if (strcmp(tok,"lasermode")==0) {
	    if (argv[0]->f > 0) {
		int col=atoi(strtok(NULL,"/"))-1;
		laserMode=(LaserMode)(col);
	    }
	    handled=true;
	}  else {
	    // Pass down to currently selected relMapping
	    dbg("Calibration.handleOSCMessage",1) << "Handing off message to curMap" << std::endl;
	    handled=curMap->handleOSCMessage(tok,argv,speed,flipX[curMap->getUnit(0)],flipY[curMap->getUnit(0)],flipX[curMap->getUnit(1)],flipY[curMap->getUnit(1)]);
	}
    }
    if (handled)
	updateUI();
    delete[] pathCopy;
    return handled?0:1;
}

void Calibration::updateUI() const {
    curMap->updateUI(flipX[curMap->getUnit(0)],flipY[curMap->getUnit(0)],flipX[curMap->getUnit(1)],flipY[curMap->getUnit(1)]);
    send("/cal/speed",speedToRotary(speed));
    send("/cal/lasermode/"+std::to_string((int)laserMode+1)+"/1",1.0);
    float e2sum=0;
    int e2cnt=0;
    for (int i=0;i<relMappings.size();i++) {
	relMappings[i]->sendCnt();
	e2sum+=relMappings[i]->getE2Sum();
	e2cnt+=relMappings[i]->getE2Cnt();
    }
    send("/cal/error/total",std::round(sqrt(e2sum/e2cnt)*1000)/1000);
    for (int i=0;i<statusLines.size();i++)
	send("/cal/status/"+std::to_string(i+1),statusLines[i]);

    updateTracker();
}

void Calibration::updateTracker() const {
    // Send cursor positions to tracker app
    std::vector<Cursor> c;
    for (int i=0;i<nunits;i++) {
	std::vector<Point> pts=getCalPoints(i);
	for (int j=0;j<pts.size();j++)
	    c.push_back(Cursor(i,pts[j].X(),pts[j].Y()));
    }
    TrackerComm::instance()->sendCursors(c);


    for (int i=0;i<poses.size();i++) {
	TrackerComm::instance()->sendScreenToWorld(i, homographies[i]);
	cv::Mat inv=homographies[i].inv();
	inv=inv/inv.at<double>(2,2);
	TrackerComm::instance()->sendWorldToScreen(i, inv);
	TrackerComm::instance()->sendProjection(i,projection);

	// Convert to rotation matrix
	cv::Mat rotMat;
	cv::Rodrigues(rvecs[i],rotMat);
	TrackerComm::instance()->sendCameraView(i,rotMat,tvecs[i]);
	TrackerComm::instance()->sendPose(i,poses[i]);
    }
}

void RelMapping::sendCnt() const {
    int cnt=0;
    for (int i=0;i<locked.size();i++)
	if (locked[i]) cnt++;
    
    std::string cstr=std::to_string(cnt);
    if (cnt==0)
	cstr="";
    if (isWorld)
	send("/cal/cnt/"+std::to_string(unit1+1)+"/W",cstr);
    else
	send("/cal/cnt/"+std::to_string(unit1+1)+"/"+std::to_string(unit2+1),cstr);
}

void RelMapping::updateUI(bool flipX1,bool flipY1, bool flipX2, bool flipY2) const {
    dbg("RelMapping",1) << "updateUI for " << unit1 << "-" << unit2 << std::endl;
    send("/cal/sel/val/1",std::to_string(unit1+1));
    if (isWorld)
	send("/cal/sel/val/2","W");
    else
	send("/cal/sel/val/2",std::to_string(unit2+1));
    
    send("/cal/lock",locked[selected]?1.0:0.0);
    for (int i=0;i<locked.size();i++)
	send("/cal/led/"+std::to_string(i+1),locked[i]?1.0:0.0);
    send("/cal/selpair/"+std::to_string(selected+1)+"/1",1.0);
    send("/cal/flipx/1",flipX1?1.0:0.0);
    send("/cal/flipy/1",flipY1?1.0:0.0);
    send("/cal/flipx/2",flipX2?1.0:0.0);
    send("/cal/flipy/2",flipY2?1.0:0.0);

    send("/cal/xy/1",pt1[selected].X()*(flipX1?-1:1),pt1[selected].Y()*(flipY1?-1:1));
    send("/cal/x/1",getDevicePt(0,-1,true).X());
    send("/cal/y/1",getDevicePt(0,-1,true).Y());
    send("/cal/xy/2",pt2[selected].X()*(flipX2?-1:1),pt2[selected].Y()*(flipY2?-1:1));
    send("/cal/x/2",getDevicePt(1,-1,true).X());
    send("/cal/y/2",getDevicePt(1,-1,true).Y());
    std::vector<float> error=((RelMapping *)this)->updateErrors();
    for (int i=0;i<error.size();i++)
	if (isnan(error[i]))
	    send("/cal/error/"+std::to_string(i+1),"");
	else {
	    if (error[i] >= 100)
		send("/cal/error/"+std::to_string(i+1),std::round(error[i]));
	    else if (error[i] >= 10)
		send("/cal/error/"+std::to_string(i+1),std::round(error[i]*10)/10);
	    else if (error[i] >= 10)
		send("/cal/error/"+std::to_string(i+1),std::round(error[i]*100)/100);
	    else
		send("/cal/error/"+std::to_string(i+1),std::round(error[i]*1000)/1000);
	}
}

// Display status in touchOSC
void Calibration::showStatus(std::string line)  {
    dbg("Calibration.showStatus",1) << line << std::endl;
    for (int i=0;i<statusLines.size()-1;i++)
	statusLines[i]=statusLines[i+1];
    statusLines[statusLines.size()-1]=line;
}

void RelMapping::save(ptree &p) const {
    dbg("RelMapping.save",1) << "Saving relMapping " << unit1 << "-" << unit2 << " to ptree" << std::endl;
    p.put("unit1",unit1);
    p.put("unit2",unit2);
    p.put("isWorld",isWorld);
    p.put("selected",selected);
    ptree pairs;
    for (int i=0;i<pt1.size();i++) {
	ptree dp;
	dp.put("pt1.x",pt1[i].X());
	dp.put("pt1.y",pt1[i].Y());
	dp.put("pt2.x",pt2[i].X());
	dp.put("pt2.y",pt2[i].Y());
	dp.put("locked",locked[i]);
	pairs.push_back(std::make_pair("",dp));
    }
    p.put_child("pairs",pairs);
}

void RelMapping::load(ptree &p) {
    dbg("RelMapping.save",1) << "Loading relMapping from ptree" << std::endl;
    unit1=p.get("unit1",0);
    unit2=p.get("unit2",1);
    selected=p.get("selected",selected);

    try {
	ptree dp=p.get_child("pairs");
	int i=0;
	for (ptree::iterator v = dp.begin(); v != dp.end();++v) {
	    if (i>=pt1.size())
		break;
	    ptree val=v->second;
	    pt1[i]=Point(val.get<double>("pt1.x",pt1[i].X()),val.get<double>("pt1.y",pt1[i].Y()));
	    pt2[i]=Point(val.get<double>("pt2.x",pt2[i].X()),val.get<double>("pt2.y",pt2[i].Y()));
	    locked[i]=val.get<bool>("locked",locked[i]);
	    i++;
	}
    } catch (boost::property_tree::ptree_bad_path ex) {
	std::cerr << "Unable to find 'pairs' in laser settings" << std::endl;
    }
}

void Calibration::save()  {
    dbg("Calibration.save",1) << "Saving calibration to ptree" << std::endl;
    ptree p;
    p.put("nunits",nunits);
    ptree rm;
    for (unsigned int i=0;i<relMappings.size();i++)  {
	ptree mapping;
	relMappings[i]->save(mapping);
	rm.push_back(std::make_pair("",mapping));
    }
    p.put_child("mappings",rm);
    p.put("speed",speed);
    ptree flips;
    for (int i=0;i<flipX.size();i++) {
	ptree f;
	f.put("flipX",flipX[i]);
	f.put("flipY",flipY[i]);
	flips.push_back(std::make_pair("",f));
    }
    p.put_child("flips",flips);
    config.pt().put_child("calibration",p);
    config.save();
    ((Calibration *)this)->showStatus("Saved configuration");
}

void Calibration::load() {
    dbg("Calibration.load",1) << "Loading transform from ptree" << std::endl;

    config.load();
    ptree &p1=config.pt();
    ptree p;
    try {
	p=p1.get_child("calibration");
    } catch (boost::property_tree::ptree_bad_path ex) {
	std::cerr << "Unable to find 'calib' in settings file" << std::endl;
	return;
    }

    int ldunits=p.get("nunits",nunits);
    if (ldunits!=nunits) {
	dbg("Calibration.load",1) << "Loading file with " << ldunits << " into program setup with " << nunits << " units." << std::endl;
    }
    speed=p.get("speed",speed);
    ptree f;
    f=p.get_child("flips");
    int i=0;
    for (ptree::iterator v = f.begin(); v != f.end();++v) {
	if (i>=flipX.size())
	    break;
	ptree val=v->second;
	flipX[i]=val.get<bool>("flipX",flipX[i]);
	flipY[i]=val.get<bool>("flipY",flipY[i]);
	i++;
    }
    
    ptree rm;
    try {
	rm=p.get_child("mappings");
	int i=0;
	for (ptree::iterator v = rm.begin(); v != rm.end();++v) {
	    if (i>=relMappings.size())
		break;
	    relMappings[i]->load(v->second);
	    i++;
	}
    } catch (boost::property_tree::ptree_bad_path ex) {
	std::cerr << "Unable to find 'mappings' in laser settings" << std::endl;
    }
    recompute();
    showStatus("Loaded configuration");
}

// Add these mapping(s) to the lists of features and pairwiseMatches
// features vector has one entry for each laser (N)
// matches has one entry for every permutation (i.e. N^2)
// matches are entered with lowest number unit as src
// note: queryIdx refers to srcImg,  trainIdx refers to dstImg
int RelMapping::addMatches(std::vector<cv::detail::ImageFeatures> &features,    std::vector<cv::detail::MatchesInfo> &pairwiseMatches) const {
    int numAdded=0;
    for (int i=0;i<pairwiseMatches.size();i++) {
	cv::detail::MatchesInfo &pm = pairwiseMatches[i];
	if (pm.src_img_idx==unit1 && pm.dst_img_idx==unit2) {
	    for (int j=0;j<pt1.size();j++) {
		if (!locked[j])
		    continue;
		Point flat1=getDevicePt(0,j);
		Point flat2;
		if (isWorld)
		    flat2=getDevicePt(1,j);
		else
		    // Note: world is only on unit2, never on unit1
		    flat2=getDevicePt(1,j);
		dbg("RelMapping.addMatches",1) << "Adding pair from relMapping of laser " <<pm.src_img_idx << "@" << flat1 << " <->  laser " <<pm.dst_img_idx << "@" << flat2 << std::endl;
		cv::KeyPoint kp1,kp2;
		kp1.pt.x=flat1.X();
		kp1.pt.y=flat1.Y();
		features[pm.src_img_idx].keypoints.push_back(kp1);

		kp2.pt.x=flat2.X();
		kp2.pt.y=flat2.Y();
		features[pm.dst_img_idx].keypoints.push_back(kp2);

		cv::DMatch m;   // Declared in features2d/features2d.hpp
		m.queryIdx=features[pm.src_img_idx].keypoints.size()-1;
		m.trainIdx=features[pm.dst_img_idx].keypoints.size()-1;
		dbg("RelMapping.addMatches",2) << pm.dst_img_idx << "." << m.trainIdx << std::endl;
		m.distance=0;	// Probably usually set later as a function of how the match works
		m.imgIdx=pm.dst_img_idx; // Unsure... appears to not be used, but in any case it seems that it is the train image index
		pm.inliers_mask.push_back(1);	// This match is an "inlier"
		pm.matches.push_back(m);
		pm.num_inliers++;
		numAdded++;
	    }
	}
    }
    return numAdded;
}

std::ostream &flatMat(std::ostream &s, const cv::Mat &m) {
//    s<<m << std::endl;
//    return s;
//    s << "m.type=" << std::hex <<  m.type() << ",  type&0x18=" << (m.type()&0x18) << std::dec << std::endl;
    
    if ((m.type()&0x7)!=CV_64F && (m.type()&0x7)!=CV_32F) {
	s << "flatMat: unexpected m.type=" << m.type() << std::endl;
	return s;
    }
    
    //assert(m.type()==CV_64F);
    s << "[";
    for (int i=0;i<m.rows;i++) {
	for (int j=0;j<m.cols;j++)
	    if ((m.type()&0x18) == 0x8) {
		// 2d vector
		s << m.at<cv::Point2f>(i,j).x << "," << m.at<cv::Point2f>(i,j).y << std::endl;
	    } else if ((m.type()&0x18) == 0x10) {
		// 3d vector
		s << m.at<cv::Point3f>(i,j).x << "," << m.at<cv::Point3f>(i,j).y <<  "," << m.at<cv::Point3f>(i,j).z << std::endl;
	    } else {
		if ((m.type() & 0x7) == CV_64F)
		    s << m.at<double>(i,j) << " ";
		else if ((m.type() & 0x7) == CV_32F)
		    s << m.at<float>(i,j) << " ";
	    }
	if (i==m.rows-1)
	    s << "]";
	s << std::endl;
    }
    return s;
}

// Compute extrinsics (translation, rotation of camera) given a set of matchpoints and the camera intrinsic projection matrix
// sensorPts (Nx2) - image points on camera sensor (0:width, 0:height) -- origin is at top-left
// worldPts (Nx3) - corresponding points in world frame of reference
static float computeExtrinsics(std::vector<cv::Point2f> &sensorPts, std::vector<cv::Point3f> &worldPts,  cv::Mat &projection, cv::Mat &rvec, cv::Mat &tvec, cv::Mat &pose) {
    cv::Mat opoints = ((cv::InputArray)worldPts).getMat(), ipoints = ((cv::InputArray)sensorPts).getMat();

	dbg("Calibration.recompute",1) << "computeExtrinsics() with " << sensorPts.size() << ", " << worldPts.size() << " points"  << std::endl;
	flatMat(DbgFile(dbgf__,"Calibration.recompute",1) << "Projection = \n",projection) << std::endl;
	flatMat(DbgFile(dbgf__,"Calibration.recompute",1) << "sensorPts = \n",ipoints) << std::endl;
	flatMat(DbgFile(dbgf__,"Calibration.recompute",1) << "worldPts = \n",opoints) << std::endl;

	if (false) {
	    std::vector<std::vector<cv::Point2f>>  imagePoints;
	    std::vector<std::vector<cv::Point3f>> objectPoints;
	    objectPoints.push_back(worldPts);
	    imagePoints.push_back(sensorPts);
	    cv::Mat distCoeffs;
	    double fv=projection.at<double>(2,2);
	    projection.at<double>(2,2)=std::abs(fv);
	    cv::calibrateCamera(objectPoints, imagePoints, cv::Size(1920,1080), projection, distCoeffs, rvec, tvec,CV_CALIB_USE_INTRINSIC_GUESS);
	    std::cout << "Updated projection matrix: " << projection << std::endl;
	} else  {
	    cv::solvePnP(worldPts,sensorPts,projection,cv::Mat(),rvec,tvec,false,CV_EPNP);
	}
	// Reproject the points
	cv::Mat reconPts;
	cv::projectPoints(worldPts, rvec, tvec, projection, cv::Mat(), reconPts);
	float totalErr=0;
	for (int i=0;i<reconPts.rows;i++) {
	    float e=norm(reconPts.at<cv::Point2f>(0,i)-ipoints.at<cv::Point2f>(0,i));
	    std::cout << ipoints.at<cv::Point2f>(0,i) << " -> " << opoints.at<cv::Point3f>(0,i) << " -> " << reconPts.at<cv::Point2f>(i,0) << " e=" << e << std::endl;
	    totalErr+=e;
	}
	totalErr /= reconPts.rows;
	std::cout << "RMS error " << totalErr << " pixels" << std::endl;
	// Convert to rotation matrix
	cv::Mat rotMat;
	cv::Rodrigues(rvec,rotMat);
	// Map tvec back to world coordinates
	pose=-rotMat.inv()*tvec;
	std::cout << "Final pose = " << pose << std::endl;
	return totalErr;
}
    

// Compute all the homographies using openCV
// see OpenCV motion_estimators.cpp for use (documentation doesn't cut it)
int Calibration::recompute() {
    std::vector<cv::detail::ImageFeatures> features(nunits+1);
    // One feature entry for each image
    for (int i=0;i<nunits+1;i++) {
	features[i].img_idx=i;
    }
    std::vector<cv::detail::MatchesInfo> pairwiseMatches;
    // One pairwiseMatches entry for each combination
    for (int i=0;i<nunits+1;i++) {
	for (int j=0;j<nunits+1;j++) {
	    if (i!=j) {
		cv::detail::MatchesInfo pm;
		pm.src_img_idx=i;
		pm.dst_img_idx=j;
		pairwiseMatches.push_back(pm);
	    }
	}
    }
    
    // Build features, matches vectors
    int numMatches = 0;
    for (int i=0;i<relMappings.size();i++)
	numMatches+=relMappings[i]->addMatches(features, pairwiseMatches);
    dbg("Calibration.recompute",1) << "Have " << features.size() << " features with " << numMatches << " pairwise matches." << std::endl;

    // Build matrix of linkage counts
    cv::Mat_<int> linkages = cv::Mat::zeros(nunits+1,nunits+1,CV_32S);
    std::vector<int> total(nunits+1);
    for (int i=0;i<pairwiseMatches.size();i++) {
	cv::detail::MatchesInfo pm = pairwiseMatches[i];
	if (pm.src_img_idx != pm.dst_img_idx) {
	    int n=pm.matches.size();
	    linkages[pm.src_img_idx][pm.dst_img_idx]+=n;
	    linkages[pm.dst_img_idx][pm.src_img_idx]+=n;
	    total[pm.src_img_idx]+=n;
	}
    }
    // Find best starting image -- has most pairs with some other image and, in case of a tie, has the most total connections
    int bestcnt=0;
    int refLaser=0;
    for (int i=0;i<pairwiseMatches.size();i++) {
	cv::detail::MatchesInfo pm = pairwiseMatches[i];
	if (pm.matches.size() >= bestcnt) {
	    if (pm.matches.size()>bestcnt || total[pm.src_img_idx] > total[refLaser]) {
		refLaser = pm.src_img_idx;
		bestcnt=pm.matches.size();
	    }
	}
    }
    dbg("Calibration.recompute",1) << "Using laser " << refLaser << " as reference with " << bestcnt << " connections to another image and " << total[refLaser] << " total connections." << std::endl;
    homographies[refLaser]=cv::Mat::eye(3, 3, CV_64F);	// Use first laser as a reference for now

    std::vector<bool> found(nunits+1);	// Flag for whether a laser has been calibrated
    found[refLaser]=true;

    // Find homographies (nunits+1)-1 times
    int resultCode=0;
    for (int rep=0;rep<nunits;rep++) {
	// Match next unit with found ones

	// Count number of matches from each unit to set of found ones and select mostly highly connected one (lowest unit in case of ties)
	int bestcnt=-1;
	int curUnit=-1;
	for (int i=0;i<nunits+1;i++) {
	    if (!found[i]) {
		int nmatches=0;
		for (int j=0;j<nunits+1;j++)
		    if (found[j])
			nmatches+=linkages[i][j];
		if (nmatches>bestcnt) {
		    bestcnt=nmatches;
		    curUnit=i;
		}
	    }
	}
	dbg("Calibration.recompute",1) << "Computing linkage to laser " << curUnit <<  " with " << bestcnt << " matches." << std::endl;
	if (bestcnt < 4) {
	    showStatus("Not enough calibration points to compute homography to "+(curUnit<nunits?"laser "+std::to_string(curUnit+1):"world")+"; only have "+std::to_string(bestcnt)+"/4 points.");
	    resultCode = -1;
	    homographies[curUnit]=cv::Mat::eye(3, 3, CV_64F);	// Make it a default transform
	} else {
	    std::vector<cv::Point2f> src,dst;

	    for (int j=0;j<pairwiseMatches.size();j++) {
		cv::detail::MatchesInfo pm = pairwiseMatches[j];
		if (found[pm.src_img_idx] && pm.dst_img_idx==curUnit) {
		    for (int i=0;i<pm.matches.size();i++) {
			// Project the point
			std::vector<cv::Point2f> tmp(1),mapped(1);
			tmp[0]=features[pm.src_img_idx].keypoints[pm.matches[i].queryIdx].pt;
			cv::perspectiveTransform(tmp,mapped,homographies[pm.src_img_idx]);
			src.push_back(mapped[0]);
			dst.push_back(features[pm.dst_img_idx].keypoints[pm.matches[i].trainIdx].pt);
			dbg("Calibration.recompute",2) << pm.dst_img_idx << "." << pm.matches[i].trainIdx << std::endl;
		    }
		}
		else if (found[pm.dst_img_idx] && pm.src_img_idx==curUnit) {
		    for (int i=0;i<pm.matches.size();i++) {
			// Project the point
			std::vector<cv::Point2f> tmp(1),mapped(1);
			tmp[0]=features[pm.dst_img_idx].keypoints[pm.matches[i].trainIdx].pt;
			cv::perspectiveTransform(tmp,mapped,homographies[pm.dst_img_idx]);
			src.push_back(mapped[0]);
			dst.push_back(features[pm.src_img_idx].keypoints[pm.matches[i].queryIdx].pt);
		    }
		}
	    }
	    assert(src.size()==bestcnt);
	    dbg("Calibration.recompute",2) << "Computing homography: " << std::endl;
	    for (int k=0;k<src.size();k++) {
		dbg("Calibration.recompute",2) << "   " << src[k] << "    " << dst[k]  << std::endl;
	    }
	    homographies[curUnit]=cv::findHomography(dst,src); // ,CV_RANSAC,.001);
	    found[curUnit]=true;
	}
	flatMat(DbgFile(dbgf__,"Calibration.recompute",1) << "Homography for laser " << curUnit << " = \n",homographies[curUnit]) << std::endl;
    }
    
    // Make all mappings refer to world coords
    for (int i=0;i<nunits+1;i++) {
	cv::Mat h=homographies[i];
	cv::Mat inv=homographies[nunits].inv();
	homographies[i]=inv*h;  // Need to map to common coords, then to world
	homographies[i]=homographies[i]/homographies[i].at<double>(2,2);      
	flatMat(DbgFile(dbgf__,"Calibration.recompute",1) << "Final homography for laser " << i << " = \n",homographies[i]) << std::endl;
    }
    
    // Compute origins
    for (int k=0;k<nunits;k++) {
	std::vector<cv::Point2f> src;
	std::vector<cv::Point3f> dst;
	for (int j=0;j<pairwiseMatches.size();j++) {
	    cv::detail::MatchesInfo pm = pairwiseMatches[j];
	    if (pm.src_img_idx==k && pm.dst_img_idx==nunits) {  // Correspondence with world
		for (int i=0;i<pm.matches.size();i++) {
		    src.push_back(features[pm.src_img_idx].keypoints[pm.matches[i].queryIdx].pt);
		    cv::Point2f dstpt=features[pm.dst_img_idx].keypoints[pm.matches[i].trainIdx].pt;
		    dst.push_back(cv::Point3f(dstpt.x,dstpt.y,0.0));
		    dbg("Calibration.recompute",2) << pm.dst_img_idx << "." << pm.matches[i].trainIdx << std::endl;
		}
	    }
	}
	if (src.size()<3) {
	    dbg("Calibration.recompute",0) << "Not enough correspondences between unit " << k << " and world to calculate positions (have " << src.size() << ")" << std::endl;
	    continue;
	}
	computeExtrinsics(src,dst,projection,rvecs[k], tvecs[k], poses[k]);
	dbg("Calibration.recompute",1) << "Laser " << k << " at [" << poses[k].at<double>(0,0) << "," << poses[k].at<double>(1,0) << "," << poses[k].at<double>(2,0) << "]" << std::endl;
    }
    
    // Push mappings to transforms.cc
    for (int i=0;i<nunits;i++) {
	cv::Mat inv=homographies[i].inv();
	inv=inv/inv.at<double>(2,2);
	dbg("Calibration",0) << "TODO: set transform/origin" << std::endl;
	// Lasers::instance()->getLaser(i)->getTransform().setTransform(inv,homographies[i]);
	// Last column of poses is translation -- origin is negative of the translation
	// Lasers::instance()->getLaser(i)->getTransform().setOrigin(Point(poses[i].at<double>(0,3),-poses[i].at<double>(1,3)));
    }
    testMappings();

    // Evaluate matches
    for (int i=0;i<relMappings.size();i++) {
	relMappings[i]->updateErrors();
    }

    return resultCode;
}

void Calibration::testMappings() const {
    std::vector<Point> worldPts = { Point(0,0), Point(0,1), Point(5,0), Point(0,5) };
    for (int i=0;i<worldPts.size();i++) {
	Point devPt=map(worldPts[i],nunits,0);
	Point finalPt=map(devPt,0,nunits);
	std::cout << "world: " << worldPts[i] << " -> device: " << devPt << " -> world: " << finalPt << std::endl;
    }
}

// Map to/from device or world  coordinates
Point Calibration::map(Point p, int fromUnit, int toUnit) const {
    if (toUnit<0)
	toUnit=nunits;	// Map to world 
    std::vector<cv::Point2f> p1, p2, world;
    Point flat1;
    if (fromUnit<nunits) {
	flat1=p;
    } else
	flat1=p;
	
    p1.push_back(cv::Point2f(flat1.X(),flat1.Y()));

    cv::perspectiveTransform(p1,world,homographies[fromUnit]);
    cv::perspectiveTransform(world,p2,homographies[toUnit].inv());

    Point flat2=Point(p2[0].x,p2[0].y);
    Point dev2;

    if (toUnit<nunits) {
	dev2=flat2;
    } else
	dev2=flat2;
    
    dbg("Calibration.map",2) << "Mapped " << p << " on unit " << fromUnit << " to " << dev2 << " on  unit " << toUnit << std::endl;
    return dev2;
}

std::vector<float> RelMapping::updateErrors() {
    std::vector<float> error(pt1.size());
    e2sum=0;
    e2cnt=0;
    for (int j=0;j<pt1.size();j++) {
	Point w1=Calibration::instance()->map(getDevicePt(0,j),unit1);
	Point w2=Calibration::instance()->map(getDevicePt(1,j),unit2);
	Point err=w2-w1;
	dbg("relMappings.updateErrors",1) << "L" << unit1 << "@" << pt1[j] << " -> " << w1 << " - L" << unit2  << "@" << pt2[j] << " -> " << w2  << ", e=" << err << ", rms=" << err.norm() << std::endl;
	error[j]=err.norm();
	if (locked[j]) {
	    e2sum+=error[j]*error[j];
	    e2cnt++;
	}
    }
    return error;
}

// Get the set of calibration points that should be drawn for the given laser
std::vector<Point> Calibration::getCalPoints(int unit) const {
    std::vector<Point> result;
    switch (laserMode) {
    case CM_NORMAL:
	// Nothing to do
	break;
    case CM_HOME:
	result.push_back(Point(0,0));
	break;
    case CM_CURPT:
	result = curMap->getCalPoints(unit,true);
	break;
    case CM_CURPAIR:
	result = curMap->getCalPoints(unit,false);
	break;
    case CM_ALL:
	for (int i=0;i<relMappings.size();i++) {
	    std::vector<Point> p=relMappings[i]->getCalPoints(unit,false);
	    result.insert(result.end(),p.begin(),p.end());
	}
	break;
    }
    return result;
}

// Get the set of calibration points that should be drawn for the given laser
std::vector<Point> RelMapping::getCalPoints(int unit, bool selectedOnly) const {
    std::vector<Point> result;
    const std::vector<Point> *p = 0;	// Point to point list for given unit
    if (unit1==unit)
	p=&pt1;
    else if (unit2==unit)
	p=&pt2;
    else
	// No match
	return result;
    if (selectedOnly)
	result.push_back(getDevicePt(unit==unit1?0:1));	// Only selected point
    else
	for (int i=0;i<pt1.size();i++)
	    if (i==selected || locked[i])
		result.push_back(getDevicePt(unit==unit1?0:1,i));	// All locked or selected points

    return result;
}
