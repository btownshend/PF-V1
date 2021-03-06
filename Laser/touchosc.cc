#include <fstream>
#include <sys/time.h>
#include "touchosc.h"
#include "person.h"
#include "connections.h"
#include "music.h"
#include "conductor.h"
#include "lasers.h"

TouchOSC *TouchOSC::theInstance=NULL;
static const std::string flags[]={"body","legs","grid","background","alignment","test","outline","allocationTest","intensity"};
static const int nflags=sizeof(flags)/sizeof(flags[0]);

TouchOSC::TouchOSC()  {
    theInstance=this;
    URLConfig urls("urlconfig.txt");

    /* Setup touchOSC sending */
    int touchOSCPort=urls.getPort("TO");
    const char *touchOSCHost=urls.getHost("TO");
    if (touchOSCPort==-1 || touchOSCHost==0) {
	fprintf(stderr,"Unable to locate TO in urlconfig.txt\n");
	remote=0;
    } else {
	char cbuf[10];
	sprintf(cbuf,"%d",touchOSCPort);
	remote = lo_address_new(touchOSCHost, cbuf);
	dbg("TouchOSC",1)  << "Set remote to " << loutil_address_get_url(remote) << std::endl;
	// Don't call sendOSC here as it will cause a recursive loop
    }

    // Setup 
    selectedGroup=0;
    selectedLaser=-1;
    frozen=false;
    layeringEnabled=false;
    onePerEnabled=true;
    attrsEnabled=true;
    fusionEnabled=true;
    labelsEnabled=true;
    maxConnections=10;
    visualThreshold=0.0;
    conductorGlobal=1.0;
    cellGlobal=1.0;
    pressTime.tv_sec=0;
    trackUID1=-1;
    trackUID2=-1;

    // Update UI
    updateLaserUI();
}
		       
TouchOSC::~TouchOSC() {
    if (remote)
	lo_address_free(remote);
}

int Fader::sendOSC() const {
    std::string faderspath=std::string("/ui/conn/faders/")+std::to_string(pos+1);
    std::string togglespath=std::string("/ui/conn/toggles/")+std::to_string(pos+1);
    if (TouchOSC::instance()->send((faderspath+"/label"),name) < 0)
	return -1;
    if (TouchOSC::instance()->send(faderspath,value) < 0)
	return -1;
    dbg("Fader.sendOSC",1) << "Set fader " << pos+1 << " to " << value << std::endl;
    if (TouchOSC::instance()->send((togglespath+"/1"),useValue?1.0:0.0) < 0)
	return -1;
    dbg("Fader.sendOSC",1) << "Set useValue toggle " << pos+1 << " to " << useValue << std::endl;
    return 0;
}

int Button::sendOSC() const {
    std::string buttonspath=std::string("/ui/conn/buttons/")+std::to_string(pos+1);
    if (TouchOSC::instance()->send((buttonspath+"/label"),name) < 0)
	return -1;
    if (TouchOSC::instance()->send(buttonspath+"/1",enabled?1.0:0.0) < 0)
	return -1;
    dbg("Button.sendOSC",1) << "Set button " << pos+1 << " to " << enabled << std::endl;
    return 0;
}

int Setting::sendOSC() const {
    if (TouchOSC::instance()->send("/ui/conn/selected",groupName) < 0)
	return -1;
    // Set selection
    std::string selpath="/ui/conn/select/"+std::to_string(pos%4+1)+"/"+std::to_string(pos/4+1);
    dbg("Setting.sendOSC",1) << "Sending " << selpath << "," << 1.0f << std::endl;
    if (TouchOSC::instance()->send(selpath,1.0f) <0 )
	return -1;

    const bool preClear = false;

    if (preClear) {
	// Clear all the current fader values and labels
	for (unsigned int i=0;i<MAXFADERS;i++) {
	    std::string faderspath=std::string("/ui/conn/faders/")+std::to_string(i+1);
	    if (TouchOSC::instance()->send(faderspath,0.0f) < 0)
		return -1;
	    if (TouchOSC::instance()->send((faderspath+"/label"),"") < 0)
		return -1;
	}
	// Clear all the current button values and labels
	for (unsigned int i=0;i<MAXBUTTONS;i++) {
	    std::string buttonspath=std::string("/ui/conn/buttons/")+std::to_string(i+1);
	    if (TouchOSC::instance()->send(buttonspath,0.0f) < 0)
		return -1;
	    if (TouchOSC::instance()->send((buttonspath+"/label"),"") < 0)
		return -1;
	}
    }

    for (unsigned int i=0;i<faders.size();i++)
	if (faders[i].sendOSC() < 0)
	    return -1;

    for (unsigned int i=0;i<buttons.size();i++)
	if (buttons[i].sendOSC() < 0)
	    return -1;

    return 0;
}


