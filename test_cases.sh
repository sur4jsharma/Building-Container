#!/bin/bash

echo '\nFILES IN ROOT DIRECTORY'
ls ..
echo '\nHOSTNAME OF CHILD NAMESPACE'
hostname
echo '\nVIEW OF PROCESSES IN PID NAMESPACE'
ps
echo '\nLIMITING MEMORY IN CONTAINER'
PYTHONHASHSEED=0 python2 /code/memory_hungry.py
echo '\nISOLATION OF NETWORK NAMESPACE'
PYTHONHASHSEED=0 python2 /code/server.py $1
