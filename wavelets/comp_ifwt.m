function f = comp_ifwt(c,g,J,a,Lc,Ls,ext)
%COMP_IFWT_ALL Compute Inverse DWT
%   Usage:  f = comp_ifwt_all(c,g,J,Ls,type,ext);
%
%   Input parameters:
%         c     : Coefficients stored in a cell-array.
%         g     : Synthesis wavelet filters.
%         Ls    : Length of the reconstructed signal.
%         ext   : 'per','zero','odd','even', Type of the forward transform boundary handling.
%
%   Output parameters:
%         f     : Reconstructed data.
%

% see comp_fwt for explanantion
assert(a(1)==a(2),'First two elements of a are not equal. Such wavelet filterbank is not suported.');

filtNo = numel(g);
LcStart = 1 + cumsum([0;Lc(1:end-1)]); 
LcEnd = cumsum(Lc); 
Lc(end+1) = Ls;

% Determine number of channels.
chans = size(c,2); 
f = zeros(Ls,chans);


% For holding the impulse responses.
tmpg = cell(filtNo,1);
% For holding the 
skip = zeros(filtNo,1);
for ff=1:filtNo
  tmpg{ff} =  g{ff}.h; 
  if(strcmp(ext,'per'))
     % Initial shift of the filter to compensate for it's delay.
     % "Zero" delay reconstruction is produced.
     skip(ff) = g{ff}.d-1;
  else
     % -1 + 1 = 0 is used for better readability and to be consistent
     % with the shift in comp_fwt.
     % Here we are cheating, because we are making the filters
     % anti-causal to compensate for the delay introduced by causal
     % analysis filters. 
     % Instead, we could have used causal filters here and do the
     % delay compensation at the end (cropping f).
     skip(ff) = length(g{ff}.h)-1 - (a(ff)-1);
  end
end
%tempca = c{1}(:,ch);

for ch=1:chans
  tempca = c(LcStart(1):LcEnd(1),ch);
  LcRunPtr = filtNo+1;
  cRunPtr = 2;
  for jj=1:J
     tempca = comp_upconv({tempca}, Lc(LcRunPtr),{tmpg{1}},a(1),skip(1),ext,0);
     for ff=2:filtNo
        % tempca = tempca + comp_upconv({c{cRunPtr}(:,ch)}, Lc(LcRunPtr),{tmpg},a(ff),skip,doNoExt,0);
        tempca = tempca + comp_upconv({c(LcStart(cRunPtr):LcEnd(cRunPtr),ch)}, Lc(LcRunPtr),{tmpg{ff}},a(ff),skip(ff),ext,0);
        cRunPtr = cRunPtr + 1;
     end
     LcRunPtr = LcRunPtr + filtNo -1;
  end
  f(:,ch) = tempca;
end


    
    