void Settings::sendOSC(int selectedGroup) const {
    // Send labels for attribute selection
    for (int p=0;p<MAXGROUPS;p++) {
	int col=(p%4)+1;
	int row=p/4 + 1;
	std::string path="/ui/conn/select/"+std::to_string(col)+"/"+std::to_string(row);
	//  set label
	std::string label="";
	const Setting *s=getSetting(p);
	if (s!=NULL) {
	    label=s->getGroupName();
	}
	dbg("Setting.sendOSC",3) << "Sending " << path << "," << label << std::endl;
	if (TouchOSC::instance()->send((path+"/label"),label) <0 ) 
	    return;
    }
    
    // Send settings for current select
    const Setting *s= getSetting(selectedGroup);
    if (s==NULL) {
	dbg("TouchOSC.sendOSC",1) << "Selected group " << selectedGroup << " is not valid, switching to group 0" << std::endl;
	s=getSetting(0);
	if (s==NULL)
	    return;
    }
    if(s->sendOSC() <0)
	return;
}

void TouchOSC::frameTick_impl(int frame) {
    dbg("TouchOSC.frameTick",3) << "Frame " << frame << std::endl;
    if (frame % 50 == 0) {
	static bool toggle;
	if (send("/health/LS",toggle?1.0f:0.0f) <0 ) 
	    return;
	toggle=!toggle;
    }
    if (frame % 100 == 4) {
	updateConnectionMap();
    }
    if (frame % 100 == 2) {
	updateLaserUI();
    }
}

Fader *TouchOSC::getFader_impl(std::string groupName, std::string faderName) {
    bool updatedUI=false;
    Setting *s=settings.getSetting(groupName);
    if (s==NULL) {
	dbg("TouchOSC.getValue",1) << "Requested group " << groupName << " does not exists; adding." << std::endl;
	settings.addGroup(groupName);
	s=settings.getSetting(groupName);
	updatedUI=true;
    }
    Fader *f=s->getFader(faderName);
    if (f==NULL) {
	dbg("TouchOSC.getValue",1) << "Requested fader " << faderName << " does not exist in group " << groupName << " adding it" << std::endl;
	s->addFader(faderName,-1,0.5f);
	f=s->getFader(faderName);
	updatedUI=true;
    }
    if (updatedUI)
	updateLaserUI();
    return f;
}

Button *TouchOSC::getButton_impl(std::string groupName, std::string buttonName) {
    bool updatedUI=false;
    Setting *s=settings.getSetting(groupName);
    if (s==NULL) {
	dbg("TouchOSC.getValue",1) << "Requested group " << groupName << " does not exists; adding." << std::endl;
	settings.addGroup(groupName);
	s=settings.getSetting(groupName);
	updatedUI=true;
    }
    Button *f=s->getButton(buttonName);
    if (f==NULL) {
	dbg("TouchOSC.getValue",1) << "Requested button " << buttonName << " does not exist in group " << groupName << " adding it" << std::endl;
	s->addButton(buttonName);
	f=s->getButton(buttonName);
	updatedUI=true;
    }
    if (updatedUI)
	updateLaserUI();
    return f;
}

