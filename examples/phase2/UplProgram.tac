# v0: int sum, scope 0, declared at 2:3
# v1: int i, scope 1, declared at 3:8
  v0 = 0
  v1 = 1
L0:
  t0:bool = 4 > v1
  ifFalse t0 goto L1
  t1:bool = v1 == 2
  ifFalse t1 goto L2
  t2:int = v1 * 10
  t3:int = v0 + t2
  v0 = t3
  goto L3
L2:
  t4:int = v0 + v1
  v0 = t4
L3:
  t5:int = v1 + 1
  v1 = t5
  goto L0
L1:
L4:
  t6:int = v0 + 1
  v0 = t6
  t7:bool = 26 > v0
  if t7 goto L4
  t8:bool = v0 >= 26
  ifFalse t8 goto L5
  print v0
L5:
  t9:bool = v0 == 26
  print t9
