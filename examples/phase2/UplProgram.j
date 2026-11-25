.class public UplProgram
.super java/lang/Object

.method public static main([Ljava/lang/String;)V
  .limit stack 2
  .limit locals 13
  iconst_0
  istore 1
  iconst_1
  istore 2
L0:
  iconst_4
  iload 2
  if_icmpgt C3True
  iconst_0
  goto C3End
C3True:
  iconst_1
C3End:
  istore 3
  iload 3
  ifne B4Next
  goto_w L1
B4Next:
  iload 2
  iconst_2
  if_icmpeq C5True
  iconst_0
  goto C5End
C5True:
  iconst_1
C5End:
  istore 4
  iload 4
  ifne B6Next
  goto_w L2
B6Next:
  iload 2
  bipush 10
  imul
  istore 5
  iload 1
  iload 5
  iadd
  istore 6
  iload 6
  istore 1
  goto_w L3
L2:
  iload 1
  iload 2
  iadd
  istore 7
  iload 7
  istore 1
L3:
  iload 2
  iconst_1
  iadd
  istore 8
  iload 8
  istore 2
  goto_w L0
L1:
L4:
  iload 1
  iconst_1
  iadd
  istore 9
  iload 9
  istore 1
  bipush 26
  iload 1
  if_icmpgt C22True
  iconst_0
  goto C22End
C22True:
  iconst_1
C22End:
  istore 10
  iload 10
  ifeq B23Next
  goto_w L4
B23Next:
  iload 1
  bipush 26
  if_icmpge C24True
  iconst_0
  goto C24End
C24True:
  iconst_1
C24End:
  istore 11
  iload 11
  ifne B25Next
  goto_w L5
B25Next:
  getstatic java/lang/System/out Ljava/io/PrintStream;
  iload 1
  invokevirtual java/io/PrintStream/println(I)V
L5:
  iload 1
  bipush 26
  if_icmpeq C28True
  iconst_0
  goto C28End
C28True:
  iconst_1
C28End:
  istore 12
  getstatic java/lang/System/out Ljava/io/PrintStream;
  iload 12
  invokevirtual java/io/PrintStream/println(Z)V
  return
.end method