int TouchOSC::handleOSCMessage_impl(const char *path, const char *types, lo_arg **argv,int argc,lo_message msg) {
    dbg("TouchOSC.handleOSCMessage",1)  << "Got message: " << path << "(" << types << ") from " << loutil_address_get_url(lo_message_get_source(msg)) << std::endl;

    std::string host=lo_address_get_hostname(lo_message_get_source(msg));
    char *pathCopy=new char[strlen(path)+1];
    strcpy(pathCopy,path);
    const char *tok=strtok(pathCopy,"/");

    bool handled=false;
    if (strcmp(tok,"ui")==0) {
	tok=strtok(NULL,"/");
	if (strcmp(tok,"conn")==0) {
	    tok=strtok(NULL,"/");
	    if (strcmp(tok,"select")==0) {
		int col=atoi(strtok(NULL,"/"))-1;
		int row=atoi(strtok(NULL,"/"))-1;
		dbg("TouchOSC.conn",1) << "Selected group position " << col << "/" << row << " with value " << argv[0]->f << std::endl;
		if (argv[0]->f>0.5) {
		    int gpos=row*4+col;
		    const Setting *newGroup=settings.getSetting(gpos);
		    if (newGroup==NULL) {
			dbg("TouchOSC.conn",1) << "No group at position " << gpos << std::endl;
		    } else {
			selectedGroup=gpos;
			dbg("TouchOSC.conn",1) << "Switch to group " << newGroup->getGroupName() << std::endl;
			updateLaserUI();
		    }
		}
		handled=true;
	    } else if (strcmp(tok,"faders")==0) {
		int pos=atoi(strtok(NULL,"/"))-1;
		dbg("TouchOSC.faders",1) << "Set fader " << pos << " with value " << argv[0]->f << std::endl;
		Setting *s=settings.getSetting(selectedGroup);
		if (s==NULL) {
		    dbg("TouchOSC.faders",1) << "Selected group " << selectedGroup << " is not valid." << std::endl;
		} else {
		    Fader *f=s->getFader(pos);
		    if (f==NULL) {
			dbg("TouchOSC.faders",1) << "Selected fader " << pos << " is not valid." << std::endl;
		    } else {
			f->set(argv[0]->f);
		    }
		}
		handled=true;
	    } else if (strcmp(tok,"toggles")==0) {
		int col=atoi(strtok(NULL,"/"))-1;
		int row=atoi(strtok(NULL,"/"))-1;
		dbg("TouchOSC.toggles",1) << "Set toggle " << col << "/" << row << " with value " << argv[0]->f << std::endl;
		Setting *s=settings.getSetting(selectedGroup);
		if (s==NULL) {
		    dbg("TouchOSC.toggles",1) << "Selected group " << selectedGroup << " is not valid." << std::endl;
		} else {
		    Fader *f=s->getFader(col);
		    if (f==NULL) {
			dbg("TouchOSC.toggles",1) << "Selected fader " << col << " is not valid." << std::endl;
		    } else {
			if (row==0)
			    f->setUseValueToggle(argv[0]->f>0.5);
			else
			    dbg("TouchOSC.toggles",1) << "Bad row: " << row << " from /ui/conn/toggles" << std::endl;
		    }
		}
		handled=true;
	    } else if (strcmp(tok,"buttons")==0) {
		int col=atoi(strtok(NULL,"/"))-1;
		int row=atoi(strtok(NULL,"/"))-1;
		int pos=col+row*Setting::MAXBUTTONS;
		dbg("TouchOSC.buttons",1) << "Received command to set button " << pos << " with value " << argv[0]->f << std::endl;
		Setting *s=settings.getSetting(selectedGroup);
		if (s==NULL) {
		    dbg("TouchOSC.buttons",1) << "Selected group " << selectedGroup << " is not valid." << std::endl;
		} else {
		    Button *f=s->getButton(pos);
		    if (f==NULL) {
			dbg("TouchOSC.buttons",1) << "Selected button " << pos << " is not valid in group " << s->getGroupName() << std::endl;
		    } else {
			f->set(argv[0]->f>0.5);
		    }
		}
		handled=true;
	    }
	} else if (strcmp(tok,"preset")==0) {
	    tok=strtok(NULL,"/");
	    struct timeval now;
	    gettimeofday(&now,0);
	    if (argv[0]->f > 0.5)
		pressTime=now;
	    else {
		if (pressTime.tv_sec==0) {
		    dbg("TouchOSC.preset",1) << "Missed preset " << tok << " press" << std::endl;
		} else {
		    float delta=(now.tv_sec-pressTime.tv_sec)+(now.tv_usec-pressTime.tv_usec)/1e6;
		    dbg("TouchOSC.preset",1) << "Got preset " << tok << " held for " << delta << " seconds" << std::endl;
		    if (strcmp(tok,"10")==0) 
			// Defaults
			load(std::string("settings_default.txt"));
		    else if (delta>1) 
			save(std::string("settings_")+tok+".txt");
		    else
			load(std::string("settings_")+tok+".txt");
		    pressTime.tv_sec=0;
		}
	    }
	    handled=true;
	} else if (strcmp(tok,"linked") == 0) {
	    int col=atoi(strtok(NULL,"/"))-1;
	    int row=atoi(strtok(NULL,"/"))-1;
	    dbg("TouchOSC.linked",1) << "Got linked touch  at " << col << "," << row << " v=" << argv[0]->f << std::endl;
	    std::vector<int> uids = People::instance()->getIDs();
	    if (col>=uids.size() || row>=uids.size()) {
		dbg("TouchOSC.linked",1) << "Only have " << uids.size() << "UIDs; touch out of range" << std::endl;
	    } else {
		trackUID1=uids[row];
		trackUID2=uids[col];
		dbg("TouchOSC.linked",1) << "Displaying UIDs " << trackUID1 << "," << trackUID2 << std::endl;
	    }
	    struct timeval now;
	    gettimeofday(&now,0);
	    if (argv[0]->f > 0.5)
		pressTime=now;
	    else {
		if (pressTime.tv_sec==0) {
		    dbg("TouchOSC.linked",1) << "Missed linked " << tok << " press" << std::endl;
		} else {
		    float delta=(now.tv_sec-pressTime.tv_sec)+(now.tv_usec-pressTime.tv_usec)/1e6;
		    dbg("TouchOSC.linked",1) << "Got linked held for " << delta << " seconds" << std::endl;
		    if (delta>1) {
			// Force a connection or attribute between the selected people
			Setting *s=settings.getSetting(selectedGroup);
			if (s==NULL) {
			    dbg("TouchOSC.linked",1) << "No attribute selected" << std::endl;
			} else {
			    std::string attribute = s->getGroupName();
			    if (trackUID1!= trackUID2) {
				// A connection
				CIDType cid;
				if (trackUID1>trackUID2) 
				    std::swap(trackUID1,trackUID2);
				Connection conn(std::to_string(trackUID1)+"-"+std::to_string(trackUID2),trackUID1,trackUID2);
				conn.set("persistent",attribute,trackUID1,trackUID2);
				conn.setAge(-100);
				dbg("TouchOSC.linked",1) << "Adding connection " << conn << std::endl;
				Connections::instance()->add(conn);
			    }
			}
		    }
		    pressTime.tv_sec=0;
		}
	    }

	    handled=true;
	} else if (strcmp(tok,"laser")==0) {
	    tok=strtok(NULL,"/");
	    for (int i=0;i<nflags;i++) {
		if (flags[i]==tok) {
		    Lasers::instance()->setFlag(tok,argv[0]->f>0.5);
		    handled=true;
		}
	    }
	    if (!handled) {
		if (strcmp(tok,"freeze")==0) {
		    frozen=argv[0]->f>0.5;
		}  else if (strcmp(tok,"select")==0) {
		    int row=atoi(strtok(NULL,"/"))-1;
		    int col=atoi(strtok(NULL,"/"))-1;
		    dbg("TouchOSC",1) << "Got /ui/laser/select, row=" << row << ", col=" << col << ", value=" << argv[0]->f << std::endl;
		    if (argv[0]->f > 0.5)
			selectedLaser=col;
		    else
			selectedLaser=-1;
		} else if (strcmp(tok,"save") == 0 && argv[0]->f>0) {
		    Lasers::instance()->save();
		} else if (strcmp(tok,"load") == 0 && argv[0]->f>0) {
		    Lasers::instance()->load();
		} else if (strcmp(tok,"enable")==0) {
		    int row=atoi(strtok(NULL,"/"))-1;
		    int col=atoi(strtok(NULL,"/"))-1;
		    dbg("TouchOSC",1) << "Got /ui/laser/enable, row=" << row << ", col=" << col << ", value=" << argv[0]->f << std::endl;
		    Lasers::instance()->enable(col,argv[0]->f>0.5);
		} else if (strcmp(tok,"power")==0) {
		    std::shared_ptr<Laser> laser=Lasers::instance()->getLaser(selectedLaser);
		    if (laser!=nullptr) {
			laser->setPower(argv[0]->f);
		    }
		} else if (strcmp(tok,"red")==0) {
		    std::shared_ptr<Laser> laser=Lasers::instance()->getLaser(selectedLaser);
		    if (laser!=nullptr) {
			Color c=laser->getMaxColor();
			c=Color(argv[0]->f/100.0,c.green(),c.blue());
			laser->setMaxColor(c);
		    }
		} else if (strcmp(tok,"green")==0) {
		    std::shared_ptr<Laser> laser=Lasers::instance()->getLaser(selectedLaser);
		    if (laser!=nullptr) {
			Color c=laser->getMaxColor();
			c=Color(c.red(),argv[0]->f/100.0,c.blue());
			laser->setMaxColor(c);
		    }
		} else if (strcmp(tok,"blue")==0) {
		    std::shared_ptr<Laser> laser=Lasers::instance()->getLaser(selectedLaser);
		    if (laser!=nullptr) {
			Color c=laser->getMaxColor();
			c=Color(c.red(),c.green(),argv[0]->f/100.0);
			laser->setMaxColor(c);
		    }
		} else if (strcmp(tok,"xmin")==0 ||strcmp(tok,"ymin")==0 ||strcmp(tok,"xmax")==0 ||strcmp(tok,"ymax")==0||strcmp(tok,"hfov")||strcmp(tok,"vfov")) {
		    dbg("TouchOSC",1) << "Normalize range[ " << tok << "] set to " << argv[0]->f << std::endl;
		    std::shared_ptr<Laser> laser=Lasers::instance()->getLaser(selectedLaser);
		    if (laser!=nullptr) {
			Lasers::instance()->lock();   // In case another thread tries to use the transform while we're changing it
			if (strcmp(tok,"xmin")==0)
			    laser->getTransform().setMinX(argv[0]->f);
			else if (strcmp(tok,"ymin")==0)
			    laser->getTransform().setMinY(argv[0]->f);
			else if (strcmp(tok,"xmax")==0)
			    laser->getTransform().setMaxX(argv[0]->f);
			else if (strcmp(tok,"ymax")==0)
			    laser->getTransform().setMaxY(argv[0]->f);
			else if (strcmp(tok,"hfov")==0)
			    laser->setHFOV(argv[0]->f*M_PI/180.0);
			else if (strcmp(tok,"vfov")==0)
			    laser->setVFOV(argv[0]->f*M_PI/180.0);
			Lasers::instance()->unlock();
		    }
		}
		handled=true;
	    }
	} else if (strcmp(tok,"attrenable")==0) {
	    attrsEnabled=argv[0]->f>0.5;
	    handled=true;
	} else if (strcmp(tok,"layer")==0) {
	    layeringEnabled=argv[0]->f>0.5;
	    handled=true;
	} else if (strcmp(tok,"oneper")==0) {
	    onePerEnabled=argv[0]->f>0.5;
	    handled=true;
	} else if (strcmp(tok,"fusion")==0) {
	    fusionEnabled=argv[0]->f>0.5;
	    handled=true;
	} else if (strcmp(tok,"labels")==0) {
	    labelsEnabled=argv[0]->f>0.5;
	    handled=true;
	} else if (strcmp(tok,"maxconn")==0) {
	    maxConnections=argv[0]->f;
	    handled=true;
	} else if (strcmp(tok,"visthresh")==0) {
	    visualThreshold=argv[0]->f;
	    handled=true;
	} else if (strcmp(tok,"condglobal")==0) {
	    // Pass on to conductor TODO
	    Conductor::instance()->send("/ui/condglobal",argv[0]->f);
	    conductorGlobal=argv[0]->f;
	    handled=true;
	} else if (strcmp(tok,"cellglobal")==0) {
	    // Pass on to conductor TODO
	    Conductor::instance()->send("/ui/cellglobal",argv[0]->f);
	    cellGlobal=argv[0]->f;
	    handled=true;
	}	    
    }
    if (handled) {
	updateLaserUI();
    } else {
	dbg("TouchOSC.handleOSCMessage",1) << "Unhandled message: " << path << ": parse failed at token: " << tok << std::endl;
    }
    
    delete [] pathCopy;
    return handled?0:1;
}

