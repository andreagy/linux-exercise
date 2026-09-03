# Linux Exercise

# Task 1

## Design

The client1 program opens a TCP connection to each server port. The sockets are set to non-blocking mode so that the program can continue processing the timer and other sockets. The program uses select() to monitor all three TCP sockets and a repeating timerfd simultaneously. Socket data is read whenever it becomes available. Data is accumulated in a buffer for every socket until a newline character is received. Each complete line represents one server value. If multiple complete lines are received during a 100ms interval, the newest value replaces the previous one in the latest_value variable.

The timer is configured with an initial expiration and a repeating interval of 100ms. When it expires the program prints one JSON object and resets the latest values to "--". So if a port doesn't provide a value during the next 100ms interval the corresponding output will be "--".

## Decisions

### How to measure the timing of 100ms?
The task requires that client1 print a line every 100 ms. One might consider to set a select() timeout of 100 ms and print after it returns. However, this introduces two problems:
- Early wake-up: if data arrives sooner than 100ms the loop prints prematurely, since select() returns as soon as data is available.
- Drift: if we try to compensate e.g. with sleeping for the remaining time, small variations in processing time could accumulate, resulting in a drift from the original schedule.

Therefore I chose to use timerfd. The kernel maintains the timer's repeating schedule independently of the socket activity, so the program prints without timer drift. Also, timerfd integrates with the existing I/O processes, it can be added to the same select() set as the TCP sockets. Although since the timer schedule is tracked by the kernel independent of other loop processes, printing occurs at consistent 100ms intervals.

### Timer and socket events
It is possible for the timer and a socket to become ready at approximately the same time. This doesn't change the timer's period, but can influence the timing of that particular print in that cycle. I was considering putting the timer expiration check before the data parsing process, to the beginning of the loop. But this would result in printing the socket data that was parsed in the previous cycle. I chose to keep the timer expiration check after the data parsing, so the print includes the freshest available values which matches the requirements to print the most recent value recieved during each 100ms interval. However, regardless of the order of timer and socket events, the operating system may introduce scheduling jitter as well anyway.


## Results

All three outputs resemble a certain type of waveform or function. I determined frequency by checking the time difference between the start and end of the period.

What are the frequencies, amplitues and shapes you see on the server outputs?

For out1, I temporarily changed the client timer interval from 100ms to 10ms, which provided a denser set of values to identify the start and end of a complete period easier.

Out1 (Port 4001)<br>
Shape: Sine wave<br>
Frequency: 0.5 Hz (period ≈ 2000 ms)<br>
Amplitude: 5 (range –5.0 to 5.0)<br>

Out2 (Port 4002)<br>
Shape: Triangular wave<br>
Frequency: 0.25 Hz (period ≈ 4000 ms)<br>
Amplitude: 5 (range –5.0 to 5.0)<br>

Out3 (Port 4003)<br>
Shape: Square wave<br>
Frequency: 0.125 Hz (period ≈ 8000 ms)<br>
Amplitude: 5 (range 0 to 5.0)<br>

## Validation

Checking consecutive timestamps, they were separated by 100 ms. At some point, a difference of 99ms appeared (see output_client1.log file), but this doesn't mean that the timer interval changed. The timer uses CLOCK_MONOTONIC, and the required Unix-epoch timestamp is obtained with CLOCK_REALTIME. The small variation also can result from process scheduling and conversion to whole milliseconds. So all in all, the solution correctly prints the lines at 100ms intervals with an occasional small variation most probably not caused by the timer.

I also checked that:
- every output line is a seperate JSON object
- each object contains the timestamp, out1, out2, and out3
- timestamps represent Unix epoch time in ms
- missing values are printed as "--"
- values appear under the correct output field (checked the output of each TCP port using 'timeout 3 nc localhost 4001' in the docker container)
- only JSON output is written to STDOUT
- the latest complete value from each TCP socket is printed: each successful read is processed until a read returns -1, with errno equal to EAGAIN or EWOULDBLOCK, indicating that no more data is currently available. Since each newer complete line overwrites the previous value, the latest available value is printed.