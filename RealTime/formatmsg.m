% Format an OSC message into a string for printing
function s=formatmsg(path,data,sep)
if nargin<3
  sep=',';
end
s=path;
for i=1:length(data)
  if ischar(data{i})
    s=[s,sep,data{i}];
  else
    s=[s,sep,num2str(data{i})];
  end
end