#!/usr/bin/env bash
app="kcachegrind"
for i in 32 48 64 128; do
    ksvgtopng5 $i $i $app.svg $i-apps-$app.png
done
