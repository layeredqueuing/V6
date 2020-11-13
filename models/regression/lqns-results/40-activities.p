# lqns 6.1
# lqns --pragma=variance=mol,threads=hyper --parseable 40-activities.in
# $Id: 40-activities.p 13907 2020-10-01 16:08:43Z greg $
V y
C 8.89772e-06
I 9
PP 2
NP 1

#!Comment: Activities with AND fork/join.
#!User:  0:00:00.010
#!Sys:   0:00:00.000
#!Real:  0:00:00.010
#!Solver: 3 27 880 125358 751308 2.10703e+11 0

B 2
client         :client          0.47619     
server         :server          0.779221    
-1

W 1
client         :-1
               :
                client          server          0            -1 
                -1 
-1

J 0
server         :fork1           fork2           1.09454     0.458255   
-1 
-1

X 2
client         :client          2.59456    -1 
                -1
:
                client          2.59456     -1 
                -1 
server         :server          1.59454    -1 
                -1
:
                fork1           0.420823    -1 
                fork2           1           -1 
                join            0.25        -1 
                server          0.25        -1 
                -1 
-1

VAR 2
client         :client          6.5275     -1 
                -1
:
                client          6.5275      -1 
                -1 
server         :server          0.583255   -1 
                -1
:
                fork1           0.160434    -1 
                fork2           0.520001    -1 
                join            0.0625      -1 
                server          0.0625      -1 
                -1 
-1

FQ 2
client         :client          0.385423    1          -1 1
                -1
:
                client          0.385423    1           -1 
                -1 
server         :server          0.385423    0.614572   -1 0.614572
                -1
:
                fork1           0.385423    0.162195    -1 
                fork2           0.385423    0.385424    -1 
                join            0.385423    0.0963558   -1 
                server          0.385423    0.0963558   -1 
                -1 
-1

P client 1
client          1  0 1  client          0.385423    0          -1 
-1
                       :
                        client          0.385423    0           -1 
                        -1 
                                        0.385423    
-1 

P server 1
server          1  0 1  server          0.578135    0          -1 
-1
                       :
                        fork1           0.154169    0.0208211   -1 
                        fork2           0.231254    0.400001    -1 
                        join            0.0963558   0           -1 
                        server          0.0963558   0           -1 
                        -1 
                                        0.578135    
-1 

-1

