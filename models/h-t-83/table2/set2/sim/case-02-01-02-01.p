# petrisrvn 4.1
# petrisrvn -p  case-02-01-02-01.in
V y
C 9.8735e-06
I 236
PP 5
NP 1
#!User:  0:00:00.20
#!Sys:   0:00:00.14
#!Real:  0:00:00.56

W 16
  t1 : -1

:
     a1 d1 0.000000 -1
     a1 d2 0.000000 -1
     a1 d3 0.000000 -1
     a1 d4 0.000000 -1
     b1 d1 0.005879 -1
     b1 d2 0.005879 -1
     b1 d3 0.005879 -1
     b1 d4 0.005879 -1
     b2 d1 0.005879 -1
     b2 d2 0.005879 -1
     b2 d3 0.005879 -1
     b2 d4 0.005879 -1
     b3 d1 0.005753 -1
     b3 d2 0.005753 -1
     b3 d3 0.005753 -1
     b3 d4 0.005753 -1
  -1
-1


J 1
t1 : b3 b1 1.076939 0.000000
  -1
-1


X 5
t1 : e1 0.815401 -1
  -1

:
     a1 0.600000 -1
     a2 0.000000 -1
     b1 0.646207 -1
     b2 0.646207 -1
     b3 0.451028 -1
     c1 0.000000 -1
  -1
d1 : d1 0.040000 -1
  -1
d2 : d2 0.040000 -1
  -1
d3 : d3 0.040000 -1
  -1
d4 : d4 0.040000 -1
  -1
-1

FQ 5
t1 : e1 1.226391 1.000000 -1 1.000000
-1

:
    a1              1.226391 0.735834 -1
    a2              0.245295 0.000000 -1
    b1              0.245338 0.158539 -1
    b2              0.245338 0.158539 -1
    b3              0.245293 0.110634 -1
    c1              0.245377 0.000000 -1
  -1
d1 : d1 4.873091 0.194924 -1 0.194924
-1
d2 : d2 4.873091 0.194924 -1 0.194924
-1
d3 : d3 4.873092 0.194924 -1 0.194924
-1
d4 : d4 4.873092 0.194924 -1 0.194924
-1
-1

P p1 1
t1 1 0 1 e1 0.341661 0.000000 -1 -1

:
         a1 0.245278 0 -1
         a2 0 0 -1
         b1 0.0481915 0 -1
         b2 0.0481915 0 -1
         b3 0 0 -1
         c1 0 0 -1
 -1
-1

P d1 1
d1 1 0 1 d1 0.194924 0.000000 -1 -1
-1

P d2 1
d2 1 0 1 d2 0.194924 0.000000 -1 -1
-1

P d3 1
d3 1 0 1 d3 0.194924 0.000000 -1 -1
-1

P d4 1
d4 1 0 1 d4 0.194924 0.000000 -1 -1
-1
-1

