function c=comp_uwfbt(f,wtNodes,nodesUps,rangeLoc,rangeOut,scaling)
%COMP_UWFBT Compute Undecimated Wavelet Filterbank Tree
%   Usage:  c=comp_uwfbt(f,wtNodes,nodesUps,rangeLoc,rangeOut);
%
%   Input parameters:
%         f        : Input L*W array.
%         wtNodes  : Filterbank tree nodes (elementary filterbanks) in
%                    BF order. Length *nodeNo* cell array of structures.
%         nodesUps : Filters upsampling factor of each node. Array of
%                    length *nodeNo*.
%         rangeLoc : Idxs of each node terminal outputs. Length *nodeNo* 
%                    cell array of vectors.
%         rangeOut : Output subband idxs of each node terminal outputs.
%
%   Output parameters:
%         c     : Coefficient array of dim. L*M*W.
%


% Pre-allocated output
[L, W] = size(f);
M = sum(cellfun(@(rEl) numel(rEl),rangeOut));
c = zeros(L,M,W,assert_classname(f,wtNodes{1}.h{1}.h));

% Convenience input reshape
ca = reshape(f,size(f,1),1,size(f,2));
% For each node in tree in the BF order...
for jj=1:numel(wtNodes)
   % Node filters subs. factors
   a = wtNodes{jj}.a;
   
   % Optionally scale the filters
   h = comp_filterbankscale(wtNodes{jj}.h(:),a(:),scaling);
   
   % Node filters to a matrix
   hMat = cell2mat(cellfun(@(hEl) conj(flipud(hEl.h(:))),h','UniformOutput',0));

   % Node filters initial skips
   hOffset = cellfun(@(hEl) 1-numel(hEl.h)-hEl.offset,wtNodes{jj}.h);
   % Zero index position of the upsampled filters.
   offset = nodesUps(jj).*(hOffset);
   
   % Run filterbank.
   catmp=comp_atrousfilterbank_td(squeeze(ca(:,1,:)),hMat,nodesUps(jj),offset);
   % Bookkeeping
   % Copy what goes directly to the output...
   c(:,rangeOut{jj},:)=catmp(:,rangeLoc{jj},:);
   % ...and save the rest.
   diffRange = 1:size(hMat,2);
   diffRange(rangeLoc{jj}) = [];
   ca = [ca(:,2:end,:), catmp(:,diffRange,:)];
end 


