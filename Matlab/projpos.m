p(1).actual=[-4 4 1.2];
p(2).actual=[1 0.3 1.2];

bounds=[0 0 
        1920 0
        1920 1080
        0 1080
        0 0];
bounds(:,3)=1;

for proj=1:length(p)
  % Compute transformation from components
  cview=p(proj).cameraview;
  cview(4,:)=[0 0 0 1];
  projmat=p(proj).proj;
  projmat(4,4)=0;
  projmat(4,3)=1;
  trans=projmat*cview
  
  pname=sprintf('Proj %d',proj);

  spts=maps{proj,3}.p1;
  wpts=maps{proj,3}.p2;
  wpts(:,3)=0;	% At z=0;
  wpts(:,4)=1;	% Homography
  cpts=(cview*wpts')';

  setfig(pname);clf;
  for i=1:size(spts,1)
    % World
    subplot(131);
    plot3(wpts(i,1),wpts(i,2),0,'o');
    hold on;
    text(wpts(i,1)+0.1,wpts(i,2)+0.1,0,sprintf('%d',i));
    axis equal

    subplot(132);
    plot3(cpts(i,1),cpts(i,2),cpts(i,3),'o');
    hold on;
    text(cpts(i,1)+0.1,cpts(i,2)+0.1,cpts(i,3),sprintf('%d',i));
    axis equal

    subplot(133);
    plot(spts(i,1),spts(i,2),'o');
    hold on;
    text(spts(i,1)+20,spts(i,2)+10,sprintf('%d',i));
    axis equal
    axis ij
  end
  subplot(131);
  bmapped=(p(proj).screen2world*bounds')';
  bmapped(:,4)=bmapped(:,3);
  bmapped(:,3)=0;
  plot3(bmapped(:,1)./bmapped(:,4),bmapped(:,2)./bmapped(:,4),bmapped(:,3)./bmapped(:,4));
  plot3(p(proj).actual(1),p(proj).actual(2),p(proj).actual(3),'xg');
  plot3(p(proj).pose(1),p(proj).pose(2),p(proj).pose(3),'+r');
  title('World Coords');
  xlabel('x'); ylabel('y'); zlabel('z');
  
  subplot(132);
  
  cmapped=(cview*bmapped')';
  plot3(cmapped(:,1)./cmapped(:,4),cmapped(:,2)./cmapped(:,4),cmapped(:,3)./cmapped(:,4))
  znear=1;
  plot3(cmapped(:,1)./cmapped(:,3)*znear,cmapped(:,2)./cmapped(:,3)*znear,cmapped(:,3)./cmapped(:,3)*znear);  % frustum at 1m distance from camera
  for k=1:size(cmapped,1)
    plot3([0,cmapped(k,1)./cmapped(k,4)],[0,cmapped(k,2)./cmapped(k,4)],[0,cmapped(k,3)./cmapped(k,4)]);
  end
  xlabel('x'); ylabel('y'); zlabel('z');
  title('Camera Coords');
  
  subplot(133);
  plot(bounds(:,1),bounds(:,2));
  title('Projector Coords');
  xlabel('H'); ylabel('V');

  for k=1:size(wpts,1)
    s2=(p(proj).world2screen*wpts(k,[1,2,4])')';
    s2=s2(1:2)/s2(3);
    s3=(projmat*cview*wpts(k,:)')';
    s3=s3(1:3)/s3(4);
    fprintf('w: [%5.2f,%5.2f, %5.2f]  s: [%7.2f,%7.2f] w2s*w: [%7.2f,%7.2f] (e=%4.1f) p*c*w: [%7.2f %7.2f %7.2f] (e=%4.1f)\n',wpts(k,1:3), spts(k,1:2), s2, norm(s2-spts(k,1:2)), s3, norm(s3(1:2)-spts(k,1:2)));
    plot(s2(1),s2(2),'+');
  end
end
