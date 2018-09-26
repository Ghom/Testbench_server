#!/usr/bin/python3
import time
import RPi.GPIO as GPIO
GPIO.setmode(GPIO.BOARD)

GPIO.setup(12, GPIO.OUT)
GPIO.setup(16, GPIO.OUT)
GPIO.setup(18, GPIO.OUT)

GPIO.output(16, GPIO.LOW)
GPIO.output(18, GPIO.HIGH)

fq = 100
dc = num = 15

p = GPIO.PWM(12, fq)  # channel=18 frequency=[fq]Hz
p.ChangeDutyCycle(dc)
p.start(0)
p.ChangeDutyCycle(dc)
while 1:
    try:
        num = int(input('Enter the new duty cycle:'))
        if num in range(0,101):
            GPIO.output(16, GPIO.LOW)
            GPIO.output(18, GPIO.HIGH)
            dc = num
            p.ChangeDutyCycle(dc)
        elif num in range(-100,0):
            GPIO.output(16, GPIO.HIGH)
            GPIO.output(18, GPIO.LOW)
            dc = num*-1
            p.ChangeDutyCycle(dc)
        else:
            print('The duty cycle must be in the range [-100% ; 100%]')
    except KeyboardInterrupt:
        p.stop()
        GPIO.cleanup()
        exit()
    except:
        print('You must enter a number (\'{}\')'.format(num))

