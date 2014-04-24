function [g,a] = wpfbt2filterbank( wtdef, varargin)
%WPFBT2FILTERBANK  WPFBT equivalent non-iterated filterbank
%   Usage: [g,a] = wpfbt2filterbank(wtdef)
%
%   Input parameters:
%         wtdef : Wavelet filter tree definition
%
%   Output parameters:
%         g   : Cell array containing filters
%         a   : Vector of sub-/upsampling factors
%
%   `wpfbt2filterbank(wtdef)` calculates the impulse responses *g* and the
%   subsampling factors *a* of non-iterated filterbank, which is equivalent
%   to the wavelet packet filterbank tree described by *wtdef*. The returned
%   parameters can be used directly in |filterbank|, |ufilterbank| or
%   |filterbank|.
%
%   The function internally calls |wfbtinit| and passes *wtdef* and all
%   additional parameters to it.
%
%   Examples:
%   ---------
%
%   The following two examples create a multirate identity filterbank
%   using a tree of depth 3. In the first example, the filterbank is
%   identical to the DWT tree:::
%
%     [g,a] = wpfbt2filterbank({'db10',3,'dwt'});
%     filterbankresponse(g,a,1024,'plot','individual');
%
%
%   In the second example, the filterbank is identical to the full
%   wavelet tree:::
%
%     [g,a] = wpfbt2filterbank({'db10',3,'full'});
%     filterbankresponse(g,a,1024,'plot','individual');
%
%   See also: wfbtinit


complainif_notenoughargs(nargin,1,'WPFBT2FILTERBANK');

definput.import = {'wfbtcommon'};
definput.flags.interscaling={'intsqrt','intnoscale','intscale'};
[flags]=ltfatarghelper({},definput,varargin);

% build the tree
wt = wfbtinit({'strict',wtdef},flags.forder);

wt = comp_wpfbtscale(wt,flags.interscaling);

nIdx = nodesLevelsBForder(wt);
% Now we need to walk the tree by levels
g = {};
a = [];
for ii=1:numel(nIdx)
    rangeLoc = cellfun(@(eEl) 1:numel(eEl.h),wt.nodes(nIdx{ii}),...
                       'UniformOutput',0);
    rangeOut = cellfun(@(eEl) numel(eEl.h),wt.nodes(nIdx{ii}));
    rangeOut = mat2cell(1:sum(rangeOut),1,rangeOut);
    [gtmp,atmp] = nodesMultid(nIdx{ii},rangeLoc,rangeOut,wt);
    g(end+1:end+numel(gtmp)) = gtmp;
    a(end+1:end+numel(atmp)) = atmp;
end
g = g(:);
a = a(:);
















