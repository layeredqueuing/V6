# lqns 6.1
# lqns --pragma=variance=mol,threads=hyper --parseable 42-activities.in
# $Id: 42-activities.p 13907 2020-10-01 16:08:43Z greg $
V y
C 2.75056e-07
I 11
PP 2
NP 2

#!Comment: Activities with AND fork-join - reply on branch.
#!User:  0:00:00.012
#!Sys:   0:00:00.000
#!Real:  0:00:00.013
#!Solver: 3 33 978 121562 728382 1.89069e+11 0

B 2
client         :client          0.540541    
server         :server          0.779221    
-1

W 1
client         :client          server          0.0879996    0            -1 
                -1 
-1

J 0
server         :fork1           fork2           1.09389     0.45863    
-1 
-1

X 2
client         :client          2.338       0           -1 
                -1 
server         :server          1.25        0.343892   -1 
                -1
:
                fork1           0.418801    -1 
                fork2           1           -1 
                join            0.25        -1 
                server          0.25        -1 
                -1 
-1

VAR 2
client         :client          9.04673     0           -1 
                -1 
server         :server          0.5825      0.52113    -1 
                -1
:
                fork1           0.160353    -1 
                fork2           0.52        -1 
                join            0.0625      -1 
                server          0.0625      -1 
                -1 
-1

FQ 2
client         :client          0.427716    1           0           -1 1
                -1 
server         :server          0.427716    0.534645    0.147088   -1 0.681733
                -1
:
                fork1           0.427716    0.179128    -1 
                fork2           0.427716    0.427716    -1 
                join            0.427716    0.106929    -1 
                server          0.427716    0.106929    -1 
                -1 
-1

P client 1
client          1  0 1  client          0.427716    0           0           -1 
                        -1 
-1 

P server 1
server          1  0 1  server          0.641574    0           0          -1 
-1
                       :
                        fork1           0.171086    0.0188008   -1 
                        fork2           0.25663     0.4         -1 
                        join            0.106929    0           -1 
                        server          0.106929    0           -1 
                        -1 
                                        0.641574    
-1 

-1

