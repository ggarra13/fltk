#!/usr/bin/env bash

cp -rf merging_config ../.git/config
git fetch --all
git switch -c original upstream/master
