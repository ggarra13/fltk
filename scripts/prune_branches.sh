#!/usr/bin/env bash

git fetch --all --prune && git branch -vv | grep ': gone]' | awk '{print $1}' | xargs git branch -D
