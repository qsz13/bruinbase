import urllib
import hashlib

#hash between number and character
#values = ['a', 'b', 'c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z']
#keys = [0,1, 2, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25]
#hash = {k:v for k, v in zip(keys, values)}

f=open('test.del','w')
for i in range(100):
    f.write(str(i)+',test'+str(i)+'\n')
f.close()
