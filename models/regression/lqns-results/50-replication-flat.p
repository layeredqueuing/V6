# lqns 6.0
# lqns --pragma=variance=mol,threads=mak --parseable 50-replication-flat.in
# $Id: 50-replication-flat.p 13070 2017-06-13 16:09:36Z greg $
V y
C 9.566e-06
I 4
PP 6
NP 1

#!Comment: Simplest model.
#!User:  0:00:00.001
#!Sys:   0:00:00.000
#!Real:  0:00:00.001
#!Solver: 2 8 129 3749 140424 1.29292e+10 0

B 6
client_1       :client_1        0.5         
client_2       :client_2        0.5         
client_3       :client_3        0.5         
client_4       :client_4        0.5         
server_1       :server_1        1           
server_2       :server_2        1           
-1

W 4
client_1       :client_1        server_1        0.5         -1 
                -1 
client_2       :client_2        server_2        0.5         -1 
                -1 
client_3       :client_3        server_1        0.5         -1 
                -1 
client_4       :client_4        server_2        0.5         -1 
                -1 
-1

X 6
client_1       :client_1        2.5         -1 
                -1 
client_2       :client_2        2.5         -1 
                -1 
client_3       :client_3        2.5         -1 
                -1 
client_4       :client_4        2.5         -1 
                -1 
server_1       :server_1        1           -1 
                -1 
server_2       :server_2        1           -1 
                -1 
-1

VAR 6
client_1       :client_1        11.875      -1 
                -1 
client_2       :client_2        11.875      -1 
                -1 
client_3       :client_3        11.875      -1 
                -1 
client_4       :client_4        11.875      -1 
                -1 
server_1       :server_1        1           -1 
                -1 
server_2       :server_2        1           -1 
                -1 
-1

FQ 6
client_1       :client_1        0.4         1           -1 1
                -1 
client_2       :client_2        0.4         1           -1 1
                -1 
client_3       :client_3        0.4         1           -1 1
                -1 
client_4       :client_4        0.4         1           -1 1
                -1 
server_1       :server_1        0.8         0.8         -1 0.8
                -1 
server_2       :server_2        0.8         0.8         -1 0.8
                -1 
-1

P client_1 1
client_1        1  0 1  client_1        0.4         0           -1 
                        -1 
-1 

P client_2 1
client_2        1  0 1  client_2        0.4         0           -1 
                        -1 
-1 

P client_3 1
client_3        1  0 1  client_3        0.4         0           -1 
                        -1 
-1 

P client_4 1
client_4        1  0 1  client_4        0.4         0           -1 
                        -1 
-1 

P server_1 1
server_1        1  0 1  server_1        0.8         0           -1 
                        -1 
-1 

P server_2 1
server_2        1  0 1  server_2        0.8         0           -1 
                        -1 
-1 

-1

