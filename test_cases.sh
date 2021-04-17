#!/bin/bash

echo '\nFILES IN ROOT DIRECTORY'
ls ..
echo '\nHOSTNAME OF CHILD NAMESPACE'
hostname
echo '\nVIEW OF PROCESSES IN PID NAMESPACE'
ps
echo '\nLIMITING MEMORY IN CONTAINER'
python /code/memory_hungry.py
echo '\nISOLATION OF NETWORK NAMESPACE'
python /code/server.py $1
