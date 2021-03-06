% Update current hyothesis of locations 
% Input:
%   h.prob(k,isize,isize) - for each individual, k, a map of the prob they are centered at the given coord (1=possibly there, 0=not there)
%   h.pos(k,2) - MLE position estimate for person k
%   possible(isize,isize) - pixel positions that could be a center point of a person
%   maxmovement - max movement of anyone in m
% Output:
%   hf.prob - updated hprob
function hf=updatehypo(p,imap,h,possible,maxmovement)
layout=p.layout;
pghost=0.5;   % Prob any given pixel is a ghost pixel
% Account for possibility of an error - target is where possible says that someone cannot be
possible=possible+0.01/length(possible(:));
possible=possible/sum(possible(:));   %renormonalize
% Build a filter to allow movement
if isfield(p,'simul')
  fsize=ceil(m2pix(imap,p.simul.speed*p.simul.dt));
else
  fsize=ceil(m2pix(imap,maxmovement));
end
filt=fspecial('disk',fsize);
% increase probability of stationary
filt=filt/2;
filt(fsize+1,fsize+1)=filt(fsize+1,fsize+1)+0.5;

hf=struct('prob',zeros(size(h.prob)));

for i=1:size(h.prob,1)
  hf.prob(i,:,:)=imfilter(squeeze(h.prob(i,:,:)),filt).* possible;
  % Renormalize
  hf.prob(i,:,:)=hf.prob(i,:,:)/sum(sum(hf.prob(i,:,:)));
end
% Scale to account for multiple targets being at the same point
% TODO

% Compute expected value of each
indmat=zeros(size(h.prob,2),size(h.prob,3),2);
indmat(:,:,1)=repmat((1:size(h.prob,2))',1,size(h.prob,3));
indmat(:,:,2)=repmat(1:size(h.prob,3),size(h.prob,2),1);
hf.pos=[];
for i=1:size(h.prob,1)
  p=squeeze(hf.prob(i,:,:));
  % Renormalize
  p=p/sum(sum(p));
  % Wtd average
  hf.pos(i,:)=[sum(sum(indmat(:,:,1).*p)),sum(sum(indmat(:,:,2).*p))];
end

hf.pos=pix2m(imap,hf.pos);
