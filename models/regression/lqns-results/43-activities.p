# lqns 6.1
# lqns --pragma=variance=mol,threads=hyper --parseable 43-activities.in
# $Id: 43-activities.p 13907 2020-10-01 16:08:43Z greg $
V y
C 7.91977e-07
I 22
PP 2
NP 2

#!Comment: Activities with AND fork-join - reply on branch.
#!User:  0:00:00.033
#!Sys:   0:00:00.000
#!Real:  0:00:00.034
#!Solver: 3 66 667 14195 82728 3.39541e+08 0

B 2
client         :client          1.08108     
server         :server          1.55844     
-1

W 1
client         :client          server          0.222935     0            -1 
                -1 
-1

J 0
server         :fork1           fork2           0.933793    0.323089   
-1 
-1

X 2
client         :client          2.37477     0           -1 
                -1 
server         :server          1.15183     0.596606   -1 
                -1
:
                fork1           0.532168    -1 
                fork2           0.74451     -1 
                join            0.407322    -1 
                server          0.407322    -1 
                -1 
-1

VAR 2
client         :client          9.10449     0           -1 
                -1 
server         :server          0.468133    0.410339   -1 
                -1
:
                fork1           0.177468    -1 
                fork2           0.380883    -1 
                join            0.0872503   -1 
                server          0.0872503   -1 
                -1 
-1

FQ 2
client         :client          0.842188    2           0           -1 2
                -1 
server         :server          0.842188    0.970059    0.502454   -1 1.47251
                -1
:
                fork1           0.842188    0.448186    -1 
                fork2           0.842188    0.627017    -1 
                join            0.842188    0.343042    -1 
                server          0.842188    0.343042    -1 
                -1 
-1

P client 1
client          1  0 2  client          0.842188    0           0           -1 
                        -1 
-1 

P server 1
server          1  0 2  server          1.26328     0           0          -1 
-1
                       :
                        fork1           0.336875    0.132168    -1 
                        fork2           0.505313    0.14451     -1 
                        join            0.210547    0.157322    -1 
                        server          0.210547    0.157322    -1 
                        -1 
                                        1.26328     
-1 

-1

