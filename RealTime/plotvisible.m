function plotvisible(sainfo,vis)
setfig('getvisible');
clf;
ncol=1;
if isfield(vis,'xcorr')
  ncol=2;
end
if isfield(vis,'im')
  ncol=ncol+1;
end
for i=1:size(vis.v,1)
  c=sainfo.camera(i).pixcalib;
  roi=sainfo.camera(i).roi;
  col=1;
  if isfield(vis,'im')
    subplot(length(vis.im),ncol,(i-1)*ncol+col);
    col=col+1;
    imshow(vis.im{i});
    axis normal
    hold on;
    title(sprintf('Camera %d',sainfo.camera(i).id));
    pause(0.01);
    for j=1:length(c)
      if ~isnan(vis.v(i,j))
        if vis.v(i,j)
          plot(c(j).pos(1)-roi(1)+1,c(j).pos(2)-roi(3)+1,'og');
        else
          plot(c(j).pos(1)-roi(1)+1,c(j).pos(2)-roi(3)+1,'or');
        end
      end
    end
  end
  if isfield(vis,'lev')
    subplot(size(vis.v,1),ncol,(i-1)*ncol+col);col=col+1;
    lev=double(vis.lev(i,:));
    lev(isnan(vis.v(i,:)))=nan;
    plot(lev,'g');
    hold on;
    % Plot threshold
    plot(sainfo.crosstalk.thresh(i,:),'c');
    plot(sainfo.crosstalk.neighlev(i,:),'m');
    % Plot off signals in red
    lev(vis.v(i,:)==1)=nan;
    plot(lev,'rx');
    ax=axis;
    ax(3)=0;ax(4)=260;
    axis(ax);
    legend('Signal','Threshold','Neighlev');
  end
  if isfield(vis,'xcorr')
    subplot(size(vis.v,1),ncol,(i-1)*ncol+col);col=col+1;
    xcorr=vis.xcorr(i,:);
    xcorr(isnan(vis.v(i,:)))=nan;
    plot(xcorr,'g');
    hold on;
    plot([1,length(xcorr)],sainfo.analysisparams.mincorr+[0,0],'c');
    % Plot off signals in red
    xcorr(vis.v(i,:)==1)=nan;
    plot(xcorr,'rx');
    ax=axis;
    ax(3)=min(ax(3),0);ax(4)=1.0;
    axis(ax);
    legend('Correlation','Threshold');
  end
  xlabel('LED');
  ylabel('Signal');
end
