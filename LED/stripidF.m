% Check ID's of strips
% Use F commands for update
s1=arduino_ip(1);
nstrip=8;
firststrip=1;
ledperstrip=160;
ledset=160;
%nstrip=1;ledperstrip=300;
colnames={'red','green', 'blue','magenta','cyan','yellow','pinkish', 'white'};
col={[1 0 0],[0 1 0],[0 0 1],[1 0 1],[0 1 1], [1 1 0], [1 0.5 0.5], [1 1 1]};
clkpins=[48,46,44,42,36,34,32,40];
datapins=clkpins+1;
nphase=3;
phase=0;
cnt=0;
tic;
burst=3;
timing=[];
counter=syncsend(s1);  % One extra sync to pipeline things a bit more
while true
  for k=firststrip:nstrip
    counter=syncsend(s1);
    cmd=zeros(4+ledset*3,1);
    offset=(k-1)*ledperstrip;
    cmd(1)='F';
    cmd(2)=bitand(offset,255);
    cmd(3)=bitshift(offset,-8);
    cmd(4)=ledset;
    cols=zeros(ledset,3);
    for i=1:3
      cols(phase+1:nphase:end,i)=col{k}(i);
    end
    first=mod(k-3,length(col))+1;
    last=mod(k+1,length(col))+1;
    cols(1,:)=col{first};
    cols(end,:)=col{last};
    cols=cols*127;

    ind=1:3:3*ledset;
    % Reorder as GRB
    cmd(ind+4)=cols(:,2);
    cmd(ind+5)=cols(:,1);
    cmd(ind+6)=cols(:,3);
    % Set high bit of all data
    cmd(5:end)=bitset(uint8(cmd(5:end)),8);
    if mod(cnt,100)==02
      fprintf('Strip %d is %s (first=%s,last=%s).  Clk=pin %d, Data=pin %d\n', k, colnames{k},colnames{first},colnames{last},clkpins(k),datapins(k));
    end
    %fprintf('Writing cmd with %d bytes: %s...\n', length(cmd),sprintf('%02x ',uint8(cmd(1:10))));
    awrite(s1,cmd);cmd=[];
    show(s1);
    ok=syncwait(s1,mod(counter-1,256),10);  % Wait for prior sync
  end
  timing(end+1)=nstrip*ledperstrip/toc;
  fprintf('Update rate=%.2f/second\n',1/toc);
  tic;
  phase=mod(phase+1,nphase);
  cnt=cnt+1;
end
