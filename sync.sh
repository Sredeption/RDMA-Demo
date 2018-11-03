#!/usr/bin/env bash

rsync -a -e "ssh -p 3022" --exclude-from '.gitignore' /home/issac/Workspace/DistributedSystem/Migration/RDMA-Demo hkucs@localhost:/home/hkucs/Migration