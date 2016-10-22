import random
import sys
l=[i for i in range(0,int(sys.argv[1]))]
random.shuffle(l)
with open('test.del','w') as f:
  for i in l:
      f.write(str(i)+',test'+str(i)+'\n')