// Update touchosc display relating to laser controls
void TouchOSC::updateLaserUI() const {
    // Send OSC to make UI reflect current values
    dbg("TouchOSC.updateLaserUI",1) << "Sending OSC updates with group " << selectedGroup << " to " << loutil_address_get_url(remote) << std::endl;

    settings.sendOSC(selectedGroup);
    if (send("/ui/laser/freeze",frozen?1.0:0.0) < 0) return;
    if (send("/ui/attrenable",attrsEnabled?1.0:0.0) < 0) return;
    if (send("/ui/layer",layeringEnabled?1.0:0.0) < 0) return;
    if (send("/ui/oneper",onePerEnabled?1.0:0.0) < 0) return;
    if (send("/ui/fusion",fusionEnabled?1.0:0.0) < 0) return;
    if (send("/ui/labels",labelsEnabled?1.0:0.0) < 0) return;
    if (send("/ui/maxconn/label","Max Connections: "+std::to_string(maxConnections))<0) return;
    if (send("/ui/visthresh/label","Visual Threshold: "+std::to_string(int(visualThreshold*100)))<0) return;
    if (send("/ui/condglobal/label","Conductor Global: "+std::to_string(int(conductorGlobal*100)))<0) return;
    if (send("/ui/cellglobal/label","Cell Global: "+std::to_string(int(cellGlobal*100)))<0) return;

    std::shared_ptr<Laser> laser=Lasers::instance()->getLaser(selectedLaser);

    if (laser==nullptr) {
	// Blank everything
	send("/ui/laser/xmin/label","");
	send("/ui/laser/ymin/label","");
	send("/ui/laser/xmax/label","");
	send("/ui/laser/ymax/label","");
	send("/ui/laser/points/label","points");
	send("/ui/laser/skew/label","skew");
	send("/ui/laser/power/label","power");
	send("/ui/laser/red/label","red");
	send("/ui/laser/green/label","green");
	send("/ui/laser/blue/label","blue");
	send("/ui/laser/intensityPts/label","intensityPts");
	send("/ui/laser/preblank/label","pre-blank");
	send("/ui/laser/postblank/label","post-blank");
	send("/ui/laser/pps/label","PPS");
	send("/ui/laser/vfov/label","");
	send("/ui/laser/hfov/label","");
    } else {
	send("/ui/laser/xmin/label",laser->getTransform().getMinX());
	send("/ui/laser/ymin/label",laser->getTransform().getMinY());
	send("/ui/laser/xmax/label",laser->getTransform().getMaxX());
	send("/ui/laser/ymax/label",laser->getTransform().getMaxY());
	send("/ui/laser/xmin",laser->getTransform().getMinX());
	send("/ui/laser/ymin",laser->getTransform().getMinY());
	send("/ui/laser/xmax",laser->getTransform().getMaxX());
	send("/ui/laser/ymax",laser->getTransform().getMaxY());

	send("/ui/laser/points/label",std::to_string(laser->getNPoints())+" points");
	send("/ui/laser/skew/label",std::to_string(laser->getSkew())+" skew");
	send("/ui/laser/power/label","Power: "+std::to_string((int)laser->getPower())+"%");
	Color c=laser->getMaxColor();
	send("/ui/laser/red/label","Red: "+std::to_string((int)(c.red()*100))+"%");
	send("/ui/laser/green/label","Green: "+std::to_string((int)(c.green()*100))+"%");
	send("/ui/laser/blue/label","Blue: "+std::to_string((int)(c.blue()*100))+"%");
	send("/ui/laser/intensitypts/label",std::to_string(laser->getIntensityPts())+" pts");
	send("/ui/laser/preblank/label",std::to_string(laser->getPreBlanks())+" pre-blank");
	send("/ui/laser/postblank/label",std::to_string(laser->getPostBlanks())+" post-blank");
	send("/ui/laser/pps/label",std::to_string(laser->getPPS())+" PPS");
	send("/ui/laser/vfov/label",std::to_string((int)(laser->getVFOV()*180/M_PI)));
	send("/ui/laser/hfov/label",std::to_string((int)(laser->getHFOV()*180/M_PI)));

	send("/ui/laser/points",(float)laser->getNPoints());
	send("/ui/laser/skew",(float)laser->getSkew());
	send("/ui/laser/power",(float)laser->getPower());
	send("/ui/laser/red",(float)(c.red()*100));
	send("/ui/laser/green",(float)(c.green()*100));
	send("/ui/laser/blue",(float)(c.blue()*100));
	send("/ui/laser/intensityPts",(float)laser->getIntensityPts());
	send("/ui/laser/preblank",(float)laser->getPreBlanks());
	send("/ui/laser/postblank",(float)laser->getPostBlanks());
	send("/ui/laser/pps",(float)laser->getPPS());
	send("/ui/laser/vfov",laser->getVFOV()*180/M_PI);
	send("/ui/laser/hfov",laser->getHFOV()*180/M_PI);
    }
    for (int i=0;i<nflags;i++)
	send("/ui/laser/"+flags[i],Lasers::instance()->getFlag(flags[i])?1.0:0.0); 
    for (int unit=0;unit<Lasers::instance()->size();unit++) {
	send("/ui/laser/enable/1/"+std::to_string(unit+1),Lasers::instance()->getLaser(unit)->isEnabled()?1.0:0.0);
	send("/ui/laser/select/1/"+std::to_string(unit+1),(unit==selectedLaser)?1.0:0.0);
    }
}

