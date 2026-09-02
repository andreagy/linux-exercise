# Linux Exercise

# Task 1

## Decisions

### How to measure the timing of 100ms?
The task requires that client1 print a line exactly every 100 ms. One might consider to set a select() timeout of 100 ms and print after it returns. However, this introduces two problems:
- Early wake-up: if data arrives sooner than 100ms the loop prints prematurely, since select() returns as soon as data is available.
- Drift: if we try to compensate e.g. with sleeping for the remaining time, small variations in processing time could accumulate, resulting in a drift from the original schedule.

To avoid these issues I chose to use timerfd. The kernel maintains the timer's absolute schedule, timer expirations happen consistently, no matter how long the process takes in the rest of the loop. Also, timerfd integrates with the existing I/O processes, it can be added to the same select() set as the TCP sockets. This way the main loop either waits for socket data or the timer event, and printing occurs only when the timer expires.


## Results

All three outputs resemble a certain type of waveform or function. I determined frequency by checking the time difference between the start and end of the period.

What are the frequencies, amplitues and shapes you see on the server outputs?

Out1 (Port 4001)
Shape: Sine wave
Frequency: 0.5 Hz (period ≈ 2000 ms)
Amplitude: 5 (range –5.0 to 5.0)

To check the exact frequency of out1, I needed to lower the timeout interval from 100ms to 10ms because multiple values could be sent by the port in 100ms.

Out2 (Port 4002)
Shape: Triangular wave
Frequency: 0.5 Hz (period ≈ 4000 ms)
Amplitude: 5 (range –5.0 to 5.0)

Out3 (Port 4003)
Shape: Square wave
Frequency: 0.125 Hz (period ≈ 8000 ms)
Amplitude: 5 (range 0 to 5.0)