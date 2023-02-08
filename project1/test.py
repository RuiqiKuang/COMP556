import subprocess
import time
import random
cmd = ""

for i in range(100, 65501, 100):
    # change it as you wish
    cmd = "./client ccnc-06.arc.rice.edu 18100 " + str(i)+" 100"  
    subprocess.Popen(cmd, shell=True)
    slp = random.randint(1, 5)
    time.sleep(slp*0.1)
