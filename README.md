procs - Process checkpointing and migration
===========================================

*WARNING*: This is mostly a prototype, so many thing may not work
properly.

Description
-----------

*procs* is a software for checkpointing and process migration. It is
composed of two programs: *procs* and *procsd*. *procs*, as a standalone
program, can save process to files and restore them, and as the client
to *procsd*, it can migrate process to another computer either
instantaneously or saving its state before migrating. As such, it
provides a rudimentary program that it is able to freeze a process and
save its state, allowing to have a backup file if the program fails, or
a checkpoint in memory if used in conjuction with *procsd*. It can only
migrate memory, the registers, open files, and the standard streams.

Although the process will be saved to a file or sent to another machine,
it will not behave properly if it uses any other resource from the
environment.

#### Supported systems
GNU/Linux is supported in both x86 and AMD64 flavours.

Installation
------------

Download the code and issue:

				make

on the command line. Three executables files will be created on the
_bin_ subfolder. Copy all three of them to _/usr/bin_.
After that, create the directory _/etc/*procs*_ for *procsd* to work
correctly.

Running
--------

The daemon, *procsd*, can be run simply doing:

				procsd SERVER

on the command line. For example, SERVER could be 192.168.1.1, if you
want the server to bind to that IP. The client, *procs*, has many
options, all explained in the _man_ pages provided. For example, you can
save a process to a file executing:

				procs save PID -o FILE

where PID and FILE are the process ID of the process to save and FILE is
the name of the checkpoint file where you want to save the processs.

Examples
--------

				procs save 15325 -o tmp
save process with PID 15325 to file tmp

				procs restore tmp
restores process checkpointed in file tmp to current terminal

				procs receive
wait for next process to arrive and execute in current terminal

				procs send -n test -s 192.56.1.101
send process named test to server 192.56.1.101 running on default port

Other information
-----------------

#### License
GNU GPLv3. See LICENSE 

#### Things missing
See TODO.md

#### Use cases

This project is geared towards providing a portable system which
provides transparent checkpoint and instant migration between systems,
without restrains on the processes. As such, it's purpose is to provide
fault tolerance and to distribute the load between different systems.

Implementation and contributing
-------------------------------

Pull requests are welcomed and encouraged. Patches will be viewed
sternly but still are received and gladly used.

#### General
The procs software works mostly as following. First, the procs client is
executed by the user, and the process to be restored or sent is
calculated (if process is named by name or by procsd ID). It it's going
to send or save a process, procs calls functions in save to issue an
stop to the process and gets its state by means of ptrace. After the
process state has been obtained it is written to a file or sent to the
local or remote server which will store it indefinitely.

If a process is going to be restored, the procs client calls functions
in loader that call virtualizer with the process state. It then, reads
all that and injects necessary code in assembly to the same program to
be able to restore all state by jumping to it.

The procs server can communicate to the procs client via a UNIX socket
or via Internet sockets. Either way, each option is documented on the
_man_ of each program.

#### Modules

* *balancer*, where most procsd and server is implemented
* *loader*, where the code necessary to call virtualizer resides
* *saver*, which has the code to save a proces
* *sender*, responsible for sending checkpoints to server
* *utils*, different functions needed by the code
* *virtualizer*, in charge of restoring process by injecting assembly
* *procs*, which is the user interface

#### Contributors
Sebastián Ventura

#### Special thanks
Tim Post, creator of CryoPID in which this project is largely based.
