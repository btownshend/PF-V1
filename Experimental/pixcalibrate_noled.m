% Calibrate positions, directions of cameras, setup pixcalib structure
% Usage: p=pixcalibrate_noled(p)
%
% Method:
% - Setup distortion parameters of each camera (from prior calibration)
%	- load from c%d-Results_left.mat using p.camera(:).physid for indexing
% - Acquire images of checkerboard from each camera and compute extrinsics
%	- needs manual intervention to click corners
%	- keep record of calibration in p.calibrate
% - Transform coordinates to global space where
%	- all cameras are at same height
%	- positions are set so they are all as close as possible to positions preset in layout
%	  (warn if any camera is more than a certain distance from planned position)
% - Update camera cpos, cdir, cposz, cdirz, croll in this space to calibrated values
% - Choose "virtual" LEDs (lpos) around perimeter of room except along x axis
% - Compute pixcalib using distortion model of each camera

function p=pixcalibrate_noled(p,varargin)
defaults=struct('newimages',false,'doplot',false,'maxcameramovement',0.5);
args=processargs(defaults,varargin);

target=struct('dX',0.03,'dY',0.03,'nX',16,'nY',16,'bottomrowheight',.0625);
% floor are points in grid coordinate space that is on the floor (bottom edge)
target.floor=zeros(2,3);
target.floor(:,1)=target.dY*target.nY+target.bottomrowheight;
target.floor(2,2)=target.dX*target.nX;
for c=1:length(p.camera)
  cam=p.camera(c);
  if args.newimages || ~isfield(cam,'extcal') || isempty(cam.extcal)
    cam.distortion=load(sprintf('c%d-Results_left',cam.physid));
    while true
      I=arecont(cam.id);
      setfig('interactive');clf;
      imshow(I.im);
      title('Click on top left, bottom right corners, return to retry');
      [cx,cy]=ginput(2);
      if length(cx)==2
        break;
      end
      fprintf('Only got %d points -- grabbing a new image\n', length(cx));
    end
    I.bounds=round([cx,cy]);
    plot(I.bounds(:,1),I.bounds(:,2),'x');
    extrinsic=extrinsic_auto(I,target,cam.distortion,0,0);
    position=cam2grid(extrinsic,[0;0;0]);
    fprintf('Camera %d position is [%.2f,%.2f,%.2f]\n', cam.id, position);
    p.camera(c).distortion=cam.distortion;
    p.camera(c).extcal=struct('image',I,'extrinsic',extrinsic,'gridposition',position);
  else
    fprintf('Using existing extcal for camera %d\n',cam.id);
  end
end

% Construct the mapping from grid coordinates to world coordinates
p.calibration.target=target;
p=transformcameras(p);

if args.doplot
  setfig('Grid space');clf;
  hold on;
  plotworld(p,struct('R',eye(3),'T',[0;0;0]));
end

if args.doplot
  setfig('World space');clf;
  hold on;
  plotworld(p);
  axis([-5,5,0,10,0,2]);
end

