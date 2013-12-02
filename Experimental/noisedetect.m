% Do noisedetection of a frame
function noisedetect(p,vis,cam,varargin)
defaults=struct('wsize',[]);
args=processargs(defaults,varargin);

if isempty(args.wsize)
  args.wsize= p.camera(1).viscache.wsize;
end

setfig('noisedet');clf;
nplot=7; pnum=1;

nm=noisemodel(p,vis,cam);
img=im2double(vis.im{cam});
diff=img-nm.avg;
nstd=diff.*diff./nm.var;
fprintf('Mean sample level: %.1f/255, mean std: %.1f/255\n',255*mean(nm.avg(:)),255*mean(sqrt(nm.var(:))));
fprintf('Fraction >1 std from avg: %.3f\n',mean(nstd(:)>1));
fprintf('Fraction >2 std from avg: %.3f\n',mean(nstd(:)>2));

subplot(nplot,1,pnum); pnum=pnum+1;
ufscale=10;
imshow(nstd/ufscale);
title(sprintf('Unfiltered deviation / %.1f',ufscale));

thresh=p.analysisparams.fgthresh(2);

%subplot(nplot,1,pnum); pnum=pnum+1;
%hist(min(thresh,nstd(:)),1000);
%title('Distribution of nstd');

fprintf('Using window size of [%d,%d]\n',args.wsize);
f=sqrt(mean(imfilter(nstd,fspecial('average',args.wsize)),3));


subplot(nplot,1,pnum); pnum=pnum+1;
imshow(f/thresh);
title(sprintf('Filtered deviation / %.2f',thresh));

for i=1:2
  thresh=p.analysisparams.fgthresh(i);
  det=f>thresh;
  rgb=img;
  rgb(:,:,2)=max(rgb(:,:,2),det);
  subplot(nplot,1,pnum); pnum=pnum+1;
  imshow(rgb);
  title(sprintf('Deviation>%.2f',thresh));
end

%subplot(nplot,1,pnum); pnum=pnum+1;
%hist(f(:),1000);
%title('Distribution of f');

% Overall plot with LED decisions overlaid
subplot(nplot,1,pnum); pnum=pnum+1;
%imshow(nstd/max(nstd(:)));
imshow(f/thresh);
hold on;
pc=p.camera(cam).pixcalib;
vc=p.camera(cam).viscache;
roi=p.camera(cam).roi([1,3]);
nled=size(vis.corr,2);
for l=1:nled
  pl=round((vc.tlpos(l,:)+vc.brpos(l,:))/2);
  nstd=(1-vis.corr(cam,l))*p.analysisparams.fgscale;
  if nstd<p.analysisparams.fgthresh(1)
    plot(pl(1),pl(2),'g');
  elseif isnan(nstd) || nstd<p.analysisparams.fgthresh(2)
    plot(pl(1),pl(2),'y');
  else
    plot(pl(1),pl(2),'r');
  end
  if ~any(isnan(pl))
    pf(l)=f(pl(2),pl(1));
  else
    pf(l)=nan;
  end
end
subplot(nplot,1,pnum); pnum=pnum+1;
fecorr=vis.corr(cam,:);
recorr=1-pf/p.analysisparams.fgscale;
plot(fecorr,'m');
hold on;
plot(recorr);
plot([1,nled],[vis.mincorr,vis.mincorr],':');
legend('From FrontEnd','Recomputed');
title(sprintf('FrontEnd corr (mean=%.3f) and reccomputed (%.3f)', nanmean(fecorr), nanmean(recorr)));

subplot(nplot,1,pnum); pnum=pnum+1;
festd=(1-vis.corr(cam,:))*p.analysisparams.fgscale;
plot(festd,'m');
hold on;
plot(pf);
plot([1,nled],p.analysisparams.fgthresh(1)*[1,1],':');
plot([1,nled],p.analysisparams.fgthresh(2)*[1,1],':');
legend('From FrontEnd','Recomputed');
title(sprintf('FrontEnd nstd (mean=%.3f) and reccomputed (%.3f)', nanmean(festd), nanmean(pf)));
