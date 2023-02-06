import subprocess
import time
import random
cmd = ""

for i in range(100, 65500, 100):
    cmd = "./client ccnc-08.arc.rice.edu 18157 "+str(i)+" 100"
    subprocess.Popen(cmd, shell=True)
    slp=random.randint(1,5);
    time.sleep(slp)