void TouchOSC::save(std::string filename) const {
    try {
	dbg("TouchOSC.save",1) << "Saving settings in " << filename << std::endl;
	std::ofstream ofs(filename);
	if (!ofs.good()) {
	    std::cerr << "Failed open of " << filename << " for saving" << std::endl;
	    return;
	}
	boost::archive::text_oarchive oa(ofs);
	oa << settings;
	dbg("TouchOSC.save",1) << "Saved settings in " << filename << std::endl;
    } catch (std::ofstream::failure e) {
	std::cerr << "Exception saving to " << filename << ": " << e.what() << std::endl;
    } catch (boost::archive::archive_exception e) {
	std::cerr << "Exception saving to " << filename << ": " << e.what() << std::endl;
    }
}

void TouchOSC::load(std::string filename) {
    try {
	std::ifstream ifs(filename);
	if (!ifs.good()) {
	    std::cerr << "Failed open of " << filename << " for loading" << std::endl;
	    return;
	}
	boost::archive::text_iarchive ia(ifs);
	ia >> settings;
	dbg("TouchOSC.save",1) << "Loaded settings from " << filename << "; now have " << settings.size() << "groups" << std::endl;
	updateLaserUI();
    } catch (std::ifstream::failure e) {
	std::cerr << "Exception loading from " << filename << ": " << e.what() << std::endl;
    } catch (boost::archive::archive_exception e) {
	std::cerr << "Exception loading from " << filename << ": " << e.what() << std::endl;
    }
}

