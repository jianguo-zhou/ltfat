function test_failed=test_wmdct2

test_failed=0;
  
disp(' ===============  TEST_WMDCT2 ================');

% Run some fixed test to test the interface.
% This is not a thourough tester.

% --- test 1 ----------------

L=64;
M=8;
Lf=63;
W=3;

f=tester_rand(Lf,Lf,W);

g=pgauss(L,1);
gd=wildual(g,M);

[c,Ls]=wmdct2(f,g,M);
r=iwmdct2(c,gd,Ls);

res=r-f;

nres=norm(res(:));

failed='';
if nres>10e-10
  failed='FAILED';
  test_failed=test_failed+1;
end;

s=sprintf('WMDCT2 Lf:%3i L:%3i %0.5g %s',Lf,L,nres,failed);
disp(s)


% --- test 2 -------------------
L=256;
M1=16;
M2=32;
W=1;

f=tester_rand(L,L,1);

g=pgauss(L,1);

gd1=wildual(g,M1);
gd2=wildual(g,M2);

c=wmdct2(f,g,[M1,M2]);
c2=ref_wmdct2(f,g,g,M1,M2);

rc=c-c2;
nres=norm(rc(:));

failed='';
if nres>10e-10
  failed='FAILED';
  test_failed=test_failed+1;
end;

s=sprintf('WMDCT2 REF M1:%3i M2:%3i %0.5g %s',M1,M2,nres,failed);
disp(s)



r=iwmdct2(c,gd1,gd2);

res=r-f;

nres=norm(res(:));

failed='';
if nres>10e-10
  failed='FAILED';
  test_failed=test_failed+1;
end;

s=sprintf('WMDCT2 INV M1:%3i M2:%3i %0.5g %s',M1,M2,nres,failed);
disp(s)




