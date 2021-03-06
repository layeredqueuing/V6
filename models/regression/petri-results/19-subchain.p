# petrisrvn 6.0
# $Id: 19-subchain.p 13060 2017-06-07 15:36:10Z greg $
V y
C 8.30097e-06
I 0
PP 5
NP 1

#!Comment: Model 9a
#!User:  0:00:00.131
#!Sys:   0:00:00.221
#!Real:  0:00:03.935

W 4
c0             :c0              e0a             0           -1 
                -1 
c1             :c1              e0b             0           -1 
                -1 
t0             :e0a             e1              0           -1 
                e0b             e2              0.9275      -1 
                -1 
-1

X 6
c0             :c0              14.8067     -1 
                -1 
c1             :c1              12.1577     -1 
                -1 
t0             :e0a             13.8069     -1 
                e0b             11.1577     -1 
                -1 
t1             :e1              1.99999     -1 
                -1 
t2             :e2              2           -1 
                -1 
-1

FQ 5
c0             :c0              0.067537    1           -1 1
                -1 
c1             :c1              0.411261    5           -1 5
                -1 
t0             :e0a             0.067536    0.932463    -1 0.932463
                e0b             0.411263    4.58874     -1 4.58874
                -1 
                                0.478799    5.5212      -1 5.5212
t1             :e1              0.067534    0.135067    -1 0.135067
                -1 
t2             :e2              0.218247    0.436494    -1 0.436494
                -1 
-1

P c0 1
c0              1  0 1  c0              0.0675374   0           -1 
                        -1 
-1 

P c1 1
c1              1  0 5  c1              0.411261    0           -1 
                        -1 
-1 

P p0 1
t0              2  0 1  e0a             0.675356    1.80704     -1 
                        e0b             0.314756    11.5488     -1 
                        -1 
                                        0.990112    
-1 

P p1 1
t1              1  0 1  e1              0.135067    0           -1 
                        -1 
-1 

P p2 1
t2              1  0 1  e2              0.436494    0           -1 
                        -1 
-1 

-1

