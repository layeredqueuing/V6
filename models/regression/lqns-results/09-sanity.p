# lqns 6.1
# lqns --pragma=variance=mol,threads=hyper --parseable 09-sanity.in
# $Id$
V y
C 6.95178e-06
I 6
PP 2
NP 2

#!Comment: Most Common features.
#!User:  0:00:00.001
#!Sys:   0:00:00.000
#!Real:  0:00:00.001
#!Solver: 3 18 198 2380 12567 1.29959e+07 0

B 4
client         :client          1.82927     
server1        :entry1          1.92308     
server2        :entry2          5           
                entry3          3.33333     
-1

W 2
client         :client          entry1          0.511542     0            -1 
                -1 
server1        :entry1          entry2          0.120194     0            -1 
                -1 
-1

F 1
server1        :entry1          entry2          0.105005    
                -1 
-1

Z 1
server1        :entry1          entry3          0            0.420196     -1 
                -1 
-1

X 4
client         :client          2.33537     0           -1 
                -1 
server1        :entry1          0.637103    0.54948     -1 
                -1 
server2        :entry2          0.268447    0           -1 
                entry3          0.368447    0           -1 
                -1 
-1

VAR 4
client         :client          9.24335     0           -1 
                -1 
server1        :entry1          0.426908    0.252448    -1 
                -1 
server2        :entry2          0.0446851   0           -1 
                entry3          0.0946851   0           -1 
                -1 
-1

FQ 3
client         :client          1.28459     3           0           -1 3
                -1 
server1        :entry1          1.2846      0.818425    0.705864    -1 1.52429
                -1 
server2        :entry2          0.899214    0.241392    0           -1 0.241392
                entry3          0.385381    0.141993    0           -1 0.141993
                -1 
                                1.28459     0.383384    0           -1 0.383384
-1

P client 1
client          1  0 3  client          1.28459     0           0           -1 
                        -1 
-1 

P server 2
server1         1  0 2  entry1          1.2846      0.0593763   0.0494803   -1 
                        -1 
server2         2  0 1  entry2          0.179843    0.0684477   0           -1 
                        entry3          0.115614    0.0684477   0           -1 
                        -1 
                                        0.295457    
                -1 
                                        1.58006
-1 

-1

