#include <assert.h>
#include "background.h"
#include "parameters.h"
#include "dbg.h"

Background::Background() {
    scanRes=0;
}

void Background::setup(const SickIO &sick) {
    if (range[0].size() ==sick.getNumMeasurements())
	return;
    dbg("Background.setup",1) << "Setting up background vectors with " << sick.getNumMeasurements() << " entries." << std::endl;
    for (int i=0;i<NRANGES;i++) {
	range[i].resize(sick.getNumMeasurements());
	freq[i].resize(sick.getNumMeasurements());
	sigma[i].resize(sick.getNumMeasurements());
	for (int j=0;j<sick.getNumMeasurements();j++)
	    sigma[i][j]=MEANBGSIGMA;
    }
    farnotseen.resize(sick.getNumMeasurements());
    scanRes=sick.getScanRes();
}

void Background::swap(int k, int i, int j) {
    // Swap range[i][k] with range[j][k]
    float tmprange=range[i][k];
    range[i][k]=range[j][k];
    range[j][k]=tmprange;
    float tmpfreq=freq[i][k];
    freq[i][k]=freq[j][k];
    freq[j][k]=tmpfreq;
    float tmpsigma=sigma[i][k];
    sigma[i][k]=sigma[j][k];
    sigma[j][k]=tmpsigma;
}

// Return probability of each scan pixel being part of background (fixed structures not to be considered targets)
std::vector<float> Background::like(const SickIO &sick) const {
    ((Background *)this)->setup(sick);
    std::vector<float> result(sick.getNumMeasurements(),0.0);
    const unsigned int *srange = sick.getRange(0);
    for (unsigned int i=0;i<sick.getNumMeasurements();i++) {
	if (srange[i]>=MAXRANGE || srange[i]<MINRANGE)
	    result[i]=1.0;
	else {
	    // Compute result
	    result[i]=0.0;
	    for (int k=0;k<NRANGES-1;k++) {
		// This is a background pixel if it matches the ranges of this scan's background, or is farther than maximum background
		if (freq[k][i]>0 && (fabs(srange[i]-range[k][i]) < MINBGSEP || (srange[i]>range[k][i] && k==0) ))
		    result[i]+=freq[k][i];
	    }
	    if (result[i]<MINBGFREQ) {
		// No strong primary matches, If it matches adjacent scan backgrounds, consider that with a weighting
		for (int k=0;k<NRANGES-1;k++) {
		    if (i>0 && fabs(srange[i]-range[k][i-1])<MINBGSEP) 
			result[i]+=freq[k][i-1]*ADJSCANBGWEIGHT;
		    if (i+1<sick.getNumMeasurements() && fabs(srange[i]-range[k][i+1])<MINBGSEP)
			result[i]+=freq[k][i+1]*ADJSCANBGWEIGHT;
		}
	    }
	    if (result[i]==0 && freq[0][i]>0) {
		// Still no matches, check if is between this and adjacent background
		if (i>0 && freq[0][i-1]>0 && ((srange[i]>range[0][i]) != (srange[i]>range[0][i-1]))) {
		    result[i]=std::min(freq[0][i],freq[0][i-1])*INTERPSCANBGWEIGHT;
		    dbg("Background.like",4) << "Scan " << i << " at " << std::setprecision(0) << std::fixed << srange[i] << " is between adjacent background ranges of " << range[0][i] << " and " << range[0][i-1] << ": result=" << std::setprecision(3) << result[i] << std::endl;
		}
	if (i+1<sick.getNumMeasurements() && freq[0][i+1]>0 && ((srange[i]>range[0][i]) != (srange[i]>range[0][i+1]))) {
		    result[i]=std::min(freq[0][i],freq[0][i+1])*INTERPSCANBGWEIGHT;
		    dbg("Background.like",4) << "Scan " << i << " at " << std::setprecision(0)  <<std::fixed <<  srange[i] << " is between adjacent background ranges of " << range[0][i] << " and " << range[0][i+1] << ": result=" << std::setprecision(3) << result[i] << std::endl;
		}
	    }
	    if (result[i]>1.0) {
		dbg("Background.like",1) << "Scan " << i << " at " << srange[i] << " had prob " << result[i] << "; reducing to 1.0"  << std::endl;
		result[i]=1.0;
	    }
	}
	// TODO: This is not a correct likelihood -- need to have it reflect the p(obs|bg) in the same way that we have p(obs|target) so they can be compared
	// For now, assume this probability occurs over a range of [-MINBGSEP,MINBGSEP], so pdf= prob/(2*MINBGSEP)  (in meters, since all the other PDF's are in meters)
	// Problem is that even for very low probs (e.g. .002), likelihood is >>entrylikelihood
	result[i]=log(result[i]/(2.0*MINBGSEP/UNITSPERM)); 
    }
    return result;
}

