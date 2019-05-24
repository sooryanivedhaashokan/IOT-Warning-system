#/usr/bin/python3.6

import smtplib
import email.utils
import subprocess
import time
from email.mime.text import MIMEText

def send_email(i):
    global temp1
    global humd1
    global st1
    fromaddr = "cssee556@gmail.com"
    toaddr = "cssee556@gmail.com"
    if i == 1:
        msgCont = 'The battery temperature has exceeded the limit ' + str(st1)
    if i == 2:
        msgCont = 'The inverter temperature has exceeded the limit ' + str(st2)
    msg = MIMEText(msgCont)
    msg['To'] = email.utils.formataddr(('Admin', toaddr))
    msg['From'] = email.utils.formataddr(('css_group', fromaddr))
    msg['Subject'] = 'Alert Notification'
    server = smtplib.SMTP()
    server.connect ('smtp.gmail.com', 587)
    server.ehlo()
    server.starttls()
    server.ehlo()
    server.login(fromaddr, "projectpip")
    server.sendmail(fromaddr, toaddr, msg.as_string())
    server.quit()

def ReadTemp1():
    global temp1
    global humd1
    global g1
    sensor1 = 0
    sensor1 = subprocess.run('sudo stdbuf -o0 ./pip_sense.v2 l l | stdbuf -o0 grep TX:03414', stdout=subprocess.PIPE, shell=True)    
    line1 = sensor1.stdout.decode('utf-8')
    celsius1 = line1.split("temp: ")
    value1 = celsius1[-1].split("humidity: ")
    temp1 = float(value1[0])*g1
    temp1 = round(temp1,2)
    humd1 = float(value1[1]) 
    

def ReadTemp2():
    global temp2
    global humd2
    global g2
    sensor2 = 0
    sensor2 =subprocess.run('sudo stdbuf -o0 ./pip_sense.v2 l l | stdbuf -o0 grep TX:03415', stdout=subprocess.PIPE, shell=True)
    line2 = sensor2.stdout.decode('utf-8')
    celsius2 = line2.split("temp: ")
    value2 = celsius2[-1].split("humidity: ")
    temp2 = float(value2[0])*g2
    temp2 = round(temp2,2)
    humd2 = float(value2[1])

#initializing the variable to send the email once
T1EmailSent = 0
T2EmailSent = 0
g1 = 1
g2 = 1

#initialization and calibration
global temp1
global temp2
print("***UPS TEMPERATURE AND HUMIDITY MONITORING***")
time.sleep(1)
x = input("Enter ambient temperature for sensor calibration:")
st1 = input("Enter temperature threshold for battery sensor:")
st2 = input("Enter temperature threshold for inverter sensor:")
x = float(x)
st1 = float(st1)
st2 = float(st2)
time.sleep(1)
print("You entered - Ambient Temp:", x,"- Battery Temp Threshold:", st1,"- Inverter Temp Threshold:", st2)
ReadTemp1()
time.sleep(1)
ReadTemp2()
g1 = x/temp1
g2 = x/temp2
print("Calibration Done")


while True:
    time.sleep(1)
    ReadTemp1()
    time.sleep(1)
    ReadTemp2()

    # for visualizing the temperature and humidity change
    print("battery sensor temp:",temp1,"humid: ",humd1)
    print("inverter sensor temp:",temp2,"humid: ",humd2)

    # check for over temperature conditions
    if temp1>st1 and T1EmailSent==0:
        send_email(1)
        T1EmailSent = 1
        
    if temp2>st2 and T2EmailSent==0:
        send_email(2)
        T2EmailSent = 1
        
    # add some hysteresis before triggering the alarm again
    if temp1<(st1-10):
        T1EmailSent = 0
    if temp2<(st2-10):
        T2EmailSent = 0
    
    
