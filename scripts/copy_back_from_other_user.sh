#!/usr/bin/env bash
rm -rf /home/avo/Projects/elk-rewrite/fuzzing
sudo cp -r -T /home/fuzz/fuzzing /home/avo/Projects/elk-rewrite/fuzzing
sudo chown -R avo:avo /home/avo/Projects/elk-rewrite/fuzzing