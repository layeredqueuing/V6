# petrisrvn 3.10
# petrisrvn -M -p -osync1.out  sync1.in
V y
C 1.3115e-16
I 10
PP 3
NP 1
#!User:  0:00:00.29
#!Sys:   0:00:00.18
#!Real:  0:00:00.50

W 2
t1 : e1 e3 0.004545 -1
  -1
t2 : e2 e4 0.004545 -1
  -1
-1


J 1
t3 : a2 a1 1.009092 0.000000
  -1
-1


X 4
t1 : e1 1.709092 -1
  -1
t2 : e2 1.709092 -1
  -1
t3 : e3 0.704546 -1
     e4 0.704546 -1
  -1

:
     a1 0.100000 -1
     a2 0.100000 -1
     a3 0.100000 -1
  -1
-1

FQ 3
t1 : e1 0.585106 1.000000 -1 1.000000
-1
t2 : e2 0.585106 1.000000 -1 1.000000
-1
t3 : e3 0.585106 0.353723 -1 0.353723
  e4 0.585106 0.353723 -1 0.353723
-1
:
    a1              0.585106 0.050206 -1
    a2              0.585106 0.050206 -1
    a3              0.585106 0.050206 -1
  -1
    0.585106 0.707447 -1 0.707447

-1

P p1 1
t1 1 0 1 e1 0.585106 0.000000 -1 -1
-1

P p2 1
t2 1 0 1 e2 0.585106 0.000000 -1 -1
-1

P p3 1
t3 2 0 1 e3 0.117021 0.000000 -1
         e4 0.117021 0.000000 -1 -1

:
         a1 0.0585106 0 -1
         a2 0.0585106 0 -1
         a3 0.0585106 0 -1
 -1
      0.234043
-1
-1

