# lqns 6.0
# lqns --pragma=variance=mol,threads=mak --parseable 11-interlock.in
# $Id: 11-interlock.p 13117 2017-07-20 15:46:53Z greg $
V y
C 0
I 2
PP 3
NP 2

#!Comment: Send Interlock with Phase 2
#!User:  0:00:00.000
#!Sys:   0:00:00.000
#!Real:  0:00:00.000
#!Solver: 3 6 39 271 1497 818637 0

B 3
t0             :e0              0.4         
t1             :e1              0.5         
t2             :e2              1           
-1

W 3
t0             :e0              e1              0.725806    0           -1 
                e0              e2              0           0           -1 
                -1 
t1             :e1              e2              0           0           -1 
                -1 
-1

X 3
t0             :e0              3.22581     0           -1 
                -1 
t1             :e1              0.5         1.5         -1 
                -1 
t2             :e2              1           0           -1 
                -1 
-1

VAR 3
t0             :e0              16.6124     0           -1 
                -1 
t1             :e1              0.25        4.75        -1 
                -1 
t2             :e2              1           0           -1 
                -1 
-1

FQ 3
t0             :e0              0.31        1           0           -1 1
                -1 
t1             :e1              0.31        0.155       0.465       -1 0.62
                -1 
t2             :e2              0.62        0.62        0           -1 0.62
                -1 
-1

P p0 1
t0              1  0 1  e0              0.31        0           0           -1 
                        -1 
-1 

P p1 1
t1              1  0 1  e1              0.31        0           0           -1 
                        -1 
-1 

P p2 1
t2              1  0 1  e2              0.62        0           0           -1 
                        -1 
-1 

-1