// Send OSC to touchOSC to update connections
// UID labels at /ui/uid/[row]
// multipush at /ui/linked/[col]/[row]
void TouchOSC::updateConnectionMap() const {
    // Update UID labels
    dbg("TouchOSC.updateConnectionMap",1) << "Updating connection map" << std::endl;
    static const int MAXUIDDISPLAY=10;
    std::vector<int> uids = People::instance()->getIDs();
    for (int i=0;i<MAXUIDDISPLAY;i++) {
	std::string uidstring = "";
	if (i<uids.size())
	    uidstring = std::to_string(uids[i]);
	if (send("/ui/uid/"+std::to_string(i+1),uidstring) <0 ) 
	    return;
	for (int j=0;j<MAXUIDDISPLAY;j++) {
	    bool connected=false;
	    if (i<uids.size() && j<uids.size())
		connected = Connections::instance()->isConnected(uids[i],uids[j]) || Connections::instance()->isConnected(uids[j],uids[i]);
	    std::string path="/ui/linked/"+std::to_string(j+1)+"/"+std::to_string(i+1);
	    dbg("TouchOSC.updateConnectionMap",5) << "Send " << path << "," << (connected?1.0f:0.0f) << std::endl;

	    if (send(path,connected?1.0f:0.0f) <0 )
		return;
	}
    }
    
    // Display attributes of selected UIDs
    std::string uid1label="";
    std::string uid2label="";
    if (trackUID1 >= 0 && People::instance()->personExists(trackUID1)) {
	uid1label=std::to_string(trackUID1);
	Attributes attr=People::instance()->getAttributes(trackUID1);
	std::vector<std::string> attrNames=attr.getAttributeNames();
	for (int i=0;i<attrNames.size();i++) {
	    Attribute a=attr.get(attrNames[i]);
	    std::string msg=uid1label+": "+attrNames[i]+"="+std::to_string(a.getValue());
	    std::string path="/ui/connattr/"+std::to_string(i+1);
	    if (send(path,msg) <0 )
		return;
	}
	int row=attrNames.size();
	// And connections with other ID
	if (trackUID2 >= 0 && People::instance()->personExists(trackUID2)) {
	    uid2label=std::to_string(trackUID2);
	    if (Connections::instance()->isConnected(trackUID1,trackUID2)) {
		Connection c=Connections::instance()->getConnection(trackUID1,trackUID2);
		Attributes attr=c.getAttributes();
		std::vector<std::string> attrNames=attr.getAttributeNames();
		for (int i=0;i<attrNames.size();i++) {
		    Attribute a=attr.get(attrNames[i]);
		    std::string msg=uid1label+"-"+uid2label+": "+attrNames[i]+"="+std::to_string(a.getValue());
		    std::string path="/ui/connattr/"+std::to_string(i+row+1);
		    if (send(path,msg) <0 )
			return;
		}
		row+=attrNames.size();
	    }
	}
	// Blank out remaining attribute names
	static const int MAXATTR=10;
	for (int i=row;i<MAXATTR;i++) {
	    std::string path="/ui/connattr/"+std::to_string(i+1);
	    if (send(path,"") <0 ) {
		return;
	    }
	}
    }
    if (send("/ui/connattr/uid1",uid1label) <0 ) 
	return;
    if (send("/ui/connattr/uid2",uid2label) <0 )
	return;
    // Set all the conductor activity LEDs
    static const char* conxattrs[]={"grouped","friends","contact","coord","irlbuds","nearby","strangers","facing","fusion","touch"};
    for (int i=0;i<sizeof(conxattrs)/sizeof(conxattrs[0]);i++) {
	bool isset=Connections::instance()->attrCount(conxattrs[i]);
	if (isset)
	    dbg("TouchOSC.updateConnectionMap",1) << "Turning on " << conxattrs[i] << " led" << std::endl;
	if (send((std::string("/ui/cond/active/")+conxattrs[i]).c_str(),isset?1.0:0.0) <0)
	    return;
    }
    static const char* cellattrs[]={"interactive","static","kinetic","fast","timein"};
    for (int i=0;i<sizeof(cellattrs)/sizeof(cellattrs[0]);i++) {
	bool isset=People::instance()->attrCount(cellattrs[i]);
	if (isset)
	    dbg("TouchOSC.updateConnectionMap",1) << "Turning on " << cellattrs[i] << " led" << std::endl;
	if (send((std::string("/ui/cond/active/")+cellattrs[i]).c_str(),isset?1.0:0.0) <0)
	    return;
    }
}

