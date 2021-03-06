#include <cairo-xlib.h>
#include "displaydevice.h"
#include "lasers.h"

class Video;

// Cross reference to locate points in window when mouse is clicked
class XRef {
 public:
    std::shared_ptr<Laser> laser;
    int anchorNumber;
    bool dev;
    Point winpos;
    bool reset; 	// True to indicate that it should be moved to this position
    XRef(std::shared_ptr<Laser> _laser, int _anchorNumber, bool _dev, Point _winpos) {laser=_laser; anchorNumber=_anchorNumber; dev=_dev; winpos=_winpos;  reset=false;}
    // Move coord for currently selected point by amount given in device or floor coordinates
    void movePoint(Point p);
};

class XRefs {
    std::vector<XRef> xref;
    int clickedEntry;
 public:
    XRefs() { clickedEntry=-1; }
    void markClosest(Point winpt);
    void update(Point newpos, bool clear);
    XRef *lookup(std::shared_ptr<Laser>laser, int anchorNumber, bool dev);
    void push_back(const XRef &xr) { xref.push_back(xr); }
    void refresh(cairo_t *cr, std::shared_ptr<Laser>laser, Video &video, int anchorNumber, bool dev, Point pos);
    void clear() { clickedEntry=-1; xref.clear(); }
    // Move coord for currently selected point by amount given in device or floor coordinates
    void movePoint(Point p);
};

class Video: public DisplayDevice {
    // Support for screen drawing to monitor operation
    Display *dpy;
    Window window;

    cairo_surface_t *surface;
    pthread_t displayThread;
    static void *runDisplay(void *w);

    void drawDevice(cairo_t *cr, float left, float top, float width, float height, std::shared_ptr<Laser>laser);
    void drawWorld(cairo_t *cr, float left, float top, float width, float height);
    void drawText(cairo_t *cr, float left,  float top, float width, float height,const char *msg) const;
    void drawInfo(cairo_t *cr, float left,  float top, float width, float height) const;

    std::shared_ptr<Lasers> lasers;
    Bounds bounds;
    static XRefs xrefs;
    
    // Locking
    pthread_mutex_t mutex;
    void lock();
    void unlock();
    
    std::ostringstream msg; // Message for display in bottom of window
    int msglife; 	// Number of frames left before clearing
    bool dirty;
    void update();
    void save() const {
	lasers->save();
    }
    void load();
    void clearTransforms();
 public:
    // Local window routines
    Video(std::shared_ptr<Lasers> lasers);
    ~Video();

    int open();
    // Set/get bounds of active area (values are in meters)
    void setBounds(const Bounds &bounds);
    const Bounds &getBounds() const { return bounds; }
    bool inActiveArea(Point p) const;
    std::ostream &newMessage() { msg.str(""); msglife=50; return msg; }

    // Display needs refresh
    void setDirty();
};

