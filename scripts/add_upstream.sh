#!/usr/bin/env bash

cp -rf merging_config ../.git/config
git fetch --all
git switch -c original upstream/master
git switch -c moal moal_emscripten/emscripten
