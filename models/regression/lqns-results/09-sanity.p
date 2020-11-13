# lqns 6.1
# lqns --pragma=variance=mol,threads=hyper --parseable 09-sanity.in
# $Id: 09-sanity.p 14047 2020-11-06 13:41:28Z greg $
V y
C 6.95056e-06
I 6
PP 2
NP 2

#!Comment: Most Common features.
#!User:  0:00:00.002
#!Sys:   0:00:00.000
#!Real:  0:00:00.002
#!Solver: 3 18 197 2351 12393 1.25564e+07 0

B 4
client         :client          1.82927     
server1        :entry1          1.92308     
server2        :entry2          5           
                entry3          3.33333     
-1

W 2
client         :client          entry1          0.511558     0            -1 
                -1 
server1        :entry1          entry2          0.120193     0            -1 
                -1 
-1

F 1
server1        :entry1          entry2          0.105005    
                -1 
-1

Z 1
server1        :entry1          entry3          0            0.420198     -1 
                -1 
-1

X 4
client         :client          2.3354      0           -1 
                -1 
server1        :entry1          0.637116    0.54949     -1 
                -1 
server2        :entry2          0.268449    0           -1 
                entry3          0.368449    0           -1 
                -1 
-1

VAR 4
client         :client          9.24363     0           -1 
                -1 
server1        :entry1          0.426914    0.252449    -1 
                -1 
server2        :entry2          0.0446853   0           -1 
                entry3          0.0946853   0           -1 
                -1 
-1

FQ 3
client         :client          1.28458     3           0           -1 3
                -1 
server1        :entry1          1.28459     0.818431    0.705868    -1 1.5243
                -1 
server2        :entry2          0.899202    0.24139     0           -1 0.24139
                entry3          0.385376    0.141992    0           -1 0.141992
                -1 
                                1.28458     0.383382    0           -1 0.383382
-1

P client 1
client          1  0 3  client          1.28458     0           0           -1 
                        -1 
-1 

P server 2
server1         1  0 2  entry1          1.28459     0.0593885   0.0494905   -1 
                        -1 
server2         2  0 1  entry2          0.17984     0.0684495   0           -1 
                        entry3          0.115613    0.0684495   0           -1 
                        -1 
                                        0.295453    
                -1 
                                        1.58004
-1 

-1

