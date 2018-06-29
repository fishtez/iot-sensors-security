# iot-sensors-security
Internet of Things - Sensors and Security - Intel Edison and Grove Kit for IoT

This was for a school assignment, so I renamed the master branch to try to hide from crawlers. I just wanted to make it a little harder for cheaters in future classes to find my work.

Here is the assignment, copied directly from the class website:

PROJECT DESCRIPTION:
Part 1: Communication with a Logging Server

Write a program (called lab4c_tcp) that:

    builds and runs on your Edison
    is based on the temperature sensor application you built previously
    accepts the following parameters:
        --id=9-digit-number
        --host=name or address
        --log=filename
        (required) port number
    Note: that there is no --port= in front of the port number. This is non-switch parameter.
    It accepts the same commands and generates the same reports as the previous Edison project, but now the input and output are from/to a network connection to a server.
        open a TCP connection to the server at the specified address and port
        immediately send (and log) an ID terminated with a newline:
        ID=ID-number. This new report enables the server to keep track of which devices it has received reports from.
        send (and log) newline terminated temperature reports over the connection
        process (newline terminated) commands reveived over the connection
        the last command sent by the server will (as before) be OFF
    as before, assume that the temperature sensor has been connected to Analog input 0.

The ID number will appear in the TCP server log (follow the TCP server URL), and will permit you to find the reports for your sessions. To protect your privacy, You do not have to use your student ID number, but merely a nine-digit number that you will recognize and that will be different from the numbers chosen by others.

From the server status page, you will also be able to see, for each client, a log of all commands sent to and reports received from that client in the most recent session.

To facilitate development and testing I wrote my program to, if compiled with a special (-DDUMMY) define, include mock implementations for the mraa_aio_ functionality, enabling me to do most of my testing on my desktop. I then modified my Makefile run the command "uname -r", check for the presence of the string "edison" in that output, and if not found, build with a rule that passed the -DDUMMY flag to gcc.
Part 2: Authenticated TLS Session Encryption

Write a program (called lab4c_tls) that:

    builds and runs on your Edison
    is based on the remote logging appliance build in part 1
    operates by:
        opening a TLS connection to the server at the specified address and port.
        sending your student ID followed by a newline
        send temperature reports over the connection
        process commands reveived over the connection
        the last command sent by the server will be OFF

The ID number will appear in the TLS server log (follow the TLS server URL), and will permit you to find the reports for your sessions.

Note that you may choose to:

    write two versions of the program
    write a single program that can be compiled to produce two different executables
    write a single executable that implements both functionalities, and chooses which based on the name by which it was invoked. In this last case, your Makefile should produce two different links (with the required names) to that program.

SUMMARY OF EXIT CODES:

    0: successful run
    1: invalid command-line parameters (e.g. unrecognized parameter, no such host)
    2: other run-time failures
