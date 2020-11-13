# lqns 6.0
# lqns --pragma=variance=mol,threads=mak --parseable 12-interlock.in
# $Id: 12-interlock.p 13117 2017-07-20 15:46:53Z greg $
V y
C 1.18074e-06
I 6
PP 3
NP 1

#!Comment: Send Interlock with 2 clients
#!User:  0:00:00.001
#!Sys:   0:00:00.001
#!Real:  0:00:00.002
#!Solver: 3 18 173 2145 13571 3.31173e+07 0

B 3
t0             :e0              0.5         
t1             :e1              0.5         
t2             :e2              1           
-1

W 3
t0             :e0              e1              1.0646      -1 
                e0              e2              0.440275    -1 
                -1 
t1             :e1              e2              0.229728    -1 
                -1 
-1

X 3
t0             :e0              5.73461     -1 
                -1 
t1             :e1              2.22973     -1 
                -1 
t2             :e2              1           -1 
                -1 
-1

VAR 3
t0             :e0              70.7282     -1 
                -1 
t1             :e1              8.75227     -1 
                -1 
t2             :e2              1           -1 
                -1 
-1

FQ 3
t0             :e0              0.34876     2           -1 2
                -1 
t1             :e1              0.34876     0.77764     -1 0.77764
                -1 
t2             :e2              0.69752     0.69752     -1 0.69752
                -1 
-1

P p0 1
t0              1  0 2  e0              0.34876     0           -1 
                        -1 
-1 

P p1 1
t1              1  0 1  e1              0.34876     0           -1 
                        -1 
-1 

P p2 1
t2              1  0 1  e2              0.69752     0           -1 
                        -1 
-1 

-1

