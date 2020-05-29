### Async-Linux

This repository contains a proof-of-concept quality implementation for a fully async Linux system call use. It contains only the first access layer; that wrapping the actual io\_uring calls to things that can be co\_awaited. It requires everything to be called from within a coroutine itself, and each system call needs to be co\_awaited - it is not a movable type, nor does it support blocking gets or something of the sort. I've used this in a separate project to implement a HTTP client with DNS resolver, so it is tested. Keep in mind that all of this code is not well tested and works on some servers. 

Enjoy!
