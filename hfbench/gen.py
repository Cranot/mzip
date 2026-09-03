import random
random.seed(7)
D="/root/mzip-hfbench/verify_adv/"
# A6_RECORD target: SQL INSERT..VALUES tuples
rows=[]
names=["alice","bob","carol","dave","erin","frank","grace","heidi"]
for i in range(60000):
    rows.append("(%d,'%s%d',%d,%d)" % (i+1, random.choice(names), i%997, 1000+i, (i*7)%50000))
out=[]
for i in range(0,len(rows),50):
    out.append("INSERT INTO users VALUES " + ",".join(rows[i:i+50]) + ";\n")
open(D+"synth-sql.bin","w").write("".join(out))
# A7_TEMPORAL target: Apache CLF with monotone bracket timestamps
ips=["10.0.0.%d"%i for i in range(1,60)]
paths=["/index.html","/api/v1/items","/static/app.js","/favicon.ico","/login"]
lines=[]
sec=0
for i in range(120000):
    sec += 1 if i%3 else 2
    h=(sec//3600)%24; m=(sec//60)%60; s=sec%60
    lines.append('%s - - [12/Aug/2026:%02d:%02d:%02d +0000] "GET %s HTTP/1.1" 200 %d\n'
                 % (random.choice(ips),h,m,s,random.choice(paths),200+(i%3000)))
open(D+"synth-log.bin","w").write("".join(lines))
print("WROTE synth-sql.bin synth-log.bin")
