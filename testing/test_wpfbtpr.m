function test_failed = test_wpfbtpr(verbose)
%TEST_WFBTPR
%
% Checks perfect reconstruction of the general wavelet transform of different
% filters
%


test_failed = 0;
if(nargin>0)
   verbose = 1;
else
   verbose = 0;
end

type = {'dec'};
ext = {'per','zero','odd','even'};
format = {'pack','cell'};


J = 3;
%! Mild tree
wt1 = wfbtinit({'db10',6,'full'});
wt1 = wfbtremove(2,1,wt1,'force');
wt1 = wfbtremove(2,3,wt1,'force');

%! Hardcore tree
wt2 = wfbtinit({'db3',1});
wt2 = wfbtput(1,1,'mband1',wt2);
wt2 = wfbtput(2,2,'mband1',wt2);
wt2 = wfbtput(3,3,'mband1',wt2);
wt2 = wfbtput(3,1,'db10',wt2);
wt2 = wfbtput(4,1,'dgrid2',wt2);
wt2 = wfbtput(5,1,'db3',wt2);

% wt2 = wfbtinit();
% wt2 = wfbtput(0,0,{'db',4},wt2);
% wt2 = wfbtput(1,0,{'algmband',1},wt2);
% wt2 = wfbtput(1,1,{'hden',3},wt2);
% wt2 = wfbtput(2,0,{'dgrid',2},wt2);
% wt2 = wfbtput(2,1,{'dgrid',2},wt2);



test_filters = {
               {'algmband2',J} % 4 filters, uniform, crit. sub.
               {'db4',J}
               {'algmband1',J} % 3 filters, uniform, crit. sub.
               %{{'hden',3},J} % 3 filters, non-uniform, no crit. sub. no correct
               {'dgrid1',J} % 4 filters. sub. fac. 2
               wt1
               wt2
               };





%testLen = 4*2^7-1;%(2^J-1);
testLen = 53;
f = randn(testLen,1);

for extIdx=1:length(ext)  
   extCur = ext{extIdx};

   for typeIdx=1:length(type)
     for tt=1:length(test_filters)
        actFilt = test_filters{tt};
         if verbose, if(~isstruct(actFilt))fprintf('J=%d, filt=%s, ext=%s, inLen=%d \n',actFilt{2},actFilt{1},extCur,size(f,1)); else disp('Custom'); end; end;

        c = wpfbt(f,actFilt,extCur);
        fhat = iwpfbt(c,actFilt,size(f,1),extCur);
        
        %MSE
            err = sum(f(:).^2-fhat(:).^2)/length(f(:));
            if err>1e-6
               test_failed = 1; 
               if verbose
                if(~isstruct(actFilt)) fprintf('err=%d, filt=%s, ext=%s, inLen=%d \n',err,actFilt{1},extCur,testLen); else disp('Custom'); end;
                 figure(1);clf;stem([f,fhat]);
                 figure(2);clf;stem([f-fhat]);
               end
               break; 
            end
            if test_failed, break; end;
        
     end
     if test_failed, break; end;
   end
   if test_failed, break; end;
end



