int TouchOSC::send(std::string path, float value) const {
    dbg("TouchOSC.send",4) << "send " << path << "," << value << std::endl;
    if (lo_send(remote,path.c_str(),"f",value) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

int TouchOSC::send(std::string path, float v1, float v2) const {
dbg("TouchOSC.send",4) << "send " << path << "," << v1 << ", " << v2 << std::endl;
    if (lo_send(remote,path.c_str(),"ff",v1,v2) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

#if 0
int TouchOSC::send(std::string path, float v1, float v2, float v3, float v4) const {
    dbg("TouchOSC.send",4) << "send " << path << "," << v1 << ", " << v2 << ", " << v3 << ", " << v4 << std::endl;
    if (lo_send(remote,path.c_str(),"ffff",v1,v2,v3,v4) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

int TouchOSC::send(std::string path, float v1, float v2, float v3, float v4, float v5, float v6, float v7) const {
    dbg("TouchOSC.send",4) << "send " << path << "," << v1 << ", " << v2 << ", " << v3 << ", " << v4 << "..." << std::endl;
    if (lo_send(remote,path.c_str(),"fffffff",v1,v2,v3,v4,v5,v6,v7) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

int TouchOSC::send(std::string path, float v1, float v2, float v3, float v4, float v5, float v6, float v7,float v8, float v9, float v10) const {
    dbg("TouchOSC.send",4) << "send " << path << "," << v1 << ", " << v2 << ", " << v3 << ", " << v4 << "..." << std::endl;
    if (lo_send(remote,path.c_str(),"ffffffffff",v1,v2,v3,v4,v5,v6,v7,v8,v9,v10) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}

int TouchOSC::send(std::string path, float v1, float v2, float v3, float v4, float v5, float v6, float v7,float v8, float v9, float v10,float v11, float v12, float v13, float v14, float v15, float v16) const {
    dbg("TouchOSC.send",4) << "send " << path << "," << v1 << ", " << v2 << ", " << v3 << ", " << v4 << "..." << std::endl;
    if (lo_send(remote,path.c_str(),"ffffffffff",v1,v2,v3,v4,v5,v6,v7,v8,v9,v10,v11,v12,v13,v14,v15,v16) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}
#endif

int TouchOSC::send(std::string path, std::string value) const {
    dbg("TouchOSC.send",4) << "send " << path << "," << value << std::endl;
    if (lo_send(remote,path.c_str(),"s",value.c_str()) <0 ) {
	dbg("TouchOSC.send",1) << "Failed send of " << path << " to " << loutil_address_get_url(remote) << ": " << lo_address_errstr(remote) << std::endl;
	return -1;
    }
    usleep(10);
    return 0;
}
