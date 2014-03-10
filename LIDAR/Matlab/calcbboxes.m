% Calculate bounding boxes and positions based on current classifications
% Also pack class numbers to eliminate missing classes
function vis=calcbboxes(vis)
MAXSPECIAL=2;

% Calculate centroids and bounding boxes, pack class numbers
pos=[];
bbox=[];
bounded=[];
leftlegs=[];
rightlegs=[];
for i=MAXSPECIAL+1:max(vis.class)
  sel=vis.class==i;
  if sum(sel)==0
    continue;
  end

  leftlegs=[leftlegs,legmodel(vis.xy(sel&vis.leg==1,:))];
  rightlegs=[rightlegs,legmodel(vis.xy(sel&vis.leg==2,:))];
  pos(end+1,:)=mean(vis.xy(sel,:),1);
  bbox(end+1,:)=[min(vis.xy(sel,1)),min(vis.xy(sel,2)),max(vis.xy(sel,1))-min(vis.xy(sel,1)),max(vis.xy(sel,2))-min(vis.xy(sel,2))];
  fsel=find(sel);
  bounded(end+1)=~vis.shadowed(fsel(1)) && ~vis.shadowed(fsel(end));
  cnum=size(pos,1)+MAXSPECIAL;
  vis.class(vis.class==i)=cnum;
end
vis.targets=struct('legs',{{leftlegs,rightlegs}},'pos',pos,'bbox',bbox,'bounded',bounded);
