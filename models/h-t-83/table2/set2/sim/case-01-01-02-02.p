# petrisrvn 4.1
# petrisrvn -p  case-01-01-02-02.in
V y
C 9.8671e-06
I 296
PP 5
NP 1
#!User:  0:00:00.23
#!Sys:   0:00:00.12
#!Real:  0:00:00.73

W 16
  t1 : -1

:
     a1 d1 0.000000 -1
     a1 d2 0.000000 -1
     a1 d3 0.000000 -1
     a1 d4 0.000000 -1
     b1 d1 0.005000 -1
     b1 d2 0.005000 -1
     b1 d3 0.005000 -1
     b1 d4 0.005000 -1
     b2 d1 0.005000 -1
     b2 d2 0.005000 -1
     b2 d3 0.005000 -1
     b2 d4 0.005000 -1
     b3 d1 0.007139 -1
     b3 d2 0.007139 -1
     b3 d3 0.007139 -1
     b3 d4 0.007139 -1
  -1
-1


J 1
t1 : b3 b1 1.135616 0.000000
  -1
-1


X 5
t1 : e1 0.827159 -1
  -1

:
     a1 0.600000 -1
     a2 0.000000 -1
     b1 0.678130 -1
     b2 0.678130 -1
     b3 0.551635 -1
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
t1 : e1 1.208957 1.000000 -1 1.000000
-1

:
    a1              1.208957 0.725374 -1
    a2              0.241808 0.000000 -1
    b1              0.241834 0.163995 -1
    b2              0.241834 0.163995 -1
    b3              0.241830 0.133402 -1
    c1              0.241889 0.000000 -1
  -1
d1 : d1 4.803980 0.192159 -1 0.192159
-1
d2 : d2 4.803980 0.192159 -1 0.192159
-1
d3 : d3 4.803980 0.192159 -1 0.192159
-1
d4 : d4 4.803980 0.192159 -1 0.192159
-1
-1

P p1 1
t1 1 0 1 e1 0.341960 0.000000 -1 -1

:
         a1 0.241791 0 -1
         a2 0 0 -1
         b1 0.0477179 0.0379651 -1
         b2 0.0477179 0.0379651 -1
         b3 0.00473253 0.0734154 -1
         c1 0 0 -1
 -1
-1

P d1 1
d1 1 0 1 d1 0.192159 0.000000 -1 -1
-1

P d2 1
d2 1 0 1 d2 0.192159 0.000000 -1 -1
-1

P d3 1
d3 1 0 1 d3 0.192159 0.000000 -1 -1
-1

P d4 1
d4 1 0 1 d4 0.192159 0.000000 -1 -1
-1
-1