void Background::update(const SickIO &sick, const std::vector<int> &assignments, bool all) {
    setup(sick);
    const unsigned int *srange = sick.getRange(0);
   
    // range[0] is for the fartherest seen in last BGLONGDISTLIFE frames
    // range[1] is the most frequent seen, with exponential decay using UPDATETC
    // range[2] is the last value seen, not matching 0 or 1;   promoted to range[1] if its frequency passes range[1]
    for (unsigned int i=0;i<sick.getNumMeasurements();i++) {
	if (assignments[i]!=-1 && !all)
	    continue;
	float tc=UPDATETC;
	// Update if long distance
	for (int k=0;k<NRANGES;k++) {
	    // Note allow updates even if range>MAXRANGE, otherwise points slightly smaller than MAXRANGE get biased and have low freq
	    if (fabs(srange[i]-range[k][i]) < MINBGSEP) {
		range[k][i]=srange[i]*1.0f/tc + range[k][i]*(1-1.0f/tc);
		freq[k][i]+=1.0f/tc;
		// Swap ordering if needed
		for (int kk=k;kk>1;kk--)
		    if  (freq[kk][i] > freq[kk-1][i]) {
			dbg("Background.update",2) << "Promoting background at scan " << i << " with range=" << range[kk][i] << ", freq=" << freq[kk][i] << " to level " << kk-1 << "; range[0]=" << range[0][i]  << std::endl;
			swap(i,kk,kk-1);
		    } else
			break;
		if (k==0)
		    farnotseen[i]=0;
		break;
	    } else if (k==0 && srange[i] > range[k][i]) {
		// New long distance point
		dbg("Background.update",2) << "Farthest background at scan " << i << " moved from " << range[k][i] << " to " << srange[i] << std::endl;
		range[k][i]=srange[i]*1.0f/FARUPDATETC + range[k][i]*(1-1.0f/FARUPDATETC);
		freq[k][i]+=1.0f/tc;
		farnotseen[i]=0;
		break;
	    } else if (k==NRANGES-1 && srange[i]>=MINRANGE) {
		// No matches, inside active area; reset last range value 
		range[k][i]=srange[i];
		freq[k][i]=1.0f/tc;
		for (int kk=k;kk>1;kk--)
		    if  (freq[kk][i] > freq[kk-1][i])
			swap(i,kk,kk-1);
		    else
			break;
	    }
	}
	// Rescale so freq adds to 1.0
	float ftotal=0;
	for (int k=0;k<NRANGES;k++)
	    ftotal+=freq[k][i];
	for (int k=0;k<NRANGES;k++)
	    freq[k][i]/=ftotal;

	if (farnotseen[i] > BGLONGDISTLIFE) {
	    dbg("Background.update",2) << "Farthest background at scan " << i << " not seen for " << farnotseen[i] << " frames.  Resetting to " << range[1][i] << std::endl;
	    swap(i,0,1);
	    swap(i,1,2);
	    farnotseen[i]=0;
	}
    }
}

mxArray *Background::convertToMX() const {
    const char *fieldnames[]={"range","angle","freq","sigma"};
    mxArray *bg = mxCreateStructMatrix(1,1,sizeof(fieldnames)/sizeof(fieldnames[0]),fieldnames);

    mxArray *pRange = mxCreateDoubleMatrix(NRANGES,range[0].size(),mxREAL);
    assert(pRange!=NULL);
    double *data=mxGetPr(pRange);
    for (unsigned int i=0;i<range[0].size();i++)
	for (int j=0;j<NRANGES;j++)
	    *data++=range[j][i]/UNITSPERM;
    mxSetField(bg,0,"range",pRange);

    mxArray *pAngle = mxCreateDoubleMatrix(1,range[0].size(),mxREAL);
    assert(pAngle!=NULL);
    data=mxGetPr(pAngle);
    for (unsigned int i=0;i<range[0].size();i++)
	*data++=(i-(range[0].size()-1)/2.0)*scanRes*M_PI/180;
    mxSetField(bg,0,"angle",pAngle);

    mxArray *pFreq = mxCreateDoubleMatrix(NRANGES,range[0].size(),mxREAL);
    assert(pFreq!=NULL);
    data=mxGetPr(pFreq);
    for (unsigned int i=0;i<range[0].size();i++)
	for (int j=0;j<NRANGES;j++)
	    *data++=freq[j][i];
    mxSetField(bg,0,"freq",pFreq);

    mxArray *pSigma = mxCreateDoubleMatrix(NRANGES,range[0].size(),mxREAL);
    assert(pSigma!=NULL);
    data=mxGetPr(pSigma);
    for (unsigned int i=0;i<range[0].size();i++)
	for (int j=0;j<NRANGES;j++)
	    *data++=sigma[j][i]/UNITSPERM;
    mxSetField(bg,0,"sigma",pSigma);

    if (mxSetClassName(bg,"Background")) {
	fprintf(stderr,"Unable to convert background to a Matlab class\n");
    }
    return bg;
}

// Send /pf/background OSC message
void Background::sendMessages(lo_address &addr, int scanpt) const {
    assert(scanpt>=0 && scanpt<=range[0].size());
    // Send one sample of background as scanpoint#, theta (in degress), range (in meters)
    float angleDeg=scanRes*(scanpt-(range[0].size()-1)/2.0);

    lo_send(addr,"/pf/background","iiff",scanpt,range[0].size(),angleDeg,range[0][scanpt]/UNITSPERM);
}