% Now setup the pixcalib structure
ledheight=mean(p.layout.cposz);
maxoffaxis=65*pi/180;   % Maximum off-axis angle to use
for c=1:length(p.camera)
  cam=p.camera(c);
  % Camera may be operating at different resolution than was used for distortion calibration
  scale=cam.hpixels/cam.distortion.Wcal;
  if round(cam.distortion.Hcal*scale)~=cam.vpixels
    error('Nonuniform scaling of camera pixels: operating at [%d,%d], with calibration at [%d,%d]\n',cam.hpixels,cam.vpixels,cam.distortion.Wcal,cam.distortion.Hcal);
  end
  fprintf('Calibrating camera %d with pixel scaling of %.2f\n',cam.id,scale);
  Rwc=inv(cam.extcal.Rcw);
  Twc=-Rwc*cam.extcal.Tcw;
  for l=1:size(p.layout.lpos,1)
    % Calculate LED position in camera frame of reference
    lpos=[p.layout.lpos(l,:),ledheight];
    lposCF=Rwc*lpos'+Twc;
    % Check angle off lens axis
    offaxis=atan(norm(lposCF(1:2))/lposCF(3));
    if (abs(offaxis)>maxoffaxis)
      %      fprintf('LED %d is too far off axis - %.1f degrees\n', l, offaxis*180/pi);
      pc(l)=struct('pos',[nan,nan],'valid',false,'inuse',false,'diameter',nan);
    else
      % Map to camera pixel
      pixelpos=round(scale*distort(cam.distortion,(lposCF(1:2)/lposCF(3))')');
      if pixelpos(1)<0||pixelpos(1)>=cam.hpixels||pixelpos(2)<0||pixelpos(2)>=cam.vpixels
        fprintf('LED %d has pixel off sensor at [%.0f,%.0f]\n', l, pixelpos);
        pc(l)=struct('pos',[nan,nan],'valid',false,'inuse',false,'diameter',nan);
      else
        pc(l)=struct('pos',pixelpos,'valid',true,'inuse',true,'diameter',1);
      end
    end

    fprintf('LED %d global position: [%.3f,%.3f,%.3f], camera frame of reference: [%.3f,%.3f,%.3f], dir=%.1f deg, pixel=[%.0f,%.0f]\n',l,lpos,lposCF,offaxis*180/pi,pc(l).pos);
  end
  pcy=arrayfun(@(z) z.pos(2),pc);
  pcx=arrayfun(@(z) z.pos(1),pc);


  % Setup ROI for each camera
  cp=reshape([pc([pc.valid]).pos],2,[]);
  border=5;   % min(6,max([pc([pc.valid]).diameter]));
  p.camera(c).roi=[
      max(1,floor(min(cp(1,:))-border)),min(cam.hpixels,ceil(max(cp(1,:))+border)),...
      max(1,floor(min(cp(2,:))-border)),min(cam.vpixels,ceil(max(cp(2,:))+border))];
  % Make divisible by 32 for camera
  p.camera(c).roi([1,3])=floor((p.camera(c).roi([1,3])-1)/32)*32+1;
  p.camera(c).roi([2,4])=ceil((p.camera(c).roi([2,4])-1)/32)*32+1;
  roi=p.camera(c).roi;
  fprintf('Camera %d: Using %d LEDS, ROI size = %d x %d\n', c, sum([pc.valid]), (roi(2)-roi(1)),(roi(4)-roi(3)));
  if (roi(4)-roi(3))>200
    fprintf('**WARNING** Camera %d has excessive ROI size = %d x %d\n', c, (roi(2)-roi(1)),(roi(4)-roi(3)));
  end
  fprintf('Camera %d ROI=[%.0f-%.0f,%.0f-%.0f]\n',c,roi);

  p.camera(c).pixcalib=pc;
  p.camera(c).pixcalibtime=now;
  if args.doplot
    setfig(sprintf('Cam%d pixcalib',c));clf;
    imshow(imresize(cam.extcal.image.im,scale));
    hold on;
    plot(pcx,pcy,'.');
  end
end

% Setup pixellist,indices from pixelList
for c=1:length(p.camera)
  roi=p.camera(c).roi;
  for i=1:length(p.camera(c).pixcalib)
    if p.camera(c).pixcalib(i).valid
      pixelList=round(p.camera(c).pixcalib(i).pos);  % Just set pixelList to same as pos
      p.camera(c).pixcalib(i).pixelList=pixelList;
      pixelList(:,1)=pixelList(:,1)-roi(1)+1;
      pixelList(:,2)=pixelList(:,2)-roi(3)+1;
      % Setup indices from pixellists
      p.camera(c).pixcalib(i).indices=...
          sub2ind([roi(4)-roi(3),roi(2)-roi(1)],pixelList(:,2),pixelList(:,1));
      p.camera(c).pixcalib(i).rgbindices=[...
          sub2ind([roi(4)-roi(3),roi(2)-roi(1),3],pixelList(:,2),pixelList(:,1),1*ones(size(pixelList,1),1));...
          sub2ind([roi(4)-roi(3),roi(2)-roi(1),3],pixelList(:,2),pixelList(:,1),2*ones(size(pixelList,1),1));...
          sub2ind([roi(4)-roi(3),roi(2)-roi(1),3],pixelList(:,2),pixelList(:,1),3*ones(size(pixelList,1),1))];
    end
  end
end
